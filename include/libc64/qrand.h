#ifndef LQRAND_H
#define LQRAND_H

#include "types.h"

u32 qrand(void);
void sqrand(u32);
f32 fqrand(void);
f32 fqrand2(void);
#ifdef SONAR_PORT /* sonarlib port: the two words' addresses, for the port's state hashes (adapter/ac_state.c) */
void qrand_state(const u32** idum, const u32** itemp);
/* sonarlib port: sets the two words, for the port's culling hook to put them back after a draw (adapter/ac_cull.c) */
void qrand_set_state(u32 idum, u32 itemp);
#endif

#endif