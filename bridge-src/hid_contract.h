#ifndef NS09_HID_CONTRACT_H
#define NS09_HID_CONTRACT_H
#include "ns09_protocol.h"
typedef struct { uint8_t left, right; int64_t deadline; } ns09_writer_message;
typedef int (*ns09_hid_io)(void *, const uint8_t *, unsigned);
static inline int ns09_resolve_message(ns09_writer_message *m, int64_t now) {
    if (m->left > 100 || m->right > 100) return -1;
    if (now >= m->deadline) m->left = m->right = 0;
    return 0;
}
static inline int ns09_transfer(ns09_hid_io write_report, void *context, unsigned left, unsigned right) {
    if (left > 100 || right > 100) return -2;
    uint8_t report[NS09_REPORT_SIZE]; ns09_encode_report(report, (uint8_t)left, (uint8_t)right);
    return write_report(context, report, NS09_REPORT_SIZE) == NS09_REPORT_SIZE ? 0 : -1;
}
#endif
