/* games/circuit/circuit.c — "CIRCUIT": a NIGHT textured 3D racer, free driving. PORTED onto the
 * engine's 3D layer: the camera is eng_camera_look, road/grass/barrier corners project through
 * eng_project (so textured quads stay glued), and rival cars are an eng_mesh drawn with eng_mesh_draw
 * (recolored per rival via the tint). The player car is a hand-drawn steering SPRITE; a scrolling
 * neon skyline + stars + moon sit on the horizon; headlights pool on the road. Race 3 laps for
 * position; miss a curve -> barrier crash; bump a rival -> bounce.
 * Controls: UP/A accelerate, DOWN/B brake, LEFT/RIGHT steer, START play/restart.
 */
#include "engine.h"
#include <stdio.h>
#include <math.h>

#define NSEG     240
#define ROADW    8.0f
#define GRASSW   12.0f
#define BARRIER_H 2.6f
#define FL       520.0f     /* focal length: derives the camera FOV + sizes screen-space glows */
#define DRAWDIST 120
#define RX       140.0f
#define RZ       110.0f
#define HILLA    4.0f
#define WIGA     22.0f
#define CAM_BACK_D 9.0f
#define CAM_HEIGHT 4.0f
#define LOOK_FWD   11.0f
#define LOOK_UP    1.6f
#define MAXSPD   95.0f
#define GRASS_CAP 34.0f
#define ACCEL    34.0f
#define BRAKE    78.0f
#define FRICTION 20.0f
#define TURN     2.3f
#define NR       4
#define LAPS     3
#define CARFW    128
#define CARFH    88
#define SKY_W    1600
#define SKY_H    160
#define NSTAR    90
#define PI2      6.2831853f

enum { S_TITLE, S_PLAY, S_CRASH, S_DONE };

/* palette (night) */
#define C_SKY_T  ENG_RGB(8, 10, 30)
#define C_SKY_B  ENG_RGB(46, 30, 74)
#define C_HUD    ENG_RGB(240, 245, 255)
#define C_MUTE   ENG_RGB(150, 160, 180)
#define C_ACC    ENG_RGB(110, 235, 200)
#define C_WARN   ENG_RGB(245, 90, 80)
#define C_GLASS  ENG_RGB(30, 40, 66)
#define C_DARK   ENG_RGB(20, 20, 26)

typedef eng_vec3 V3;
static V3 v3(float x,float y,float z){ return eng_v3(x,y,z); }
static V3 vadd(V3 a,V3 b){return v3(a.x+b.x,a.y+b.y,a.z+b.z);}
static V3 vsub(V3 a,V3 b){return v3(a.x-b.x,a.y-b.y,a.z-b.z);}
static V3 vscl(V3 a,float s){return v3(a.x*s,a.y*s,a.z*s);}
static float vdot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static V3 vcross(V3 a,V3 b){return v3(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
static V3 vnorm(V3 a){float l=sqrtf(vdot(a,a));if(l<1e-6f)l=1;return vscl(a,1.0f/l);}

static int    W, H, st;
static V3     car; static float hd, spd, leanf, circ_fov;
static int    seg, prevseg, lap, finish_pos;
static float  laptime, best, crash_t;
static V3     lastPos; static float lastHd; static int lastSeg;
static V3     P[NSEG], LAT[NSEG], LEFT[NSEG], RIGHT[NSEG], LEFTG[NSEG], RIGHTG[NSEG];
static float  HDG[NSEG];
static struct { float s, lane, base, spd; int lap; eng_color col; } riv[NR];
static int    star_x[NSTAR], star_y[NSTAR];
static eng_mesh *g_car;               /* one recolorable car mesh, drawn per rival */
static eng_image *tex_road, *tex_grass, *tex_barrier, *tex_car, *tex_skyline, *tex_glow;
static eng_sound *sfx_lap, *sfx_crash, *sfx_engine;

/* car mesh: body/nose/roof tris are `tint` (recolored per rival); glass/spoiler/wheels are fixed */
static void build_car(void){
	g_car=eng_mesh_new();
	eng_mesh_box (g_car, v3(-0.8f,0.15f,-2.0f), v3(0.8f,0.6f,1.6f), ENG_WHITE, 1);   /* body */
	eng_mesh_quad(g_car, v3(-0.8f,0.6f,1.6f),v3(0.8f,0.6f,1.6f),v3(0.5f,0.32f,2.35f),v3(-0.5f,0.32f,2.35f), ENG_WHITE,1); /* nose */
	eng_mesh_box (g_car, v3(-0.62f,0.6f,-1.1f), v3(0.62f,1.05f,0.5f), C_GLASS, 0);   /* cabin */
	eng_mesh_quad(g_car, v3(-0.62f,1.05f,-1.1f),v3(0.62f,1.05f,-1.1f),v3(0.62f,1.05f,0.5f),v3(-0.62f,1.05f,0.5f), ENG_WHITE,1); /* roof */
	eng_mesh_quad(g_car, v3(-0.9f,0.98f,-1.8f),v3(0.9f,0.98f,-1.8f),v3(0.9f,0.92f,-2.2f),v3(-0.9f,0.92f,-2.2f), C_DARK,0); /* spoiler */
	eng_mesh_box (g_car, v3(-0.98f,0.0f,0.7f),  v3(-0.72f,0.5f,1.5f),  C_DARK, 0);   /* wheels */
	eng_mesh_box (g_car, v3( 0.72f,0.0f,0.7f),  v3( 0.98f,0.5f,1.5f),  C_DARK, 0);
	eng_mesh_box (g_car, v3(-0.98f,0.0f,-1.8f), v3(-0.72f,0.5f,-1.0f), C_DARK, 0);
	eng_mesh_box (g_car, v3( 0.72f,0.0f,-1.8f), v3( 0.98f,0.5f,-1.0f), C_DARK, 0);
}

static void build_track(void){
	for(int i=0;i<NSEG;i++){ float th=PI2*i/NSEG; P[i]=v3(RX*sinf(th)+WIGA*sinf(3*th), HILLA*sinf(2*th), RZ*cosf(th)); }
	for(int i=0;i<NSEG;i++){
		V3 fwd=vnorm(vsub(P[(i+1)%NSEG],P[(i-1+NSEG)%NSEG]));
		LAT[i]=vnorm(vcross(v3(0,1,0),fwd)); HDG[i]=atan2f(fwd.x,fwd.z);
		LEFT[i]=vadd(P[i],vscl(LAT[i],ROADW));  RIGHT[i]=vsub(P[i],vscl(LAT[i],ROADW));
		LEFTG[i]=vadd(P[i],vscl(LAT[i],ROADW+GRASSW)); RIGHTG[i]=vsub(P[i],vscl(LAT[i],ROADW+GRASSW));
	}
}
static V3 track_at(const V3*arr,float s){
	float m=fmodf(s,NSEG); if(m<0)m+=NSEG; int i=(int)m; float f=m-i; int j=(i+1)%NSEG;
	return vadd(vscl(arr[i],1.0f-f),vscl(arr[j],f));
}
static void place_at_seg(int i){ car=P[i]; hd=HDG[i]; spd=0; seg=i; prevseg=i; lastPos=car; lastHd=hd; lastSeg=i; }
static void reset_game(void){
	lap=1; laptime=0; crash_t=0; leanf=0; finish_pos=0; place_at_seg(0);
	for(int r=0;r<NR;r++){ riv[r].s=8.0f+r*7.0f; riv[r].base=(r%2?0.5f:-0.5f); riv[r].lane=riv[r].base;
		riv[r].spd=8.0f+r*1.3f; riv[r].lap=0; }
	riv[0].col=ENG_RGB(70,130,220); riv[1].col=ENG_RGB(90,200,110);
	riv[2].col=ENG_RGB(220,180,60); riv[3].col=ENG_RGB(180,90,200);
	st=S_PLAY;
}
static void on_init(void){
	W=eng_width(); H=eng_height();
	build_track(); build_car();
	eng_light(v3(-0.3f,0.5f,-0.4f), 0.25f);               /* dim cool moonlight */
	circ_fov=2.0f*atanf((H*0.5f)/FL)*57.29578f;            /* FOV that reproduces FL=520 */
	tex_road=eng_image_from_png("circuit/road.png"); tex_grass=eng_image_from_png("circuit/grass.png");
	tex_barrier=eng_image_from_png("circuit/barrier.png"); tex_car=eng_image_from_png("circuit/car.png");
	tex_skyline=eng_image_from_png("circuit/skyline.png"); tex_glow=eng_image_from_png("circuit/glow.png");
	sfx_lap=eng_sound_load("sfx/score.wav"); sfx_crash=eng_sound_load("sfx/hit.wav");
	sfx_engine=eng_sound_load("sfx/engine.wav");
	unsigned s=12345;
	for(int i=0;i<NSTAR;i++){ s=s*1103515245u+12345u; star_x[i]=(s>>9)%W; s=s*1103515245u+12345u; star_y[i]=(s>>9)%(H/2); }
	best=0; reset_game(); st=S_TITLE;
}
static int nearest_seg(void){
	int bi=seg; float bd=1e30f;
	for(int k=-4;k<=12;k++){ int i=((seg+k)%NSEG+NSEG)%NSEG; float dx=car.x-P[i].x,dz=car.z-P[i].z,d=dx*dx+dz*dz; if(d<bd){bd=d;bi=i;} }
	return bi;
}
static void do_crash(void){ spd=0; crash_t=1.1f; st=S_CRASH; if(sfx_crash) eng_sound_play(sfx_crash,0.7f); }
static int obb_hit(V3 rp,V3 rf,V3 rr){ V3 rel=vsub(car,rp); return fabsf(vdot(rel,rf))<3.8f && fabsf(vdot(rel,rr))<1.9f; }
static void bounce_off(int r,V3 rp,V3 rf,V3 rr){
	float nx=car.x-rp.x, nz=car.z-rp.z, nl=sqrtf(nx*nx+nz*nz);
	if(nl<1e-3f){ nx=rr.x; nz=rr.z; nl=1; } nx/=nl; nz/=nl;
	for(int it=0; it<16 && obb_hit(rp,rf,rr); it++){ car.x+=nx*0.5f; car.z+=nz*0.5f; }
	float pvx=sinf(hd)*spd, pvz=cosf(hd)*spd, vn=pvx*nx+pvz*nz;
	if(vn<0){ pvx-=1.4f*vn*nx; pvz-=1.4f*vn*nz; }
	spd=sqrtf(pvx*pvx+pvz*pvz)*0.8f; if(spd>0.01f) hd=atan2f(pvx,pvz);
	float side=vdot(vsub(car,rp),rr)>0?1.0f:-1.0f; riv[r].lane+=side*0.35f;
	if(riv[r].lane>1.0f) riv[r].lane=1.0f;
	if(riv[r].lane<-1.0f) riv[r].lane=-1.0f;
	if(sfx_crash) eng_sound_play(sfx_crash,0.4f);
}
static void on_update(float dt){
	if(st==S_TITLE||st==S_DONE){ if(eng_just_pressed(ENG_START)){reset_game(); if(sfx_engine)eng_music_play(sfx_engine,0.30f);} return; }
	if(st==S_CRASH){ crash_t-=dt; if(crash_t<=0){ car=lastPos; hd=lastHd; seg=lastSeg; prevseg=lastSeg; spd=0; st=S_PLAY; } return; }
	int touch=eng_pointer_pressed(); int tx=-1,ty;
	if(touch) eng_pointer(&tx,&ty);
	int accel=eng_pressed(ENG_UP)||eng_pressed(ENG_A)||(touch&&tx>=0);
	int brk=eng_pressed(ENG_DOWN)||eng_pressed(ENG_B);
	if(accel) spd+=ACCEL*dt; else spd-=FRICTION*dt;
	if(brk) spd-=BRAKE*dt;
	if(spd<0) spd=0;
	float steer=0;
	if(eng_pressed(ENG_LEFT))  steer-=1;
	if(eng_pressed(ENG_RIGHT)) steer+=1;
	if(touch&&tx>=0) steer+=(tx-W/2)/(float)(W/2);
	if(steer<-1)steer=-1;
	if(steer>1)steer=1;
	leanf += (steer-leanf)*fminf(1.0f,dt*6.0f);
	float tf=spd/16.0f; if(tf>1)tf=1;
	hd += steer*TURN*dt*tf;
	car.x += sinf(hd)*spd*dt;  car.z += cosf(hd)*spd*dt;
	seg=nearest_seg(); car.y=P[seg].y;
	float a=fabsf(vdot(vsub(car,P[seg]),LAT[seg]));
	if(a > ROADW+GRASSW-1.0f){ do_crash(); return; }
	if(a > ROADW){ if(spd>GRASS_CAP) spd-=(spd-GRASS_CAP)*5.0f*dt; }
	else { lastPos=car; lastHd=hd; lastSeg=seg; }
	for(int r=0;r<NR;r++){
		riv[r].s += riv[r].spd*dt; if(riv[r].s>=NSEG){ riv[r].s-=NSEG; riv[r].lap++; }
		riv[r].lane += (riv[r].base-riv[r].lane)*(dt*1.5f);
		V3 rp=vadd(track_at(P,riv[r].s), vscl(track_at(LAT,riv[r].s), riv[r].lane*ROADW));
		V3 rf=vnorm(vsub(track_at(P,riv[r].s+0.5f),track_at(P,riv[r].s-0.5f)));
		V3 rr=vnorm(vcross(v3(0,1,0),rf));
		if(obb_hit(rp,rf,rr)) bounce_off(r,rp,rf,rr);
	}
	laptime+=dt;
	if(prevseg>=NSEG*3/4 && seg<NSEG/4){
		lap++; if(best==0||laptime<best)best=laptime; laptime=0; if(sfx_lap)eng_sound_play(sfx_lap,0.6f);
		if(lap>LAPS){ float me=(LAPS)*(float)NSEG; int pos=1;
			for(int r=0;r<NR;r++){ float rp=riv[r].lap*(float)NSEG+riv[r].s; if(rp>me)pos++; } finish_pos=pos; st=S_DONE; }
	}
	prevseg=seg;
}

/* ---- 3D via the engine camera (eng_project) ---- */
static int fog_shade(float z){int s=256-(int)(z*0.7f);return s<50?50:(s>256?256:s);}
static void quad_s(V3 p0,V3 p1,V3 p2,V3 p3,const eng_image*tex,
                   float u0,float v0,float u1,float v1,float u2,float v2,float u3,float v3f,int sh){
	float x0,y0,z0,x1,y1,z1,x2,y2,z2,x3,y3,z3;
	if(!eng_project(p0,&x0,&y0,&z0)||!eng_project(p1,&x1,&y1,&z1)||
	   !eng_project(p2,&x2,&y2,&z2)||!eng_project(p3,&x3,&y3,&z3)) return;
	eng_vtx_tex a={x0,y0,z0,u0,v0},b={x1,y1,z1,u1,v1},c={x2,y2,z2,u2,v2},d={x3,y3,z3,u3,v3f};
	eng_tri_tex(a,b,c,tex,sh); eng_tri_tex(a,c,d,tex,sh);
}
static void quad_tex(V3 p0,V3 p1,V3 p2,V3 p3,const eng_image*tex,
                     float u0,float v0,float u1,float v1,float u2,float v2,float u3,float v3f){
	V3 mid=vscl(vadd(vadd(p0,p1),vadd(p2,p3)),0.25f);
	float mx,my,miz; if(!eng_project(mid,&mx,&my,&miz)) return;
	quad_s(p0,p1,p2,p3,tex,u0,v0,u1,v1,u2,v2,u3,v3f, fog_shade(1.0f/miz));
}
static void tail_light(V3 wp){
	float sx,sy,iz; if(!eng_project(wp,&sx,&sy,&iz)) return;
	int rr=(int)(1.3f*FL*iz); if(rr<2)rr=2;
	eng_circle_fill((int)sx,(int)sy,rr,ENG_RGB(120,10,10));
	eng_circle_fill((int)sx,(int)sy,rr/2>1?rr/2:1,ENG_RGB(255,90,70));
}

static void on_draw_background(void){
	for(int y=0;y<H/2;y++){
		float t=y/(float)(H/2);
		int r=(int)(((C_SKY_T>>16&0xff))*(1-t)+((C_SKY_B>>16&0xff))*t);
		int g=(int)(((C_SKY_T>>8&0xff))*(1-t)+((C_SKY_B>>8&0xff))*t);
		int b=(int)(((C_SKY_T&0xff))*(1-t)+((C_SKY_B&0xff))*t);
		eng_rect_fill(0,y,W,1,ENG_RGB(r,g,b));
	}
	float pan=(st==S_TITLE)?0.0f:-fmodf(hd,PI2)/PI2*SKY_W;
	int spanx=(int)(pan*0.25f);
	for(int i=0;i<NSTAR;i++){ int x=((star_x[i]+spanx)%W+W)%W; eng_rect_fill(x,star_y[i],2,2,ENG_RGB(200,205,230)); }
	int mx=(((int)(W*0.72f+pan*0.4f))%W+W)%W;
	eng_circle_fill(mx,64,26,ENG_RGB(60,70,110)); eng_circle_fill(mx,64,22,ENG_RGB(225,230,245));
	if(tex_skyline){ int cy=(H/2+30)-SKY_H/2, base=(int)pan; for(int k=-1;k<=1;k++) eng_draw_image(tex_skyline, base+SKY_W/2+k*SKY_W, cy, ENG_WHITE); }
	for(int y=H/2;y<H;y++){ float t=(y-H/2)/(float)(H-H/2);
		int r=(int)(42*(1-t)+15*t), g=(int)(44*(1-t)+18*t), b=(int)(52*(1-t)+26*t); eng_rect_fill(0,y,W,1,ENG_RGB(r,g,b)); }
	if(st==S_TITLE) return;

	/* chase camera through the engine's 3D layer (road + cars now share it) */
	V3 fwd=v3(sinf(hd),0,cosf(hd));
	V3 eye=vadd(vsub(car,vscl(fwd,CAM_BACK_D)),v3(0,CAM_HEIGHT,0));
	V3 look=vadd(vadd(car,vscl(fwd,LOOK_FWD)),v3(0,LOOK_UP,0));
	eng_camera_look(eye, look, v3(0,1,0), circ_fov);

	eng_zclear();
	int start=seg-3;
	for(int k=0;k<=DRAWDIST;k++){
		int i=((start+k)%NSEG+NSEG)%NSEG, j=(i+1)%NSEG;
		float vi=(start+k)*1.0f, vj=vi+1.0f;
		quad_tex(LEFTG[i],LEFT[i],LEFT[j],LEFTG[j],tex_grass,0,vi*2,3,vi*2,3,vj*2,0,vj*2);
		quad_tex(LEFT[i],RIGHT[i],RIGHT[j],LEFT[j],tex_road,0,vi,1,vi,1,vj,0,vj);
		quad_tex(RIGHT[i],RIGHTG[i],RIGHTG[j],RIGHT[j],tex_grass,3,vi*2,0,vi*2,0,vj*2,3,vj*2);
		V3 lu=vadd(LEFTG[i],v3(0,BARRIER_H,0)), lu2=vadd(LEFTG[j],v3(0,BARRIER_H,0));
		V3 ru=vadd(RIGHTG[i],v3(0,BARRIER_H,0)), ru2=vadd(RIGHTG[j],v3(0,BARRIER_H,0));
		int bsh=235-k; if(bsh<110)bsh=110;
		quad_s(lu,LEFTG[i],LEFTG[j],lu2,tex_barrier,vi,0,vi,1,vj,1,vj,0,bsh);
		quad_s(RIGHTG[i],ru,ru2,RIGHTG[j],tex_barrier,vi,1,vi,0,vj,0,vj,1,bsh);
	}
	for(int r=0;r<NR;r++){
		V3 rp=vadd(track_at(P,riv[r].s),vscl(track_at(LAT,riv[r].s),riv[r].lane*ROADW));
		V3 rf=vnorm(vsub(track_at(P,riv[r].s+0.5f),track_at(P,riv[r].s-0.5f)));
		V3 rr=vnorm(vcross(v3(0,1,0),rf));
		eng_mesh_draw(g_car, rp, atan2f(rf.x,rf.z), 0.0f, 0.0f, 1.0f, riv[r].col);
		V3 rear=vsub(rp,vscl(rf,2.0f)); rear.y+=0.55f;
		tail_light(vadd(rear,vscl(rr,0.6f))); tail_light(vsub(rear,vscl(rr,0.6f)));
	}
	if(tex_glow){
		eng_draw_image(tex_glow, W/2-70, (int)(H*0.60f), ENG_RGB(255,235,180));
		eng_draw_image(tex_glow, W/2+70, (int)(H*0.60f), ENG_RGB(255,235,180));
	}
	if(tex_car){
		int fr=2+(int)(leanf*2.0f+(leanf>=0?0.5f:-0.5f)); if(fr<0)fr=0; if(fr>4)fr=4;
		eng_draw_image_cell(tex_car, W/2, H-58, CARFW, CARFH, fr, 0, ENG_WHITE);
	}
}

static void on_draw_overlay(void){
	char buf[48];
	if(st==S_TITLE){
		eng_text_aligned(W/2,H/2-82,84,C_HUD,ENG_ALIGN_CENTER,"CIRCUIT");
		eng_text_aligned(W/2,H/2+4,24,C_MUTE,ENG_ALIGN_CENTER,"UP ACCEL  DOWN BRAKE  L/R STEER");
		eng_text_aligned(W/2,H/2+44,30,C_ACC,ENG_ALIGN_CENTER,"PRESS START");
		return;
	}
	float me=(lap-1)*(float)NSEG+seg; int pos=1;
	for(int r=0;r<NR;r++){ float rp=riv[r].lap*(float)NSEG+riv[r].s; if(rp>me)pos++; }
	snprintf(buf,sizeof buf,"%d KM/H",(int)(spd*3.6f)); eng_text(16,12,30,C_HUD,buf);
	snprintf(buf,sizeof buf,"POS %d/%d",pos,NR+1); eng_text(16,46,26,C_ACC,buf);
	snprintf(buf,sizeof buf,"LAP %d/%d",lap>LAPS?LAPS:lap,LAPS); eng_text_aligned(W-16,12,30,C_HUD,ENG_ALIGN_RIGHT,buf);
	snprintf(buf,sizeof buf,"%.1fs",laptime); eng_text_aligned(W-16,46,24,C_MUTE,ENG_ALIGN_RIGHT,buf);
	if(best>0){snprintf(buf,sizeof buf,"BEST %.1fs",best); eng_text_aligned(W-16,74,22,C_ACC,ENG_ALIGN_RIGHT,buf);}
	float a=fabsf(vdot(vsub(car,P[seg]),LAT[seg]));
	if(st==S_PLAY && a>ROADW) eng_text_aligned(W/2,H-90,26,ENG_RGB(240,210,90),ENG_ALIGN_CENTER,"OFF TRACK");
	if(st==S_CRASH) eng_text_aligned(W/2,H/2-20,80,C_WARN,ENG_ALIGN_CENTER,"CRASH!");
	if(st==S_DONE){
		eng_text_aligned(W/2,H/2-60,80,C_ACC,ENG_ALIGN_CENTER,"FINISH");
		snprintf(buf,sizeof buf,finish_pos==1?"1ST PLACE!":"POSITION %d/%d",finish_pos,NR+1);
		eng_text_aligned(W/2,H/2+20,40,C_HUD,ENG_ALIGN_CENTER,buf);
		eng_text_aligned(W/2,H/2+70,24,C_MUTE,ENG_ALIGN_CENTER,"PRESS START");
	}
}

int main(void){
	static const eng_game game={.title="Circuit",.init=on_init,.update=on_update,
		.draw_background=on_draw_background,.draw_overlay=on_draw_overlay};
	return eng_run(&game);
}
