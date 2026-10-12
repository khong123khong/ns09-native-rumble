#include "rumble_core.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
int main(void) {
    ns09_core s; ns09_output o;
    ns09_init(&s, 29);
    CHECK(ns09_render(&s, 0).left == 0);
    CHECK(ns09_upload(&s, 32, 1, 1, 1, 0, 0) == -1);
    ns09_upload(&s, 0, 65535, 0, 500, 100, 0); ns09_play(&s, 0, 1, 0);
    CHECK(ns09_render(&s, 99).left == 0);
    o = ns09_render(&s, 100); CHECK(o.left == 29 && o.right == 0 && o.next == 600);
    CHECK(ns09_render(&s, 599).left == 29);
    CHECK(ns09_render(&s, 600).left == 0);
    ns09_upload(&s, 1, 0, 65535, 1000, 0, 700); ns09_play(&s, 1, 1, 700);
    ns09_play(&s, 0, 1, 700); o = ns09_render(&s, 900);
    CHECK(o.left == 29 && o.right == 29);
    ns09_play(&s, 0, 0, 901); o = ns09_render(&s, 901);
    CHECK(o.left == 0 && o.right == 29);
    ns09_erase(&s, 1); CHECK(ns09_render(&s, 902).right == 0);
    ns09_upload(&s, 2, 65535, 32768, 100, 50, 0); ns09_play(&s, 2, 2, 1000);
    CHECK(ns09_render(&s, 1049).left == 0);
    CHECK(ns09_render(&s, 1050).left == 29);
    CHECK(ns09_render(&s, 1150).left == 0);
    CHECK(ns09_render(&s, 1200).left == 29);
    CHECK(ns09_render(&s, 1300).left == 0);
    ns09_upload(&s, 3, 65535, 65535, 0, 0, 2000); ns09_play(&s, 3, 100000, 2000);
    CHECK(ns09_render(&s, 6999).left == 29);
    CHECK(ns09_render(&s, 7000).left == 0);
    ns09_upload(&s, 4, 65535, 65535, 1000, 0, 8000); ns09_play(&s, 4, 1, 8000);
    ns09_upload(&s, 4, 0, 10000, 1000, 0, 8100); o = ns09_render(&s, 8100);
    CHECK(o.left == 0 && o.right > 0 && o.right < 29);
    s.gain = 0; CHECK(ns09_render(&s, 8101).right == 0);
    ns09_init(&s, 255); CHECK(s.cap == 100);
    for (unsigned n = 0; n <= 65535; n++) {
        unsigned v = ns09_scale((uint16_t)n, 65535, 29);
        CHECK(v <= 29);
        if (n) CHECK(v >= ns09_scale((uint16_t)(n - 1), 65535, 29));
    }
    ns09_init(&s, 100);
    CHECK(ns09_render(&s, 0).next == INT64_MAX);
    CHECK(ns09_scale(19660,65535,100)==29);
    CHECK(ns09_scale(65535,65535,100)==100);
    CHECK(ns09_scale(1,65535,100)==1);
    CHECK(ns09_scale(0,65535,100)==0);
    ns09_upload(&s, 0, 65535, 16384, 100, 6000, 0); ns09_play(&s,0,1,0);
    CHECK(ns09_render(&s,5999).left==0); CHECK(ns09_render(&s,6000).left==100);
    CHECK(ns09_render(&s,6099).right==25); CHECK(ns09_render(&s,6100).left==0);
    ns09_upload(&s,0,65535,0,100,0,7000); ns09_play(&s,0,1,7000);
    ns09_upload(&s,0,0,65535,100,0,7200); CHECK(ns09_render(&s,7200).right==0);
    for (unsigned n=1; n<=65535; n++) {
        unsigned scaled=ns09_scale((uint16_t)n,65535,100);
        CHECK(scaled>=1 && scaled<=100);
        CHECK(scaled>=ns09_scale((uint16_t)(n-1),65535,100));
    }
    printf("PASS: %u scheduler/intensity assertions; overlap, delay, stop, erase, gain, lease, full range\n", checks);
    return 0;
}
