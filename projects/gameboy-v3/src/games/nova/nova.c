/* games/nova/nova.c — "NOVA": a polygon 3D rail shooter (Star Fox-inspired; original name + models).
 * PORTED onto the engine's 3D layer: geometry is eng_mesh, the camera is eng_camera_look, and the
 * transform/project/shade/rasterize is eng_mesh_draw + eng_project (was hand-coded here). You fly
 * forward through space; enemy ships + asteroids tumble toward you over a parallax starfield. Move
 * the reticle, fire twin lasers. Controls: ARROWS aim, A (z/Space) fire, START play/restart.
 */
#include "engine.h"
#include <stdio.h>
#include <math.h>

#define FL      650.0f     /* focal length (game uses it for unproject + effect sizing; camera matches) */
#define MAXEN   16
#define MAXBOLT 24
#define MAXFX   16
#define NSTARS  140
#define SPAWN_Z 52.0f
#define PASS_Z  -3.0f

enum { S_TITLE, S_PLAY, S_DEAD };

#define FIRE_CD   0.16f
#define BOLT_SPD  70.0f
#define RETICLE_V 560.0f
#define HEALTH0   100

/* palette */
#define C_SPACE  ENG_RGB(6, 8, 18)
#define C_HUD    ENG_RGB(150, 220, 255)
#define C_MUTE   ENG_RGB(150, 160, 180)
#define C_HP     ENG_RGB(90, 220, 110)
#define C_WARN   ENG_RGB(240, 90, 90)
#define C_LASER  ENG_RGB(120, 255, 130)
#define C_CROSS  ENG_RGB(120, 255, 160)
#define C_GO     ENG_RGB(120, 220, 140)

typedef eng_vec3 V3;
static V3 v3(float x,float y,float z){ return eng_v3(x,y,z); }
typedef struct { unsigned char a, b, c; eng_color col; } Tri;

/* --- original low-poly solids: ship = elongated octahedron (nose at -z), rock = lumpy octahedron --- */
static const V3 SHIP_V[6] = {
	{ 0.0f, 0.0f,-1.7f},{ 0.0f, 0.0f, 1.3f},{-1.2f, 0.0f, 0.3f},
	{ 1.2f, 0.0f, 0.3f},{ 0.0f, 0.75f,0.3f},{ 0.0f,-0.65f,0.3f},
};
#define HULL  ENG_RGB(96, 132, 196)
#define HULL2 ENG_RGB(120, 156, 216)
static const Tri SHIP_F[8] = {
	{0,4,3,HULL2},{0,3,5,HULL},{0,5,2,HULL2},{0,2,4,HULL},
	{1,3,4,HULL},{1,5,3,HULL2},{1,2,5,HULL},{1,4,2,HULL2},
};
static const V3 ROCK_V[6] = {
	{-1.5f,0.2f,0.1f},{1.4f,-0.3f,0.2f},{0.2f,1.5f,-0.2f},
	{-0.2f,-1.4f,0.3f},{0.1f,0.1f,1.6f},{-0.1f,0.0f,-1.5f},
};
#define ROCK1 ENG_RGB(150, 140, 128)
#define ROCK2 ENG_RGB(120, 112, 102)
static const Tri ROCK_F[8] = {
	{5,2,0,ROCK1},{5,1,2,ROCK2},{5,3,1,ROCK1},{5,0,3,ROCK2},
	{4,0,2,ROCK2},{4,2,1,ROCK1},{4,1,3,ROCK2},{4,3,0,ROCK1},
};
static const float RADIUS[2] = { 1.5f, 1.8f };     /* ship, rock (collision) */
static eng_mesh *g_mesh[2];                         /* built from the arrays above */
static eng_mesh *g_planet;                          /* smooth-shaded backdrop planet (Gouraud showcase) */
static float g_time;

typedef struct { V3 pos; float ry, rx, dry, drx, spd, dx, dy; int model, alive, hp; } Enemy;
typedef struct { V3 pos, prev, vel; int alive; } Bolt;
typedef struct { V3 pos; float t; int on; } Fx;

static int    W, H, st, score;
static float  health, fire_cd, hurt_fx, retX, retY, nova_fov;
static Enemy  en[MAXEN];
static Bolt   bolt[MAXBOLT];
static Fx     fx[MAXFX];
static V3     star[NSTARS];
static eng_sound *sfx_laser, *sfx_boom, *sfx_hurt;
static uint32_t rng = 0x1234abcdu;
static float frand(void){ rng=rng*1103515245u+12345u; return ((rng>>16)&0x7fff)/32767.0f; }
static float frange(float lo,float hi){ return lo+(hi-lo)*frand(); }

static void spawn_enemy(int i){
	en[i].pos=v3(frange(-7,7),frange(-4,4),SPAWN_Z+frange(0,14));
	en[i].ry=frange(0,6.2f); en[i].rx=frange(0,6.2f);
	en[i].dry=frange(-1.6f,1.6f); en[i].drx=frange(-1.2f,1.2f);
	en[i].spd=frange(9,17); en[i].dx=frange(-0.7f,0.7f); en[i].dy=frange(-0.5f,0.5f);
	en[i].model=(frand()<0.35f)?1:0; en[i].hp=en[i].model?2:1; en[i].alive=1;
}
static void reset_game(void){
	health=HEALTH0; score=0; fire_cd=0; hurt_fx=0; retX=W/2.0f; retY=H/2.0f;
	for(int i=0;i<MAXEN;i++) en[i].alive=0;
	for(int i=0;i<MAXBOLT;i++) bolt[i].alive=0;
	for(int i=0;i<MAXFX;i++) fx[i].on=0;
	for(int i=0;i<6;i++) spawn_enemy(i);
	for(int i=0;i<NSTARS;i++){ star[i]=v3(frange(-24,24),frange(-16,16),frange(1,60)); }
	st=S_PLAY;
}
static void add_fx(V3 p){ for(int i=0;i<MAXFX;i++) if(!fx[i].on){ fx[i].pos=p; fx[i].t=0; fx[i].on=1; return; } }

static void fire(void){
	float az=30.0f;
	V3 aim=v3((retX-W/2.0f)*az/FL, -(retY-H/2.0f)*az/FL, az);
	V3 mz[2]={ {-0.8f,-0.6f,1.2f},{0.8f,-0.6f,1.2f} };
	int made=0;
	for(int m=0;m<2&&made<2;m++) for(int i=0;i<MAXBOLT;i++) if(!bolt[i].alive){
		V3 d=v3(aim.x-mz[m].x,aim.y-mz[m].y,aim.z-mz[m].z);
		float L=sqrtf(d.x*d.x+d.y*d.y+d.z*d.z); if(L<1e-3f)L=1;
		bolt[i].vel=v3(d.x/L*BOLT_SPD,d.y/L*BOLT_SPD,d.z/L*BOLT_SPD);
		bolt[i].pos=mz[m]; bolt[i].prev=mz[m]; bolt[i].alive=1; made++; break;
	}
	if(sfx_laser) eng_sound_play(sfx_laser,0.4f);
}

static void on_update(float dt){
	g_time+=dt;
	if(st!=S_PLAY){ if(eng_just_pressed(ENG_START)){ if(st==S_TITLE)reset_game(); else st=S_TITLE; } return; }
	if(eng_pressed(ENG_LEFT))  retX-=RETICLE_V*dt;
	if(eng_pressed(ENG_RIGHT)) retX+=RETICLE_V*dt;
	if(eng_pressed(ENG_UP))    retY-=RETICLE_V*dt;
	if(eng_pressed(ENG_DOWN))  retY+=RETICLE_V*dt;
	if(eng_pointer_pressed()){ int px,py; eng_pointer(&px,&py); if(px>=0){ retX=px; retY=py; } }
	if(retX<40)retX=40;
	if(retX>W-40)retX=W-40;
	if(retY<40)retY=40;
	if(retY>H-40)retY=H-40;

	fire_cd-=dt; if(hurt_fx>0)hurt_fx-=dt;
	if((eng_pressed(ENG_A)||eng_pointer_pressed())&&fire_cd<=0){ fire(); fire_cd=FIRE_CD; }

	for(int i=0;i<MAXEN;i++){
		if(!en[i].alive)continue;
		en[i].pos.z-=en[i].spd*dt; en[i].pos.x+=en[i].dx*dt; en[i].pos.y+=en[i].dy*dt;
		en[i].ry+=en[i].dry*dt; en[i].rx+=en[i].drx*dt;
		if(en[i].pos.z<1.1f && fabsf(en[i].pos.x)<1.5f && fabsf(en[i].pos.y)<1.5f){
			health-=20; hurt_fx=0.25f; en[i].alive=0; if(sfx_hurt)eng_sound_play(sfx_hurt,0.6f);
		} else if(en[i].pos.z<PASS_Z) en[i].alive=0;
	}
	for(int b=0;b<MAXBOLT;b++){
		if(!bolt[b].alive)continue;
		bolt[b].prev=bolt[b].pos;
		bolt[b].pos.x+=bolt[b].vel.x*dt; bolt[b].pos.y+=bolt[b].vel.y*dt; bolt[b].pos.z+=bolt[b].vel.z*dt;
		if(bolt[b].pos.z>SPAWN_Z+16.0f){ bolt[b].alive=0; continue; }
		for(int i=0;i<MAXEN;i++){
			if(!en[i].alive)continue;
			float dx=bolt[b].pos.x-en[i].pos.x,dy=bolt[b].pos.y-en[i].pos.y,dz=bolt[b].pos.z-en[i].pos.z;
			float rr=RADIUS[en[i].model]; rr*=rr;
			if(dx*dx+dy*dy+dz*dz<rr){
				bolt[b].alive=0;
				if(--en[i].hp<=0){ en[i].alive=0; score+=en[i].model?150:100; add_fx(en[i].pos); if(sfx_boom)eng_sound_play(sfx_boom,0.6f); }
				break;
			}
		}
	}
	for(int i=0;i<MAXFX;i++) if(fx[i].on){ fx[i].t+=dt; if(fx[i].t>0.30f)fx[i].on=0; }
	for(int i=0;i<MAXEN;i++) if(!en[i].alive && frand()<0.02f) spawn_enemy(i);
	if(health<=0){ health=0; st=S_DEAD; }
}

static eng_mesh *build_mesh(const V3 *v,const Tri *f,int nf){
	eng_mesh *m=eng_mesh_new();
	for(int t=0;t<nf;t++) eng_mesh_tri(m, v[f[t].a], v[f[t].b], v[f[t].c], f[t].col, 0);
	return m;
}
static void on_init(void){
	W=eng_width(); H=eng_height();
	g_mesh[0]=build_mesh(SHIP_V,SHIP_F,8); g_mesh[1]=build_mesh(ROCK_V,ROCK_F,8);
	g_planet=eng_mesh_new(); eng_mesh_sphere(g_planet, v3(0,0,0), 20.0f, 3, ENG_RGB(70,110,190), 0);
	eng_light(v3(-0.35f,0.68f,-0.64f), 0.32f);          /* matches nova's old 0.32+0.68*d */
	nova_fov=2.0f*atanf((H*0.5f)/FL)*57.29578f;          /* FOV that reproduces FL=650 */
	sfx_laser=eng_sound_load("sfx/laser.wav"); sfx_boom=eng_sound_load("sfx/boom.wav"); sfx_hurt=eng_sound_load("sfx/hurt.wav");
	reset_game(); st=S_TITLE;
}

static void on_draw_background(void){
	eng_clear(C_SPACE);
	eng_camera_look(v3(0,0,0), v3(0,0,1), v3(0,1,0), nova_fov);   /* fixed camera at origin, looking +z */
	/* parallax starfield (projected through the engine camera; behind the 3D) */
	for(int i=0;i<NSTARS;i++){
		star[i].z-=7.0f*0.016f;
		if(star[i].z<0.5f){ star[i]=v3(frange(-24,24),frange(-16,16),60.0f); }
		float sx,sy,iz; if(!eng_project(star[i],&sx,&sy,&iz)) continue;
		if(sx<0||sx>=W||sy<0||sy>=H) continue;
		int b=60+(int)(195*(1.0f-star[i].z/60.0f));
		eng_rect_fill((int)sx,(int)sy,2,2,ENG_RGB(b,b,b+20>255?255:b+20));
	}
	if(st!=S_PLAY && st!=S_DEAD) return;

	eng_zclear();
	eng_mesh_draw(g_planet, v3(-26,14,120), g_time*0.06f, 0.35f, 0.0f, 1.0f, ENG_WHITE);   /* distant rotating planet, smooth-shaded */
	for(int i=0;i<MAXEN;i++) if(en[i].alive) eng_mesh_draw(g_mesh[en[i].model], en[i].pos, en[i].ry, en[i].rx, 0.0f, 1.0f, ENG_WHITE);

	for(int b=0;b<MAXBOLT;b++){
		if(!bolt[b].alive)continue;
		float sx,sy,iz,px,py,piz;
		if(!eng_project(bolt[b].pos,&sx,&sy,&iz)) continue;
		if(!eng_project(bolt[b].prev,&px,&py,&piz)){ px=sx; py=sy; }
		eng_line((int)px,(int)py,(int)sx,(int)sy,C_LASER);
		int r=(int)(90.0f*iz)+1; eng_circle_fill((int)sx,(int)sy,r,C_LASER);
	}
	for(int i=0;i<MAXFX;i++){
		if(!fx[i].on)continue;
		float sx,sy,iz; if(!eng_project(fx[i].pos,&sx,&sy,&iz)) continue;
		int r=(int)((0.4f+fx[i].t*6.0f)*FL*iz); float k=1.0f-fx[i].t/0.30f;
		eng_circle_fill((int)sx,(int)sy,r,ENG_RGB(255,(int)(200*k),60));
	}
}

static void on_draw_overlay(void){
	char buf[48];
	if(st==S_TITLE){
		eng_text_aligned(W/2,H/2-82,84,C_HUD,ENG_ALIGN_CENTER,"NOVA");
		eng_text_aligned(W/2,H/2+4,24,C_MUTE,ENG_ALIGN_CENTER,"ARROWS AIM   A FIRE");
		eng_text_aligned(W/2,H/2+44,30,C_HP,ENG_ALIGN_CENTER,"PRESS START");
		return;
	}
	if(hurt_fx>0){ eng_rect(0,0,W,H,C_WARN); eng_rect(1,1,W-2,H-2,C_WARN); }
	int rx=(int)retX, ry=(int)retY;
	eng_rect_fill(rx-12,ry-1,24,2,C_CROSS); eng_rect_fill(rx-1,ry-12,2,24,C_CROSS); eng_rect(rx-12,ry-12,24,24,C_CROSS);
	snprintf(buf,sizeof buf,"HP %d",(int)health); eng_text(16,12,28,health>30?C_HP:C_WARN,buf);
	snprintf(buf,sizeof buf,"SCORE %d",score); eng_text_aligned(W-16,12,28,C_HUD,ENG_ALIGN_RIGHT,buf);
	if(st==S_DEAD){
		eng_text_aligned(W/2,H/2-40,80,C_WARN,ENG_ALIGN_CENTER,"GAME OVER");
		snprintf(buf,sizeof buf,"SCORE %d",score); eng_text_aligned(W/2,H/2+30,30,C_GO,ENG_ALIGN_CENTER,buf);
		eng_text_aligned(W/2,H/2+66,24,C_MUTE,ENG_ALIGN_CENTER,"PRESS START");
	}
}

int main(void){
	static const eng_game game={ .title="Nova", .init=on_init, .update=on_update,
		.draw_background=on_draw_background, .draw_overlay=on_draw_overlay };
	return eng_run(&game);
}
