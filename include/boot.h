#ifndef BOOT_H
#define BOOT_H

#include "types.h"
#include "dolphin/os/OSModule.h"
#include "dolphin/os/OSLink.h"
#include "dolphin/os/OSTime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct boot_tbl_s {
  const char* msg;
  u16 param_upper;
  u16 param_lower;
} boot_tbl_t;

extern u32 convert_partial_address(u32 partial_addr);
extern void HotResetIplMenu();

extern void* boot_copyDate;
extern void* HotStartEntry;
extern OSTime InitialStartTime;
extern u8 boot_sound_initializing;

typedef void(*HotStartProc)();

#ifdef SONAR_PORT
/* sonarlib port: the memory a restart keeps, which the console kept at a fixed address (port/adapter/dolphin_os.c) */
extern u8 ac_nmi_save_area[0x40];
#define NMISaveArea ((void*)ac_nmi_save_area)
#else
#define NMISaveArea (void*)0x811FFFC0
#endif
#ifdef SONAR_PORT
/* sonarlib port: the console's low memory, which the port's platform layer stands in for */
extern OSModuleHeader* BaseModule;
#else
OSModuleHeader* BaseModule AT_ADDRESS(0x800030C8);
#endif

// TODO: this macro should probably go elsewhere
#define MAKE_PARTIAL_ADDRESS(segment, ofs) \
  (((segment) << 28) + (ofs & 0x01FFFFFF))

#if VERSION == VER_GAFU01_00
extern u8 SoftResetEnable;
#endif

#ifdef __cplusplus
}
#endif

#endif
