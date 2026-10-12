#define _GNU_SOURCE
#include "rumble_core.h"
#include "ns09_protocol.h"
#include "native_contract.h"
#include "hid_contract.h"
#include "xbox_profile.h"
#include "target_identity.h"
#include <linux/input.h>
#include <linux/uinput.h>
#include <linux/hidraw.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <poll.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <glob.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define BIT_BYTES(n) (((n) + 7) / 8)
static volatile sig_atomic_t quitting;
static char module_dir[PATH_MAX];
static char address[13];
static int watch_fd = -1, lock_fd = -1, guardian_fd = -1;
static nx_profile input_profile;
static nx_state input_state;
static int64_t now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000; }
static void quit_signal(int n) { (void)n; quitting = 1; }
static int bit(const unsigned char *b, unsigned n) { return (b[n / 8] >> (n % 8)) & 1; }
static int address_match(const char *s) {
    return ns09_target_matches(s,address);
}
#include "node_guard.h"
_Static_assert(FF_RUMBLE == NS09_FF_RUMBLE_TYPE, "FF ABI mismatch");
_Static_assert(EV_SYN == NS09_EV_SYN && EV_KEY == NS09_EV_KEY && EV_ABS == NS09_EV_ABS && EV_MSC == NS09_EV_MSC && SYN_DROPPED == NS09_SYN_DROPPED, "input ABI mismatch");
_Static_assert(KEY_CNT==NX_KEYS && ABS_CNT==NX_AXES && BTN_A==NX_A && BTN_B==NX_B && BTN_X==NX_X && BTN_Y==NX_Y && BTN_TL==NX_LB && BTN_TR==NX_RB && BTN_SELECT==NX_BACK && BTN_START==NX_START && BTN_MODE==NX_GUIDE && BTN_THUMBL==NX_L3 && BTN_THUMBR==NX_R3 && KEY_MENU==NX_MENU && KEY_HOMEPAGE==NX_HOMEPAGE && BTN_DPAD_UP==NX_UP && BTN_DPAD_RIGHT==NX_RIGHT, "button ABI mismatch");
_Static_assert(ABS_X==NX_X_AXIS && ABS_Y==NX_Y_AXIS && ABS_Z==NX_Z_AXIS && ABS_RX==NX_RX && ABS_RY==NX_RY && ABS_RZ==NX_RZ && ABS_BRAKE==NX_BRAKE && ABS_GAS==NX_GAS && ABS_HAT0X==NX_HAT_X && ABS_HAT0Y==NX_HAT_Y, "axis ABI mismatch");
static void bound_log(void) { struct stat s; if (!fstat(STDERR_FILENO, &s) && S_ISREG(s.st_mode) && s.st_size > 262144) ftruncate(STDERR_FILENO, 0); }
static void say(const char *message) { bound_log(); fprintf(stderr, "%lld %s\n", (long long)now_ms(), message); fflush(stderr); }
static void error_log(const char *operation) { int saved = errno; bound_log(); fprintf(stderr, "%lld %s: %s (%d)\n", (long long)now_ms(), operation, strerror(saved), saved); fflush(stderr); }
static int file_flag(const char *name) { char p[PATH_MAX]; snprintf(p, sizeof(p), "%s/%s", module_dir, name); return access(p, F_OK) == 0; }
static void status(const char *state) {
    static char previous[128]; if (!strcmp(previous, state)) return; snprintf(previous, sizeof(previous), "%s", state);
    char p[PATH_MAX], tmp[PATH_MAX]; snprintf(p, sizeof(p), "%s/bridge-status.txt", module_dir); snprintf(tmp, sizeof(tmp), "%s/bridge-status.tmp", module_dir);
    FILE *f = fopen(tmp, "w"); if (!f) return;
    fprintf(f, "NS09 Native Rumble Xbox360 Edition 1.0.0\nState=%s\nPid=%d\nSource=Android input-device FF_RUMBLE\nVirtualName=Microsoft X-Box 360 pad\nVirtualVID=045E\nVirtualPID=028E\nNativeRC1UserAccepted=YES\nXbox360ProfileOnDeviceVerified=UNCONFIRMED\n", state, getpid()); fclose(f); rename(tmp, p);
}
static int stopped(void) { return quitting || file_flag("disable") || file_flag("remove") || file_flag("bridge.off"); }
static void drain_watch(void) {
    char b[8192]; while (read(watch_fd, b, sizeof(b)) > 0) {}
    inotify_add_watch(watch_fd, "/dev/input", IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
}
static void init_watch(void) {
    watch_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (watch_fd < 0) return;
    int devices = inotify_add_watch(watch_fd, "/dev", IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
    inotify_add_watch(watch_fd, "/dev/input", IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
    int flags = inotify_add_watch(watch_fd, module_dir, IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
    if (devices < 0 || flags < 0) { close(watch_fd); watch_fd = -1; say("inotify unavailable: diagnostic retry interval30s, signals still stop immediately"); }
}
static void physical_probe(int source, const char *hid, const char *event) {
    char name[256] = {0}, uniq[128] = {0}, driver[PATH_MAX] = {0}, p[PATH_MAX];
    unsigned char ff[BIT_BYTES(FF_CNT)] = {0}, types[BIT_BYTES(EV_CNT)] = {0}; struct input_id id = {0};
    ioctl(source, EVIOCGNAME(sizeof(name)-1), name); ioctl(source, EVIOCGUNIQ(sizeof(uniq)-1), uniq);
    ioctl(source, EVIOCGID, &id); int query_ff = ioctl(source, EVIOCGBIT(EV_FF, sizeof(ff)), ff);
    int query_types = ioctl(source, EVIOCGBIT(0, sizeof(types)), types);
    snprintf(p, sizeof(p), "/sys/class/hidraw/%s/device/driver", strrchr(hid, '/')+1);
    ssize_t n = readlink(p, driver, sizeof(driver)-1); if (n > 0) driver[n] = 0;
    snprintf(p, sizeof(p), "%s/logs/physical-capabilities.txt", module_dir);
    FILE *f = fopen(p, "w"); if (!f) return;
    fprintf(f, "MonotonicMs=%lld\nHidraw=%s\nEvdev=%s\nName=%s\nBus=%04X\nVID=%04X\nPID=%04X\nHID_UNIQ=%s\nDriver=%s\nEV_FF=%d\nFF_RUMBLE=%d\nUinputAccessible=%d\nArchitecture=uinput userspace; no verified systemless physical-driver correction\n", (long long)now_ms(), hid, event, name, id.bustype, id.vendor, id.product, uniq, driver, query_types < 0 ? -1 : bit(types, EV_FF), query_ff < 0 ? -1 : bit(ff, FF_RUMBLE), access("/dev/uinput", R_OK|W_OK) == 0 || access("/dev/input/uinput", R_OK|W_OK) == 0);
    fclose(f);
}
static int framework_probe(int require_bridge) {
    pid_t child = fork(); if (child < 0) return -1;
    if (!child) {
        char dex[PATH_MAX], log[PATH_MAX]; snprintf(dex, sizeof(dex), "%s/bin/framework-probe.dex", module_dir);
        snprintf(log, sizeof(log), "%s/logs/framework-%s.txt", module_dir, require_bridge ? "bridge" : "physical");
        int out = open(log, O_WRONLY | O_CREAT | O_TRUNC, 0600); if (out < 0) _exit(11);
        dup2(out, STDOUT_FILENO); dup2(out, STDERR_FILENO); close(out);
        setenv("CLASSPATH", dex, 1);
        execl("/system/bin/app_process", "app_process", "/system/bin", "NS09FrameworkProbe", require_bridge ? "--require-bridge" : "--snapshot", address, (char*)NULL);
        _exit(11);
    }
    int result = 0; int64_t end = now_ms()+8000;
    while (waitpid(child, &result, WNOHANG) == 0) {
        if (now_ms() >= end || stopped()) { kill(child, SIGKILL); waitpid(child, NULL, 0); return -1; }
        usleep(50000);
    }
    return WIFEXITED(result) && WEXITSTATUS(result) == 0 ? 0 : -1;
}
static int start_guardian(pid_t *child) {
    int pipefd[2]; if (pipe2(pipefd, O_CLOEXEC)) return -1;
    pid_t p = fork(); if (p < 0) { close(pipefd[0]); close(pipefd[1]); return -1; }
    if (!p) {
        signal(SIGTERM, SIG_IGN); signal(SIGINT, SIG_IGN); signal(SIGHUP, SIG_IGN);
        /* Do not retain the parent's grabbed evdev/uinput/lock open descriptions. */
        long count = sysconf(_SC_OPEN_MAX); if (count > 65536 || count < 0) count = 65536;
        for (int fd = 3; fd < count; fd++) if (fd != pipefd[0]) close(fd);
        char b; ssize_t n; do { n = read(pipefd[0], &b, 1); } while (n < 0 && errno == EINTR);
        if (recover_nodes()) error_log("guardian restore NS09 node");
        close(pipefd[0]); _exit(0);
    }
    close(pipefd[0]); *child = p; return pipefd[1];
}
static int find_target(char *hid_path, char *event_path) {
    glob_t g = {0}; int matches = 0;
    char selected_address[13]={0};
    if (glob("/sys/class/hidraw/hidraw*", 0, NULL, &g)) return 0;
    for (size_t i = 0; i < g.gl_pathc; i++) {
        char node[PATH_MAX]; const char *base = strrchr(g.gl_pathv[i], '/');
        snprintf(node, sizeof(node), "/dev/%s", base + 1);
        int h = open(node, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
        if (h < 0) continue;
        struct hidraw_devinfo info = {0}; char uniq[128] = {0};
        int good = ioctl(h, HIDIOCGRAWINFO, &info) == 0 && ioctl(h, HIDIOCGRAWUNIQ(sizeof(uniq) - 1), uniq) >= 0;
        close(h);
        char normalized[13];
        if (!good || info.bustype != BUS_BLUETOOTH || (unsigned short)info.vendor != 0x045e || (unsigned short)info.product != 0x02e0 || !ns09_candidate_address(uniq,normalized)) continue;
        char pattern[PATH_MAX]; glob_t e = {0};
        snprintf(pattern, sizeof(pattern), "%s/device/input/input*/event*", g.gl_pathv[i]);
        if (glob(pattern, 0, NULL, &e)) { globfree(&e); continue; }
        for (size_t j = 0; j < e.gl_pathc; j++) {
            char ev[PATH_MAX]; snprintf(ev, sizeof(ev), "/dev/input/%s", strrchr(e.gl_pathv[j], '/') + 1);
            int fd = open(ev, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0) continue;
            struct input_id id = {0}; unsigned char keys[BIT_BYTES(KEY_CNT)] = {0}, axes[BIT_BYTES(ABS_CNT)] = {0};
            good = ioctl(fd, EVIOCGID, &id) == 0 && ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) >= 0 && ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(axes)), axes) >= 0;
            close(fd);
            if (good && id.bustype == BUS_BLUETOOTH && id.vendor == 0x045e && id.product == 0x02e0 && bit(keys, BTN_GAMEPAD) && bit(axes, ABS_X) && bit(axes, ABS_Y)) {
                matches++; snprintf(hid_path, PATH_MAX, "%s", node); snprintf(event_path, PATH_MAX, "%s", ev);
                memcpy(selected_address,normalized,sizeof(selected_address));
            }
        }
        globfree(&e);
    }
    globfree(&g);
    if(matches==1) memcpy(address,selected_address,sizeof(address));
    return matches == 1;
}
static int target_hid(int fd) {
    struct hidraw_devinfo info = {0}; char uniq[128] = {0};
    return ioctl(fd, HIDIOCGRAWINFO, &info) == 0 && ioctl(fd, HIDIOCGRAWUNIQ(sizeof(uniq)-1), uniq) >= 0 &&
        info.bustype == BUS_BLUETOOTH && (unsigned short)info.vendor == 0x045e &&
        (unsigned short)info.product == 0x02e0 && address_match(uniq);
}
static int create_proxy(int physical) {
    unsigned char keys[BIT_BYTES(KEY_CNT)] = {0}, axes[BIT_BYTES(ABS_CNT)] = {0};
    nx_caps caps = {0};
    if (ioctl(physical, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0 || ioctl(physical, EVIOCGBIT(EV_ABS, sizeof(axes)), axes) < 0) return -1;
    for (unsigned k=0;k<KEY_CNT;k++) caps.keys[k]=(uint8_t)bit(keys,k);
    for (unsigned a=0;a<ABS_CNT;a++) if (bit(axes,a)) {
        struct input_absinfo ai;
        if (ioctl(physical,EVIOCGABS(a),&ai)) return -1;
        caps.axes[a]=1; caps.abs[a]=(nx_absinfo){ai.value,ai.minimum,ai.maximum,ai.fuzz,ai.flat,ai.resolution};
    }
    if (nx_init(&input_profile,&caps)) { errno=ENOTSUP; say("unsupported/ambiguous physical input layout: keeping original controller"); return -1; }
    memset(&input_state,0,sizeof(input_state));
    char path[PATH_MAX]; snprintf(path,sizeof(path),"%s/logs/xbox360-mapping.txt",module_dir);
    FILE *mapping=fopen(path,"w");
    if (mapping) {
        fprintf(mapping,"VirtualName=%s\nVID=%04X\nPID=%04X\nVersion=%04X\nButtonLayout=%s\nPhysicalVIDPID=045E:02E0 (unchanged)\n",NS09_XBOX_NAME,NS09_XBOX_VENDOR,NS09_XBOX_PRODUCT,NS09_XBOX_VERSION,input_profile.sequential ? "NS09 sequential HID1..10" : "Linux standard gamepad");
        for(unsigned k=0;k<KEY_CNT;k++) if(input_profile.key_to[k]>=0) fprintf(mapping,"key %u -> %d\n",k,input_profile.key_to[k]);
        for(unsigned a=0;a<ABS_CNT;a++) if(input_profile.axis_to[a]>=0) { int dst=input_profile.axis_to[a]; fprintf(mapping,"axis %u [%d,%d] -> %d [%d,%d] flat=%d fuzz=%d resolution=%d\n",a,caps.abs[a].minimum,caps.abs[a].maximum,dst,input_profile.abs[dst].minimum,input_profile.abs[dst].maximum,input_profile.abs[dst].flat,input_profile.abs[dst].fuzz,input_profile.abs[dst].resolution); }
        fprintf(mapping,"DigitalTriggerFallback=%d\nDigitalHatFallback=%d\n",input_profile.digital_trigger[0]>=0,input_profile.digital_hat); fclose(mapping);
    }
    int u = open("/dev/uinput", O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (u < 0) u = open("/dev/input/uinput", O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (u < 0) return -1;
    struct uinput_setup d = {0};
    snprintf(d.name, sizeof(d.name), "%s", NS09_XBOX_NAME);
    // Only the uinput identity changes. Discovery/hidraw retain Bluetooth045e:02e0.
    d.id=(struct input_id){BUS_USB,NS09_XBOX_VENDOR,NS09_XBOX_PRODUCT,NS09_XBOX_VERSION};
    d.ff_effects_max = NS09_SLOTS;
    if (ioctl(u, UI_SET_EVBIT, EV_KEY) || ioctl(u, UI_SET_EVBIT, EV_ABS) || ioctl(u, UI_SET_EVBIT, EV_FF) || ioctl(u, UI_SET_FFBIT, FF_RUMBLE) || ioctl(u, UI_SET_FFBIT, FF_GAIN)) goto fail;
    for (unsigned k = 0; k < KEY_CNT; k++) if (input_profile.keys[k] && ioctl(u, UI_SET_KEYBIT, k)) goto fail;
    for (unsigned a = 0; a < ABS_CNT; a++) if (input_profile.axes[a]) {
        nx_absinfo *ai=&input_profile.abs[a];
        if (ioctl(u, UI_SET_ABSBIT, a)) goto fail;
        struct uinput_abs_setup setup = {0}; setup.code = (uint16_t)a;
        setup.absinfo=(struct input_absinfo){ai->value,ai->minimum,ai->maximum,ai->fuzz,ai->flat,ai->resolution};
        if (ioctl(u, UI_ABS_SETUP, &setup)) goto fail;
    }
    char phys[64]; snprintf(phys,sizeof(phys),"ns09-native/%s/input0",address);
    if (ioctl(u, UI_SET_PHYS, phys) || ioctl(u, UI_DEV_SETUP, &d) || ioctl(u, UI_DEV_CREATE)) goto fail;
    return u;
fail: { int saved = errno; close(u); errno = saved; return -1; }
}
static int emit(int u, unsigned type, unsigned code, int value) {
    struct input_event e = {0}; e.type = (uint16_t)type; e.code = (uint16_t)code; e.value = value;
    return write(u, &e, sizeof(e)) == sizeof(e) ? 0 : -1;
}
static int resync_input(int source, int u) {
    unsigned char supported[BIT_BYTES(KEY_CNT)] = {0}, held[BIT_BYTES(KEY_CNT)] = {0}, axes[BIT_BYTES(ABS_CNT)] = {0};
    if (ioctl(source, EVIOCGBIT(EV_KEY, sizeof(supported)), supported) < 0 || ioctl(source, EVIOCGKEY(sizeof(held)), held) < 0 || ioctl(source, EVIOCGBIT(EV_ABS, sizeof(axes)), axes) < 0) return -1;
    memset(&input_state,0,sizeof(input_state)); nx_event mapped;
    for (unsigned k = 0; k < KEY_CNT; k++) if (bit(supported,k) && nx_translate(&input_profile,&input_state,EV_KEY,k,bit(held,k),&mapped) && emit(u,mapped.type,mapped.code,mapped.value)) return -1;
    for (unsigned a = 0; a < ABS_CNT; a++) if (bit(axes, a)) {
        struct input_absinfo ai; if (ioctl(source, EVIOCGABS(a), &ai)) return -1;
        if(nx_translate(&input_profile,&input_state,EV_ABS,a,ai.value,&mapped) && emit(u,mapped.type,mapped.code,mapped.value)) return -1;
    }
    return emit(u, EV_SYN, SYN_REPORT, 0);
}
typedef ns09_writer_message writer_message;
static int hid_write_native(void *context, const uint8_t *report, unsigned length) {
    int fd = *(int*)context; return (int)write(fd, report, length);
}
static int hid_send(int h, unsigned left, unsigned right) {
    // The timeout terminates only the HID writer, never blocks input forwarding.
    alarm(1); int result = ns09_transfer(hid_write_native, &h, left, right); alarm(0);
    return result;
}
static void writer_loop(int socket, const char *path) {
    signal(SIGALRM, SIG_DFL); signal(SIGPIPE, SIG_IGN);
    int h = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (h < 0 || !target_hid(h) || hid_send(h, 0, 0)) { if (h >= 0) close(h); _exit(1); }
    char ack = 'K'; send(socket, &ack, 1, MSG_NOSIGNAL);
    unsigned last_left = 0, last_right = 0; int64_t deadline = 0, last_write = 0; writer_message message;
    while (!quitting) {
        int timeout = -1;
        if (last_left || last_right) { int64_t wait = deadline - now_ms(); timeout = wait <= 0 ? 0 : wait > INT_MAX ? INT_MAX : (int)wait; }
        struct pollfd p = {socket, POLLIN, 0}; int rc = poll(&p, 1, timeout);
        if (rc < 0 && errno != EINTR) break;
        if (p.revents & (POLLERR | POLLHUP | POLLNVAL)) break;
        if (p.revents & POLLIN) {
            ssize_t n = recv(socket, &message, sizeof(message), 0); if (n != sizeof(message)) break;
            // Drain queued commands: newest desired state wins.
            writer_message newer;
            while (recv(socket, &newer, sizeof(newer), MSG_DONTWAIT) == sizeof(newer)) message = newer;
            if (ns09_resolve_message(&message, now_ms())) break;
            if (message.left != last_left || message.right != last_right) {
                int64_t elapsed = now_ms() - last_write;
                if ((message.left || message.right) && elapsed < 16) usleep((useconds_t)((16 - elapsed) * 1000));
                if (ns09_resolve_message(&message, now_ms())) break;
                if (hid_send(h, message.left, message.right)) break;
                bound_log();
                fprintf(stderr, "%lld HID report=03 0F 00 00 %02X %02X FF 00 EB\n", (long long)now_ms(), message.left, message.right);
                last_write = now_ms();
                last_left = message.left; last_right = message.right;
            }
            deadline = message.deadline; send(socket, &ack, 1, MSG_NOSIGNAL);
        }
        if ((last_left || last_right) && now_ms() >= deadline) {
            if (hid_send(h, 0, 0)) break;
            last_left = last_right = 0;
        }
    }
    hid_send(h, 0, 0); close(h); close(socket); _exit(0);
}
static void reap_bounded(pid_t child) {
    int64_t until = now_ms() + 1300; pid_t rc;
    do { rc = waitpid(child, NULL, WNOHANG); if (rc != 0) return; usleep(20000); } while (now_ms() < until);
    kill(child, SIGKILL); waitpid(child, NULL, WNOHANG);
}
static void emergency_stop(const char *path) {
    // A killed/timed-out writer may not have reached its own Stop cleanup.
    pid_t p = fork();
    if (p == 0) { signal(SIGALRM, SIG_DFL); if (guardian_fd >= 0) close(guardian_fd); int h = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC); if (h >= 0) { if (target_hid(h)) hid_send(h, 0, 0); close(h); } _exit(0); }
    if (p > 0) reap_bounded(p);
}
static int start_writer(const char *hid, int source, int u, pid_t *child) {
    int s[2]; if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, s)) return -1;
    pid_t p = fork();
    if (p < 0) { close(s[0]); close(s[1]); return -1; }
    if (p == 0) { close(s[0]); close(source); close(u); if (guardian_fd >= 0) close(guardian_fd); if (watch_fd >= 0) close(watch_fd); if (lock_fd >= 0) close(lock_fd); writer_loop(s[1], hid); }
    close(s[1]); *child = p;
    struct pollfd wait = {s[0], POLLIN, 0}; char ready = 0;
    if (poll(&wait, 1, 1800) <= 0 || recv(s[0], &ready, 1, MSG_DONTWAIT) != 1 || ready != 'K') {
        close(s[0]); kill(p, SIGTERM); reap_bounded(p); emergency_stop(hid); return -1;
    }
    fcntl(s[0], F_SETFL, O_NONBLOCK); return s[0];
}
typedef struct { int fd; struct uinput_ff_upload native; } upload_adapter;
static int upload_begin(void *ctx, ns09_ff_upload_request *r) {
    upload_adapter *a = ctx; a->native.request_id = r->request_id;
    if (ioctl(a->fd, UI_BEGIN_FF_UPLOAD, &a->native)) return -1;
    r->effect_id = a->native.effect.id; r->effect_type = a->native.effect.type;
    r->strong = a->native.effect.u.rumble.strong_magnitude; r->weak = a->native.effect.u.rumble.weak_magnitude;
    r->length = a->native.effect.replay.length; r->delay = a->native.effect.replay.delay; return 0;
}
static int upload_end(void *ctx, ns09_ff_upload_request *r) {
    upload_adapter *a = ctx; a->native.retval = r->retval; return ioctl(a->fd, UI_END_FF_UPLOAD, &a->native);
}
typedef struct { int fd; struct uinput_ff_erase native; } erase_adapter;
static int erase_begin(void *ctx, ns09_ff_erase_request *r) {
    erase_adapter *a = ctx; a->native.request_id = r->request_id;
    if (ioctl(a->fd, UI_BEGIN_FF_ERASE, &a->native)) return -1;
    r->effect_id = a->native.effect_id; return 0;
}
static int erase_end(void *ctx, ns09_ff_erase_request *r) {
    erase_adapter *a = ctx; a->native.retval = r->retval; return ioctl(a->fd, UI_END_FF_ERASE, &a->native);
}
typedef struct { int source, u; const struct input_event *event; } input_adapter;
static int forward_native(void *ctx) {
    input_adapter *a = ctx; nx_event mapped;
    if(!nx_translate(&input_profile,&input_state,a->event->type,a->event->code,a->event->value,&mapped)) return 0;
    struct input_event event=*a->event; event.type=(uint16_t)mapped.type; event.code=(uint16_t)mapped.code; event.value=mapped.value;
    return write(a->u,&event,sizeof(event))==sizeof(event) ? 0 : -1;
}
static int resync_native(void *ctx) { input_adapter *a = ctx; return resync_input(a->source, a->u); }
static int ff_event(int u, const struct input_event *e, ns09_core *core, uint64_t *uploads, uint64_t *plays) {
    int64_t now = now_ms();
    if (e->type == EV_UINPUT && e->code == UI_FF_UPLOAD) {
        ns09_ff_upload_request request = {0}; request.request_id = (uint32_t)e->value;
        upload_adapter a = {0}; a.fd = u;
        if (ns09_ff_upload_contract(core, &request, now, upload_begin, upload_end, &a)) return -1;
        if (!request.retval) {
            (*uploads)++; bound_log();
            fprintf(stderr, "%lld FF_UPLOAD id=%d strong=%u weak=%u duration=%u delay=%u\n", (long long)now, request.effect_id, request.strong, request.weak, request.length, request.delay);
        }
    } else if (e->type == EV_UINPUT && e->code == UI_FF_ERASE) {
        ns09_ff_erase_request request = {0}; request.request_id = (uint32_t)e->value;
        erase_adapter a = {0}; a.fd = u;
        if (ns09_ff_erase_contract(core, &request, erase_begin, erase_end, &a)) return -1;
    } else if (e->type == EV_FF && e->code == FF_GAIN) core->gain = (uint16_t)(e->value < 0 ? 0 : e->value > 65535 ? 65535 : e->value);
    else if (e->type == EV_FF && e->code < NS09_SLOTS) { ns09_play(core, e->code, e->value, now); (*plays)++; bound_log(); fprintf(stderr, "%lld FF_PLAY id=%u count=%d\n", (long long)now, e->code, e->value); }
    return 0;
}
static int run_connection(const char *hid, const char *event) {
    uint64_t uploads = 0, plays = 0, commands = 0, input_events = 0, logged_uploads = 0, logged_plays = 0;
    int source = open(event, O_RDONLY | O_NONBLOCK | O_CLOEXEC); if (source < 0) { error_log("open target evdev"); status("BLOCKED_PHYSICAL_INPUT_ACCESS"); return -1; }
    if (!target_evdev(source)) { say("target identity changed before open; refusing takeover"); close(source); return -1; }
    physical_probe(source, hid, event); framework_probe(0);
    int u = create_proxy(source); if (u < 0) { error_log("create uinput proxy; original input unchanged"); status("BLOCKED_UINPUT_ACCESS_OR_CAPABILITIES"); close(source); return -1; }
    pid_t guardian = -1, child = -1; int writer = -1, grabbed = 0, hidden = 0, result = -1;
    node_guard guard = {0};
    guardian_fd = start_guardian(&guardian);
    if (guardian_fd < 0) { error_log("start input recovery guardian"); goto cleanup; }
    writer = start_writer(hid, source, u, &child);
    if (writer < 0) { say("HID writer baseline Stop failed; original input unchanged"); status("BLOCKED_HID_OUTPUT_ACCESS"); goto cleanup; }
    if (ioctl(source, EVIOCGRAB, 1)) { error_log("grab only NS09 evdev"); goto cleanup; }
    grabbed = 1;
    /* Flush pre-grab history; then publish the current key/axis snapshot. */
    struct input_event pending[64]; while (read(source, pending, sizeof(pending)) > 0) {}
    if (resync_input(source, u)) { error_log("initial button/axis state"); goto cleanup; }
    if (hide_node(&guard, source, event)) { error_log("hide physical NS09 node; refusing duplicate pads"); status("BLOCKED_DUPLICATE_PREVENTION"); goto cleanup; }
    hidden = 1;
    if (framework_probe(1)) { say("framework did not confirm one NS09 gamepad + vibrator IDs 0/1 with amplitude; restoring physical input"); status("BLOCKED_FRAMEWORK_DUAL_RUMBLE"); goto cleanup; }
    if (watch_fd >= 0) drain_watch();
    say("NS09 connected: proxy ready; FF_RUMBLE listening; no synthetic vibration"); status("CONNECTED_WAITING_FOR_GAME_FF");
    ns09_core core; ns09_init(&core, 100);
    int64_t heartbeat = INT64_MAX, ack_deadline = INT64_MAX, log_at = now_ms(); unsigned last_l = 0, last_r = 0;
    int dropped = 0;
    while (!stopped()) {
        int64_t now = now_ms(); ns09_output out = ns09_render(&core, now);
        if (out.left != last_l || out.right != last_r || ((out.left || out.right) && now >= heartbeat)) {
            writer_message m = {out.left, out.right, now + 1500};
            if (send(writer, &m, sizeof(m), MSG_DONTWAIT | MSG_NOSIGNAL) != sizeof(m)) { error_log("HID writer IPC"); break; }
            if (out.left != last_l || out.right != last_r) commands++;
            last_l = out.left; last_r = out.right; heartbeat = (out.left || out.right) ? now + 750 : INT64_MAX;
            if (ack_deadline == INT64_MAX) ack_deadline = now + 1800;
        }
        if (now >= ack_deadline) { say("HID writer timed out; releasing input proxy"); break; }
        int64_t next = out.next; if (heartbeat < next) next = heartbeat;
        if (ack_deadline < next) next = ack_deadline;
        int timeout = next == INT64_MAX ? -1 : next <= now ? 0 : next-now > INT_MAX ? INT_MAX : (int)(next-now);
        if (watch_fd < 0 && (timeout < 0 || timeout > 30000)) timeout = 30000;
        struct pollfd fds[5] = {{source, POLLIN, 0}, {u, POLLIN, 0}, {writer, POLLIN, 0}, {watch_fd, POLLIN, 0}, {guardian_fd, 0, 0}};
        int rc = poll(fds, 5, timeout); if (rc < 0) { if (errno == EINTR) continue; error_log("poll bridge"); break; }
        if (fds[3].revents & POLLIN) drain_watch();
        int bad = 0; for (unsigned i = 0; i < 3; i++) if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) bad = 1;
        if (fds[4].revents & (POLLERR | POLLHUP | POLLNVAL)) { say("input recovery guardian exited; restoring physical input"); bad = 1; }
        if (bad) { say("connection/evdev/writer closed; Stop and release"); break; }
        if (fds[2].revents & POLLIN) { char a[64]; while (recv(writer, a, sizeof(a), MSG_DONTWAIT) > 0) ack_deadline = INT64_MAX; }
        if (fds[1].revents & POLLIN) {
            struct input_event events[32]; ssize_t n;
            while ((n = read(u, events, sizeof(events))) > 0) {
                if (n % sizeof(events[0])) { bad = 1; break; }
                for (size_t i = 0; i < (size_t)n / sizeof(events[0]); i++) if (ff_event(u, &events[i], &core, &uploads, &plays)) { error_log("FF upload/erase handshake"); bad = 1; break; }
                if (bad) break;
            }
            if (uploads) status("CONNECTED_FF_RECEIVED");
            if (bad || n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR)) { error_log("read uinput FF events"); break; }
        }
        if (fds[0].revents & POLLIN) {
            struct input_event events[64]; ssize_t n;
            while ((n = read(source, events, sizeof(events))) > 0) {
                if (n % sizeof(events[0])) { bad = 1; break; }
                for (size_t i = 0; i < (size_t)n / sizeof(events[0]); i++) {
                    struct input_event *e = &events[i];
                    input_adapter a = {source, u, e};
                    int forwarded = ns09_forward_contract(&dropped, e->type, e->code, forward_native, resync_native, &a);
                    if (forwarded < 0) { bad = 1; break; } if (forwarded > 0) input_events++;
                }
                if (bad) break;
            }
            if (bad || n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR)) { error_log("forward input events"); break; }
        }
        if (now_ms() - log_at >= 10000) {
            if (uploads != logged_uploads || plays != logged_plays) {
                bound_log();
                fprintf(stderr, "%lld ff_uploads=%llu ff_plays_or_stops=%llu state_changes=%llu input_events=%llu\n", (long long)now_ms(), (unsigned long long)uploads, (unsigned long long)plays, (unsigned long long)commands, (unsigned long long)input_events);
                logged_uploads = uploads; logged_plays = plays;
            }
            log_at = now_ms();
        }
    }
    result = 0;
cleanup:
    if (writer >= 0) {
        writer_message stop = {0, 0, now_ms()}; send(writer, &stop, sizeof(stop), MSG_DONTWAIT | MSG_NOSIGNAL);
        close(writer); kill(child, SIGTERM); reap_bounded(child); emergency_stop(hid);
    }
    if (hidden && restore_node(&guard)) error_log("restore physical input path; guardian will retry");
    if (grabbed) ioctl(source, EVIOCGRAB, 0);
    ioctl(u, UI_DEV_DESTROY); close(u); close(source);
    if (guardian_fd >= 0) { close(guardian_fd); guardian_fd = -1; reap_bounded(guardian); }
    fprintf(stderr, "%lld session ended ff_uploads=%llu ff_plays_or_stops=%llu\n", (long long)now_ms(), (unsigned long long)uploads, (unsigned long long)plays);
    if (result == 0) status("DISCONNECTED_OR_STOPPED"); return result;
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--recover")) return getuid() == 0 && recover_nodes() == 0 ? 0 : 1;
    if (argc != 3 || strcmp(argv[1], "--module")) { fprintf(stderr, "Usage: ns09-bridged --module /data/adb/modules/ns09_rumble_bridge | --recover\n"); return 2; }
    if (!realpath(argv[2], module_dir)) { perror("module directory"); return 2; }
    if (getuid() != 0) { say("root required"); return 3; }
    signal(SIGTERM, quit_signal); signal(SIGINT, quit_signal); signal(SIGHUP, quit_signal); signal(SIGPIPE, SIG_IGN);
    char lockpath[PATH_MAX]; snprintf(lockpath, sizeof(lockpath), "%s/daemon.lock", module_dir);
    lock_fd = open(lockpath, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (lock_fd < 0 || flock(lock_fd, LOCK_EX | LOCK_NB)) { say("another daemon is active or lock unavailable"); return 4; }
    if (recover_nodes()) { error_log("recover previous input node"); status("BLOCKED_NODE_RECOVERY"); close(lock_fd); return 5; }
    init_watch();
    char pidpath[PATH_MAX]; snprintf(pidpath, sizeof(pidpath), "%s/bridge.pid", module_dir);
    FILE *pid = fopen(pidpath, "w"); if (pid) { fprintf(pid, "%d\n", getpid()); fclose(pid); }
    say("NS09 Native Xbox360 Edition1.0.0: native FF_RUMBLE -> unchanged report03"); status("WAITING_FOR_NS09");
    int64_t retry_until = 0;
    while (!stopped()) {
        /* Reap exited children from a previously timed-out writer without waiting. */
        while (waitpid(-1, NULL, WNOHANG) > 0) {}
        char hid[PATH_MAX] = {0}, event[PATH_MAX] = {0};
        if (now_ms() >= retry_until && find_target(hid, event)) {
            fprintf(stderr, "%lld target hidraw=%s evdev=%s\n", (long long)now_ms(), hid, event);
            if (run_connection(hid, event)) retry_until = now_ms()+30000;
            else { retry_until = now_ms()+1000; status("WAITING_FOR_NS09"); }
        }
        if (stopped()) break;
        int64_t remaining = retry_until - now_ms();
        int timeout = remaining > 0 ? (int)remaining : watch_fd < 0 ? 30000 : -1;
        struct pollfd p = {watch_fd, POLLIN, 0};
        if (poll(&p, 1, timeout) < 0 && errno != EINTR) break;
        if (p.revents & POLLIN) drain_watch();
    }
    status("SERVICE_DISABLED_OR_EXITED"); unlink(pidpath); if (watch_fd >= 0) close(watch_fd); close(lock_fd); return 0;
}
