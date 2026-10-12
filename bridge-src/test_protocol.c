#include "ns09_protocol.h"
#include "rumble_core.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
int main(void) {
    /* Literal expectations from local capture / Android replay evidence. */
    static const uint8_t left[] = {3,15,0,0,29,0,255,0,235};
    static const uint8_t right[] = {3,15,0,0,0,29,255,0,235};
    static const uint8_t both[] = {3,15,0,0,29,29,255,0,235};
    static const uint8_t stop[] = {3,15,0,0,0,0,255,0,235};
    uint8_t report[9];
    ns09_encode_report(report, 29, 0); CHECK(memcmp(report, left, 9) == 0);
    ns09_encode_report(report, 0, 29); CHECK(memcmp(report, right, 9) == 0);
    ns09_encode_report(report, 29, 29); CHECK(memcmp(report, both, 9) == 0);
    ns09_encode_report(report, 0, 0); CHECK(memcmp(report, stop, 9) == 0);
    CHECK(memcmp(NS09_STOP_REPORT, stop, 9) == 0);
    ns09_encode_report(report, ns09_scale(19660, 65535, 100), 0);
    CHECK(memcmp(report, left, 9) == 0);
    ns09_encode_report(report, ns09_scale(655, 65535, 100), ns09_scale(65535, 65535, 100));
    CHECK(report[4] == 1 && report[5] == 100);
    ns09_encode_report(report, 255, 101); CHECK(report[4] == 100 && report[5] == 100);
    for (unsigned l = 0; l <= 100; l++) for (unsigned r = 0; r <= 100; r++) {
        ns09_encode_report(report, (uint8_t)l, (uint8_t)r);
        CHECK(report[4] == l && report[5] == r);
        for (unsigned i = 0; i < 9; i++) if (i != 4 && i != 5) CHECK(report[i] == stop[i]);
    }
    printf("PASS: %u protocol assertions; exact captured fixtures, full range, independent channels, immutable framing\n", checks);
    return 0;
}
