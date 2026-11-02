/* sonarlib port: the 32-bit handle a mail to the DSP carries in place of a host address (adapter/dolphin_dsp.c). */
#ifndef PORT_DSP_MAIL_H
#define PORT_DSP_MAIL_H

#include "dolphin/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The same handle for the same address every time, 0 for NULL. The port's DSP turns it back into the address. The
   audio thread. */
u32 ac_dsp_mail_address(const void* address);

#ifdef __cplusplus
}
#endif

#endif
