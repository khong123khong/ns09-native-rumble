#include "hid_contract.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); exit(1); } } while(0)
typedef struct { int result; unsigned calls; uint8_t report[9]; } transport;
static int injected_write(void *context, const uint8_t *report, unsigned length) {
    transport *t=context; CHECK(length==9); t->calls++; memcpy(t->report,report,9); return t->result;
}
int main(void) {
    transport t={0}; t.result=9;
    CHECK(ns09_transfer(injected_write,&t,29,0)==0); CHECK(t.report[4]==29 && t.report[5]==0);
    CHECK(ns09_transfer(injected_write,&t,0,0)==0); CHECK(!memcmp(t.report,NS09_STOP_REPORT,9));
    for (int n=-1; n<9; n++) { t.result=n; CHECK(ns09_transfer(injected_write,&t,100,100)==-1); }
    unsigned calls=t.calls; CHECK(ns09_transfer(injected_write,&t,101,0)==-2); CHECK(t.calls==calls);
    ns09_writer_message m={20,80,500}; CHECK(!ns09_resolve_message(&m,499)); CHECK(m.left==20 && m.right==80);
    CHECK(!ns09_resolve_message(&m,500)); CHECK(m.left==0 && m.right==0);
    m.left=255; CHECK(ns09_resolve_message(&m,0)==-1);
    m.left=29; m.right=29; m.deadline=1000; CHECK(!ns09_resolve_message(&m,1200)); CHECK(!m.left && !m.right);
    m.left=10; m.right=30; m.deadline=INT64_MAX; CHECK(!ns09_resolve_message(&m,0)); CHECK(m.left==10 && m.right==30);
    printf("PASS: %u production HID transport/watchdog assertions; full write, errors, partial write, exact Stop, stale command expiry\n", checks);
    return 0;
}
