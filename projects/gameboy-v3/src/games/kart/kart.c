/* games/kart/kart.c — "TURBO KART": an ORIGINAL kart racer (genre clone, not Nintendo IP) on the
 * engine 3D layer. Designed circuits (Catmull-Rom spline waypoints, arc-length resampled) with
 * VARIABLE WIDTH (wide straights / tight technical corners), BANKED corners, and a JUMP RAMP.
 * Two selectable tracks. DRIFT mini-boost + an 8-item wheel (nitro/nitro-trio/dart/seeker/slick/storm/star/apex),
 * spin-outs + boost pads. Rivals are rotated top-down SPRITES. Race 3 laps for position.
 * Controls: title = L/R pick track, START go. Race = L/R steer, B(hold) drift, A item, DOWN brake.
 */
#include "engine.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define NSEG 240
#define PLAYW 14.0f     /* playable grass margin before spinout */
#define FARW 90.0f      /* ground apron reaches ~here, then hazes into the horizon fog */
#define WIDE 11.0f
#define NARROW 6.5f
#define FL 520.0f
#define DRAWDIST 60
#define CAM_BACK_D 8.5f
#define CAM_HEIGHT 4.4f
#define LOOK_FWD 11.0f
#define LOOK_UP 1.7f
#define CRUISE 54.0f
#define BOOST_ADD 48.0f
#define GRASS_CAP 12.0f
#define TURN 2.5f
#define GRAV 30.0f
#define WALL_H 2.0f      /* guardrail height at the track edge */
#define NR 5
#define CARFW 128
#define CARFH 96
#define NBOX 6
#define NSKID 220
#define MAXHAZ 10
#define NPROJ 3        /* projectiles that can be airborne at once */
#define PI2 6.2831853f

enum { S_TITLE, S_PLAY, S_DONE };
enum { IT_NONE, IT_BOOST, IT_TRIO, IT_DART, IT_SEEKER, IT_SLICK, IT_STORM, IT_STAR, IT_APEX };

#define C_SKY_T ENG_RGB(90,160,235)
#define C_SKY_B ENG_RGB(200,230,255)
#define C_HUD ENG_RGB(255,255,255)
#define C_MUTE ENG_RGB(70,90,120)
#define C_ACC ENG_RGB(255,210,60)
#define C_DRV ENG_RGB(40,60,150)
#define C_HELM ENG_RGB(240,240,245)
#define C_WHEEL ENG_RGB(30,30,34)

static const char *CHAR_NAME[6]={"RUSTY","HOPPER","WHISKERS","BRUNO","VOLT","SHELLY"};
static const eng_color CHAR_COL[6]={ENG_RGB(228,64,64),ENG_RGB(86,196,104),ENG_RGB(150,155,165),ENG_RGB(150,100,60),ENG_RGB(80,200,220),ENG_RGB(70,182,160)};
typedef eng_vec3 V3;
static V3 v3(float x,float y,float z){return eng_v3(x,y,z);}

static V3 vadd(V3 a,V3 b){return v3(a.x+b.x,a.y+b.y,a.z+b.z);}

static V3 vsub(V3 a,V3 b){return v3(a.x-b.x,a.y-b.y,a.z-b.z);}

static V3 vscl(V3 a,float s){return v3(a.x*s,a.y*s,a.z*s);}

static float vdot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}

static V3 vcross(V3 a,V3 b){return v3(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}

static V3 vnorm(V3 a){float l=sqrtf(vdot(a,a));if(l<1e-6f)l=1;return vscl(a,1.0f/l);}

static float terr(float x,float z);      /* ground-height query (heightmap or ambient) — defined below */
static float ambient(float x,float z);   /* procedural sine terrain (terr's fallback backend) */
static float road_elev(float s);         /* authored road height at fractional segment s */
static void  bake_heightmap(void);        /* build the conforming heightmap from the elevation profile */
static void draw_flame(int cx,int cy,float sc,int hot);   /* fwd (defined below) */
/* the jump ramp is a real terrain MOUND (grass + road drape over it together) rather than a road slab
 * floating +6 above flat ground; its world centre is captured in build_track and folded into terr().
 * Wide + gentle so the coarse 20u grass grid tracks it (a tight mound curves faster than the grid can
 * sample, and its linear facets then poke up THROUGH the road). error ~= 300*H/R^2 < the 0.4u gap. */
static float g_rampx=1e9f, g_rampz=1e9f, g_ramp_h=5.0f, g_ramp_r=70.0f;   /* current track's ramp mound (set in build_track) */

/* Tracks are DATA now (JSON files under assets/kart/tracks/, listed by index.json), parsed with cJSON — the
 * authored layer of the industry pipeline (waypoints = the racing-line spline, plus gameplay markers).
 * build_track turns the waypoints into the same P[]/LAT[]/WID[] the whole game drives on. */
#define MAXTRACKS 8
#define MAXWP 40
#define MAXTBOX 12
#define MAXELEV 24
#define MAXREG 12
enum { REG_GRASS, REG_MOUNT, REG_BEACH, REG_WATER };   /* biome per baked cell: ground texture + cross-section (WATER draws the sea plane) */
typedef struct {
	char name[32], meshfile[64];
	int laps, nwp, ramp_seg, nbox, bp_every, bp_from, bp_to, nelev, nregions;
	int has_water, beach_side, has_bridge, bridge_from, bridge_to;   /* water plane + one authored bridge */
	float wide, narrow, ramp_h, ramp_r, water_level, bridge_deck;
	float wp[MAXWP][2];
	struct { int seg; float lane; } box[MAXTBOX];
	float elev[MAXELEV][2];   /* authored road-height profile: (segment, y) control points, smooth+cyclic; empty => flat/ambient */
	struct { int from, to, type; } region[MAXREG];   /* biome ranges over segments; empty => all grass */
} track_def;
static track_def g_tracks[MAXTRACKS]; static int g_ntracks;
static int g_has_water, g_beach_side; static float g_water_level;   /* current track's sea/river level + which side of the road the ocean is on */
static unsigned char seg_bridge[NSEG];                              /* segment is a bridge span (deck decoupled, river below) */
/* Terrain query seam: terr(x,z) is what every ground consumer calls. Backend #1 = the procedural sine
 * field (ambient); backend #2 = a baked heightmap that CONFORMS to the authored elevation + biome
 * cross-sections (built once per track). Swappable so an imported heightmap/mesh can be backend #3. */
static float *g_hmap; static unsigned char *g_rmap;   /* height + biome-id, same baked grid */
static int g_hmw, g_hmh;
static float g_hmx0, g_hmz0, g_hmstep;
static unsigned char seg_region[NSEG];               /* biome per track segment (default grass) */
static track_def *TK;                                        /* current track (set by build_track) */
static int g_ramp_seg=58, g_laps=3, g_bp_every=40, g_bp_from=36, g_bp_to=40;   /* current track's markers */
/* Road surface = an imported OBJ mesh (baked by tools/gen_kart_track.py), drawn window-culled per
 * segment with the game's own per-segment texture; spline stays authoritative for physics (T113-safe). */
#define TRACK_SUB 3
static eng_mesh *g_road_mesh; static int g_road_ok, g_road_tps=TRACK_SUB*2;

static int W,H,st,sel_track,cur_track,p_accel,sel_char;
static V3 car; static float hd,spd,leanf,g_time,air,air_vy;
static float drift,drift_charge,boost_t,spin_t,shake;
static V3 skid[NSKID]; static int skidn;
static int seg,prevseg,lap,finish_pos,item,lastSeg;
static float laptime,best;
static V3 P[NSEG],LAT[NSEG],LEFT[NSEG],RIGHT[NSEG],LEFTG[NSEG],RIGHTG[NSEG];
static float HDG[NSEG],WID[NSEG],BNK[NSEG];
static float tb_x0,tb_x1,tb_z0,tb_z1;   /* track bounds for the minimap */
static struct { float s,lane,base,spd,spin; int lap,charrow; eng_color col; } riv[NR];
static struct { int seg; float lane, respawn; } box[NBOX];
static struct { int on; V3 pos; } haz[MAXHAZ];
static struct { int on; float s,lane,dir,life; int homing,target; } proj[NPROJ];
static int item_charge; static float surge_t, star_t, boost_add=BOOST_ADD;
static eng_mesh *g_tree,*g_kart,*g_wheel,*g_rock[3],*g_crate,*g_shell,*g_banana;
static eng_image *tex_road,*tex_grass,*tex_boost,*tex_ramp,*tex_karts,*tex_banner,*tex_wall,*tex_rock_ground,*tex_sand,*tex_water;
static eng_sound *sfx_lap,*sfx_hit,*sfx_item,*sfx_boost,*sfx_engine;
static unsigned rng=0x51ed3a;
static float frand(void){rng=rng*1103515245u+12345u;return((rng>>16)&0x7fff)/32767.0f;}

static int cdist(int a,int b){int d=a-b; if(d<0)d=-d; return d<NSEG-d?d:NSEG-d;}

static float clampf(float v,float lo,float hi){return v<lo?lo:v>hi?hi:v;}

static float cr1(float a,float b,float c,float e,float t){float t2=t*t,t3=t2*t;
	return 0.5f*((2*b)+(-a+c)*t+(2*a-5*b+4*c-e)*t2+(-a+3*b-3*c+e)*t3);}

/* --- track data loading (cJSON) ------------------------------------------------------------------ */
static char *read_asset(const char *rel){        /* whole asset file -> malloc'd NUL-terminated buffer */
	const char *base=getenv("CANVAS_ASSETS");
	if(!base||!*base) base="/usr/share/canvas";
	char full[512];
	snprintf(full,sizeof full,"%s/%s",base,rel);
	FILE *f=fopen(full,"rb");
	if(!f){ fprintf(stderr,"kart: can't open %s\n",full); return NULL; }
	fseek(f,0,SEEK_END);
	long n=ftell(f);
	fseek(f,0,SEEK_SET);
	if(n<0)n=0;
	char *buf=malloc((size_t)n+1);
	if(!buf){ fclose(f); return NULL; }
	size_t rd=fread(buf,1,(size_t)n,f);
	buf[rd]=0;
	fclose(f);
	return buf;
}

static double jnum(const cJSON *o,const char *k,double d){ const cJSON *x=o?cJSON_GetObjectItem(o,k):NULL; return (x&&cJSON_IsNumber(x))?x->valuedouble:d; }

static int load_track(const char *file, track_def *t){
	char rel[160];
	snprintf(rel,sizeof rel,"kart/tracks/%s",file);
	char *txt=read_asset(rel);
	if(!txt) return 0;
	cJSON *j=cJSON_Parse(txt);
	free(txt);
	if(!j){ fprintf(stderr,"kart: bad JSON in %s\n",file); return 0; }
	memset(t,0,sizeof *t);
	{ char b[48]; snprintf(b,sizeof b,"%s",file); char *dot=strrchr(b,'.'); if(dot)*dot=0;   /* sunset.json -> the baked road mesh */
		snprintf(t->meshfile,sizeof t->meshfile,"kart/tracks/%s.obj",b); }
	const cJSON *nm=cJSON_GetObjectItem(j,"name");
	snprintf(t->name,sizeof t->name,"%s",(nm&&cJSON_IsString(nm))?nm->valuestring:"TRACK");
	t->laps=(int)jnum(j,"laps",3);
	const cJSON *w=cJSON_GetObjectItem(j,"width");
	t->wide=(float)jnum(w,"wide",WIDE);
	t->narrow=(float)jnum(w,"narrow",NARROW);
	const cJSON *rp=cJSON_GetObjectItem(j,"ramp");
	t->ramp_seg=(int)jnum(rp,"seg",58);
	t->ramp_h=(float)jnum(rp,"height",5.0);
	t->ramp_r=(float)jnum(rp,"radius",70.0);
	const cJSON *bp=cJSON_GetObjectItem(j,"boostPads");
	t->bp_every=(int)jnum(bp,"every",40);
	t->bp_from=(int)jnum(bp,"from",36);
	t->bp_to=(int)jnum(bp,"to",40);
	const cJSON *wp=cJSON_GetObjectItem(j,"waypoints"), *pt;
	if(wp&&cJSON_IsArray(wp)) cJSON_ArrayForEach(pt,wp){ if(t->nwp>=MAXWP)break; const cJSON *a=cJSON_GetArrayItem(pt,0),*b=cJSON_GetArrayItem(pt,1);
		if(a&&b){ t->wp[t->nwp][0]=(float)a->valuedouble; t->wp[t->nwp][1]=(float)b->valuedouble; t->nwp++; } }
	const cJSON *bx=cJSON_GetObjectItem(j,"boxes"), *b;
	if(bx&&cJSON_IsArray(bx)) cJSON_ArrayForEach(b,bx){ if(t->nbox>=MAXTBOX)break; t->box[t->nbox].seg=(int)jnum(b,"seg",0); t->box[t->nbox].lane=(float)jnum(b,"lane",0); t->nbox++; }
	const cJSON *el=cJSON_GetObjectItem(j,"elevation"), *ep;   /* optional authored road-height profile */
	if(el&&cJSON_IsArray(el)) cJSON_ArrayForEach(ep,el){ if(t->nelev>=MAXELEV)break; t->elev[t->nelev][0]=(float)jnum(ep,"seg",0); t->elev[t->nelev][1]=(float)jnum(ep,"y",0); t->nelev++; }
	const cJSON *rg=cJSON_GetObjectItem(j,"regions"), *rr;    /* optional biome ranges (grass/mountain/beach) */
	if(rg&&cJSON_IsArray(rg)) cJSON_ArrayForEach(rr,rg){ if(t->nregions>=MAXREG)break;
		const cJSON *ty=cJSON_GetObjectItem(rr,"type"); const char *ts=(ty&&cJSON_IsString(ty))?ty->valuestring:"grass";
		int tid=REG_GRASS; if(!strcmp(ts,"mountain"))tid=REG_MOUNT; else if(!strcmp(ts,"beach"))tid=REG_BEACH;
		t->region[t->nregions].from=(int)jnum(rr,"from",0); t->region[t->nregions].to=(int)jnum(rr,"to",0); t->region[t->nregions].type=tid; t->nregions++; }
	const cJSON *wt=cJSON_GetObjectItem(j,"water");           /* optional sea/river plane */
	if(wt&&cJSON_IsObject(wt)){ t->has_water=1; t->water_level=(float)jnum(wt,"level",0);
		const cJSON *bsd=cJSON_GetObjectItem(wt,"beachSide"); t->beach_side=(bsd&&cJSON_IsString(bsd)&&!strcmp(bsd->valuestring,"left"))?-1:1; }
	const cJSON *br=cJSON_GetObjectItem(j,"bridge");          /* optional bridge (deck decoupled from terrain) */
	if(br&&cJSON_IsObject(br)){ t->has_bridge=1; t->bridge_from=(int)jnum(br,"from",0); t->bridge_to=(int)jnum(br,"to",0); t->bridge_deck=(float)jnum(br,"deckY",5.0); }
	cJSON_Delete(j);
	if(t->nwp<3){ fprintf(stderr,"kart: track %s has too few waypoints\n",file); return 0; }
	return 1;
}

static void load_tracks(void){                    /* read tracks/index.json -> load each listed track */
	g_ntracks=0;
	char *txt=read_asset("kart/tracks/index.json");
	if(txt){ cJSON *j=cJSON_Parse(txt); free(txt);
		if(j&&cJSON_IsArray(j)){ const cJSON *e; cJSON_ArrayForEach(e,j){ if(g_ntracks>=MAXTRACKS)break;
			if(cJSON_IsString(e)&&load_track(e->valuestring,&g_tracks[g_ntracks])) g_ntracks++; } }
		if(j) cJSON_Delete(j);
	}
	if(g_ntracks==0) fprintf(stderr,"kart: no tracks loaded (check assets/kart/tracks/index.json)\n");
}

/* Authored road height at fractional segment s: smooth (smoothstep) interpolation across the profile's
 * (seg,y) control points, treated as a cycle over [0,NSEG). Only called when the track has a profile. */
static float road_elev(float s){
	int nn=TK->nelev;
	if(nn==1) return TK->elev[0][1];
	float m=fmodf(s,NSEG); if(m<0) m+=NSEG;
	for(int a=0;a<nn;a++){
		float s0=TK->elev[a][0];
		float s1=(a+1<nn)?TK->elev[a+1][0]:TK->elev[0][0]+NSEG;   /* last->first wraps past NSEG */
		float mm=m; if(mm<s0) mm+=NSEG;
		if(mm>=s0 && mm<s1){
			float t=(s1>s0)?(mm-s0)/(s1-s0):0.0f, u=t*t*(3.0f-2.0f*t);
			float y0=TK->elev[a][1], y1=(a+1<nn)?TK->elev[a+1][1]:TK->elev[0][1];
			return y0+(y1-y0)*u;
		}
	}
	return TK->elev[0][1];
}
/* Bake the ground into a heightmap that follows the authored road elevation near the track and blends
 * out to the ambient sine landscape away from it (the "terrain conforms to the road" step). Sampled
 * O(1) at runtime by terr(). Nearest-segment search is O(NSEG) per cell but runs once, at track load. */
static void bake_heightmap(void){
	if(g_hmap){ free(g_hmap); g_hmap=NULL; }
	if(g_rmap){ free(g_rmap); g_rmap=NULL; }
	const float M=232.0f;                        /* cover the car-centred grass grid extent (car +-220) + margin */
	g_hmstep=6.0f; g_hmx0=tb_x0-M; g_hmz0=tb_z0-M;
	g_hmw=(int)((tb_x1-tb_x0+2*M)/g_hmstep)+2; if(g_hmw>256) g_hmw=256;
	g_hmh=(int)((tb_z1-tb_z0+2*M)/g_hmstep)+2; if(g_hmh>256) g_hmh=256;
	g_hmap=malloc((size_t)g_hmw*g_hmh*sizeof(float));
	g_rmap=malloc((size_t)g_hmw*g_hmh);
	if(!g_hmap||!g_rmap){ free(g_hmap); free(g_rmap); g_hmap=NULL; g_rmap=NULL; return; }
	const float NEARW=18.0f, FADE=48.0f;         /* <NEARW: road level; beyond NEARW+FADE: the biome cross-section */
	for(int cz=0;cz<g_hmh;cz++) for(int cx=0;cx<g_hmw;cx++){
		float wx=g_hmx0+cx*g_hmstep, wz=g_hmz0+cz*g_hmstep;
		float bd=1e30f; int bi=0;                /* nearest track point -> its biome + road level */
		for(int i=0;i<NSEG;i++){ float ddx=P[i].x-wx,ddz=P[i].z-wz,d=ddx*ddx+ddz*ddz; if(d<bd){bd=d;bi=i;} }
		float dist=sqrtf(bd);
		int reg=seg_region[bi], water=0;
		float t=clampf((dist-NEARW)/FADE,0.0f,1.0f), u=t*t*(3.0f-2.0f*t);
		float h;
		if(seg_bridge[bi]){                        /* river under/around the bridge: all low, water fills to sea level */
			h=g_water_level-5.0f; water=1;
		} else {
			float road=P[bi].y-0.25f;              /* ground level under the road (P.y is road surface = ground+0.25) */
			float far; int ocean=0;
			if(reg==REG_MOUNT)      far=road+clampf((dist-NEARW)*0.9f,0.0f,70.0f);   /* steep walls: road runs a pass */
			else if(reg==REG_BEACH){                                                 /* flat sand; ocean drops away on one side */
				V3 fw=vsub(P[(bi+1)%NSEG],P[(bi-1+NSEG)%NSEG]); float side=(wx-P[bi].x)*fw.z-(wz-P[bi].z)*fw.x;
				if(g_has_water && ((side>0)?1:-1)==g_beach_side){ far=g_water_level-6.0f; ocean=1; } else far=road;
			}
			else                    far=ambient(wx,wz);                              /* grass: the natural roll (never floods) */
			h=road+(far-road)*u;
			if(ocean && h<g_water_level) water=1;                                     /* only the beach ocean side becomes sea */
		}
		g_hmap[cz*g_hmw+cx]=h;
		g_rmap[cz*g_hmw+cx]=(unsigned char)(water?REG_WATER:reg);
	}
}
static void build_track(int which){
	if(which<0||which>=g_ntracks) which=0;
	TK=&g_tracks[which];
	if(g_hmap){ free(g_hmap); g_hmap=NULL; }   /* clear previous track's maps so terr()=ambient while building P[] */
	if(g_rmap){ free(g_rmap); g_rmap=NULL; }
	for(int i=0;i<NSEG;i++) seg_region[i]=REG_GRASS;              /* biome per segment (default grass) */
	for(int r=0;r<TK->nregions;r++){ int a=TK->region[r].from, b=TK->region[r].to, ty=TK->region[r].type;
		if(a<0)a=0;
		if(b>NSEG)b=NSEG;
		if(a<=b){ for(int i=a;i<b;i++) seg_region[i]=(unsigned char)ty; }
		else { for(int i=a;i<NSEG;i++) seg_region[i]=(unsigned char)ty; for(int i=0;i<b;i++) seg_region[i]=(unsigned char)ty; }   /* wraps loop seam */
	}
	g_has_water=(TK->has_water||TK->has_bridge); g_water_level=TK->water_level; g_beach_side=TK->beach_side?TK->beach_side:1;
	for(int i=0;i<NSEG;i++) seg_bridge[i]=0;                      /* bridge span (deck decoupled, river below) */
	if(TK->has_bridge){ int a=TK->bridge_from, b=TK->bridge_to; if(a<0)a=0; if(b>NSEG)b=NSEG;
		if(a<=b){ for(int i=a;i<b;i++) seg_bridge[i]=1; }
		else { for(int i=a;i<NSEG;i++) seg_bridge[i]=1; for(int i=0;i<b;i++) seg_bridge[i]=1; }
	}
	g_ramp_seg=TK->ramp_seg;
	g_ramp_h=TK->ramp_h;
	g_ramp_r=TK->ramp_r;
	g_laps=TK->laps;
	g_bp_every=TK->bp_every;
	g_bp_from=TK->bp_from;
	g_bp_to=TK->bp_to;
	const float (*WP)[2]=TK->wp;
	int nwp=TK->nwp;
	static float dx[2600],dz[2600],cum[2601];
	int n=0, steps=2400/nwp;
	for(int w=0;w<nwp;w++){ int i0=(w-1+nwp)%nwp,i1=w,i2=(w+1)%nwp,i3=(w+2)%nwp;
		for(int t2=0;t2<steps;t2++){ float t=(float)t2/steps;
			dx[n]=cr1(WP[i0][0],WP[i1][0],WP[i2][0],WP[i3][0],t);
			dz[n]=cr1(WP[i0][1],WP[i1][1],WP[i2][1],WP[i3][1],t);
			n++; } }
	cum[0]=0;
	for(int i=0;i<n;i++){int j=(i+1)%n;float a=dx[j]-dx[i],b=dz[j]-dz[i];cum[i+1]=cum[i]+sqrtf(a*a+b*b);}
	float total=cum[n];
	int di=0;
	{ float tg=total*g_ramp_seg/NSEG; int d2=0; while(d2<n-1&&cum[d2+1]<tg)d2++;    /* ramp mound centre (before terr() is sampled) */
		float sl=cum[d2+1]-cum[d2],f=sl>1e-4f?(tg-cum[d2])/sl:0;
		int j2=(d2+1)%n;
		g_rampx=dx[d2]+(dx[j2]-dx[d2])*f;
		g_rampz=dz[d2]+(dz[j2]-dz[d2])*f; }
	for(int i=0;i<NSEG;i++){
		float target=total*i/NSEG;
		while(di<n-1&&cum[di+1]<target)di++;
		float sl=cum[di+1]-cum[di],f=sl>1e-4f?(target-cum[di])/sl:0;
		int j=(di+1)%n;
		float px=dx[di]+(dx[j]-dx[di])*f, pz=dz[di]+(dz[j]-dz[di])*f;
		float y=(TK->nelev>0 ? road_elev((float)i) : ambient(px,pz)) + 0.25f;   /* authored profile, else drape on ambient */
		if(seg_bridge[i]) y=TK->bridge_deck+0.25f;                              /* bridge deck: held above the river, decoupled from terrain */
		P[i]=v3(px, y, pz);
	}
	tb_x0=tb_x1=P[0].x; tb_z0=tb_z1=P[0].z;                  /* track bounds (needed by the heightmap bake) */
	for(int i=1;i<NSEG;i++){ if(P[i].x<tb_x0)tb_x0=P[i].x; if(P[i].x>tb_x1)tb_x1=P[i].x; if(P[i].z<tb_z0)tb_z0=P[i].z; if(P[i].z>tb_z1)tb_z1=P[i].z; }
	if(TK->nelev>0 || TK->nregions>0 || TK->has_bridge || TK->has_water) bake_heightmap();   /* conform terrain to road + biomes + water/bridge */
	float turn[NSEG];
	for(int i=0;i<NSEG;i++){
		V3 a=vnorm(vsub(P[i],P[(i-1+NSEG)%NSEG])), b=vnorm(vsub(P[(i+1)%NSEG],P[i]));
		turn[i]=atan2f(a.x*b.z-a.z*b.x, a.x*b.x+a.z*b.z);
	}
	for(int i=0;i<NSEG;i++){                                   /* smooth curvature over neighbours */
		float st_=0;
		for(int k=-3;k<=3;k++) st_+=turn[((i+k)%NSEG+NSEG)%NSEG];
		st_/=7.0f;
		WID[i]=clampf(TK->wide-fabsf(st_)*55.0f, TK->narrow, TK->wide);
		BNK[i]=clampf(st_*3.4f, -0.30f, 0.30f);
	}
	for(int i=0;i<NSEG;i++){
		V3 fwd=vnorm(vsub(P[(i+1)%NSEG],P[(i-1+NSEG)%NSEG]));
		LAT[i]=vnorm(vcross(v3(0,1,0),fwd));
		HDG[i]=atan2f(fwd.x,fwd.z);
		float wr=WID[i], gr=WID[i]+FARW;
		LEFT[i]=vadd(P[i],vscl(LAT[i],wr));
		LEFT[i].y=terr(LEFT[i].x,LEFT[i].z)+(P[i].y-terr(P[i].x,P[i].z));
		RIGHT[i]=vsub(P[i],vscl(LAT[i],wr));
		RIGHT[i].y=terr(RIGHT[i].x,RIGHT[i].z)+(P[i].y-terr(P[i].x,P[i].z));
		LEFTG[i]=vadd(P[i],vscl(LAT[i],gr));
		LEFTG[i].y=P[i].y+terr(LEFTG[i].x,LEFTG[i].z);
		RIGHTG[i]=vsub(P[i],vscl(LAT[i],gr));
		RIGHTG[i].y=P[i].y+terr(RIGHTG[i].x,RIGHTG[i].z);
	}
	if(g_road_mesh){ eng_mesh_free(g_road_mesh); g_road_mesh=NULL; }
	if(TK->nelev>0 || TK->has_bridge){                       /* authored elevation/bridge move the road -> procedural road (baked OBJ stale) */
		g_road_ok=0;
	} else {                                                 /* flat tracks: imported baked road surface (segment-ordered) */
		g_road_mesh=eng_mesh_from_obj(TK->meshfile);
		g_road_ok = (g_road_mesh && eng_mesh_ntris(g_road_mesh)==NSEG*g_road_tps);   /* stale/missing -> procedural road */
		if(g_road_mesh && !g_road_ok) fprintf(stderr,"kart: %s has %d tris (want %d) — using procedural road\n", TK->meshfile, eng_mesh_ntris(g_road_mesh), NSEG*g_road_tps);
	}
}

static V3 track_at(const V3*a,float s){float m=fmodf(s,NSEG);if(m<0)m+=NSEG;int i=(int)m;float f=m-i;int j=(i+1)%NSEG;return vadd(vscl(a[i],1-f),vscl(a[j],f));}

static float surf_y(int sg,float lat){ V3 p=vadd(P[sg],vscl(LAT[sg],lat)); return terr(p.x,p.z) + (P[sg].y - terr(P[sg].x,P[sg].z)); }

/* height of the rendered GRASS surface at signed lateral offset `lat` (linear across the quad:
 * road edge -> far apron edge). Props/wall use this so they sit exactly on the visible ground. */
static float grass_y(int sg,float lat){ V3 p=vadd(P[sg],vscl(LAT[sg],lat)); return terr(p.x,p.z); }

static float ambient(float x,float z){                                    /* procedural sine landscape + jump-ramp mound */
	float h=6.0f*sinf(x*0.028f)+5.0f*sinf(z*0.023f+1.3f);                   /* rolling far-grass */
	float dx=x-g_rampx, dz=z-g_rampz, d2=dx*dx+dz*dz;                       /* jump-ramp mound */
	if(d2<g_ramp_r*g_ramp_r){ float t=1.0f-sqrtf(d2)/g_ramp_r; h+=g_ramp_h*t*t*(3.0f-2.0f*t); }   /* smoothstep hill */
	return h;
}
static float terr(float x,float z){                                        /* THE ground-height query (seam) */
	if(!g_hmap) return ambient(x,z);                                       /* backend #1: no authored elevation -> ambient */
	float fx=(x-g_hmx0)/g_hmstep, fz=(z-g_hmz0)/g_hmstep;                   /* backend #2: bilinear-sample the conforming heightmap */
	int ix=(int)floorf(fx), iz=(int)floorf(fz);
	if(ix<0||iz<0||ix>=g_hmw-1||iz>=g_hmh-1) return ambient(x,z);           /* outside the baked area */
	float tx=fx-ix, tz=fz-iz; const float *H=g_hmap; int w=g_hmw;
	float a=H[iz*w+ix], b=H[iz*w+ix+1], c=H[(iz+1)*w+ix], d=H[(iz+1)*w+ix+1];
	float top=a+(b-a)*tx, bot=c+(d-c)*tx;
	return top+(bot-top)*tz;
}
static int region_at(float x,float z){                                     /* baked biome id at a world point */
	if(!g_rmap) return REG_GRASS;
	int ix=(int)floorf((x-g_hmx0)/g_hmstep), iz=(int)floorf((z-g_hmz0)/g_hmstep);
	if(ix<0||iz<0||ix>=g_hmw||iz>=g_hmh) return REG_GRASS;
	return g_rmap[iz*g_hmw+ix];
}
static const eng_image *biome_tex(int r){ return r==REG_MOUNT?tex_rock_ground : r==REG_BEACH?tex_sand : r==REG_WATER?tex_water : tex_grass; }

static void reset_game(void){
	lap=1;
	laptime=0;
	leanf=0;
	drift=0;
	drift_charge=0;
	boost_t=0;
	spin_t=0;
	air=0;
	air_vy=0;
	finish_pos=0;
	item=IT_NONE;
	car=P[0];
	hd=HDG[0];
	spd=0;
	seg=0;
	prevseg=0;
	lastSeg=0;
	eng_particles_reset();
	skidn=0;
	shake=0;
	for(int r=0;r<NR;r++){ riv[r].s=6.0f+r*6.0f; riv[r].base=((r%2)?0.55f:-0.55f); riv[r].lane=riv[r].base;
		riv[r].spd=7.0f+r*0.7f;
		riv[r].lap=0;
		riv[r].spin=0; }
	for(int r=0;r<NR;r++){ riv[r].charrow=(sel_char+1+r)%6; riv[r].col=CHAR_COL[riv[r].charrow]; }
	for(int i=0;i<NBOX;i++){ if(i<TK->nbox){ box[i].seg=TK->box[i].seg; box[i].lane=TK->box[i].lane; box[i].respawn=0; } else box[i].respawn=1e9f; }   /* item boxes from the track file */
	for(int i=0;i<MAXHAZ;i++) haz[i].on=0;
	for(int i=0;i<NPROJ;i++)proj[i].on=0;
	item_charge=0;
	surge_t=0;
	star_t=0;
	boost_add=BOOST_ADD;
	st=S_PLAY;
}

static void on_init(void){
	W=eng_width();
	H=eng_height();
	load_tracks();                 /* read the JSON track files (authored waypoints + markers) */
	build_track(0);
	cur_track=0;
	g_tree=eng_mesh_from_obj("kart/tree.obj");
	g_rock[0]=eng_mesh_from_obj("kart/rock1.obj");
	g_rock[1]=eng_mesh_from_obj("kart/rock2.obj");
	g_rock[2]=eng_mesh_from_obj("kart/rock3.obj");
	g_crate=eng_mesh_from_obj("kart/crate.obj");
	g_shell=eng_mesh_from_obj("kart/shell.obj");
	g_banana=eng_mesh_from_obj("kart/banana.obj");
	g_kart=eng_mesh_from_obj("kart/kart.obj");   /* the kart is authored DATA now, not code (edit src/assets/kart/kart.obj) */
	g_wheel=eng_mesh_from_obj("kart/wheel.obj");  /* front wheels drawn separately so they STEER (body stays upright) */
	eng_light(v3(-0.45f,0.8f,-0.35f), 0.34f);
	eng_fog(C_SKY_B, 120.0f, 245.0f);   /* haze == horizon-sky colour so far terrain melts INTO the sky (no white band) */
	tex_road=eng_image_from_png("kart/road.png");
	tex_grass=eng_image_from_png("kart/grass.png");
	tex_rock_ground=eng_image_from_png("kart/rock_ground.png"); tex_sand=eng_image_from_png("kart/sand.png");
	tex_water=eng_image_from_png("kart/water.png");
	tex_boost=eng_image_from_png("kart/boost.png");
	tex_ramp=eng_image_from_png("kart/ramp.png");
	tex_karts=eng_image_from_png("kart/karts.png");
	tex_banner=eng_image_from_png("kart/banner.png");
	tex_wall=eng_image_from_png("kart/wall.png");
	sfx_lap=eng_sound_load("sfx/score.wav");
	sfx_hit=eng_sound_load("sfx/bump.wav");
	sfx_item=eng_sound_load("sfx/ding.wav");
	sfx_boost=eng_sound_load("sfx/kboost.wav");
	sfx_engine=eng_sound_load("sfx/kartmusic.wav");
	best=0;
	reset_game();
	st=S_TITLE;
}

static int nearest_seg(void){
	int bi=seg;
	float bd=1e30f;
	for(int k=-4;k<=12;k++){int i=((seg+k)%NSEG+NSEG)%NSEG;float dx=car.x-P[i].x,dz=car.z-P[i].z,d=dx*dx+dz*dz;if(d<bd){bd=d;bi=i;}}
	return bi;
}

static void spinout(void){ if(star_t>0) return; spin_t=1.0f; boost_t=0; drift=0; drift_charge=0; if(sfx_hit)eng_sound_play(sfx_hit,0.6f);
	for(int q=0;q<12;q++){ float a=q/12.0f*6.2832f; eng_particle(W/2+cosf(a)*8,H-64,cosf(a)*170,sinf(a)*170-50,520,0.6f,5,ENG_RGB(255,255,150),ENG_RGB(255,110,40)); }
	shake=fmaxf(shake,0.5f); }
static int race_pos(void){ float me=(lap-1)*(float)NSEG+seg; int pos=1;
	for(int r=0;r<NR;r++){ float rp=riv[r].lap*(float)NSEG+riv[r].s; if(rp>me)pos++; } return pos; }
static int leader_rival(void){ int b=-1; float bp=-1e30f;
	for(int r=0;r<NR;r++){ float rp=riv[r].lap*(float)NSEG+riv[r].s; if(rp>bp){bp=rp;b=r;} } return b; }
static int free_proj(void){ for(int i=0;i<NPROJ;i++) if(!proj[i].on) return i; return -1; }

/* position-weighted box roll (MK-style): leaders roll mild/defensive, trailers roll strong catch-up */
static int roll_item(int pos){
	static const int IID[8]={IT_BOOST,IT_TRIO,IT_DART,IT_SEEKER,IT_SLICK,IT_STORM,IT_STAR,IT_APEX};
	static const int WT[8][6]={ {5,5,5,4,3,2},{0,1,2,3,3,3},{2,3,3,3,2,1},{1,3,4,4,3,2},
	                            {5,4,3,2,1,1},{0,0,1,2,3,4},{0,0,0,1,2,4},{0,0,0,0,2,4} };
	int p=pos-1;
	if(p<0)p=0;
	if(p>5)p=5;
	int sum=0;
	for(int i=0;i<8;i++) sum+=WT[i][p];
	int r=(int)(frand()*sum);
	for(int i=0;i<8;i++){ r-=WT[i][p]; if(r<0) return IID[i]; }
	return IT_BOOST; }
static void use_item(void){
	float fx=sinf(hd), fz=cosf(hd);
	if(item==IT_BOOST){ float straight=clampf((WID[seg]-TK->narrow)/(TK->wide-TK->narrow),0.0f,1.0f);   /* banked for a straight pays ~2x */
		boost_add=BOOST_ADD*(0.7f+0.6f*straight);
		boost_t=1.0f+0.6f*straight;
		if(sfx_boost)eng_sound_play(sfx_boost,0.6f); }
	else if(item==IT_TRIO){ boost_t=fmaxf(boost_t,1.0f); if(sfx_boost)eng_sound_play(sfx_boost,0.5f);
		item_charge--;
		if(item_charge>0) return; }   /* stays in the slot for 3 uses */
	else if(item==IT_DART){ int pi=free_proj(); if(pi>=0){ int dir=eng_pressed(ENG_DOWN)?-1:1;   /* straight lane-locked shot, DOWN = back-shot */
		proj[pi].on=1;
		proj[pi].s=fmodf(seg+dir*2+NSEG,NSEG);
		proj[pi].lane=vdot(vsub(car,P[seg]),LAT[seg])/fmaxf(WID[seg],0.1f);
		proj[pi].dir=(float)dir;
		proj[pi].life=(float)NSEG;
		proj[pi].homing=0;
		proj[pi].target=-1;
		if(sfx_boost)eng_sound_play(sfx_boost,0.4f); } }
	else if(item==IT_SEEKER){ int pi=free_proj(); if(pi>=0){
		proj[pi].on=1;
		proj[pi].s=fmodf(seg+2,NSEG);
		proj[pi].lane=0;
		proj[pi].dir=1.0f;
		proj[pi].life=(float)(NSEG*2);
		proj[pi].homing=1;
		proj[pi].target=-1;
		if(sfx_boost)eng_sound_play(sfx_boost,0.4f); } }
	else if(item==IT_APEX){ int pi=free_proj(); if(pi>=0){ int lead=(race_pos()==1)?-1:leader_rival();   /* dethrone the leader; forward shot if you ARE leading */
		proj[pi].on=1;
		proj[pi].s=fmodf(seg+2,NSEG);
		proj[pi].lane=0;
		proj[pi].dir=1.0f;
		proj[pi].life=(float)(NSEG*3);
		proj[pi].homing=(lead>=0)?1:0;
		proj[pi].target=lead;
		if(sfx_boost)eng_sound_play(sfx_boost,0.5f); } }
	else if(item==IT_SLICK){ int dir=eng_pressed(ENG_UP)?1:-1; for(int i=0;i<MAXHAZ;i++) if(!haz[i].on){ haz[i].on=1; haz[i].pos=vadd(car,vscl(v3(fx,0,fz),3.0f*dir)); break; } }
	else if(item==IT_STORM){ float me=(lap-1)*(float)NSEG+seg;                                   /* catch-up: spins only karts AHEAD + self-boost */
		for(int r=0;r<NR;r++){ float rp=riv[r].lap*(float)NSEG+riv[r].s; if(rp>me) riv[r].spin=fmaxf(riv[r].spin,0.8f); }
		boost_t=fmaxf(boost_t,0.6f);
		surge_t=0.45f;
		shake=fmaxf(shake,0.4f);
		if(sfx_boost)eng_sound_play(sfx_boost,0.6f); }
	else if(item==IT_STAR){ star_t=6.0f; boost_t=fmaxf(boost_t,0.4f); if(sfx_boost)eng_sound_play(sfx_boost,0.6f); }
	item=IT_NONE;
}

static void on_update(float dt){
	g_time+=dt;
	if(shake>0){ shake-=dt*2.2f; if(shake<0)shake=0; }
	if(st==S_TITLE){
		if(eng_just_pressed(ENG_LEFT)) sel_track=(sel_track-1+g_ntracks)%g_ntracks;
		if(eng_just_pressed(ENG_RIGHT)) sel_track=(sel_track+1)%g_ntracks;
		if(eng_just_pressed(ENG_UP)) sel_char=(sel_char+5)%6;
		if(eng_just_pressed(ENG_DOWN)) sel_char=(sel_char+1)%6;
		if(eng_just_pressed(ENG_START)){ cur_track=sel_track; build_track(cur_track); reset_game(); if(sfx_engine)eng_music_play(sfx_engine,0.5f); }
		return;
	}
	if(st==S_DONE){ if(eng_just_pressed(ENG_START)){ build_track(cur_track); reset_game(); } return; }

	for(int i=0;i<NBOX;i++) if(box[i].respawn>0) box[i].respawn-=dt;
	for(int i=0;i<NPROJ;i++){ if(!proj[i].on)continue;
		proj[i].s+=proj[i].dir*(95.0f/3.3f)*dt;
		proj[i].life-=(95.0f/3.3f)*dt;
		if(proj[i].homing){ int tgt=proj[i].target;
			if(tgt<0){ float bd=1e30f; for(int r=0;r<NR;r++){ float ds=fmodf(riv[r].s-proj[i].s+NSEG,NSEG); if(ds<bd){bd=ds;tgt=r;} } }
			if(tgt>=0) proj[i].lane+=(riv[tgt].lane-proj[i].lane)*fminf(1.0f,dt*3.0f); }
		if(proj[i].life<=0) proj[i].on=0; }

	if(spin_t>0 && air<=0){ p_accel=0; spd=0; spin_t-=dt; hd+=8.0f*dt;
		if(spin_t<=0){ car=P[lastSeg]; hd=HDG[lastSeg]; seg=lastSeg; prevseg=lastSeg; car.y=surf_y(lastSeg,0); }   /* respawn ON the track (centerline) */
	}
	else {
		int touch=eng_pointer_pressed();
		int tx=-1,ty;
		if(touch)eng_pointer(&tx,&ty);
		float steer=0;
		if(eng_pressed(ENG_LEFT)) steer-=1;
		if(eng_pressed(ENG_RIGHT)) steer+=1;
		if(touch&&tx>=0) steer+=(tx-W/2)/(float)(W/2);
		steer=clampf(steer,-1,1);
		leanf+=(steer-leanf)*fminf(1.0f,dt*8.0f);
		int accel = eng_pressed(ENG_UP) || (touch&&tx>=0);
		p_accel=accel;
		int drifting = eng_pressed(ENG_B) && fabsf(steer)>0.2f && spd>16.0f && air<=0;
		if(drifting){ if(drift==0) drift=(steer>0?1:-1); drift_charge+=dt; }
		else if(drift!=0){ if(drift_charge>0.5f) boost_t=fmaxf(boost_t, drift_charge>1.3f?1.1f:0.6f), (sfx_boost?eng_sound_play(sfx_boost,0.4f):(void)0); drift=0; drift_charge=0; }
		float turn=TURN*(drift?1.55f:1.0f)*(air>0?0.5f:1.0f);
		float tf=spd/18.0f;
		if(tf>1)tf=1;
		hd+=steer*turn*dt*tf;
		if(drift) hd+=drift*0.5f*dt;
		int go = accel || boost_t>0;                         /* forward only on UP (or during a boost) */
		if(go) spd += ((CRUISE+(boost_t>0?boost_add:0.0f))-spd)*fminf(1.0f,dt*4.0f);
		else if(eng_pressed(ENG_DOWN)) spd += (0-spd)*fminf(1.0f,dt*6.0f);
		else spd -= 22.0f*dt;
		if(spd<0)spd=0;
		if(boost_t>0)boost_t-=dt;
		else boost_add=BOOST_ADD;
		if(surge_t>0)surge_t-=dt;
		if(star_t>0)star_t-=dt;
		if((eng_just_pressed(ENG_A)||eng_pointer_just_pressed())&&item!=IT_NONE) use_item();

		car.x+=sinf(hd)*spd*dt;
		car.z+=cosf(hd)*spd*dt;
		seg=nearest_seg();
		float latoff=vdot(vsub(car,P[seg]),LAT[seg]);
		if(air>0){                                            /* airborne: ballistic arc */
			car.y+=air_vy*dt;
			air_vy-=GRAV*dt;
			float g=surf_y(seg,latoff);
			if(car.y<=g && air_vy<0){ car.y=g; air=0; shake=fmaxf(shake,0.32f); if(sfx_hit)eng_sound_play(sfx_hit,0.3f); }
		} else {
			car.y=(fabsf(latoff)>WID[seg]?grass_y(seg,latoff):surf_y(seg,latoff));
			float off=fabsf(latoff), w=WID[seg];
			if(off>w+PLAYW){ float sgn=latoff>0?1.0f:-1.0f, lim=w+PLAYW-0.6f;   /* scrape the wall: clamp inside, scrub speed, no void spinout */
				car=vadd(P[seg],vscl(LAT[seg],sgn*lim));
				car.y=grass_y(seg,sgn*lim);
				spd*=0.45f;
				shake=fmaxf(shake,0.35f);
				if(sfx_hit)eng_sound_play(sfx_hit,0.5f);
				for(int q=0;q<6;q++) eng_particle(W/2+(int)(sgn*60),H-40,sgn*-120*frand(),-70-frand()*60,300,0.3f,5,ENG_RGB(230,230,235),ENG_RGB(200,60,55)); }
			else if(off>w){ if(spd>GRASS_CAP) spd-=(spd-GRASS_CAP)*9.0f*dt; }
			else lastSeg=seg;
			if(spd>26.0f && cdist(seg,g_ramp_seg)<=1){ air=1; air_vy=6.0f+spd*0.12f; if(sfx_boost)eng_sound_play(sfx_boost,0.3f); }   /* launch */
			{ int bpm=seg%g_bp_every; if(bpm>=g_bp_from && bpm<g_bp_to && fabsf(latoff)<=WID[seg]){ if(boost_t<=0.05f && sfx_boost) eng_sound_play(sfx_boost,0.4f); boost_t=fmaxf(boost_t,0.5f); shake=fmaxf(shake,0.18f); } }  /* boost pad kick */
			for(int i=0;i<NBOX;i++){ if(box[i].respawn>0)continue;
				V3 bp=vadd(track_at(P,box[i].seg),vscl(track_at(LAT,box[i].seg),box[i].lane*WID[box[i].seg]));
				float dx=car.x-bp.x,dz=car.z-bp.z;
				if(dx*dx+dz*dz<3.4f*3.4f && item==IT_NONE){ item=roll_item(race_pos()); if(item==IT_TRIO)item_charge=3; box[i].respawn=3.0f;
				if(sfx_item)eng_sound_play(sfx_item,0.75f);
				for(int q=0;q<14;q++){ float a=q/14.0f*6.2832f; eng_particle(W/2+cosf(a)*10,H-72,cosf(a)*150,sinf(a)*150-50,320,0.5f,5,ENG_RGB(255,255,180),ENG_RGB(120,220,255)); } } }
			for(int i=0;i<MAXHAZ;i++){ if(!haz[i].on)continue; float dx=car.x-haz[i].pos.x,dz=car.z-haz[i].pos.z; if(dx*dx+dz*dz<1.6f*1.6f){haz[i].on=0;spinout();} }
			/* --- juice: particles + skid marks --- */
			float cs=cosf(hd), sn=sinf(hd);
			if(drift!=0){ eng_color sc=drift_charge>1.3f?ENG_RGB(255,140,40):drift_charge>0.5f?ENG_RGB(120,200,255):ENG_RGB(205,205,215);
				for(int q=0;q<2;q++) eng_particle(W/2+drift*30, H-24, drift*(90+frand()*90), -60-frand()*70, 700, 0.28f, 5, ENG_RGB(255,255,225), sc); }
			if(off>w && spd>8){ for(int q=0;q<2;q++) eng_particle(W/2+(q?40:-40), H-18, (q?70:-70)*frand(), -80-frand()*60, 420, 0.4f, 6, ENG_RGB(150,132,92), ENG_RGB(120,150,110)); }
			if(boost_t>0) eng_particle(W/2+frand()*20-10, H-16, 0, 210+frand()*120, 0, 0.22f, 7, ENG_RGB(235,246,255), ENG_RGB(60,120,240));
			if(drift!=0 && off<=w+PLAYW){ V3 fwd2=v3(sn,0,cs), rgt=v3(cs,0,-sn), rear=vsub(car,vscl(fwd2,1.4f));
				skid[skidn%NSKID]=vadd(rear,vscl(rgt,0.7f));
				skidn++;
				skid[skidn%NSKID]=vsub(rear,vscl(rgt,0.7f));
				skidn++; }
		}
	}

	for(int r=0;r<NR;r++){
		if(riv[r].spin>0) riv[r].spin-=dt;
		else { riv[r].s+=riv[r].spd*dt; if(riv[r].s>=NSEG){riv[r].s-=NSEG;riv[r].lap++;} riv[r].lane+=(riv[r].base-riv[r].lane)*(dt*1.5f); }
		for(int pi=0;pi<NPROJ;pi++){ if(!proj[pi].on)continue; float ds=fmodf(riv[r].s-proj[pi].s+NSEG,NSEG);
			if((ds<2.0f||ds>NSEG-2.0f) && (proj[pi].homing||fabsf(riv[r].lane-proj[pi].lane)<0.5f)){ riv[r].spin=1.0f; proj[pi].on=0; if(sfx_hit)eng_sound_play(sfx_hit,0.5f); } }
		V3 rp=vadd(track_at(P,riv[r].s),vscl(track_at(LAT,riv[r].s),riv[r].lane*WID[(int)riv[r].s%NSEG]));
		for(int i=0;i<MAXHAZ;i++){ if(!haz[i].on)continue; float dx=rp.x-haz[i].pos.x,dz=rp.z-haz[i].pos.z; if(dx*dx+dz*dz<1.6f*1.6f){haz[i].on=0;riv[r].spin=1.0f;} }
		if(star_t>0){ float dxr=car.x-rp.x,dzr=car.z-rp.z; if(dxr*dxr+dzr*dzr<2.6f*2.6f) riv[r].spin=fmaxf(riv[r].spin,1.0f); }   /* star: KO on contact */
	}

	laptime+=dt;
	if(prevseg>=NSEG*3/4 && seg<NSEG/4){
		lap++;
		if(best==0||laptime<best)best=laptime;
		laptime=0;
		if(sfx_lap)eng_sound_play(sfx_lap,0.6f);
		if(lap>g_laps){ float me=g_laps*(float)NSEG; int pos=1; for(int r=0;r<NR;r++){float rp=riv[r].lap*(float)NSEG+riv[r].s; if(rp>me)pos++;} finish_pos=pos; st=S_DONE; }
	}
	eng_particles_update(dt);
	prevseg=seg;
}

/* ---- render ---- */
static int fog_shade(float z){int s=256-(int)(z*0.45f);return s<120?120:(s>256?256:s);}

static void quad_s(V3 p0,V3 p1,V3 p2,V3 p3,const eng_image*tex,float u0,float v0,float u1,float v1,float u2,float v2,float u3,float v3f,int sh){
	eng_tri_tex_world(p0,p1,p2,tex,u0,v0,u1,v1,u2,v2,sh);   /* near-plane clipped -> no gap at the camera */
	eng_tri_tex_world(p0,p2,p3,tex,u0,v0,u2,v2,u3,v3f,sh);
}

static void quad_tex(V3 p0,V3 p1,V3 p2,V3 p3,const eng_image*tex,float u0,float v0,float u1,float v1,float u2,float v2,float u3,float v3f){
	V3 mid=vscl(vadd(vadd(p0,p1),vadd(p2,p3)),0.25f);
	float mx,my,miz;
	int sh = eng_project(mid,&mx,&my,&miz) ? fog_shade(1.0f/miz) : 256;   /* behind-camera midpoint: draw anyway (clip handles it) */
	quad_s(p0,p1,p2,p3,tex,u0,v0,u1,v1,u2,v2,u3,v3f,sh);
}

/* --- smooth road: sample the track as a CURVE (Catmull-Rom through P[]) at draw time, not the raw
 * NSEG polyline, so edges/centre-line don't kink on turns. All physics/logic stays at the NSEG grid. */
static V3 cr_at(const V3*a,float s){                     /* Catmull-Rom position at fractional segment s */
	float m=fmodf(s,NSEG);
	if(m<0)m+=NSEG;
	int i=(int)m;
	float t=m-i;
	V3 p0=a[(i-1+NSEG)%NSEG],p1=a[i],p2=a[(i+1)%NSEG],p3=a[(i+2)%NSEG];
	return v3(cr1(p0.x,p1.x,p2.x,p3.x,t), cr1(p0.y,p1.y,p2.y,p3.y,t), cr1(p0.z,p1.z,p2.z,p3.z,t));
}

static float wid_at(float s){ float m=fmodf(s,NSEG); if(m<0)m+=NSEG; int i=(int)m; float t=m-i; return WID[i]*(1-t)+WID[(i+1)%NSEG]*t; }

static V3 road_lat(float s){ V3 f=vsub(cr_at(P,s+0.06f),cr_at(P,s-0.06f)); return vnorm(vcross(v3(0,1,0),f)); }

static V3 drape_off(V3 c,V3 lat,float off){ V3 p=vadd(c,vscl(lat,off)); p.y=terr(p.x,p.z)+(c.y-terr(c.x,c.z)); return p; }  /* road-surface drape */
static V3 grass_off(V3 c,V3 lat,float off){ V3 p=vadd(c,vscl(lat,off)); p.y=terr(p.x,p.z); return p; }                       /* ground drape (wall base) */
static void ground_shadow(V3 wp,float k){        /* flat z-tested quad so hills occlude it (no screen-space bleed) */
	float r=k*1.15f, y=wp.y+0.03f;
	float ax,ay,ai,bx,by,bi,cx,cy,ci,dx,dy,di;
	if(!eng_project(v3(wp.x-r,y,wp.z-r),&ax,&ay,&ai)) return;
	if(!eng_project(v3(wp.x+r,y,wp.z-r),&bx,&by,&bi)) return;
	if(!eng_project(v3(wp.x+r,y,wp.z+r),&cx,&cy,&ci)) return;
	if(!eng_project(v3(wp.x-r,y,wp.z+r),&dx,&dy,&di)) return;
	eng_vtx A={ax,ay,ai},B={bx,by,bi},C={cx,cy,ci},D={dx,dy,di};
	eng_tri(A,B,C,ENG_RGB(52,58,54));
	eng_tri(A,C,D,ENG_RGB(52,58,54)); }
/* the two FRONT wheels are a separate mesh at the body's front axle, yawed by heading + steer, so the
 * wheels TURN when you steer instead of the body banking like a motorbike. */
static void draw_front_wheels(V3 pos,float yaw,float steer,float scale,float roll){
	if(!g_wheel) return;
	const float OX=0.74f, OY=0.34f, OZ=0.80f;      /* front axle offset (the removed body wheels' coords) */
	float cy=cosf(yaw), sy=sinf(yaw);
	for(int s=-1;s<=1;s+=2){                        /* left (-1) + right (+1) */
		float ox=OX*s*scale, oz=OZ*scale;
		V3 wp=v3(pos.x+ox*cy+oz*sy, pos.y+OY*scale, pos.z-ox*sy+oz*cy);
		eng_mesh_draw(g_wheel, wp, yaw+steer, 0, roll, scale, ENG_WHITE);
	}
}
static void draw_column(V3 c,float y0,float y1,float r,const eng_image*tex){   /* a bridge pier: 4 vertical faces from y0..y1 */
	float x0=c.x-r,x1=c.x+r,z0=c.z-r,z1=c.z+r;
	V3 a=v3(x0,y0,z0),b=v3(x1,y0,z0),cc=v3(x1,y0,z1),d=v3(x0,y0,z1);   /* bottom */
	V3 A=v3(x0,y1,z0),B=v3(x1,y1,z0),C=v3(x1,y1,z1),D=v3(x0,y1,z1);   /* top */
	quad_s(A,B,b,a,tex,0,0,1,0,1,1,0,1,190);   /* -z face */
	quad_s(B,C,cc,b,tex,0,0,1,0,1,1,0,1,150);  /* +x */
	quad_s(C,D,d,cc,tex,0,0,1,0,1,1,0,1,190);  /* +z */
	quad_s(D,A,a,d,tex,0,0,1,0,1,1,0,1,150);   /* -x */
}

static void on_draw_background(void){
	for(int y=0;y<H/2;y++){ float t=y/(float)(H/2);
		int r=(int)((C_SKY_T>>16&0xff)*(1-t)+(C_SKY_B>>16&0xff)*t),g=(int)((C_SKY_T>>8&0xff)*(1-t)+(C_SKY_B>>8&0xff)*t),b=(int)((C_SKY_T&0xff)*(1-t)+(C_SKY_B&0xff)*t);
		eng_rect_fill(0,y,W,1,ENG_RGB(r,g,b)); }
	{ int sxp=(int)(W*0.23f), syp=(int)(H*0.17f);                       /* sun + soft halo (matches the light dir) */
		eng_circle_fill(sxp,syp,46,ENG_RGB(150,198,238));
		eng_circle_fill(sxp,syp,32,ENG_RGB(214,232,250));
		eng_circle_fill(sxp,syp,22,ENG_RGB(255,250,224)); }
	float pan=(st==S_TITLE)?0:-fmodf(hd,PI2)/PI2*W*2;
	for(int i=0;i<5;i++){ int cxp=(((int)(i*173+pan*0.3f))%(W+120)+W+120)%(W+120)-60, cyp=40+(i*22)%70;
		eng_circle_fill(cxp,cyp,26,ENG_RGB(245,248,255));
		eng_circle_fill(cxp+22,cyp+6,20,ENG_RGB(245,248,255));
		eng_circle_fill(cxp-20,cyp+8,18,ENG_RGB(245,248,255)); }
	for(int y=H/2;y<H;y++){ float t=(y-H/2)/(float)(H-H/2); int r=(int)(184*(1-t)+62*t),g=(int)(208*(1-t)+138*t),b=(int)(222*(1-t)+60*t); eng_rect_fill(0,y,W,1,ENG_RGB(r,g,b)); }
	if(st==S_TITLE) return;

	V3 fwd=v3(sinf(hd),0,cosf(hd));
	V3 eye=vadd(vsub(car,vscl(fwd,CAM_BACK_D)),v3(0,CAM_HEIGHT,0));
	if(shake>0){ eye.x+=(frand()*2-1)*shake*0.7f; eye.y+=(frand()*2-1)*shake*0.5f; }
	V3 look=vadd(vadd(car,vscl(fwd,LOOK_FWD)),v3(0,LOOK_UP,0));
	eng_camera_look(eye,look,v3(0,1,0),2.0f*atanf((H*0.5f)/FL)*57.29578f + (boost_t>0?8.0f:0.0f));

	eng_zclear();
	{ const float ST=20.0f, US=0.09f; const int GR=11;                            /* terrain grass grid (fills ALL ground; ~±220u to match fog/road) */
		float cx0=floorf(car.x/ST)*ST, cz0=floorf(car.z/ST)*ST;
		for(int gi=-GR;gi<=GR;gi++) for(int gj=-GR;gj<=GR;gj++){
			float x0=cx0+gi*ST, z0=cz0+gj*ST, x1=x0+ST, z1=z0+ST;                   /* grid sits 0.15 below terrain so the road/props never z-fight it */
			int rg=region_at(x0+ST*0.5f,z0+ST*0.5f);
			if(rg==REG_WATER){                                                     /* flat sea/river plane at water level (z-tested: land occludes it) */
				float wy=g_water_level;
				V3 a=v3(x0,wy,z0), b=v3(x1,wy,z0), c=v3(x1,wy,z1), d=v3(x0,wy,z1);
				quad_tex(a,b,c,d,tex_water, x0*US,z0*US, x1*US,z0*US, x1*US,z1*US, x0*US,z1*US);
			} else {
				V3 a=v3(x0,terr(x0,z0)-0.15f,z0), b=v3(x1,terr(x1,z0)-0.15f,z0), c=v3(x1,terr(x1,z1)-0.15f,z1), d=v3(x0,terr(x0,z1)-0.15f,z1);
				quad_tex(a,b,c,d,biome_tex(rg), x0*US,z0*US, x1*US,z0*US, x1*US,z1*US, x0*US,z1*US);   /* biome ground */
			}
		} }
	int start=seg-3;
	for(int k=0;k<=DRAWDIST;k++){
		int i=((start+k)%NSEG+NSEG)%NSEG;
		{ float dcx=P[i].x-car.x,dcz=P[i].z-car.z; if(dcx*dcx+dcz*dcz>205.0f*205.0f) continue; }   /* skip road beyond the grid */
		int bpm=i%g_bp_every;
		const eng_image*rt = (cdist(i,g_ramp_seg)<=5)?tex_ramp : (bpm>=g_bp_from&&bpm<g_bp_to)?tex_boost : tex_road;
		if(g_road_ok){                                       /* imported baked road surface: draw this segment's tris with its texture */
			float sx,sy,iz;
			int sh = eng_project(P[i],&sx,&sy,&iz)?fog_shade(1.0f/iz):256;
			int t0=i*g_road_tps;
			for(int t=0;t<g_road_tps;t++){ V3 a,b,c; float uv[6]; eng_mesh_get_tri(g_road_mesh,t0+t,&a,&b,&c,uv);
				eng_tri_tex_world(a,b,c, rt, uv[0],uv[1],uv[2],uv[3],uv[4],uv[5], sh); }
		} else {                                             /* fallback (missing/stale OBJ): curve-subdivided procedural road */
			int SUB=(k<14)?4:(k<30)?2:1;
			float base=(float)(start+k);
			for(int u=0;u<SUB;u++){ float s0=base+(float)u/SUB,s1=base+(float)(u+1)/SUB;
				V3 c0=cr_at(P,s0),c1=cr_at(P,s1),l0=road_lat(s0),l1=road_lat(s1);
				float w0=wid_at(s0),w1=wid_at(s1);
				quad_tex(drape_off(c0,l0,w0),drape_off(c0,l0,-w0),drape_off(c1,l1,-w1),drape_off(c1,l1,w1), rt, 0,s0,1,s0,1,s1,0,s1); }
		}
		if(tex_wall){                                        /* guardrails stay procedural (curve-subdivided) */
			int SUB=(k<14)?4:(k<30)?2:1, br=seg_bridge[i];
			float base=(float)(start+k);
			for(int u=0;u<SUB;u++){ float s0=base+(float)u/SUB,s1=base+(float)(u+1)/SUB;
				V3 c0=cr_at(P,s0),c1=cr_at(P,s1),l0=road_lat(s0),l1=road_lat(s1);
				float w0=wid_at(s0),w1=wid_at(s1);
				float wl0=br?w0+0.5f:w0+PLAYW, wl1=br?w1+0.5f:w1+PLAYW;      /* on a bridge the rail hugs the deck edge, not the far grass */
				V3 Lb0,Lb1,Rb0,Rb1;
				if(br){ Lb0=vadd(c0,vscl(l0,wl0)); Lb1=vadd(c1,vscl(l1,wl1)); Rb0=vsub(c0,vscl(l0,wl0)); Rb1=vsub(c1,vscl(l1,wl1));
					Lb0.y=Lb1.y=Rb0.y=Rb1.y=TK->bridge_deck; }   /* rail sits on the deck, not the riverbed */
				else { Lb0=grass_off(c0,l0,wl0); Lb1=grass_off(c1,l1,wl1); Rb0=grass_off(c0,l0,-wl0); Rb1=grass_off(c1,l1,-wl1); }
				quad_tex(vadd(Lb0,v3(0,WALL_H,0)),Lb0,Lb1,vadd(Lb1,v3(0,WALL_H,0)),tex_wall,s0,0,s0,1,s1,1,s1,0);
				quad_tex(vadd(Rb0,v3(0,WALL_H,0)),Rb0,Rb1,vadd(Rb1,v3(0,WALL_H,0)),tex_wall,s0,0,s0,1,s1,1,s1,0); }
		}
	}
	if(TK->has_bridge && tex_rock_ground){                       /* bridge piers: columns from the riverbed up to the deck */
		for(int k=0;k<=DRAWDIST;k++){ int i=((start+k)%NSEG+NSEG)%NSEG;
			if(!seg_bridge[i] || (i%3)) continue;
			float dcx=P[i].x-car.x,dcz=P[i].z-car.z; if(dcx*dcx+dcz*dcz>205.0f*205.0f) continue;
			draw_column(P[i], g_water_level-5.0f, TK->bridge_deck, 1.3f, tex_rock_ground);
		}
	}
	{ int cnt=skidn<NSKID?skidn:NSKID; for(int i=0;i<cnt;i++){ float sx,sy,iz; if(!eng_project(skid[i],&sx,&sy,&iz))continue; int r=(int)(0.32f*FL*iz); if(r<1)r=1; eng_circle_fill((int)sx,(int)sy,r,ENG_RGB(42,42,46)); } }
	/* roadside scenery: trees far->near + a start/finish banner */
	for(int k=DRAWDIST;k>=0;k--){ int i=((start+k)%NSEG+NSEG)%NSEG;
		{ float ddx=P[i].x-car.x, ddz=P[i].z-car.z; if(ddx*ddx+ddz*ddz > 205.0f*205.0f) continue; }   /* skip props beyond the grass grid (no floaters) */
		if(i%9==0){                                                  /* 3D pine trees, lit, both sides */
			float off=WID[i]+18.0f;
			V3 tl=vadd(P[i],vscl(LAT[i],off));
			tl.y=grass_y(i,off);
			V3 tr=vsub(P[i],vscl(LAT[i],off));
			tr.y=grass_y(i,-off);
			eng_mesh_draw(g_tree, tl, (float)(i*13%360), 0, 0.0f, 0.9f+0.25f*((i*3%7)/7.0f), ENG_WHITE);
			eng_mesh_draw(g_tree, tr, (float)(i*21%360), 0, 0.0f, 0.9f+0.25f*((i*5%7)/7.0f), ENG_WHITE);
		}
		if(i%14==0){                                                 /* scattered boulders */
			float off=WID[i]+7.0f, rsc=0.7f+0.6f*((i*7%10)/10.0f);
			V3 bl=vadd(P[i],vscl(LAT[i],off));
			bl.y=grass_y(i,off);
			ground_shadow(bl, 1.4f*rsc);                             /* contact patch anchors it to the ground */
			bl.y-=0.4f;                                              /* sink the base so it never hovers over the coarse grass grid */
			eng_mesh_draw(g_rock[i%3], bl, (float)i, 0, 0.0f, rsc, ENG_WHITE);
		}
	}
	if(tex_banner){ V3 bp=vadd(P[0],v3(0,5.0f,0)); float sx,sy,iz; if(eng_project(bp,&sx,&sy,&iz)){ float sc=(2.0f*WID[0]*FL*iz)/256.0f; if(sc>0.02f) eng_draw_sprite_z(tex_banner,(int)sx,(int)sy,256,40,0,0,0,sc,ENG_WHITE,iz); } }
	for(int b=0;b<NBOX;b++){ if(box[b].respawn>0)continue;
		V3 bp=vadd(track_at(P,box[b].seg),vscl(track_at(LAT,box[b].seg),box[b].lane*WID[box[b].seg]));
		bp.y=surf_y(box[b].seg,box[b].lane*WID[box[b].seg])+1.1f;
		eng_mesh_draw(g_crate, bp, g_time*2.0f, 0, 0.0f, 1.0f, ENG_WHITE); }
	for(int i=0;i<MAXHAZ;i++){ if(!haz[i].on)continue; ground_shadow(haz[i].pos,0.9f); eng_mesh_draw(g_banana, haz[i].pos, (float)i*1.7f, 0, 0, 1.1f, ENG_WHITE); }
	for(int pi=0;pi<NPROJ;pi++){ if(!proj[pi].on)continue;
		V3 bp=vadd(track_at(P,proj[pi].s),vscl(track_at(LAT,proj[pi].s),proj[pi].lane*WID[(int)proj[pi].s%NSEG]));
		bp.y+=0.55f;
		ground_shadow(bp,0.9f);
		float spin=g_time*7.0f;
		if(proj[pi].homing){ eng_color c=(proj[pi].target>=0)?ENG_RGB(70,120,235):ENG_RGB(232,72,58);  /* apex=blue shell / seeker=red shell */
			eng_mesh_draw(g_shell, bp, spin, 0, 0, (proj[pi].target>=0)?1.55f:1.25f, c); }
		else eng_mesh_draw(g_shell, bp, spin, 0, 0, 1.2f, ENG_RGB(70,205,95)); }   /* dart = green shell */
	/* rivals as lit 3D kart meshes (z-buffered, so no painter sort needed) */
	for(int r=0;r<NR;r++){ int rs=(int)riv[r].s%NSEG;
		V3 rp=vadd(track_at(P,riv[r].s),vscl(track_at(LAT,riv[r].s),riv[r].lane*WID[rs]));
		rp.y = surf_y(rs, riv[r].lane*WID[rs]);                   /* sit on the road surface (draped on terrain) */
		{ float dcx=rp.x-car.x,dcz=rp.z-car.z; if(dcx*dcx+dcz*dcz>205.0f*205.0f) continue; }   /* cull rivals beyond the grid */
		if(!eng_project(vadd(rp,v3(0,0.5f,0)),0,0,0)) continue;   /* cull if behind camera */
		ground_shadow(vadd(rp,v3(0,0.05f,0)),1.0f);
		float yaw=HDG[rs] + (riv[r].spin>0 ? g_time*9.0f : 0.0f);
		float dch=HDG[(rs+3)%NSEG]-HDG[rs];
		while(dch>3.14159f)dch-=6.2832f;
		while(dch<-3.14159f)dch+=6.2832f;
		float rsteer=(riv[r].spin>0)?0.0f:clampf(dch*2.2f,-0.45f,0.45f);   /* front wheels turn into the corner; body stays upright */
		eng_mesh_draw(g_kart, rp, yaw, 0, 0.0f, 1.0f, riv[r].col);
		draw_front_wheels(rp, yaw, rsteer, 1.0f, 0.0f);
	}
	/* player kart: a lit 3D mesh at the car's world pos (was a fixed bottom-screen sprite). hd yaws it,
	 * car.y carries the jump arc, hd spins it during a spin-out, boost pops the scale, star tints it. */
	if(st==S_PLAY||st==S_DONE){
		ground_shadow(vadd(car,v3(0,0.05f,0)), 0.8f);
		eng_color pt = star_t>0 ? ENG_RGB((int)(160+95*sinf(g_time*18)),(int)(160+95*sinf(g_time*18+2.1f)),(int)(160+95*sinf(g_time*18+4.2f))) : CHAR_COL[sel_char];
		float plean=(spin_t>0)?0.0f:-clampf((float)drift,-1.0f,1.0f)*0.12f;   /* body upright; a subtle lean only while DRIFTING (a slide) */
		float psteer=(spin_t>0)?0.0f:clampf(leanf,-1.0f,1.0f)*0.45f;         /* front wheels turn with the steering input */
		float pscale=1.0f+(boost_t>0?0.14f:0.0f);
		eng_mesh_draw(g_kart, car, hd, 0, plean, pscale, pt);
		draw_front_wheels(car, hd, psteer, pscale, plean);
		float fi = boost_t>0 ? 1.8f : ((p_accel && spd>8.0f) ? 0.6f+0.4f*fminf(1.0f,spd/CRUISE) : 0.0f);
		if(fi>0 && spin_t<=0){ V3 fwd=v3(sinf(hd),0,cosf(hd)), rgt=v3(cosf(hd),0,-sinf(hd));
			V3 base=vadd(car,vscl(fwd,-1.25f));
			base.y+=0.5f;
			int hot=boost_t>0;
			float sx,sy,iz;
			if(eng_project(vadd(base,vscl(rgt,0.45f)),&sx,&sy,&iz)) draw_flame((int)sx,(int)sy,fi,hot);
			if(eng_project(vsub(base,vscl(rgt,0.45f)),&sx,&sy,&iz)) draw_flame((int)sx,(int)sy,fi,hot); }
		if(star_t>0){ float sx,sy,iz; if(eng_project(vadd(car,v3(0,1.2f,0)),&sx,&sy,&iz)) eng_particle(sx+frand()*40-20,sy,frand()*100-50,-frand()*90,260,0.4f,4,ENG_RGB(255,240,130),ENG_RGB(120,200,255)); }
	}
	eng_post_aa();   /* smooth the jagged road/fence/kart edges before the HUD text is drawn (overlay) */
}

static void draw_flame(int cx,int cy,float sc,int hot){
	float fl=(0.78f+0.22f*sinf(g_time*34.0f+cx*0.7f))*sc;
	int L=(int)(36*fl);
	if(L<5)L=5;
	for(int i=0;i<L;i++){ float t=(float)i/L; int w=(int)((1.0f-t)*(hot?10:8)*fl)+1;
		eng_color c = hot ? (t<0.3f?ENG_RGB(235,245,255):t<0.6f?ENG_RGB(130,185,255):t<0.85f?ENG_RGB(80,120,240):ENG_RGB(40,60,175))
		                  : (t<0.25f?ENG_RGB(255,255,225):t<0.55f?ENG_RGB(255,215,90):t<0.8f?ENG_RGB(255,150,40):ENG_RGB(215,55,25));
		eng_rect_fill(cx-w/2, cy+i, w, 1, c); }
}

static void draw_item_icon(int x,int y,int it){
	if(it==IT_BOOST||it==IT_TRIO){                                 /* mushroom */
		eng_rect_fill(x-6,y+2,12,15,ENG_RGB(235,225,205));
		eng_circle_fill(x,y+1,12,ENG_RGB(220,72,60));
		eng_circle_fill(x-4,y-3,3,ENG_RGB(250,242,235));
		eng_circle_fill(x+5,y+1,2,ENG_RGB(250,242,235));
		if(it==IT_TRIO) for(int c=0;c<item_charge;c++) eng_circle_fill(x-8+c*8,y+24,3,ENG_RGB(220,72,60)); }
	else if(it==IT_DART||it==IT_SEEKER||it==IT_APEX){              /* shell: green / red / blue */
		eng_color col=(it==IT_DART)?ENG_RGB(70,205,95):(it==IT_SEEKER)?ENG_RGB(232,72,58):ENG_RGB(70,120,235);
		eng_color rim=ENG_RGB(((col>>16)&0xff)*6/10,((col>>8)&0xff)*6/10,(col&0xff)*6/10);
		eng_circle_fill(x,y+3,13,rim);
		eng_circle_fill(x,y+3,11,col);
		eng_line(x-11,y+3,x+11,y+3,rim);
		eng_line(x,y-8,x,y+3,rim);
		eng_line(x-7,y-4,x-9,y+3,rim);
		eng_line(x+7,y-4,x+9,y+3,rim); }
	else if(it==IT_SLICK){                                         /* banana */
		int bx[5]={x-10,x-4,x+2,x+8,x+12}, by[5]={y+8,y+1,y-2,y+1,y+8}, br[5]={3,5,6,5,3};
		for(int k=0;k<5;k++) eng_circle_fill(bx[k],by[k],br[k],ENG_RGB(240,205,50));
		eng_circle_fill(bx[0],by[0],2,ENG_RGB(90,70,30)); }
	else if(it==IT_STORM){ eng_line(x-2,y-8,x-8,y+4,ENG_RGB(150,220,255)); eng_line(x-8,y+4,x+2,y+4,ENG_RGB(150,220,255)); eng_line(x+2,y+4,x-4,y+18,ENG_RGB(150,220,255)); for(int q=0;q<3;q++) eng_circle_fill(x+9,y-6+q*10,2,ENG_RGB(200,235,255)); }
	else if(it==IT_STAR){ eng_circle_fill(x,y+4,7,ENG_RGB(255,225,60)); for(int q=0;q<5;q++){ float a=q/5.0f*6.2832f-1.5708f; eng_line(x,y+4,x+(int)(cosf(a)*13),y+4+(int)(sinf(a)*13),ENG_RGB(255,225,60)); } }
}

static void draw_minimap(void){
	int S=118, ox=W-S-14, oy=H-S-14;
	eng_rect_fill(ox,oy,S,S,ENG_RGB(24,44,26));
	eng_rect(ox,oy,S,S,ENG_RGB(210,225,210));
	float pad=10, ax=(S-2*pad)/(tb_x1-tb_x0+0.01f), az=(S-2*pad)/(tb_z1-tb_z0+0.01f), sc=ax<az?ax:az;
	float cx0=(tb_x0+tb_x1)*0.5f, cz0=(tb_z0+tb_z1)*0.5f, mcx=ox+S*0.5f, mcy=oy+S*0.5f;
	#define MMX(wx) ((int)(mcx+((wx)-cx0)*sc))
	#define MMY(wz) ((int)(mcy-((wz)-cz0)*sc))
	for(int i=0;i<NSEG;i+=2){ int j=(i+2)%NSEG; eng_line(MMX(P[i].x),MMY(P[i].z),MMX(P[j].x),MMY(P[j].z),ENG_RGB(150,150,162)); }
	for(int r=0;r<NR;r++){ V3 rp=track_at(P,riv[r].s); eng_circle_fill(MMX(rp.x),MMY(rp.z),3,riv[r].col); }
	eng_circle_fill(MMX(car.x),MMY(car.z),4,ENG_RGB(255,240,60));
	#undef MMX
	#undef MMY
}

static void on_draw_overlay(void){
	char buf[48];
	if(st==S_TITLE){
		eng_text_aligned(W/2,44,70,C_ACC,ENG_ALIGN_CENTER,"TURBO KART");
		if(tex_karts) eng_draw_sprite(tex_karts,W/2,H/2-6,CARFW,CARFH,2,sel_char,0,1.7f,ENG_WHITE);
		snprintf(buf,sizeof buf,"<  %s  >",CHAR_NAME[sel_char]);
		eng_text_aligned(W/2,H/2+86,32,C_HUD,ENG_ALIGN_CENTER,buf);
		snprintf(buf,sizeof buf,"TRACK: %s",g_tracks[sel_track].name);
		eng_text_aligned(W/2,H/2+124,26,C_MUTE,ENG_ALIGN_CENTER,buf);
		eng_text_aligned(W/2,H-64,22,C_MUTE,ENG_ALIGN_CENTER,"UP/DOWN CHARACTER   L/R TRACK");
		eng_text_aligned(W/2,H-38,26,ENG_RGB(40,150,60),ENG_ALIGN_CENTER,"PRESS START");
		return;
	}
	if(boost_t>0){ int cxx=W/2, cyy=H/2-10;                       /* warp-speed streaks radiating from centre */
		for(int q=0;q<20;q++){ float a=q/20.0f*PI2+g_time*4.0f, fl=200.0f+120.0f*(0.5f+0.5f*sinf(g_time*22.0f+q));
			eng_line(cxx+(int)(cosf(a)*140),cyy+(int)(sinf(a)*95),cxx+(int)(cosf(a)*(140+fl)),cyy+(int)(sinf(a)*(95+fl*0.7f)),(q&1)?ENG_RGB(210,235,255):ENG_WHITE); } }
	/* the player kart is now a 3D mesh drawn in on_draw_background (with the world) */
	eng_particles_draw();
	int pos=race_pos();
	snprintf(buf,sizeof buf,"%d KM/H",(int)(spd*3.6f));
	eng_text(16,12,30,C_HUD,buf);
	snprintf(buf,sizeof buf,"POS %d/%d",pos,NR+1);
	eng_text(16,46,26,C_ACC,buf);
	snprintf(buf,sizeof buf,"LAP %d/%d",lap>g_laps?g_laps:lap,g_laps);
	eng_text_aligned(W-16,12,30,C_HUD,ENG_ALIGN_RIGHT,buf);
	eng_rect(W/2-28,10,56,56,C_HUD);
	if(item!=IT_NONE){ draw_item_icon(W/2,26,item); eng_text_aligned(W/2,44,20,C_ACC,ENG_ALIGN_CENTER,"A"); }
	draw_minimap();
	if(surge_t>0){ float pr=(0.45f-surge_t)/0.45f; int rr=(int)(pr*W*0.7f); for(int q=0;q<48;q++){ float a=q/48.0f*6.2832f; eng_circle_fill(W/2+(int)(cosf(a)*rr),H/2+(int)(sinf(a)*rr),3,ENG_RGB(210,240,255)); } }
	if(boost_t>0) eng_text_aligned(W/2,H-96,26,ENG_RGB(255,140,40),ENG_ALIGN_CENTER,"BOOST!");
	if(star_t>0) eng_text_aligned(W/2,H-96,26,ENG_RGB(255,225,60),ENG_ALIGN_CENTER,"INVINCIBLE!");
	if(air>0) eng_text_aligned(W/2,120,30,C_ACC,ENG_ALIGN_CENTER,"JUMP!");
	if(st==S_DONE){
		eng_text_aligned(W/2,H/2-60,80,C_ACC,ENG_ALIGN_CENTER,"FINISH");
		snprintf(buf,sizeof buf,finish_pos==1?"1ST PLACE!":"POSITION %d/%d",finish_pos,NR+1);
		eng_text_aligned(W/2,H/2+20,40,C_HUD,ENG_ALIGN_CENTER,buf);
		eng_text_aligned(W/2,H/2+70,24,C_MUTE,ENG_ALIGN_CENTER,"PRESS START");
	}
}

int main(void){
	static const eng_game game={.title="Turbo Kart",.init=on_init,.update=on_update,.draw_background=on_draw_background,.draw_overlay=on_draw_overlay};
	return eng_run(&game);
}
