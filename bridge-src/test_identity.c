#include "device_identity.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); exit(1); } } while(0)
int main(void) {
    ns09_inode a = {17, 991, 1234}, b = a;
    CHECK(ns09_same_inode(a,b)); b.inode++; CHECK(!ns09_same_inode(a,b));
    b=a; b.device++; CHECK(!ns09_same_inode(a,b)); b=a; b.filesystem++; CHECK(!ns09_same_inode(a,b));
    CHECK(ns09_restore_decision(0,0,1)==NS09_RESTORE_NODE);
    CHECK(ns09_restore_decision(0,0,0)==NS09_REMOVE_BACKUP);
    CHECK(ns09_restore_decision(1,1,1)==NS09_REMOVE_BACKUP);
    CHECK(ns09_restore_decision(1,0,1)==NS09_REFUSE_REPLACE);
    CHECK(ns09_restore_decision(1,0,0)==NS09_REMOVE_BACKUP);
    for (unsigned n=0; n<10000; n++) {
        b=a; b.inode+=n+1; CHECK(!ns09_same_inode(a,b));
        CHECK(ns09_restore_decision(1,ns09_same_inode(a,b),1)==NS09_REFUSE_REPLACE);
    }
    printf("PASS: %u inode identity / hotplug generation / occupied path recovery assertions\n",checks);
    return 0;
}
