#ifndef NS09_RUMBLE_CORE_H
#define NS09_RUMBLE_CORE_H
#include <stdint.h>
#include <string.h>
#define NS09_SLOTS 32
#define NS09_LEASE_MS 5000
#define NS09_LOGICAL_MAX 100
/* Single event-loop owner; no state is shared with the HID watchdog. */
typedef struct {
    unsigned valid, active;
    uint16_t strong, weak, length, delay;
    uint32_t repeats;
    int64_t started, lease;
} ns09_effect;
typedef struct { ns09_effect effect[NS09_SLOTS]; uint16_t gain; uint8_t cap; } ns09_core;
typedef struct { uint8_t left, right; int64_t next; } ns09_output;
static inline int64_t ns09_deadline(int64_t now, int64_t delay) {
    return now > INT64_MAX - delay ? INT64_MAX : now + delay;
}
static inline void ns09_init(ns09_core *s, uint8_t cap) {
    memset(s, 0, sizeof(*s)); s->gain = 65535; s->cap = cap > 100 ? 100 : cap;
}
static inline int ns09_expired(const ns09_effect *e, int64_t now) {
    int64_t length = e->length ? e->length : NS09_LEASE_MS;
    int64_t total = (length + e->delay) * e->repeats;
    return now >= e->lease || now < e->started || now - e->started >= total;
}
static inline int ns09_upload(ns09_core *s, unsigned id, uint16_t strong, uint16_t weak,
                       uint16_t length, uint16_t delay, int64_t now) {
    if (id >= NS09_SLOTS) return -1;
    ns09_effect *e = &s->effect[id];
    /* Expired playback must not be resurrected by upload alone. */
    if (e->active && ns09_expired(e, now)) e->active = 0;
    e->valid = 1; e->strong = strong; e->weak = weak; e->length = length; e->delay = delay;
    /* Linux ff-memless style update: use a fresh replay window. */
    if (e->active) { e->started = now; e->lease = ns09_deadline(now, (int64_t)delay + NS09_LEASE_MS); }
    return 0;
}
static inline void ns09_play(ns09_core *s, unsigned id, int count, int64_t now) {
    if (id >= NS09_SLOTS || !s->effect[id].valid) return;
    ns09_effect *e = &s->effect[id];
    e->active = count > 0;
    e->repeats = count > 0 ? (uint32_t)count : 0;
    /* Initial delay is silent and does not consume the motor-on safety lease. */
    e->started = now; e->lease = ns09_deadline(now, (int64_t)e->delay + NS09_LEASE_MS);
}
static inline void ns09_erase(ns09_core *s, unsigned id) {
    if (id < NS09_SLOTS) memset(&s->effect[id], 0, sizeof(s->effect[id]));
}
static inline uint8_t ns09_scale(uint16_t mag, uint16_t gain, uint8_t cap) {
    if (!mag || !gain || !cap) return 0;
    /* Logical 0..100, floor preserves captured word19660 -> 29. Small nonzero
       requests use the minimum logical step. cap is a ceiling, not compression.
       This is not a measured physical motor response curve. */
    uint64_t value = (uint64_t)mag * gain * NS09_LOGICAL_MAX / 4294836225ULL;
    if (!value) value = 1;
    if (cap > NS09_LOGICAL_MAX) cap = NS09_LOGICAL_MAX;
    return (uint8_t)(value > cap ? cap : value);
}
static inline ns09_output ns09_render(ns09_core *s, int64_t now) {
    /* No active effect requires no timer; wake on the next external event. */
    ns09_output out = {0, 0, INT64_MAX};
    uint16_t left = 0, right = 0;
    for (unsigned i = 0; i < NS09_SLOTS; i++) {
        ns09_effect *e = &s->effect[i];
        if (!e->valid || !e->active) continue;
        if (ns09_expired(e, now)) { e->active = 0; continue; }
        int64_t len = e->length ? e->length : NS09_LEASE_MS;
        int64_t period = len + e->delay;
        int64_t elapsed = now - e->started;
        int64_t phase = elapsed % period;
        int64_t next = ns09_deadline(now, phase < e->delay ? e->delay - phase : period - phase);
        if (e->lease < next) next = e->lease;
        if (next < out.next) out.next = next;
        if (phase >= e->delay) {
            /* Overlap uses independent maxima, never amplifying a request. */
            if (e->strong > left) left = e->strong;
            if (e->weak > right) right = e->weak;
        }
    }
    out.left = ns09_scale(left, s->gain, s->cap);
    out.right = ns09_scale(right, s->gain, s->cap);
    return out;
}
#endif
