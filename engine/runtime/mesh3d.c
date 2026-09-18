/* engine/mesh3d.c — the 3D scene layer: a look-at camera, world-point projection, and flat-shaded
 * meshes, all on top of the eng_tri rasterizer + its z-buffer (scene.c). This folds the transform ->
 * project -> shade -> rasterize boilerplate that nova.c and circuit.c used to hand-code into one
 * reusable place; games now declare geometry (eng_mesh_*) + a camera (eng_camera_look) and draw.
 * The exact software-GPU pipeline, but data-driven. No pixels here — eng_tri owns those. */
#include "engine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define NEAR3D 0.30f

static eng_vec3 c_eye, c_r, c_u, c_f;     /* camera position + orthonormal basis (right/up/forward) */
static float    c_fl = 500.0f;            /* focal length derived from the vertical FOV */
static eng_vec3 l_dir = { -0.359f, 0.717f, -0.598f };   /* unit length (Finding 2) */
static float    l_amb = 0.30f;

static eng_vec3 v_sub(eng_vec3 a, eng_vec3 b) { return eng_v3(a.x-b.x, a.y-b.y, a.z-b.z); }
static eng_vec3 v_cross(eng_vec3 a, eng_vec3 b) { return eng_v3(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x); }
static float    v_dot(eng_vec3 a, eng_vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static eng_vec3 v_norm(eng_vec3 a) { float l = sqrtf(v_dot(a, a)); if (l < 1e-6f) l = 1; return eng_v3(a.x/l, a.y/l, a.z/l); }

void eng_camera_look(eng_vec3 eye, eng_vec3 target, eng_vec3 up, float fov_deg)
{
	c_eye = eye;
	c_f = v_norm(v_sub(target, eye));
	c_r = v_norm(v_cross(up, c_f));       /* right-handed, un-mirrored */
	c_u = v_cross(c_f, c_r);
	float t = tanf(fov_deg * 0.5f * 3.14159265f / 180.0f); if (t < 1e-4f) t = 1e-4f;
	c_fl = (eng_height() * 0.5f) / t;
}
void eng_light(eng_vec3 dir, float ambient) { l_dir = v_norm(dir); l_amb = ambient; }

int eng_project(eng_vec3 w, float *sx, float *sy, float *iz)
{
	eng_vec3 r = v_sub(w, c_eye);
	float cz = v_dot(r, c_f);
	if (cz < NEAR3D) return 0;
	if (sx) *sx = eng_width()  * 0.5f + v_dot(r, c_r) * c_fl / cz;
	if (sy) *sy = eng_height() * 0.5f - v_dot(r, c_u) * c_fl / cz;
	if (iz) *iz = 1.0f / cz;
	return 1;
}

/* Textured triangle given in WORLD space, near-plane clipped: transforms to camera space, clips the
 * triangle against z=NEAR3D (so a vertex behind the camera no longer discards the whole quad — the
 * in-front part is still drawn), projects, and rasterizes. This is what lets ground/road quads at the
 * camera's feet fill in instead of leaving a gap. */
void eng_tri_tex_world(eng_vec3 wa, eng_vec3 wb, eng_vec3 wc, const eng_image *tex,
                       float ua, float va, float ub, float vb, float uc, float vc, int shade)
{
	struct cv { eng_vec3 c; float u, v; } in[3], out[4]; int no = 0;
	eng_vec3 rs;
	rs = v_sub(wa,c_eye); in[0].c = eng_v3(v_dot(rs,c_r), v_dot(rs,c_u), v_dot(rs,c_f)); in[0].u=ua; in[0].v=va;
	rs = v_sub(wb,c_eye); in[1].c = eng_v3(v_dot(rs,c_r), v_dot(rs,c_u), v_dot(rs,c_f)); in[1].u=ub; in[1].v=vb;
	rs = v_sub(wc,c_eye); in[2].c = eng_v3(v_dot(rs,c_r), v_dot(rs,c_u), v_dot(rs,c_f)); in[2].u=uc; in[2].v=vc;
	for (int i = 0; i < 3; i++) {                       /* Sutherland-Hodgman clip vs z>=NEAR3D */
		int j = (i + 1) % 3; float zi = in[i].c.z, zj = in[j].c.z; int ii = zi >= NEAR3D, jj = zj >= NEAR3D;
		if (ii) out[no++] = in[i];
		if (ii != jj) { float t = (NEAR3D - zi) / (zj - zi);
			out[no].c = eng_v3(in[i].c.x+(in[j].c.x-in[i].c.x)*t, in[i].c.y+(in[j].c.y-in[i].c.y)*t, NEAR3D);
			out[no].u = in[i].u+(in[j].u-in[i].u)*t; out[no].v = in[i].v+(in[j].v-in[i].v)*t; no++; }
	}
	if (no < 3) return;
	eng_vtx_tex vv[4];
	for (int i = 0; i < no; i++) { float cz = out[i].c.z, iz = 1.0f/cz;
		vv[i].x = eng_width()*0.5f + out[i].c.x*c_fl/cz; vv[i].y = eng_height()*0.5f - out[i].c.y*c_fl/cz;
		vv[i].iz = iz; vv[i].u = out[i].u; vv[i].v = out[i].v; }
	for (int i = 1; i + 1 < no; i++) eng_tri_tex(vv[0], vv[i], vv[i+1], tex, shade);
}

/* ---- meshes ---- */
struct tri3 { eng_vec3 a, b, c, na, nb, nc; float ua,va,ub,vb,uc,vc; eng_color col; int tint; };  /* + per-vertex UVs (textured meshes) */
struct eng_mesh { struct tri3 *t; int n, cap; int finalized; const eng_image *tex; };  /* tex!=NULL => textured (uses UVs) */

eng_mesh *eng_mesh_new(void) { return calloc(1, sizeof(eng_mesh)); }
void eng_mesh_free(eng_mesh *m) { if (m) { free(m->t); free(m); } }
/* Raw triangle access for meshes authored in WORLD space (e.g. a track surface): the caller draws the
 * tris itself via eng_tri_tex_world with its own texture, so one mesh can be multi-textured + windowed. */
int  eng_mesh_ntris(const eng_mesh *m) { return m ? m->n : 0; }
void eng_mesh_get_tri(const eng_mesh *m, int i, eng_vec3 *a, eng_vec3 *b, eng_vec3 *c, float *uv6)
{
	const struct tri3 *t = &m->t[i];
	*a = t->a; *b = t->b; *c = t->c;
	uv6[0]=t->ua; uv6[1]=t->va; uv6[2]=t->ub; uv6[3]=t->vb; uv6[4]=t->uc; uv6[5]=t->vc;
}
static void mesh_push(eng_mesh *m, eng_vec3 a, eng_vec3 b, eng_vec3 c, eng_color col, int tint)
{
	if (!m) return;
	if (m->n == m->cap) { int nc = m->cap ? m->cap*2 : 16; struct tri3 *p = realloc(m->t, nc*sizeof *p); if (!p) return; m->t = p; m->cap = nc; }
	m->t[m->n].a = a; m->t[m->n].b = b; m->t[m->n].c = c; m->t[m->n].col = col; m->t[m->n].tint = tint;
	m->t[m->n].ua=m->t[m->n].va=m->t[m->n].ub=m->t[m->n].vb=m->t[m->n].uc=m->t[m->n].vc=0;
	m->n++;
}
void eng_mesh_tri(eng_mesh *m, eng_vec3 a, eng_vec3 b, eng_vec3 c, eng_color col, int tint) { mesh_push(m, a, b, c, col, tint); }
void eng_mesh_quad(eng_mesh *m, eng_vec3 a, eng_vec3 b, eng_vec3 c, eng_vec3 d, eng_color col, int tint) { mesh_push(m, a, b, c, col, tint); mesh_push(m, a, c, d, col, tint); }
void eng_mesh_box(eng_mesh *m, eng_vec3 lo, eng_vec3 hi, eng_color col, int tint)
{
	eng_vec3 a=eng_v3(lo.x,lo.y,lo.z), b=eng_v3(hi.x,lo.y,lo.z), c=eng_v3(hi.x,hi.y,lo.z), d=eng_v3(lo.x,hi.y,lo.z);
	eng_vec3 e=eng_v3(lo.x,lo.y,hi.z), f=eng_v3(hi.x,lo.y,hi.z), g=eng_v3(hi.x,hi.y,hi.z), h=eng_v3(lo.x,hi.y,hi.z);
	eng_mesh_quad(m,a,b,c,d,col,tint); eng_mesh_quad(m,f,e,h,g,col,tint);
	eng_mesh_quad(m,e,f,b,a,col,tint); eng_mesh_quad(m,d,c,g,h,col,tint);
	eng_mesh_quad(m,b,f,g,c,col,tint); eng_mesh_quad(m,e,a,d,h,col,tint);
}
static eng_vec3 sph_pt(eng_vec3 c, float r, float th, float ph)
{
	return eng_v3(c.x + r*sinf(th)*cosf(ph), c.y + r*cosf(th), c.z + r*sinf(th)*sinf(ph));
}
void eng_mesh_sphere(eng_mesh *m, eng_vec3 c, float r, int subdiv, eng_color col, int tint)
{
	if (subdiv < 1) subdiv = 1;
	int rings = subdiv*4, segs = subdiv*8; const float PI = 3.14159265f;
	for (int i = 0; i < rings; i++) {
		float th0 = PI*i/rings, th1 = PI*(i+1)/rings;
		for (int j = 0; j < segs; j++) {
			float p0 = 2*PI*j/segs, p1 = 2*PI*(j+1)/segs;
			eng_mesh_quad(m, sph_pt(c,r,th0,p0), sph_pt(c,r,th0,p1), sph_pt(c,r,th1,p1), sph_pt(c,r,th1,p0), col, tint);
		}
	}
}

/* ---- OBJ model loader: import geometry authored as DATA (the real-world asset flow) --------------
 * A minimal-but-real Wavefront OBJ (+ .mtl) parser. Reads positions (v), texcoords (vt) and faces
 * (f, any polygon, fan-triangulated; the v/vt/vn tokens Blender emits, negative indices). Per-face
 * colour = the current material's Kd; a material NAME containing "tint" -> recolourable by
 * eng_mesh_draw's tint; a material with map_Kd loads that image and makes the whole mesh TEXTURED
 * (faces then sample it via their UVs, Gouraud-lit). Vn is ignored (crease-smoothed normals). Paths
 * resolve under $CANVAS_ASSETS like textures (mtllib/map_Kd resolved relative to the .obj dir). */
static void resolve_asset(const char *path, char *out, size_t n){
	if (path && path[0] != '/') { const char *base=getenv("CANVAS_ASSETS"); if(!base||!*base) base="/usr/share/canvas"; snprintf(out,n,"%s/%s",base,path); }
	else snprintf(out,n,"%s",path?path:"");
}
eng_mesh *eng_mesh_from_obj(const char *path)
{
	char full[512]; resolve_asset(path, full, sizeof full);
	FILE *f=fopen(full,"r"); if(!f){ fprintf(stderr,"engine: can't open obj %s\n", full); return NULL; }
	char dir[256]=""; { const char *sl=path?strrchr(path,'/'):NULL; if(sl){ size_t n=(size_t)(sl-path)+1; if(n<sizeof dir){ memcpy(dir,path,n); dir[n]=0; } } }
	struct { char name[64]; eng_color col; int tint; const eng_image *tex; } mtl[64]; int nmtl=0;
	eng_color curcol=ENG_WHITE; int curtint=0; const eng_image *curtex=NULL;
	eng_vec3 *V=NULL; int nv=0, capv=0;
	float *TU=NULL,*TV=NULL; int nvt=0, capvt=0;
	eng_mesh *m=eng_mesh_new();
	char line[512];
	while(fgets(line,sizeof line,f)){
		if(line[0]=='v' && line[1]=='t' && (line[2]==' '||line[2]=='\t')){
			float u=0,v=0; if(sscanf(line+3,"%f %f",&u,&v)>=1){
				if(nvt==capvt){ capvt=capvt?capvt*2:64; float *a=realloc(TU,(size_t)capvt*sizeof*a),*b=realloc(TV,(size_t)capvt*sizeof*b); if(!a||!b){ free(a);free(b);free(V);fclose(f);eng_mesh_free(m);return NULL; } TU=a;TV=b; }
				TU[nvt]=u; TV[nvt]=v; nvt++; }
		} else if(line[0]=='v' && (line[1]==' '||line[1]=='\t')){
			eng_vec3 p; if(sscanf(line+2,"%f %f %f",&p.x,&p.y,&p.z)==3){
				if(nv==capv){ capv=capv?capv*2:64; eng_vec3 *nvp=realloc(V,(size_t)capv*sizeof*V); if(!nvp){ free(V);free(TU);free(TV);fclose(f);eng_mesh_free(m);return NULL; } V=nvp; }
				V[nv++]=p; }
		} else if(line[0]=='f' && (line[1]==' '||line[1]=='\t')){
			int vi[32], ti[32], nfi=0; char *tok=strtok(line+2," \t\r\n");
			while(tok && nfi<32){
				int p=atoi(tok), t=0; char *sl=strchr(tok,'/'); if(sl && sl[1] && sl[1]!='/') t=atoi(sl+1);
				vi[nfi]=(p<0)?nv+p:p-1; ti[nfi]=(t==0)?-1:((t<0)?nvt+t:t-1); nfi++; tok=strtok(NULL," \t\r\n");
			}
			for(int i=1;i+1<nfi;i++){ int a=vi[0],b=vi[i],c=vi[i+1];
				if(a<0||a>=nv||b<0||b>=nv||c<0||c>=nv) continue;
				eng_mesh_tri(m, V[a], V[b], V[c], curcol, curtint);
				struct tri3 *tr=&m->t[m->n-1]; int ta=ti[0],tb=ti[i],tc=ti[i+1];
				if(ta>=0&&ta<nvt){ tr->ua=TU[ta]; tr->va=1.0f-TV[ta]; }
				if(tb>=0&&tb<nvt){ tr->ub=TU[tb]; tr->vb=1.0f-TV[tb]; }
				if(tc>=0&&tc<nvt){ tr->uc=TU[tc]; tr->vc=1.0f-TV[tc]; }
			}
		} else if(!strncmp(line,"usemtl",6)){
			char nm[64]=""; sscanf(line+6,"%63s",nm); curcol=ENG_WHITE; curtint=0; curtex=NULL;
			for(int i=0;i<nmtl;i++) if(!strcmp(mtl[i].name,nm)){ curcol=mtl[i].col; curtint=mtl[i].tint; curtex=mtl[i].tex; break; }
			if(curtex) m->tex=curtex;
		} else if(!strncmp(line,"mtllib",6)){
			char nm[128]=""; sscanf(line+6,"%127s",nm);
			char mrel[384]; snprintf(mrel,sizeof mrel,"%s%s",dir,nm);
			char mfull[512]; resolve_asset(mrel,mfull,sizeof mfull);
			FILE *mf=fopen(mfull,"r");
			if(mf){ char ml[256];
				while(fgets(ml,sizeof ml,mf)){
					if(!strncmp(ml,"newmtl",6)){ if(nmtl<64){ mtl[nmtl].name[0]=0; sscanf(ml+6,"%63s",mtl[nmtl].name); mtl[nmtl].col=ENG_WHITE; mtl[nmtl].tint=(strstr(mtl[nmtl].name,"tint")!=NULL); mtl[nmtl].tex=NULL; nmtl++; } }
					else if(!strncmp(ml,"map_Kd",6) && nmtl>0){ char tf[160]=""; sscanf(ml+6,"%159s",tf); char trel[420]; snprintf(trel,sizeof trel,"%s%s",dir,tf); mtl[nmtl-1].tex=eng_image_from_png(trel); }
					else if(!strncmp(ml,"Kd",2) && nmtl>0){ float r,g,b; if(sscanf(ml+2,"%f %f %f",&r,&g,&b)==3) mtl[nmtl-1].col=ENG_RGB((int)(r*255),(int)(g*255),(int)(b*255)); }
				}
				fclose(mf);
			}
		}
	}
	fclose(f); free(V); free(TU); free(TV);
	if(!m->n) fprintf(stderr,"engine: obj %s has no faces\n", full);
	return m;
}


/* Smooth per-vertex normals with a crease threshold: a face contributes to a shared vertex only if
 * its normal is within ~60deg of this triangle's (aligned for winding), so cube edges (90deg) stay
 * flat while curved surfaces (sphere) shade smoothly. O(n^2), run once per mesh. */
static eng_vec3 face_n(const struct tri3 *t) { return v_norm(v_cross(v_sub(t->b,t->a), v_sub(t->c,t->a))); }
static int near_v(eng_vec3 a, eng_vec3 b) { eng_vec3 d = v_sub(a,b); return v_dot(d,d) < 1e-8f; }
static void mesh_finalize(eng_mesh *m)
{
	if (m->finalized) return;
	const float CREASE = 0.5f;   /* cos(60deg) */
	for (int i = 0; i < m->n; i++) {
		eng_vec3 ni = face_n(&m->t[i]);
		eng_vec3 vtx[3] = { m->t[i].a, m->t[i].b, m->t[i].c }, out[3];
		for (int k = 0; k < 3; k++) {
			eng_vec3 acc = eng_v3(0,0,0);
			for (int j = 0; j < m->n; j++) {
				const struct tri3 *tj = &m->t[j];
				if (!(near_v(vtx[k],tj->a) || near_v(vtx[k],tj->b) || near_v(vtx[k],tj->c))) continue;
				eng_vec3 nj = face_n(tj); float d = v_dot(ni, nj);
				if (d >= CREASE)       acc = eng_v3(acc.x+nj.x, acc.y+nj.y, acc.z+nj.z);
				else if (d <= -CREASE) acc = eng_v3(acc.x-nj.x, acc.y-nj.y, acc.z-nj.z);   /* opposite winding, same surface */
			}
			out[k] = v_norm(acc);
		}
		m->t[i].na = out[0]; m->t[i].nb = out[1]; m->t[i].nc = out[2];
	}
	m->finalized = 1;
}

static eng_color modulate(eng_color c, eng_color t)
{
	unsigned r=(((c>>16)&0xff)*((t>>16)&0xff))/255, g=(((c>>8)&0xff)*((t>>8)&0xff))/255, b=((c&0xff)*(t&0xff))/255;
	return (r<<16)|(g<<8)|b;
}
/* Gouraud: rotate each vertex's smooth normal with the mesh, light it two-sided, and let eng_tri_gouraud
 * interpolate the per-vertex intensity across the face. Box meshes keep hard normals (see mesh_finalize)
 * so they look identical to the old flat path; curved meshes now shade smoothly. */
void eng_mesh_draw(const eng_mesh *m, eng_vec3 pos, float yaw, float pitch, float roll, float scale, eng_color tint)
{
	if (!m) return;
	mesh_finalize((eng_mesh *)m);                                                /* lazy: compute normals once */
	float cyw=cosf(yaw), syw=sinf(yaw), cxw=cosf(pitch), sxw=sinf(pitch), crl=cosf(roll), srl=sinf(roll);
	for (int i = 0; i < m->n; i++) {
		eng_vec3 src[3] = { m->t[i].a, m->t[i].b, m->t[i].c };
		eng_vec3 nrm[3] = { m->t[i].na, m->t[i].nb, m->t[i].nc };
		float px[3], py[3], iz[3], inten[3]; int ok = 1;
		for (int k = 0; k < 3; k++) {
			eng_vec3 p = eng_v3(src[k].x*scale, src[k].y*scale, src[k].z*scale);
			eng_vec3 pr = eng_v3(p.x*crl - p.y*srl, p.x*srl + p.y*crl, p.z);       /* roll (bank) about +Z */
			eng_vec3 q = eng_v3(pr.x*cyw + pr.z*syw, pr.y, -pr.x*syw + pr.z*cyw);  /* yaw about +Y */
			eng_vec3 r = eng_v3(q.x, q.y*cxw - q.z*sxw, q.y*sxw + q.z*cxw);        /* pitch about +X */
			eng_vec3 w = eng_v3(r.x + pos.x, r.y + pos.y, r.z + pos.z);
			if (!eng_project(w, &px[k], &py[k], &iz[k])) { ok = 0; break; }
			eng_vec3 nrl = eng_v3(nrm[k].x*crl - nrm[k].y*srl, nrm[k].x*srl + nrm[k].y*crl, nrm[k].z);
			eng_vec3 nq = eng_v3(nrl.x*cyw + nrl.z*syw, nrl.y, -nrl.x*syw + nrl.z*cyw);
			eng_vec3 nr = eng_v3(nq.x, nq.y*cxw - nq.z*sxw, nq.y*sxw + nq.z*cxw);  /* rotate normal like the vertex */
			float d = v_dot(nr, l_dir); if (d < 0) d = -d;                        /* two-sided: robust for hand-wound meshes */
			inten[k] = l_amb + (1.0f - l_amb) * d;
		}
		if (!ok) continue;
		eng_color base = m->t[i].tint ? modulate(m->t[i].col, tint) : m->t[i].col;
		if (m->tex) {                                                            /* textured x colour x light */
			eng_vtx_tex A = { px[0], py[0], iz[0], m->t[i].ua, m->t[i].va };
			eng_vtx_tex B = { px[1], py[1], iz[1], m->t[i].ub, m->t[i].vb };
			eng_vtx_tex C = { px[2], py[2], iz[2], m->t[i].uc, m->t[i].vc };
			eng_tri_tex_lit(A, B, C, m->tex, inten[0], inten[1], inten[2], base);
		} else {                                                                 /* flat-colour + lit */
			eng_vtx A = { px[0], py[0], iz[0] }, B = { px[1], py[1], iz[1] }, C = { px[2], py[2], iz[2] };
			eng_tri_gouraud(A, B, C, base, inten[0], inten[1], inten[2]);
		}
	}
}
