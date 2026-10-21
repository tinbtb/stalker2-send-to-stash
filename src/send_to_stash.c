/* Native test implementation. No UE4SS, Lua, or asset replacement. */
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
#include "build_profile.h"

typedef struct { void *data; uint32_t flags, padding; } NativeText;
typedef struct { void *data; int32_t num, max; } NativeArray;
typedef struct {
    uint8_t visible, enabled, padding[6];
    NativeText text;
    int32_t action, slot;
    NativeArray children;
    uint32_t item, attachment;
} MenuRow;
typedef struct { uint8_t header[0x20]; MenuRow rows[27]; } MenuModel;
typedef struct { wchar_t *data; int32_t num, max; } NativeString;
_Static_assert(sizeof(NativeText)==0x10,"NativeText ABI");
_Static_assert(sizeof(MenuRow)==0x38,"MenuRow ABI");
_Static_assert(offsetof(MenuRow,action)==0x18,"Action ABI");
_Static_assert(offsetof(MenuRow,children)==0x20,"Children ABI");
_Static_assert(sizeof(MenuModel)==0x608,"MenuModel ABI");

enum { CUSTOM_ACTION=0x53545348, DROP_ACTION=19, UNEQUIP_ACTION=5 };
enum { STATUS_STARTING=0, STATUS_UNSUPPORTED=1, STATUS_READY=2, STATUS_ERROR=3 };
typedef void (*SetDataFn)(void *,const MenuModel *);
typedef void (*SelectFn)(void *,const MenuRow *);
typedef void *(*LookupFn)(void *,uint32_t);
typedef void *(*GetPlayerFn)(void);
typedef void *(*GetStoreFn)(void);
typedef uint32_t *(*TransferFn)(void *,uint32_t *,uint32_t,uint32_t,int32_t,int32_t,uint8_t);
typedef NativeText *(*TextFn)(NativeText *,const NativeString *);
static uintptr_t g_base;
static HMODULE g_module;
static HANDLE g_log=INVALID_HANDLE_VALUE;
static volatile LONG g_started, g_status;
static SetDataFn g_original_set;
static SelectFn g_original_select;
static NativeText g_label;
static DWORD g_ui_thread;
static uint32_t g_last_skipped=0xffffffffu;
static struct { void *inventory; uint32_t item; BOOL valid; } g_offer;

static void log_line(const char *fmt,...) {
    if(g_log==INVALID_HANDLE_VALUE)return;
    char buffer[768];
    int n=snprintf(buffer,sizeof(buffer),"[%llu] ",(unsigned long long)GetTickCount64());
    va_list ap;va_start(ap,fmt);
    int m=vsnprintf(buffer+n,sizeof(buffer)-(size_t)n-3,fmt,ap);va_end(ap);
    if(m<0)return;
    size_t len=strlen(buffer);buffer[len++]='\r';buffer[len++]='\n';
    DWORD written;WriteFile(g_log,buffer,(DWORD)len,&written,NULL);FlushFileBuffers(g_log);
}
static BOOL read_bytes(uintptr_t address,void *out,SIZE_T size) {
    SIZE_T got=0;
    return address && ReadProcessMemory(GetCurrentProcess(),(const void *)address,out,size,&got) && got==size;
}
static BOOL read_ptr(uintptr_t address,uintptr_t *out) { return read_bytes(address,out,sizeof(*out)) && *out; }
static BOOL read_u32(uintptr_t address,uint32_t *out) { return read_bytes(address,out,sizeof(*out)); }
static BOOL same_bytes(uint32_t rva,const unsigned char *expected,size_t n) {
    unsigned char actual[24];return n<=sizeof(actual) && read_bytes(g_base+rva,actual,n) && !memcmp(actual,expected,n);
}
static BOOL valid_uid(uint32_t uid) { return uid<0xfffffff0u; }

/* The quest's AddToPlayerStash branch uses manager+0x80 and this same pool. */
static BOOL stash_container(uintptr_t manager,uint32_t *uid) {
    if(!read_u32(manager+0x80,uid) || !valid_uid(*uid) || (*uid&0xf8000000u)!=0x38000000u)return FALSE;
    uintptr_t store=(uintptr_t)((GetStoreFn)(g_base+RVA_GET_STORE))();
    if(!store)return FALSE;
    uintptr_t block=store+0x4db070;
    uint32_t index=*uid&0x07ffffffu;
    while(index>=0x800) {
        if(!read_ptr(block,&block))return FALSE;
        index-=0x800;
    }
    uint32_t actual;
    return read_u32(block+(uintptr_t)index*0x1d0+0x10+8,&actual) && actual==*uid;
}
static BOOL carried_item(uintptr_t manager,uint32_t uid,uintptr_t *item,uint32_t *source) {
    if(!valid_uid(uid))return FALSE;
    uintptr_t player=(uintptr_t)((GetPlayerFn)(g_base+RVA_GET_PLAYER))();
    uintptr_t player_container=0,owner=0;
    uint32_t expected,actual;
    if(!player || !read_ptr(player+0x1c0,&player_container) || !read_u32(player_container+8,&expected))return FALSE;
    *item=(uintptr_t)((LookupFn)(g_base+RVA_LOOKUP_ITEM))((void *)manager,uid);
    if(!*item || !read_u32(*item+8,&actual) || actual!=uid || !read_ptr(*item+0x20,&owner) || !read_u32(owner+8,source))return FALSE;
    return *source==expected;
}
static BOOL eligible_model(const MenuModel *model,uintptr_t *manager,uint32_t *uid) {
    uintptr_t item;uint32_t source,stash;
    memcpy(uid,model->header,sizeof(*uid));
    /* Borrow the native permissions: quest/otherwise non-droppable and equipped
       items do not get a second route around their existing restrictions. */
    if(!model->rows[DROP_ACTION].visible || !model->rows[DROP_ACTION].enabled || model->rows[UNEQUIP_ACTION].visible)return FALSE;
    if(!read_ptr(g_base+0xa2da4b0,manager) || !carried_item(*manager,*uid,&item,&source) || !stash_container(*manager,&stash))return FALSE;
    return source!=stash;
}
static BOOL ensure_label(void) {
    if(g_label.data)return TRUE;
    static wchar_t label[]=L"Send to your stash";
    NativeString str={label,(int32_t)(sizeof(label)/sizeof(label[0])),(int32_t)(sizeof(label)/sizeof(label[0]))};
    ((TextFn)(g_base+RVA_TEXT_FROM_STRING))(&g_label,&str);
    /* One owned FText retained for the process lifetime; row copies AddRef it. */
    return g_label.data!=NULL;
}
static void menu_set_data(void *menu,const MenuModel *data) {
    DWORD thread=GetCurrentThreadId();
    if(g_ui_thread && thread!=g_ui_thread){g_original_set(menu,data);return;}
    MenuModel copy;
    if(!read_bytes((uintptr_t)data,&copy,sizeof(copy))){g_original_set(menu,data);return;}
    g_offer.valid=FALSE;
    uintptr_t inventory=(uintptr_t)data-0xb70,own_menu=0,vtable=0,selection=0;
    if(!read_ptr(inventory+0x6b0,&own_menu) || own_menu!=(uintptr_t)menu ||
       !read_ptr(inventory,&vtable) || !read_ptr(vtable+0x618,&selection) || selection!=g_base+RVA_SELECT_ACTION) {
        g_original_set(menu,data);return;
    }
    uintptr_t manager;uint32_t uid;
    if(!eligible_model(&copy,&manager,&uid)){
        if(uid!=g_last_skipped){log_line("not offered item=%08x drop=%u/%u unequip=%u (native permission or ownership/stash validation)",uid,copy.rows[DROP_ACTION].visible,copy.rows[DROP_ACTION].enabled,copy.rows[UNEQUIP_ACTION].visible);g_last_skipped=uid;}
        g_original_set(menu,data);return;
    }
    int slot=26;
    while(slot>=0 && copy.rows[slot].visible)--slot;
    if(slot<0 || !ensure_label()){g_original_set(menu,data);return;}
    g_ui_thread=thread;
    MenuRow *row=&copy.rows[slot];memset(row,0,sizeof(*row));
    row->visible=row->enabled=1;row->text=g_label;row->action=CUSTOM_ACTION;
    row->slot=-1;row->item=uid;row->attachment=0xffffffffu;
    /* SetData's parameter is &InventoryNew::CurrSelectedItem, at +0xb70.
       It reads the input synchronously and deep-copies each visible row. */
    g_offer.inventory=(void *)inventory;g_offer.item=uid;g_offer.valid=TRUE;
    log_line("offered item=%08x slot=%d",uid,slot);
    g_original_set(menu,&copy);
}
static void select_action(void *inventory,const MenuRow *payload) {
    MenuRow row;
    if(!read_bytes((uintptr_t)payload,&row,sizeof(row)) || row.action!=CUSTOM_ACTION){g_original_select(inventory,payload);return;}
    uint32_t uid=0,stash=0,source=0,current=0;uintptr_t manager=0,item=0;
    BOOL accepted=g_offer.valid && inventory==g_offer.inventory && GetCurrentThreadId()==g_ui_thread;
    uid=g_offer.item;g_offer.valid=FALSE;
    accepted=accepted && read_u32((uintptr_t)inventory+0xb70,&current) && current==uid &&
        read_ptr(g_base+0xa2da4b0,&manager) && carried_item(manager,uid,&item,&source) &&
        stash_container(manager,&stash) && source!=stash;
    /* An unknown enum follows the native no-action cleanup. Do this while the
       source item still exists, before a transfer might merge/destroy its UID. */
    row.action=0x7fffffff;
    g_original_select(inventory,&row);
    if(!accepted){log_line("rejected stale/unavailable send item=%08x",uid);return;}
    if(!carried_item(manager,uid,&item,&source) || !stash_container(manager,&stash) || source==stash) {
        log_line("rejected changed ownership/destination item=%08x",uid);return;
    }
    uint32_t result=0xffffffffu;
    log_line("native transfer requested item=%08x source=%08x stash=%08x",uid,source,stash);
    ((TransferFn)(g_base+RVA_TRANSFER_ITEM))((void *)manager,&result,stash,uid,-1,-1,0);
    log_line("native transfer returned=%08x (stack selection may be pending)",result);
}

static BOOL supported_executable(void) {
    wchar_t path[32768];if(!GetModuleFileNameW(NULL,path,32768))return FALSE;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    if(file==INVALID_HANDLE_VALUE)return FALSE;
    LARGE_INTEGER size;
    if(!GetFileSizeEx(file,&size) || size.QuadPart!=SUPPORTED_FILE_SIZE){CloseHandle(file);return FALSE;}
    BCRYPT_ALG_HANDLE alg=NULL;BCRYPT_HASH_HANDLE hash=NULL;unsigned char digest[32];
    BOOL ok=FALSE;DWORD got;
    unsigned char *buffer=HeapAlloc(GetProcessHeap(),0,1024*1024);
    if(!buffer)goto done;
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0)<0 || BCryptCreateHash(alg,&hash,NULL,0,NULL,0,0)<0)goto done;
    for(;;){
        if(!ReadFile(file,buffer,1024*1024,&got,NULL))goto done;
        if(!got)break;
        if(BCryptHashData(hash,buffer,got,0)<0)goto done;
    }
    if(BCryptFinishHash(hash,digest,sizeof(digest),0)<0 || memcmp(digest,SUPPORTED_SHA256,32))goto done;
    IMAGE_DOS_HEADER dos;IMAGE_NT_HEADERS64 nt;
    if(!read_bytes(g_base,&dos,sizeof(dos)) || dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<=0 || dos.e_lfanew>0x100000 ||
       !read_bytes(g_base+(uint32_t)dos.e_lfanew,&nt,sizeof(nt)) || nt.Signature!=IMAGE_NT_SIGNATURE ||
       nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.SizeOfImage!=SUPPORTED_IMAGE_SIZE)goto done;
    ok=TRUE;
#define CHECK_SITE(n) if(!same_bytes(RVA_##n,BYTES_##n,sizeof(BYTES_##n))){log_line("site mismatch: " #n);ok=FALSE;}
    CHECK_SITE(MENU_SET_DATA);CHECK_SITE(SELECT_ACTION);CHECK_SITE(LOOKUP_ITEM);
    CHECK_SITE(GET_PLAYER);CHECK_SITE(TRANSFER_ITEM);CHECK_SITE(TEXT_FROM_STRING);CHECK_SITE(GET_STORE);
#undef CHECK_SITE
done:
    if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);
    if(buffer)HeapFree(GetProcessHeap(),0,buffer);CloseHandle(file);return ok;
}
static DWORD WINAPI initialize(void *unused) {
    (void)unused;
    wchar_t path[32768];DWORD n=GetModuleFileNameW(g_module,path,32768);
    if(n && n<32760){wchar_t *dot=wcsrchr(path,L'.');if(dot)wcscpy(dot,L".log");g_log=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);}
    g_base=(uintptr_t)GetModuleHandleW(NULL);
    log_line("SendToStash native test build; UE4SS not used");
    if(!supported_executable()){log_line("unsupported executable or modified hook sites; hooks disabled");InterlockedExchange(&g_status,STATUS_UNSUPPORTED);return 0;}
    MH_STATUS s=MH_Initialize();
    if(s!=MH_OK){log_line("MH_Initialize failed=%d",s);InterlockedExchange(&g_status,STATUS_ERROR);return 0;}
    s=MH_CreateHook((void *)(g_base+RVA_MENU_SET_DATA),(void *)menu_set_data,(void **)&g_original_set);
    if(s==MH_OK)s=MH_CreateHook((void *)(g_base+RVA_SELECT_ACTION),(void *)select_action,(void **)&g_original_select);
    if(s==MH_OK)s=MH_QueueEnableHook((void *)(g_base+RVA_MENU_SET_DATA));
    if(s==MH_OK)s=MH_QueueEnableHook((void *)(g_base+RVA_SELECT_ACTION));
    if(s==MH_OK)s=MH_ApplyQueued();
    if(s!=MH_OK){MH_DisableHook((void *)(g_base+RVA_MENU_SET_DATA));MH_DisableHook((void *)(g_base+RVA_SELECT_ACTION));MH_Uninitialize();log_line("hook setup failed=%d; disabled",s);InterlockedExchange(&g_status,STATUS_ERROR);return 0;}
    log_line("ready: exact executable and 7 code-site checks passed; 2 hooks enabled");
    InterlockedExchange(&g_status,STATUS_READY);return 0;
}
static void start_once(void) {
    if(InterlockedCompareExchange(&g_started,1,0))return;
    HANDLE thread=CreateThread(NULL,0,initialize,NULL,0,NULL);
    if(thread)CloseHandle(thread);else InterlockedExchange(&g_status,STATUS_ERROR);
}
__declspec(dllexport) void InitializeASI(void) { start_once(); }
__declspec(dllexport) LONG SendToStash_GetStatus(void) { return InterlockedCompareExchange(&g_status,0,0); }
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved) {
    (void)reserved;
    if(reason==DLL_PROCESS_ATTACH){g_module=instance;DisableThreadLibraryCalls(instance);start_once();}
    /* Hooks live until process exit. Do not FreeLibrary this module in-game. */
    return TRUE;
}
