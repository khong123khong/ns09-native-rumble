#ifndef NS09_DEVICE_IDENTITY_H
#define NS09_DEVICE_IDENTITY_H
#include <stdint.h>
typedef struct { uint64_t filesystem, inode, device; } ns09_inode;
static inline int ns09_same_inode(ns09_inode a, ns09_inode b) {
    return a.filesystem == b.filesystem && a.inode == b.inode && a.device == b.device;
}
enum { NS09_REMOVE_BACKUP, NS09_RESTORE_NODE, NS09_REFUSE_REPLACE };
static inline int ns09_restore_decision(int occupied, int same, int live) {
    if (occupied) return same || !live ? NS09_REMOVE_BACKUP : NS09_REFUSE_REPLACE;
    return live ? NS09_RESTORE_NODE : NS09_REMOVE_BACKUP;
}
#endif
