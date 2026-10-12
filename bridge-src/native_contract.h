#ifndef NS09_NATIVE_CONTRACT_H
#define NS09_NATIVE_CONTRACT_H

#include "rumble_core.h"
#include <errno.h>

/* Kernel-independent representation of the uinput FF handshake. Production
 * adapters perform UI_BEGIN/END_FF_*; tests inject failures into those same
 * transitions. Neither an unsuccessful BEGIN nor END commits core state. */
#define NS09_FF_RUMBLE_TYPE 0x50u
typedef struct {
    uint32_t request_id;
    int effect_id;
    unsigned effect_type;
    uint16_t strong, weak, length, delay;
    int retval;
} ns09_ff_upload_request;
typedef struct {
    uint32_t request_id, effect_id;
    int retval;
} ns09_ff_erase_request;
typedef int (*ns09_ff_upload_io)(void *, ns09_ff_upload_request *);
typedef int (*ns09_ff_erase_io)(void *, ns09_ff_erase_request *);

/* Result: 0 = completed kernel handshake (check request.retval), -1 = BEGIN
 * failed, -2 = END failed. A caller must terminate its connection on errors. */
static int ns09_ff_upload_contract(ns09_core *core,
                                  ns09_ff_upload_request *request,
                                  int64_t now, ns09_ff_upload_io begin,
                                  ns09_ff_upload_io end, void *context) {
    if (begin(context, request)) return -1;
    request->retval = request->effect_type == NS09_FF_RUMBLE_TYPE &&
        request->effect_id >= 0 && request->effect_id < NS09_SLOTS ? 0 : -EINVAL;
    if (end(context, request)) return -2;
    if (!request->retval)
        ns09_upload(core, (unsigned)request->effect_id, request->strong,
                    request->weak, request->length, request->delay, now);
    return 0;
}
static int ns09_ff_erase_contract(ns09_core *core,
                                 ns09_ff_erase_request *request,
                                 ns09_ff_erase_io begin,
                                 ns09_ff_erase_io end, void *context) {
    if (begin(context, request)) return -1;
    request->retval = request->effect_id < NS09_SLOTS ? 0 : -EINVAL;
    if (end(context, request)) return -2;
    if (!request->retval) ns09_erase(core, request->effect_id);
    return 0;
}

/* Input values are Linux UAPI event numbers; the daemon asserts equality to
 * its compiled headers. Callbacks retain the original native event so its
 * full timestamp and value are forwarded without alteration. */
#define NS09_EV_SYN 0u
#define NS09_EV_KEY 1u
#define NS09_EV_ABS 3u
#define NS09_EV_MSC 4u
#define NS09_SYN_REPORT 0u
#define NS09_SYN_DROPPED 3u
typedef int (*ns09_input_io)(void *);
/* Result: 1 forwarded unchanged, 0 suppressed/resynced, -1 I/O failure. */
static int ns09_forward_contract(int *dropped, unsigned type, unsigned code,
                                 ns09_input_io forward, ns09_input_io resync,
                                 void *context) {
    if (type == NS09_EV_SYN && code == NS09_SYN_DROPPED) {
        *dropped = 1;
        return 0;
    }
    if (*dropped) {
        if (type == NS09_EV_SYN && code == NS09_SYN_REPORT) {
            if (resync(context)) return -1;
            *dropped = 0;
        }
        return 0;
    }
    if (type != NS09_EV_KEY && type != NS09_EV_ABS &&
        type != NS09_EV_MSC && type != NS09_EV_SYN) return 0;
    return forward(context) ? -1 : 1;
}
#endif
