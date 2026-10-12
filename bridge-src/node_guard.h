#ifndef NS09_NODE_GUARD_H
#define NS09_NODE_GUARD_H
/* The original character inode is retained outside EventHub's watched directory.
 * No chmod/chcon/mknod or driver operation is performed. */
#include <dirent.h>
#include <sys/sysmacros.h>
#include "device_identity.h"
static const char *guard_dir = "/dev/ns09_rumble_bridge";
typedef struct { char original[PATH_MAX], backup[PATH_MAX]; struct stat inode; } node_guard;
static int event_name(const char *s) {
    if (strncmp(s, "event", 5) || !isdigit((unsigned char)s[5])) return 0;
    for (s += 5; *s; s++) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}
static int same_node(const struct stat *a, const struct stat *b) {
    ns09_inode x = {a->st_dev, a->st_ino, a->st_rdev}, y = {b->st_dev, b->st_ino, b->st_rdev};
    return S_ISCHR(a->st_mode) && S_ISCHR(b->st_mode) && ns09_same_inode(x, y);
}
static int target_evdev(int fd) {
    struct input_id id = {0}; char uniq[128] = {0};
    unsigned char keys[BIT_BYTES(KEY_CNT)] = {0}, axes[BIT_BYTES(ABS_CNT)] = {0};
    return ioctl(fd, EVIOCGID, &id) == 0 && id.bustype == BUS_BLUETOOTH &&
        id.vendor == 0x045e && id.product == 0x02e0 &&
        ioctl(fd, EVIOCGUNIQ(sizeof(uniq)-1), uniq) >= 0 && address_match(uniq) &&
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) >= 0 && bit(keys, BTN_GAMEPAD) &&
        ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(axes)), axes) >= 0 && bit(axes, ABS_X) && bit(axes, ABS_Y);
}
static int live_node(const node_guard *g) {
    const char *base = strrchr(g->original, '/') + 1;
    char p[PATH_MAX]; snprintf(p, sizeof(p), "/sys/class/input/%s/dev", base);
    FILE *f = fopen(p, "r"); unsigned maj = 0, min = 0;
    if (!f) return 0;
    int ok = fscanf(f, "%u:%u", &maj, &min) == 2; fclose(f);
    if (!ok || makedev(maj, min) != g->inode.st_rdev) return 0;
    int fd = open(g->backup, O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return 0;
    struct stat s; ok = fstat(fd, &s) == 0 && same_node(&s, &g->inode) && target_evdev(fd);
    close(fd); return ok;
}
static int restore_node(node_guard *g) {
    struct stat saved, current;
    if (lstat(g->backup, &saved)) return errno == ENOENT ? 0 : -1;
    if (!same_node(&saved, &g->inode)) { errno = ESTALE; return -1; }
    if (lstat(g->original, &current) == 0) {
        /* Never replace a path, including an event number reused on reconnect. */
        if (ns09_restore_decision(1, same_node(&saved, &current), live_node(g)) == NS09_REMOVE_BACKUP) return unlink(g->backup);
        errno = EEXIST; return -1;
    }
    if (errno != ENOENT) return -1;
    if (ns09_restore_decision(0, 0, live_node(g)) == NS09_REMOVE_BACKUP) return unlink(g->backup);
    if (link(g->backup, g->original)) return -1; /* emits IN_CREATE */
    if (lstat(g->original, &current) || !same_node(&saved, &current)) { errno = ESTALE; return -1; }
    return unlink(g->backup);
}
static int recover_nodes(void) {
    struct stat directory;
    if (lstat(guard_dir, &directory)) return errno == ENOENT ? 0 : -1;
    if (!S_ISDIR(directory.st_mode) || directory.st_uid != 0 || (directory.st_mode & 0077)) { errno = EPERM; return -1; }
    DIR *d = opendir(guard_dir); if (!d) return errno == ENOENT ? 0 : -1;
    int result = 0; struct dirent *e;
    while ((e = readdir(d))) {
        if (!event_name(e->d_name)) continue;
        node_guard g = {0};
        snprintf(g.original, sizeof(g.original), "/dev/input/%s", e->d_name);
        snprintf(g.backup, sizeof(g.backup), "%s/%s", guard_dir, e->d_name);
        if (lstat(g.backup, &g.inode) || !S_ISCHR(g.inode.st_mode)) { result = -1; continue; }
        if (restore_node(&g)) result = -1;
    }
    closedir(d); return result;
}
static int hide_node(node_guard *g, int fd, const char *original) {
    const char *base = strrchr(original, '/');
    if (!base || !event_name(base+1) || strncmp(original, "/dev/input/", 11)) { errno = EINVAL; return -1; }
    struct stat directory, path;
    if (mkdir(guard_dir, 0700) && errno != EEXIST) return -1;
    if (lstat(guard_dir, &directory) || !S_ISDIR(directory.st_mode) || directory.st_uid != 0 || (directory.st_mode & 0077)) { errno = EPERM; return -1; }
    if (fstat(fd, &g->inode) || lstat(original, &path) || !same_node(&g->inode, &path) || !target_evdev(fd)) { errno = ESTALE; return -1; }
    snprintf(g->original, sizeof(g->original), "%s", original);
    snprintf(g->backup, sizeof(g->backup), "%s/%s", guard_dir, base+1);
    if (link(original, g->backup)) return -1;
    if (!live_node(g) || lstat(original, &path) || !same_node(&path, &g->inode)) {
        unlink(g->backup); errno = ESTALE; return -1;
    }
    if (unlink(original)) { int saved = errno; unlink(g->backup); errno = saved; return -1; }
    return 0; /* emits IN_DELETE: EventHub removes only the physical InputDevice */
}
#endif
