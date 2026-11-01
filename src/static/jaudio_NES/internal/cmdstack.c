#include "jaudio_NES/cmdstack.h"
#include "jaudio_NES/playercall.h"
#include "dolphin/os.h"

JPorthead_ cmd_once;
JPorthead_ cmd_stay;

/*
 * --INFO--
 * Address:	8000E300
 * Size:	000028
 */
void Add_PortcmdOnce(JPortcmd_* a1)
{
	Add_Portcmd(&cmd_once, a1);
}

/*
 * --INFO--
 * Address:	........
 * Size:	000028
 */
void Add_PortcmdStay(void)
{
	// UNUSED FUNCTION
}

/*
 * --INFO--
 * Address:	8000E340
 * Size:	000018
 */
int Set_Portcmd(JPortcmd_* a1, JPortcmdCallback a2, uaddr a3)
{
	a1->callback = a2;
	a1->arg      = a3;
	a1->port     = NULL;
	return 1;
}

/*
 * --INFO--
 * Address:	8000E360
 * Size:	000078
 */
BOOL Add_Portcmd(JPorthead_* port, JPortcmd_* a2)
{
	BOOL interrupt = OSDisableInterrupts();

	if (a2->port) {
		OSRestoreInterrupts(interrupt);
		return FALSE;
	}

	if (port->_04) {
		port->_04->next = a2;
	} else {
		port->_00 = a2;
	}

	port->_04 = a2;
	a2->next  = NULL;
	a2->port  = port;
	OSRestoreInterrupts(interrupt);
	return TRUE;
}

/*
 * --INFO--
 * Address:	8000E3E0
 * Size:	000040
 */
static JPortcmd_* Get_Portcmd(JPorthead_* port)
{
	JPortcmd_* a = port->_00;
	if (a) {
		port->_00 = port->_00->next;
		if (port->_00 == NULL) {
			port->_04 = NULL;
		}
		a->port = NULL;
	} else {
		a = NULL;
	}

	return a;
}

/*
 * --INFO--
 * Address:	........
 * Size:	000080
 */
void Cancel_Portcmd(void)
{
	// UNUSED FUNCTION
}

/*
 * --INFO--
 * Address:	........
 * Size:	000028
 */
void Cancel_PortcmdStay(void)
{
	// UNUSED FUNCTION
}

/*
 * --INFO--
 * Address:	8000E420
 * Size:	000050
 */
int Jac_Portcmd_Proc_Once(JPorthead_* port)
{
	JPortcmd_* p;
	while (1) {
		p = Get_Portcmd(port);
		if (!p) {
			break;
		}
		p->callback(p->arg);
	}
	return 0;
}

/*
 * --INFO--
 * Address:	8000E480
 * Size:	00004C
 */
int Jac_Portcmd_Proc_Stay(JPorthead_* port)
{
	JPortcmd_* p = port->_00;
	while (1) {
		if (!p) {
			break;
		}
		p->callback(p->arg);

		p = p->next;
	}
	return 0;
}

/*
 * --INFO--
 * Address:	8000E4E0
 * Size:	000030
 */
static s32 Portcmd_Main(void* a)
{
	Jac_Portcmd_Proc_Once(&cmd_once);
	Jac_Portcmd_Proc_Stay(&cmd_stay);
	return 0;
}

/*
 * --INFO--
 * Address:	8000E520
 * Size:	000010
 */
void Jac_Porthead_Init(JPorthead_* port)
{
	port->_00 = NULL;
	port->_04 = NULL;
}

/*
 * --INFO--
 * Address:	8000E540
 * Size:	00003C
 */
void Jac_Portcmd_Init(void)
{
	Jac_Porthead_Init(&cmd_once);
	Jac_Porthead_Init(&cmd_stay);
	Jac_RegisterPlayerCallback(Portcmd_Main, 0);
}

/*
 * --INFO--
 * Address:	........
 * Size:	00001C
 */
void JP_Pitch1Shot(void)
{
	// UNUSED FUNCTION
}

/*
 * --INFO--
 * Address:	........
 * Size:	00006C
 */
void JP_Start1Shot(void)
{
	// UNUSED FUNCTION
}

/*
 * --INFO--
 * Address:	........
 * Size:	00004C
 */
void JP_Stop1Shot(void)
{
	// UNUSED FUNCTION
}
