#include "target_identity.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);} } while(0)
int main(void) {
    /* Synthetic identifiers only; none belong to a real test device. */
    char clean[13];
    CHECK(ns09_candidate_address("98:b6:ea:00:00:01",clean));
    CHECK(!strcmp(clean,"98B6EA000001"));
    CHECK(ns09_target_matches("98-B6-EA-00-00-01",clean));
    CHECK(!ns09_target_matches("98:B6:EA:00:00:02",clean));
    CHECK(ns09_target_matches("98:B6:EA:00:00:01",""));
    CHECK(!ns09_candidate_address("AA:BB:CC:00:00:01",clean));
    CHECK(!ns09_candidate_address("98:B6:EA:00:00",clean));
    CHECK(!ns09_candidate_address("98:B6:EA:00:00:01:00",clean));
    CHECK(!ns09_candidate_address("98:B6:EA:00:00:GG",clean));
    puts("PASS: public identity normalization, manufacturer restriction, complete session binding and malformed/foreign rejection");
    return 0;
}
