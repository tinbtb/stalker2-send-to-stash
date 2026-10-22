/* Unified callbacks. Address resolution runs once on the startup worker. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>
#include "MinHook.h"
#ifdef SEND_TO_STASH_UE4SS
#include "ue4ss_resolver.h"
#else
#include "address_resolver.h"
#endif
static uint32_t g_rvas[STS_PROFILE_COUNT];
static uintptr_t g_manager_address;
#define RVA_MENU_SET_DATA g_rvas[STS_MENU_SET_DATA]
#define RVA_SELECT_ACTION g_rvas[STS_SELECT_ACTION]
#define RVA_LOOKUP_ITEM g_rvas[STS_LOOKUP_ITEM]
#define RVA_GET_PLAYER g_rvas[STS_GET_PLAYER]
#define RVA_TRANSFER_ITEM g_rvas[STS_TRANSFER_ITEM]
#define RVA_TEXT_FROM_STRING g_rvas[STS_TEXT_FROM_STRING]
#define RVA_GET_STORE g_rvas[STS_GET_STORE]
#define MANAGER_ADDRESS g_manager_address
#ifdef SEND_TO_STASH_UE4SS
/* No disk I/O, string formatting, or log flushes from UI/transfer hooks. */
#define log_event(...) ((void)0)
#else
#define log_event log_line
#endif

#include "stash_runtime.inc"

static DWORD WINAPI initialize(void *unused) {
    (void)unused;
    wchar_t path[32768];DWORD n=GetModuleFileNameW(g_module,path,32768);
    if(n && n<32760){wchar_t *dot=wcsrchr(path,L'.');if(dot)wcscpy(dot,L".log");g_log=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);}
    g_base=(uintptr_t)GetModuleHandleW(NULL);
    SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
    ULONGLONG began=GetTickCount64();
#ifdef SEND_TO_STASH_UE4SS
    log_line("SendToStash UE4SS native helper; resolving once off the game thread");
    if(GetModuleHandleW(L"SendToStash.asi")) {
        log_line("SendToStash.asi also loaded; remove it before using the UE4SS edition");
        InterlockedExchange(&g_status,STATUS_ERROR);return 0;
    }
#else
    log_line("SendToStash unified ASI test: startup signature resolution required; UE4SS not used");
#endif
    if(!sts_resolve((HMODULE)g_base,g_rvas,&g_manager_address,log_line)) {
        log_line("incompatible/ambiguous native layout; hooks disabled");
        InterlockedExchange(&g_status,STATUS_UNSUPPORTED);return 0;
    }
    MH_STATUS s=MH_Initialize();
    if(s!=MH_OK){log_line("MH_Initialize failed=%d",s);InterlockedExchange(&g_status,STATUS_ERROR);return 0;}
    s=MH_CreateHook((void *)(g_base+RVA_MENU_SET_DATA),(void *)menu_set_data,(void **)&g_original_set);
    if(s==MH_OK)s=MH_CreateHook((void *)(g_base+RVA_SELECT_ACTION),(void *)select_action,(void **)&g_original_select);
    if(s==MH_OK)s=MH_QueueEnableHook((void *)(g_base+RVA_MENU_SET_DATA));
    if(s==MH_OK)s=MH_QueueEnableHook((void *)(g_base+RVA_SELECT_ACTION));
    if(s==MH_OK)s=MH_ApplyQueued();
    if(s!=MH_OK){MH_DisableHook((void *)(g_base+RVA_MENU_SET_DATA));MH_DisableHook((void *)(g_base+RVA_SELECT_ACTION));MH_Uninitialize();log_line("hook setup failed=%d; disabled",s);InterlockedExchange(&g_status,STATUS_ERROR);return 0;}
    log_line("ready: startup=%llu ms; resolved-address callbacks; 2 hooks enabled; no runtime scanning",GetTickCount64()-began);
    InterlockedExchange(&g_status,STATUS_READY);return 0;
}
static void start_once(void) {
    if(InterlockedCompareExchange(&g_started,1,0))return;
    HANDLE thread=CreateThread(NULL,0,initialize,NULL,0,NULL);
    if(thread)CloseHandle(thread);else InterlockedExchange(&g_status,STATUS_ERROR);
}
#ifdef SEND_TO_STASH_UE4SS
/* Lua C-function ABI. No Lua API calls or Lua state retained by the worker. */
__declspec(dllexport) int luaopen_SendToStash(void *lua_state) {
    (void)lua_state;
    HMODULE pinned=NULL;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                           (LPCWSTR)(uintptr_t)&luaopen_SendToStash,&pinned)) {
        InterlockedExchange(&g_status,STATUS_ERROR);return 0;
    }
    start_once();return 0;
}
#else
__declspec(dllexport) void InitializeASI(void) { start_once(); }
#endif
__declspec(dllexport) LONG SendToStash_GetStatus(void) { return InterlockedCompareExchange(&g_status,0,0); }
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved) {
    (void)reserved;
    if(reason==DLL_PROCESS_ATTACH){g_module=instance;DisableThreadLibraryCalls(instance);
#ifndef SEND_TO_STASH_UE4SS
        start_once();
#endif
    }
    /* Hooks live until process exit. Do not FreeLibrary this module in-game. */
    return TRUE;
}
