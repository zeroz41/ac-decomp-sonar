#ifndef _JAUDIO_CMDSTACK_H
#define _JAUDIO_CMDSTACK_H

#include "types.h"

typedef struct JPortcmd_ JPortcmd_;
typedef int (*JPortcmdCallback)(uaddr arg);

typedef struct JPorthead_ {
	JPortcmd_* _00; // _00, first command
	JPortcmd_* _04; // _04, last command
} JPorthead_;

struct JPortcmd_ {
	u32 _00[3];                // _00
	JPorthead_* port;          // _0C, the list holding the command
	JPortcmd_* next;           // _10
	JPortcmdCallback callback; // _14
	uaddr arg;                 // _18
};

#ifdef __cplusplus
extern "C" {
#endif // ifdef __cplusplus

void Add_PortcmdOnce(JPortcmd_*);
void Add_PortcmdStay(void); // UNUSED but we know it's extern "C"
int Set_Portcmd(JPortcmd_*, JPortcmdCallback, uaddr);
BOOL Add_Portcmd(JPorthead_*, JPortcmd_*);
void Cancel_Portcmd(void);     // UNUSED but we know it's extern "C"
void Cancel_PortcmdStay(void); // UNUSED but we know it's extern "C"
int Jac_Portcmd_Proc_Once(JPorthead_*);
int Jac_Portcmd_Proc_Stay(JPorthead_*);
void Jac_Porthead_Init(JPorthead_*);
void Jac_Portcmd_Init(void);
void JP_Pitch1Shot(void); // UNUSED but we know it's extern "C"
void JP_Start1Shot(void); // UNUSED but we know it's extern "C"
void JP_Stop1Shot(void);  // UNUSED but we know it's extern "C"

#ifdef __cplusplus
}
#endif // ifdef __cplusplus

static JPortcmd_* Get_Portcmd(JPorthead_* port);
static s32 Portcmd_Main(void* a);

#endif
