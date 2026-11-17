/* Universal source-level maneuver geometry, derived in design from the Luka
 * route-path approach. It does NOT claim visual equivalence with his GLES2
 * shaders, FBO masks, 3D lighting, lane panels or motion engine. */
#include "alt_scene_geometry.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define PI 3.14159265358979323846f
#define SHOULDER_W .270f
#define ROAD_W .230f
#define ROUTE_W .135f
#define RING_R .28f
#define RING_SEG 64
#define ROUTE_SEG 32
#define JOINT_SEG 12

static float rad(float d){return d*(PI/180.f);}
static float clampf(float v,float a,float b){return v<a?a:v>b?b:v;}
static alt_scene_point pt(float x,float y){alt_scene_point p={x,y};return p;}
static int finitep(alt_scene_point p){return isfinite(p.x)&&isfinite(p.y);}
static void tri(alt_scene_mesh *o,int layer,alt_scene_point a,alt_scene_point b,alt_scene_point c){
 unsigned n=o->count[layer];
 if(n+3>ALT_SCENE_MAX_VERTICES||!finitep(a)||!finitep(b)||!finitep(c)) {o->overflow=1;return;}
 o->xy[layer][2*n]=a.x;o->xy[layer][2*n+1]=a.y;
 o->xy[layer][2*n+2]=b.x;o->xy[layer][2*n+3]=b.y;
 o->xy[layer][2*n+4]=c.x;o->xy[layer][2*n+5]=c.y;
 o->count[layer]=n+3;
}
static void disk(alt_scene_mesh *o,int layer,alt_scene_point p,float r,int steps){
 int k; for(k=0;k<steps;k++){
  float a=2.f*PI*k/steps,b=2.f*PI*(k+1)/steps;
  tri(o,layer,p,pt(p.x+r*cosf(a),p.y+r*sinf(a)),pt(p.x+r*cosf(b),p.y+r*sinf(b)));
 }
}
static void segment(alt_scene_mesh *o,int layer,alt_scene_point a,alt_scene_point b,float w){
 float dx=b.x-a.x,dy=b.y-a.y,n=hypotf(dx,dy),ox,oy;
 if(n<1e-5f)return;
 ox=-dy*w/(2.f*n);oy=dx*w/(2.f*n);
 tri(o,layer,pt(a.x+ox,a.y+oy),pt(a.x-ox,a.y-oy),pt(b.x+ox,b.y+oy));
 tri(o,layer,pt(a.x-ox,a.y-oy),pt(b.x-ox,b.y-oy),pt(b.x+ox,b.y+oy));
}
static void polyline(alt_scene_mesh *o,int layer,const alt_scene_point *p,int n,float w){
 int i;if(n<2)return;
 for(i=0;i<n-1;i++)segment(o,layer,p[i],p[i+1],w);
 for(i=1;i<n-1;i++)disk(o,layer,p[i],w*.5f,JOINT_SEG);
}
static void road(alt_scene_mesh *o,const alt_scene_point *p,int n){
 polyline(o,ALT_SCENE_SHOULDER,p,n,SHOULDER_W);
 polyline(o,ALT_SCENE_ASPHALT,p,n,ROAD_W);
}
static void arrow(alt_scene_mesh *o,alt_scene_point tip,float vx,float vy){
 float len=hypotf(vx,vy),dx,dy;
 if(len<1e-6f)return;
 dx=vx/len;dy=vy/len;
 tri(o,ALT_SCENE_ACCENT,tip,
     pt(tip.x-dx*.22f-dy*.145f,tip.y-dy*.22f+dx*.145f),
     pt(tip.x-dx*.22f+dy*.145f,tip.y-dy*.22f-dx*.145f));
}
static void active(alt_scene_mesh *o,const alt_scene_point *p,int n){
 if(n<2)return;
 polyline(o,ALT_SCENE_ACTIVE,p,n,ROUTE_W);
 arrow(o,p[n-1],p[n-1].x-p[n-2].x,p[n-1].y-p[n-2].y);
}
/* Luka-compatible control-point semantics. A route is built as straight
 * segments and circular fillets at every junction. The GLES backend receives
 * only triangles, never navigation decisions. */
static int filleted(const alt_scene_point *control,int n,alt_scene_point *p,int cap){
    int i,k,count=0;
    alt_scene_point cursor=control[0];
    if(n<2||cap<3)return 0;
    p[count++]=cursor;
    for(i=1;i<n-1;i++){
        alt_scene_point before=control[i-1],here=control[i],after=control[i+1];
        float dx=here.x-before.x,dy=here.y-before.y;
        float ex=after.x-here.x,ey=after.y-here.y;
        float dl=hypotf(dx,dy),el=hypotf(ex,ey);
        float dot,cross,alpha,tanHalf,t,sign,cx,cy,start,end,delta;
        if(dl<1e-6f||el<1e-6f){if(count<cap)p[count++]=here;continue;}
        dx/=dl;dy/=dl;ex/=el;ey/=el;
        dot=clampf(dx*ex+dy*ey,-1.f,1.f);alpha=acosf(dot);
        if(alpha<.09f||alpha>PI*.97f){if(count<cap)p[count++]=here;continue;}
        tanHalf=tanf((PI-alpha)*.5f);
        if(tanHalf<1e-6f){if(count<cap)p[count++]=here;continue;}
        t=clampf(.0675f/tanHalf,0.f,fminf(dl,el)*.45f);
        cross=dx*ey-dy*ex;sign=cross>=0.f?1.f:-1.f;
        {float x1=here.x-dx*t,y1=here.y-dy*t;
         float r=t*tanHalf;
         if(count>=cap-20)return 0;
         p[count++]=pt(x1,y1);
         cx=x1+sign*(-dy)*r;cy=y1+sign*dx*r;
         start=atan2f(y1-cy,x1-cx);
         end=atan2f(here.y+ey*t-cy,here.x+ex*t-cx);
         if(sign>0)while(end<=start)end+=2.f*PI;
         else while(end>=start)end-=2.f*PI;
         delta=end-start;
         for(k=1;k<=14;k++){
             float a=start+delta*k/14.f;
             p[count++]=pt(cx+r*cosf(a),cy+r*sinf(a));
         }
         cursor=p[count-1];
        }
    }
    if(count<cap)p[count++]=control[n-1];
    (void)cursor;
    return count;
}
static int rounded_curve(float degrees,alt_scene_point *p){
    alt_scene_point c[3];float a=rad(clampf(degrees,-175.f,175.f));
    c[0]=pt(0.f,-.55f);c[1]=pt(0.f,0.f);
    c[2]=pt(.5f*sinf(a),.5f*cosf(a));
    return filleted(c,3,p,RING_SEG+6);
}
static int s_bend(int icon,int direction,alt_scene_point *p){
    float sign=direction<0?-1.f:1.f;
    alt_scene_point c[4];
    if(icon==ALT_SCENE_MERGE){
        float start=-sign*.38f;
        c[0]=pt(start,-.55f);c[1]=pt(start,-.10f);
        c[2]=pt(0.f,.15f);c[3]=pt(0.f,.5f);
    }else{
        float shift=sign*.35f;
        c[0]=pt(0.f,-.55f);c[1]=pt(0.f,-.15f);
        c[2]=pt(shift,.15f);c[3]=pt(shift,.5f);
    }
    return filleted(c,4,p,RING_SEG+6);
}
static int uturn(alt_scene_point *p,int driving_side,int direction){
    int k,n=0;
    int left=direction!=0?direction<0:driving_side==0;
    float inX=left?.18f:-.18f, outX=-inX;
    float start=left?0.f:PI,end=left?PI:0.f;
    p[n++]=pt(inX,-.55f);p[n++]=pt(inX,.30f);
    for(k=1;k<=ROUTE_SEG;k++){
        float a=start+(end-start)*k/ROUTE_SEG;
        p[n++]=pt(.18f*cosf(a),.30f+.18f*sinf(a));
    }
    p[n++]=pt(outX,-.10f);
    return n;
}
static int build_ring(const alt_scene_maneuver *m,alt_scene_point *p){
    int k,n=0;float aStart=-PI*.5f,aEnd,exitDeg=(float)m->exit_angle_deg;
    float delta,r=RING_R;
    if(m->icon==ALT_SCENE_ROUNDABOUT_EXIT){
        /* Partial departure is a separate shape in Luka's route builder. */
        float sign=m->driving_side?-1.f:1.f,cx=-sign*r,cy=-.12f;
        aStart=m->driving_side?PI:0.f;aEnd=m->driving_side?PI*.75f:PI*.25f;
        p[n++]=pt(0.f,-.55f);p[n++]=pt(0.f,cy);
        for(k=1;k<=ROUTE_SEG;k++){
            float a=aStart+(aEnd-aStart)*k/ROUTE_SEG;
            p[n++]=pt(cx+r*cosf(a),cy+r*sinf(a));
        }
        {alt_scene_point last=p[n-1]; p[n++]=pt(last.x+.38f*cosf(aEnd),last.y+.38f*sinf(aEnd));}
        return n;
    }
    if(exitDeg<-180||exitDeg>180)return 0;
    /* Exact exit side-angle known? Snap within 30 deg to an existing arm. */
    for(k=0;k<m->junction_count;k++){
        float diff=fabsf(exitDeg-(float)m->junction_angles[k]);
        if(diff>180.f)diff=360.f-diff;
        if(diff<30.f){exitDeg=m->junction_angles[k];break;}
    }
    aEnd=rad(90.f-exitDeg);
    if(!m->driving_side){while(aEnd<=aStart)aEnd+=2.f*PI;}
    else{while(aEnd>=aStart)aEnd-=2.f*PI;}
    delta=aEnd-aStart;
    p[n++]=pt(0.f,-.55f);p[n++]=pt(0.f,-r);
    for(k=1;k<=RING_SEG;k++){
        float a=aStart+delta*k/RING_SEG;
        p[n++]=pt(r*cosf(a),r*sinf(a));
    }
    p[n++]=pt(.5f*cosf(aEnd),.5f*sinf(aEnd));
    return n;
}
static void other_road(alt_scene_mesh *o,float degrees){
 alt_scene_point p[2];float a=rad(degrees);
 p[0]=pt(0.f,0.f);p[1]=pt(.68f*sinf(a),.68f*cosf(a));road(o,p,2);
}
static void arrival(alt_scene_mesh *o){
 alt_scene_point p[2]={pt(0,-.72f),pt(0,.12f)};int i;
 road(o,p,2);polyline(o,ALT_SCENE_ACTIVE,p,2,ROUTE_W);
 disk(o,ALT_SCENE_ACCENT,pt(0,.12f),.15f,32);
 /* White perimeter ring is computed with triangles rather than shader stroke. */
 for(i=0;i<40;i++){
  float a=2*PI*i/40,b=2*PI*(i+1)/40;
  alt_scene_point outerA=pt(.28f*cosf(a),.12f+.28f*sinf(a));
  alt_scene_point outerB=pt(.28f*cosf(b),.12f+.28f*sinf(b));
  alt_scene_point innerA=pt(.255f*cosf(a),.12f+.255f*sinf(a));
  alt_scene_point innerB=pt(.255f*cosf(b),.12f+.255f*sinf(b));
  tri(o,ALT_SCENE_SHOULDER,outerA,innerA,outerB);tri(o,ALT_SCENE_SHOULDER,innerA,innerB,outerB);
 }
}
int alt_scene_build(const alt_scene_maneuver *m,alt_scene_mesh *o){
 alt_scene_point p[RING_SEG+6];int n=0,i;float a;
 if(!m||!o)return 0;
 memset(o,0,sizeof(*o));
 if(m->icon<=ALT_SCENE_NONE||m->icon>ALT_SCENE_ROUNDABOUT_EXIT||
    m->junction_count<0||m->junction_count>ALT_SCENE_MAX_JUNCTIONS||
    (m->driving_side!=0&&m->driving_side!=1)||m->direction<-1||m->direction>1)return 0;
 if(m->icon==ALT_SCENE_ARRIVED){arrival(o);return !o->overflow;}
 if(m->icon==ALT_SCENE_ROUNDABOUT||m->icon==ALT_SCENE_ROUNDABOUT_EXIT){
   n=build_ring(m,p);
   if(n==0)return 0;
   /* Native-looking ring and all explicitly supplied junction arms. */
   for(i=0;i<m->junction_count;i++){
     float angle=rad(clampf((float)m->junction_angles[i],-180.f,180.f));
     alt_scene_point branch[2]={pt(RING_R*sinf(angle),RING_R*cosf(angle)),
                                 pt(.75f*sinf(angle),.75f*cosf(angle))};
     road(o,branch,2);
   }
   road(o,p,n);active(o,p,n);return !o->overflow;
 }
 if(m->icon==ALT_SCENE_UTURN){n=uturn(p,m->driving_side,m->direction);}
 else {
   a=(m->exit_angle_deg>=-175&&m->exit_angle_deg<=175)?m->exit_angle_deg:
     (m->direction* (m->icon==ALT_SCENE_EXIT?30.f:90.f));
   if(m->icon==ALT_SCENE_MERGE||m->icon==ALT_SCENE_LANE_CHANGE||m->icon==ALT_SCENE_EXIT)
       n=s_bend(m->icon,m->direction,p);
   else if(m->icon==ALT_SCENE_APPROACH){p[0]=pt(0.f,-.55f);p[1]=pt(0.f,-.11f);n=2;}
   else n=rounded_curve(a,p);
 }
 /* Side streets are valid ONLY if supplied by route guidance. */
 for(i=0;i<m->junction_count;i++){
   int deg=m->junction_angles[i];
   if(deg>=-180&&deg<=180&&abs(deg)>5)other_road(o,(float)deg);
 }
 road(o,p,n);active(o,p,n);
 return !o->overflow&&o->count[ALT_SCENE_ACTIVE]>0;
}
