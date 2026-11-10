#include "libc64/qrand.h"

static u32 __qrand_idum = 1;
static u32 __qrand_itemp;

u32 qrand(void){
    __qrand_idum = __qrand_idum * 0x19660D + 0x3C6EF35F;
    return __qrand_idum;
}

void sqrand(u32 seed){
    __qrand_idum = seed;
}

float fqrand(void){
    __qrand_idum = __qrand_idum * 0x19660D + 0x3C6EF35F;
    __qrand_itemp = __qrand_idum >> 9 | 0x3F800000;
    __qrand_itemp = __qrand_itemp;
    return *(float*)&__qrand_itemp - 1.0f;
}

float fqrand2(void){
    __qrand_idum = __qrand_idum * 0x19660D + 0x3C6EF35F;
    __qrand_itemp = __qrand_idum >> 9 | 0x3F800000;
    return *(float*)&__qrand_itemp - 1.5f;
}

#ifdef SONAR_PORT /* sonarlib port: the two words' addresses, for the port's state hashes (adapter/ac_state.c) */
void qrand_state(const u32** idum, const u32** itemp) {
    *idum = &__qrand_idum;
    *itemp = &__qrand_itemp;
}

/* sonarlib port: sets the two words, for the port's culling hook to put them back after a draw (adapter/ac_cull.c) */
void qrand_set_state(u32 idum, u32 itemp) {
    __qrand_idum = idum;
    __qrand_itemp = itemp;
}
#endif
