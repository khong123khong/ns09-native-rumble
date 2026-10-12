#ifndef NS09_TARGET_IDENTITY_H
#define NS09_TARGET_IDENTITY_H
#include <ctype.h>
#include <string.h>
/* Manufacturer OUI from the tested controller family, not a personal MAC.
 * The complete identifier is discovered locally and retained in memory only. */
#define NS09_TESTED_OUI "98B6EA"
static inline int ns09_clean_address(const char *raw, char clean[13]) {
    unsigned n=0;
    for(;*raw;raw++) {
        if(*raw==':' || *raw=='-' || *raw==' ') continue;
        if(!isxdigit((unsigned char)*raw) || n>=12) return 0;
        clean[n++]=(char)toupper((unsigned char)*raw);
    }
    clean[n]=0; return n==12;
}
static inline int ns09_candidate_address(const char *raw,char clean[13]) {
    return ns09_clean_address(raw,clean) && !strncmp(clean,NS09_TESTED_OUI,6);
}
static inline int ns09_target_matches(const char *raw,const char *bound) {
    char clean[13];
    if(!ns09_candidate_address(raw,clean)) return 0;
    /* Unbound recovery only examines the bridge-owned backup inode plus its
       live input ID; an active session always compares the entire identity. */
    return !bound[0] || !strcmp(clean,bound);
}
#endif
