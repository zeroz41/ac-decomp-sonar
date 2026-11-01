/* The out-of-line copy of each of MSL's math inlines, for the calls the host compiler does not inline. The original
   compiler left one in each unit that needed it; the port's headers inline them only, and this unit defines them. */
#include "MSL_C/math.h"

/* MSL_Common/float.h's, which C defines here only with a declaration that is not inline. */
int __fpclassifyf(float);
int __fpclassifyd(double);
