#include "native_contract.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
typedef struct {
    ns09_core *core;
    ns09_core before;
    ns09_ff_upload_request effect;
    unsigned erase_id;
    unsigned begins, ends;
    int fail_begin, fail_end, end_retval;
    uint32_t expected_request_id;
} injected_io;

static int upload_begin(void *opaque, ns09_ff_upload_request *request) {
    injected_io *io = opaque;
    CHECK(request->request_id == io->expected_request_id);
    CHECK(io->begins == 0 && io->ends == 0);
    CHECK(!memcmp(io->core, &io->before, sizeof(io->before)));
    io->begins++;
    if (io->fail_begin) return -1;
    *request = io->effect;
    request->request_id = io->expected_request_id;
    return 0;
}
static int upload_end(void *opaque, ns09_ff_upload_request *request) {
    injected_io *io = opaque;
    CHECK(request->request_id == io->expected_request_id);
    CHECK(io->begins == 1 && io->ends == 0);
    CHECK(!memcmp(io->core, &io->before, sizeof(io->before)));
    io->ends++;
    io->end_retval = request->retval;
    return io->fail_end ? -1 : 0;
}
static int erase_begin(void *opaque, ns09_ff_erase_request *request) {
    injected_io *io = opaque;
    CHECK(request->request_id == io->expected_request_id);
    CHECK(io->begins == 0 && io->ends == 0);
    CHECK(!memcmp(io->core, &io->before, sizeof(io->before)));
    io->begins++;
    if (io->fail_begin) return -1;
    request->effect_id = io->erase_id;
    return 0;
}
static int erase_end(void *opaque, ns09_ff_erase_request *request) {
    injected_io *io = opaque;
    CHECK(request->request_id == io->expected_request_id);
    CHECK(io->begins == 1 && io->ends == 0);
    CHECK(!memcmp(io->core, &io->before, sizeof(io->before)));
    io->ends++;
    io->end_retval = request->retval;
    return io->fail_end ? -1 : 0;
}
static injected_io fixture(ns09_core *core) {
    injected_io io = {0}; io.core = core; io.before = *core;
    io.expected_request_id = 0x12345678;
    io.effect.effect_id = 3; io.effect.effect_type = NS09_FF_RUMBLE_TYPE;
    io.effect.strong = 65535; io.effect.weak = 0;
    io.effect.length = 100; io.effect.delay = 10;
    io.erase_id = 3;
    return io;
}
static int run_upload(injected_io *io, int64_t now) {
    ns09_ff_upload_request r = {0}; r.request_id = io->expected_request_id;
    return ns09_ff_upload_contract(io->core, &r, now, upload_begin, upload_end, io);
}
static int run_erase(injected_io *io) {
    ns09_ff_erase_request r = {0}; r.request_id = io->expected_request_id;
    return ns09_ff_erase_contract(io->core, &r, erase_begin, erase_end, io);
}

typedef struct {
    unsigned forwards, resyncs;
    int fail_forward, fail_resync;
} input_io;
static int input_forward(void *context) {
    input_io *io = context; io->forwards++;
    return io->fail_forward ? -1 : 0;
}
static int input_resync(void *context) {
    input_io *io = context; io->resyncs++;
    return io->fail_resync ? -1 : 0;
}
static int input_step(int *dropped, unsigned type, unsigned code, input_io *io) {
    return ns09_forward_contract(dropped, type, code, input_forward, input_resync, io);
}
static void check_input_contract(void) {
    int dropped = 0; input_io io = {0};
    CHECK(input_step(&dropped, NS09_EV_KEY, 304, &io) == 1);
    CHECK(input_step(&dropped, NS09_EV_ABS, 0, &io) == 1);
    CHECK(input_step(&dropped, NS09_EV_MSC, 4, &io) == 1);
    CHECK(input_step(&dropped, NS09_EV_SYN, NS09_SYN_REPORT, &io) == 1);
    CHECK(io.forwards == 4 && !io.resyncs && !dropped);
    /* Never echo FF or UI requests from the physical input stream. */
    CHECK(input_step(&dropped, 21, 3, &io) == 0);
    CHECK(input_step(&dropped, 0x101, 1, &io) == 0);
    CHECK(io.forwards == 4);
    CHECK(input_step(&dropped, NS09_EV_SYN, NS09_SYN_DROPPED, &io) == 0);
    CHECK(dropped && io.forwards == 4);
    for (unsigned i = 0; i < 100; i++)
        CHECK(input_step(&dropped, i % 2 ? NS09_EV_KEY : NS09_EV_ABS, i, &io) == 0);
    CHECK(dropped && io.forwards == 4 && !io.resyncs);
    CHECK(input_step(&dropped, NS09_EV_SYN, NS09_SYN_REPORT, &io) == 0);
    CHECK(!dropped && io.forwards == 4 && io.resyncs == 1);
    CHECK(input_step(&dropped, NS09_EV_KEY, 304, &io) == 1);
    CHECK(io.forwards == 5);
    io.fail_forward = 1;
    CHECK(input_step(&dropped, NS09_EV_ABS, 0, &io) == -1);
    CHECK(io.forwards == 6);
    io.fail_forward = 0; io.fail_resync = 1;
    CHECK(input_step(&dropped, NS09_EV_SYN, NS09_SYN_DROPPED, &io) == 0);
    CHECK(input_step(&dropped, NS09_EV_SYN, NS09_SYN_REPORT, &io) == -1);
    CHECK(dropped && io.resyncs == 2 && io.forwards == 6);
    /* An unsuccessful resync must not leak subsequent stale events. */
    CHECK(input_step(&dropped, NS09_EV_KEY, 304, &io) == 0);
    CHECK(io.forwards == 6);
    io.fail_resync = 0;
    CHECK(input_step(&dropped, NS09_EV_SYN, NS09_SYN_REPORT, &io) == 0);
    CHECK(!dropped && io.resyncs == 3);
    /* A newly reconnected source starts from a fresh dropped-state. */
    dropped = 0; io = (input_io){0};
    CHECK(input_step(&dropped, NS09_EV_ABS, 0, &io) == 1);
    CHECK(io.forwards == 1 && io.resyncs == 0);
}

int main(void) {
    ns09_core core; ns09_init(&core, 29);
    injected_io io = fixture(&core);
    CHECK(run_upload(&io, 1000) == 0);
    CHECK(io.begins == 1 && io.ends == 1 && io.end_retval == 0);
    CHECK(core.effect[3].valid && !core.effect[3].active);
    CHECK(core.effect[3].strong == 65535 && core.effect[3].weak == 0);
    CHECK(core.effect[3].length == 100 && core.effect[3].delay == 10);
    ns09_play(&core, 3, 1, 1000);
    CHECK(ns09_render(&core, 1009).left == 0);
    CHECK(ns09_render(&core, 1010).left > 0);
    CHECK(ns09_render(&core, 1010).right == 0);

    /* Uploading an active effect replaces both independent channels and uses
     * the actual length/delay from the request, without an implicit start. */
    io = fixture(&core); io.effect.strong = 0; io.effect.weak = 32768;
    io.effect.delay = 0; io.effect.length = 200;
    CHECK(run_upload(&io, 1050) == 0);
    CHECK(core.effect[3].active);
    CHECK(ns09_render(&core, 1050).left == 0);
    CHECK(ns09_render(&core, 1050).right > 0);
    CHECK(ns09_render(&core, 1249).right > 0);
    CHECK(ns09_render(&core, 1250).right == 0);
    ns09_play(&core, 3, 1, 1300); ns09_play(&core, 3, 0, 1301);
    CHECK(ns09_render(&core, 1301).right == 0);

    /* BEGIN failures never require END; END failures never commit state. */
    io = fixture(&core); io.fail_begin = 1;
    CHECK(run_upload(&io, 2000) == -1);
    CHECK(io.begins == 1 && io.ends == 0);
    CHECK(!memcmp(&core, &io.before, sizeof(core)));
    io = fixture(&core); io.fail_end = 1;
    CHECK(run_upload(&io, 2000) == -2);
    CHECK(io.begins == 1 && io.ends == 1);
    CHECK(!memcmp(&core, &io.before, sizeof(core)));

    const int invalid_ids[] = {-1, NS09_SLOTS, 65535};
    for (unsigned i = 0; i < sizeof(invalid_ids) / sizeof(invalid_ids[0]); i++) {
        io = fixture(&core); io.effect.effect_id = invalid_ids[i];
        CHECK(run_upload(&io, 2000) == 0);
        CHECK(io.begins == 1 && io.ends == 1 && io.end_retval == -EINVAL);
        CHECK(!memcmp(&core, &io.before, sizeof(core)));
    }
    io = fixture(&core); io.effect.effect_type = NS09_FF_RUMBLE_TYPE + 1;
    CHECK(run_upload(&io, 2000) == 0);
    CHECK(io.ends == 1 && io.end_retval == -EINVAL);
    CHECK(!memcmp(&core, &io.before, sizeof(core)));

    ns09_play(&core, 3, 1, 2000);
    io = fixture(&core); io.fail_begin = 1;
    CHECK(run_erase(&io) == -1 && io.ends == 0);
    CHECK(!memcmp(&core, &io.before, sizeof(core)));
    io = fixture(&core); io.fail_end = 1;
    CHECK(run_erase(&io) == -2 && io.ends == 1);
    CHECK(!memcmp(&core, &io.before, sizeof(core)));
    io = fixture(&core); io.erase_id = NS09_SLOTS;
    CHECK(run_erase(&io) == 0 && io.ends == 1 && io.end_retval == -EINVAL);
    CHECK(!memcmp(&core, &io.before, sizeof(core)));
    io = fixture(&core);
    CHECK(run_erase(&io) == 0 && io.ends == 1 && io.end_retval == 0);
    CHECK(!core.effect[3].valid && !core.effect[3].active);
    CHECK(ns09_render(&core, 2001).left == 0 && ns09_render(&core, 2001).right == 0);

    /* Repeated allocation/update/play/stop/erase across all effect slots.
     * Every callback observes pre-commit state; no stale effect survives erase. */
    for (unsigned i = 0; i < 1024; i++) {
        unsigned id = i % NS09_SLOTS;
        io = fixture(&core); io.effect.effect_id = (int)id;
        io.effect.strong = (uint16_t)(i * 61u); io.effect.weak = (uint16_t)(65535u - i * 61u);
        io.effect.length = (uint16_t)(1 + i % 500); io.effect.delay = 0;
        io.expected_request_id = i;
        CHECK(run_upload(&io, i * 1000) == 0 && io.end_retval == 0);
        ns09_play(&core, id, 2, i * 1000);
        CHECK(core.effect[id].active);
        ns09_play(&core, id, 0, i * 1000 + 1);
        CHECK(!core.effect[id].active);
        io = fixture(&core); io.erase_id = id; io.expected_request_id = i;
        CHECK(run_erase(&io) == 0 && io.end_retval == 0);
        CHECK(!core.effect[id].valid);
    }
    check_input_contract();
    printf("PASS: %u production contract assertions; FF BEGIN/END fault injection, invalid effects, atomic commit, active update, independent channels, start/stop, erase, 1024 lifecycle sequences; input filtering, SYN_DROPPED resync and forwarding failures\n", checks);
    return 0;
}
