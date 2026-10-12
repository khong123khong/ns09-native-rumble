#ifndef NS09_PROTOCOL_H
#define NS09_PROTOCOL_H
#include <stdint.h>
#include <string.h>
#include "protocol_bytes.h"
enum { NS09_REPORT_SIZE = 9, NS09_REPORT_LEFT = 4, NS09_REPORT_RIGHT = 5 };
/* hidraw: Report ID + data, no HIDP/HCI headers. Keep fixed bytes from the
   captured Stop fixture. Reversing mask0F leaves0F. Only magnitude bytes vary;
   full-range logical encoding is not a measured physical response curve. */
static inline void ns09_encode_report(uint8_t out[NS09_REPORT_SIZE], uint8_t left, uint8_t right) {
    memcpy(out, NS09_STOP_REPORT, NS09_REPORT_SIZE);
    out[NS09_REPORT_LEFT] = left > 100 ? 100 : left;
    out[NS09_REPORT_RIGHT] = right > 100 ? 100 : right;
}
#endif
