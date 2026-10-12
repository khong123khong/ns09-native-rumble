#ifndef NS09_XBOX_PROFILE_H
#define NS09_XBOX_PROFILE_H
#include <stdint.h>
#include <string.h>
/* Linux UAPI numbers. The native adapter asserts these against its headers.
   Android MotionEvent Z/RZ are supplied by keylayout from Linux RX/RY. */
#define NS09_XBOX_NAME "Microsoft X-Box 360 pad"
#define NS09_XBOX_VENDOR 0x045e
#define NS09_XBOX_PRODUCT 0x028e
#define NS09_XBOX_VERSION 0x0001
enum { NX_KEYS=768, NX_AXES=64, NX_KEY=1, NX_ABS=3,
    NX_A=304,NX_B=305,NX_C=306,NX_X=307,NX_Y=308,NX_Z=309,
    NX_LB=310,NX_RB=311,NX_L2=312,NX_R2=313,NX_BACK=314,NX_START=315,
    NX_GUIDE=316,NX_L3=317,NX_R3=318,NX_MENU=139,NX_HOMEPAGE=172,
    NX_UP=544,NX_DOWN=545,NX_LEFT=546,NX_RIGHT=547,
    NX_X_AXIS=0,NX_Y_AXIS=1,NX_Z_AXIS=2,NX_RX=3,NX_RY=4,NX_RZ=5,
    NX_GAS=9,NX_BRAKE=10,NX_HAT_X=16,NX_HAT_Y=17 };
typedef struct { int32_t value,minimum,maximum,fuzz,flat,resolution; } nx_absinfo;
typedef struct { uint8_t keys[NX_KEYS], axes[NX_AXES]; nx_absinfo abs[NX_AXES]; } nx_caps;
typedef struct {
    int key_to[NX_KEYS], axis_to[NX_AXES], axis_from[NX_AXES];
    uint8_t keys[NX_KEYS], axes[NX_AXES]; nx_absinfo abs[NX_AXES], original[NX_AXES];
    int sequential, digital_trigger[2], digital_hat;
} nx_profile;
typedef struct { uint8_t dpad[4]; } nx_state;
typedef struct { unsigned type,code; int value; } nx_event;
static inline int32_t nx_scale(int32_t value, int32_t src_min, int32_t src_max, int32_t dst_min, int32_t dst_max) {
    if (value < src_min) value=src_min; if (value > src_max) value=src_max;
    if (src_max <= src_min) return dst_min;
    return (int32_t)((int64_t)dst_min + ((int64_t)value-src_min)*((int64_t)dst_max-dst_min)/((int64_t)src_max-src_min));
}
static inline int32_t nx_distance(int32_t v, const nx_absinfo *src, int64_t span) {
    if (v <= 0) return 0;
    int64_t old=(int64_t)src->maximum-src->minimum;
    int64_t result=((int64_t)v*span+old/2)/old;
    return result > INT32_MAX ? INT32_MAX : (int32_t)result;
}
static inline int nx_bind_axis(nx_profile *p,const nx_caps *c,unsigned src,unsigned dst,int32_t lo,int32_t hi) {
    if (src>=NX_AXES || dst>=NX_AXES || !c->axes[src] || c->abs[src].maximum<=c->abs[src].minimum || p->axis_to[src]>=0 || p->axes[dst]) return -1;
    p->axis_to[src]=(int)dst; p->axis_from[dst]=(int)src; p->axes[dst]=1;
    p->original[src]=c->abs[src]; p->abs[dst]=c->abs[src];
    p->abs[dst].minimum=lo; p->abs[dst].maximum=hi;
    p->abs[dst].value=nx_scale(c->abs[src].value,c->abs[src].minimum,c->abs[src].maximum,lo,hi);
    p->abs[dst].fuzz=nx_distance(c->abs[src].fuzz,&c->abs[src],(int64_t)hi-lo);
    p->abs[dst].flat=nx_distance(c->abs[src].flat,&c->abs[src],(int64_t)hi-lo);
    p->abs[dst].resolution=nx_distance(c->abs[src].resolution,&c->abs[src],(int64_t)hi-lo);
    return 0;
}
static inline void nx_bind_key(nx_profile *p,const nx_caps *c,unsigned src,unsigned dst) {
    if (c->keys[src]) { p->key_to[src]=(int)dst; p->keys[dst]=1; }
}
static inline int nx_init(nx_profile *p,const nx_caps *c) {
    memset(p,0,sizeof(*p));
    for(unsigned k=0;k<NX_KEYS;k++) p->key_to[k]=-1;
    for(unsigned a=0;a<NX_AXES;a++) p->axis_to[a]=p->axis_from[a]=-1;
    p->digital_trigger[0]=p->digital_trigger[1]=-1;
    const unsigned native[]={NX_A,NX_B,NX_X,NX_Y,NX_LB,NX_RB,NX_BACK,NX_START,NX_L3,NX_R3};
    int is_native=1;
    for(unsigned n=0;n<sizeof(native)/sizeof(native[0]);n++) if(!c->keys[native[n]]) is_native=0;
    if(is_native) {
        for(unsigned n=0;n<sizeof(native)/sizeof(native[0]);n++) nx_bind_key(p,c,native[n],native[n]);
    } else {
        /* Descriptor of the validated NS09: Button usages1..10, generic HID
           evdev codes304..313. This matches AOSP's physical02e0 keylayout. */
        for(unsigned n=0;n<10;n++) if(!c->keys[NX_A+n]) return -1;
        if(c->keys[NX_L3] || c->keys[NX_R3]) return -1; /* ambiguous layout */
        p->sequential=1;
        for(unsigned n=0;n<10;n++) nx_bind_key(p,c,NX_A+n,native[n]);
    }
    if(c->keys[NX_GUIDE]) nx_bind_key(p,c,NX_GUIDE,NX_GUIDE);
    else if(c->keys[NX_MENU]) nx_bind_key(p,c,NX_MENU,NX_GUIDE);
    else if(c->keys[NX_HOMEPAGE]) nx_bind_key(p,c,NX_HOMEPAGE,NX_GUIDE);
    if(nx_bind_axis(p,c,NX_X_AXIS,NX_X_AXIS,-32768,32767) || nx_bind_axis(p,c,NX_Y_AXIS,NX_Y_AXIS,-32768,32767)) return -1;
    unsigned rx,ry,lt=NX_AXES,rt=NX_AXES;
    if(c->axes[NX_RX] && c->axes[NX_RY]) {
        rx=NX_RX; ry=NX_RY;
        if(c->axes[NX_BRAKE] && c->axes[NX_GAS]) { lt=NX_BRAKE; rt=NX_GAS; }
        else if(c->axes[NX_Z_AXIS] && c->axes[NX_RZ]) { lt=NX_Z_AXIS; rt=NX_RZ; }
    } else if(c->axes[NX_Z_AXIS] && c->axes[NX_RZ]) {
        rx=NX_Z_AXIS; ry=NX_RZ;
        if(c->axes[NX_BRAKE] && c->axes[NX_GAS]) { lt=NX_BRAKE; rt=NX_GAS; }
    } else return -1;
    if(nx_bind_axis(p,c,rx,NX_RX,-32768,32767) || nx_bind_axis(p,c,ry,NX_RY,-32768,32767)) return -1;
    if(lt<NX_AXES) {
        if(nx_bind_axis(p,c,lt,NX_Z_AXIS,0,65535) || nx_bind_axis(p,c,rt,NX_RZ,0,65535)) return -1;
    } else {
        if(p->sequential || !c->keys[NX_L2] || !c->keys[NX_R2]) return -1;
        p->digital_trigger[0]=NX_L2; p->digital_trigger[1]=NX_R2;
        p->axes[NX_Z_AXIS]=p->axes[NX_RZ]=1;
        p->abs[NX_Z_AXIS].maximum=p->abs[NX_RZ].maximum=65535;
    }
    if(c->axes[NX_HAT_X] && c->axes[NX_HAT_Y]) {
        if(nx_bind_axis(p,c,NX_HAT_X,NX_HAT_X,-1,1) || nx_bind_axis(p,c,NX_HAT_Y,NX_HAT_Y,-1,1)) return -1;
    } else {
        if(!c->keys[NX_UP] || !c->keys[NX_DOWN] || !c->keys[NX_LEFT] || !c->keys[NX_RIGHT]) return -1;
        p->digital_hat=1; p->axes[NX_HAT_X]=p->axes[NX_HAT_Y]=1;
        p->abs[NX_HAT_X].minimum=p->abs[NX_HAT_Y].minimum=-1;
        p->abs[NX_HAT_X].maximum=p->abs[NX_HAT_Y].maximum=1;
    }
    return 0;
}
/* At most one output per input; all transitions are published with the
   original SYN_REPORT. Unsupported scan usages are not copied to the proxy. */
static inline int nx_translate(const nx_profile *p,nx_state *state,unsigned type,unsigned code,int value,nx_event *out) {
    if(type==NX_KEY && code<NX_KEYS) {
        if(p->key_to[code]>=0) { *out=(nx_event){NX_KEY,(unsigned)p->key_to[code],value}; return 1; }
        for(unsigned i=0;i<2;i++) if((int)code==p->digital_trigger[i]) {
            *out=(nx_event){NX_ABS,i ? NX_RZ : NX_Z_AXIS,value ? 65535 : 0}; return 1;
        }
        if(p->digital_hat && code>=NX_UP && code<=NX_RIGHT) {
            state->dpad[code-NX_UP]=value!=0;
            unsigned axis=code<=NX_DOWN ? NX_HAT_Y : NX_HAT_X;
            int v=axis==NX_HAT_Y ? state->dpad[1]-state->dpad[0] : state->dpad[3]-state->dpad[2];
            *out=(nx_event){NX_ABS,axis,v}; return 1;
        }
    } else if(type==NX_ABS && code<NX_AXES && p->axis_to[code]>=0) {
        unsigned target=(unsigned)p->axis_to[code];
        *out=(nx_event){NX_ABS,target,nx_scale(value,p->original[code].minimum,p->original[code].maximum,p->abs[target].minimum,p->abs[target].maximum)}; return 1;
    } else if(type==0) { *out=(nx_event){type,code,value}; return 1; }
    return 0;
}
#endif
