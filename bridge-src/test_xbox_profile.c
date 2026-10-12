#include "xbox_profile.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);exit(1);} } while(0)
static nx_caps legacy(void) {
    nx_caps c={0};
    for(unsigned k=304;k<=313;k++) c.keys[k]=1; c.keys[NX_MENU]=1;
    for(unsigned a=0;a<6;a++) { c.axes[a]=1; c.abs[a]=(nx_absinfo){0,0,(a==2 || a==5) ? 1023 : 65535,1,0,0}; }
    c.axes[16]=c.axes[17]=1; c.abs[16]=c.abs[17]=(nx_absinfo){0,-1,1,0,0,0};
    return c;
}
int main(void) {
    nx_caps c=legacy(); nx_profile p; nx_state s={0}; nx_event e;
    CHECK(!nx_init(&p,&c)); CHECK(p.sequential);
    const unsigned expected[]={304,305,307,308,310,311,314,315,317,318};
    for(unsigned n=0;n<10;n++) {
        CHECK(p.key_to[304+n]==(int)expected[n]);
        CHECK(nx_translate(&p,&s,NX_KEY,304+n,1,&e)==1); CHECK(e.type==NX_KEY && e.code==expected[n] && e.value==1);
        CHECK(nx_translate(&p,&s,NX_KEY,304+n,0,&e)==1); CHECK(e.code==expected[n] && !e.value);
    }
    CHECK(p.key_to[NX_MENU]==NX_GUIDE); CHECK(!p.keys[NX_C] && !p.keys[NX_Z]);
    CHECK(p.axis_to[0]==0 && p.axis_to[1]==1 && p.axis_to[3]==3 && p.axis_to[4]==4 && p.axis_to[2]==2 && p.axis_to[5]==5);
    CHECK(!nx_translate(&p,&s,4,4,0x90001,&e)); /* no legacy MSC_SCAN leakage */
    for(int v=0;v<=65535;v++) {
        CHECK(nx_translate(&p,&s,NX_ABS,NX_RX,v,&e)); CHECK(e.code==NX_RX);
        CHECK(e.value==v-32768);
        CHECK(nx_translate(&p,&s,NX_ABS,NX_Y_AXIS,v,&e)); CHECK(e.value==v-32768); /* no axis inversion */
    }
    for(int v=0;v<=1023;v++) {
        CHECK(nx_translate(&p,&s,NX_ABS,NX_Z_AXIS,v,&e)); CHECK(e.code==NX_Z_AXIS);
        CHECK(e.value==(int)((int64_t)v*65535/1023));
        CHECK(nx_translate(&p,&s,NX_ABS,NX_RZ,v,&e)); CHECK(e.code==NX_RZ);
    }
    for(int v=-1;v<=1;v++) { CHECK(nx_translate(&p,&s,NX_ABS,16,v,&e)); CHECK(e.code==16 && e.value==v); CHECK(nx_translate(&p,&s,NX_ABS,17,v,&e)); CHECK(e.code==17 && e.value==v); }
    CHECK(nx_scale(-999,0,1023,0,65535)==0); CHECK(nx_scale(9999,0,1023,0,65535)==65535);
    CHECK(nx_scale(INT32_MAX,INT32_MIN,INT32_MAX,-32768,32767)==32767);
    CHECK(nx_scale(INT32_MIN,INT32_MIN,INT32_MAX,-32768,32767)==-32768);
    /* Standard Linux gamepad source, modern HID rightZ/RZ, brake/gas triggers. */
    memset(c.keys,0,sizeof(c.keys)); for(unsigned n=0;n<10;n++) c.keys[expected[n]]=1; c.keys[NX_GUIDE]=1;
    c.axes[3]=c.axes[4]=0; c.axes[9]=c.axes[10]=1;
    c.abs[2]=c.abs[5]=(nx_absinfo){32768,0,65535,3,2048,0}; c.abs[9]=c.abs[10]=(nx_absinfo){0,0,255,0,0,0};
    CHECK(!nx_init(&p,&c)); CHECK(!p.sequential);
    CHECK(p.axis_to[2]==3 && p.axis_to[5]==4 && p.axis_to[10]==2 && p.axis_to[9]==5);
    CHECK(p.abs[3].flat==2048 && p.abs[3].fuzz==3 && p.abs[3].value==0);
    CHECK(nx_translate(&p,&s,NX_ABS,10,128,&e) && e.code==2 && e.value==32896);
    CHECK(nx_translate(&p,&s,NX_ABS,5,0,&e) && e.code==4 && e.value==-32768);
    /* Digital trigger/hat fallback only where no analog source exists. */
    c.axes[9]=c.axes[10]=c.axes[16]=c.axes[17]=0; c.keys[NX_L2]=c.keys[NX_R2]=1;
    for(unsigned k=NX_UP;k<=NX_RIGHT;k++) c.keys[k]=1;
    CHECK(!nx_init(&p,&c)); CHECK(p.digital_hat && p.digital_trigger[0]==NX_L2);
    CHECK(nx_translate(&p,&s,NX_KEY,NX_L2,1,&e) && e.code==2 && e.value==65535);
    CHECK(nx_translate(&p,&s,NX_KEY,NX_L2,0,&e) && e.value==0);
    CHECK(nx_translate(&p,&s,NX_KEY,NX_UP,1,&e) && e.code==17 && e.value==-1);
    CHECK(nx_translate(&p,&s,NX_KEY,NX_DOWN,1,&e) && e.value==0);
    CHECK(nx_translate(&p,&s,NX_KEY,NX_UP,0,&e) && e.value==1);
    CHECK(nx_translate(&p,&s,NX_KEY,NX_DOWN,0,&e) && e.value==0);
    CHECK(nx_translate(&p,&s,NX_KEY,NX_LEFT,1,&e) && e.code==16 && e.value==-1);
    CHECK(nx_translate(&p,&s,NX_KEY,NX_LEFT,0,&e) && e.value==0);
    c=legacy(); c.keys[306]=0; CHECK(nx_init(&p,&c)==-1); /* don't guess unknown button layout */
    c=legacy(); c.abs[3].minimum=c.abs[3].maximum; CHECK(nx_init(&p,&c)==-1);
    c=legacy(); c.keys[317]=1; CHECK(nx_init(&p,&c)==-1); /* ambiguous sequential/standard */
    puts("PASS: Xbox360 identity and mapping: legacy/native buttons, Guide, sticks, analog/digital triggers, dpad, release, range/deadzone, unknown-layout rejection");
    printf("PASS: %u production Xbox360 profile assertions\n",checks);
    return 0;
}
