/* Standalone tests. Maps the workspace copy as DATA; never executes game code. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "address_resolver.h"
#include "address_profile.h"
#include "pe_data_image.h"
static void logger(const char *fmt,...) {
    va_list ap;va_start(ap,fmt);vprintf(fmt,ap);va_end(ap);puts("");
}
static BOOL resolve(unsigned char *image) {
    uint32_t rvas[STS_PROFILE_COUNT];uintptr_t manager;
    return sts_resolve((HMODULE)image,rvas,&manager,logger);
}
int wmain(int argc,wchar_t **argv) {
    if(argc!=3)return 2;
    unsigned char *image=load_data(argv[2]);
    if(!image)return 9;
    if(!wcscmp(argv[1],L"--inspect")) {
        uint32_t rvas[STS_PROFILE_COUNT];uintptr_t manager;
        ULONGLONG began=GetTickCount64();
        BOOL ok=sts_resolve((HMODULE)image,rvas,&manager,logger);
        printf("Offline development fixture: resolved=%u, time=%llu ms; no code executed or hooks installed\n",ok,GetTickCount64()-began);
        if(ok) {
            for(unsigned i=0;i<STS_PROFILE_COUNT;i++)printf("%s RVA=0x%x\n",sts_profiles[i].name,rvas[i]);
            printf("manager RVA=0x%llx\n",(unsigned long long)(manager-(uintptr_t)image));
        }
        VirtualFree(image,0,MEM_RELEASE);return ok?0:10;
    }
    if(wcscmp(argv[1],L"--image")){VirtualFree(image,0,MEM_RELEASE);return 2;}
    uint32_t rvas[STS_PROFILE_COUNT];uintptr_t manager;
    ULONGLONG began=GetTickCount64();
    if(!sts_resolve((HMODULE)image,rvas,&manager,logger))return 10;
    printf("Original data image resolved in %llu ms\n",GetTickCount64()-began);
    /* Simulate a different executable identity without altering game logic. */
    IMAGE_DOS_HEADER *mapped_dos=(void *)image;
    IMAGE_NT_HEADERS64 *mapped_nt=(void *)(image+mapped_dos->e_lfanew);
    mapped_nt->FileHeader.TimeDateStamp^=0x12345678;
    if(!resolve(image))return 11;
    /* Corrupt a member offset/instruction outside the short anchor. */
    unsigned char saved=image[rvas[STS_SELECT_ACTION]+100];
    image[rvas[STS_SELECT_ACTION]+100]^=1;
    if(resolve(image))return 12;
    image[rvas[STS_SELECT_ACTION]+100]=saved;
    /* Duplicate a matching unwind entry; even identical code must be rejected. */
    IMAGE_DATA_DIRECTORY table=mapped_nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    RUNTIME_FUNCTION *entries=(void *)(image+table.VirtualAddress);
    unsigned count=table.Size/sizeof(*entries),chosen=0;
    for(unsigned i=0;i<count;i++)if(entries[i].BeginAddress==rvas[0])chosen=i;
    RUNTIME_FUNCTION old=entries[0];entries[0]=entries[chosen];
    if(resolve(image))return 13;
    entries[0]=old;
    if(!resolve(image))return 14;
    /* Relocate the transfer function into an extra synthetic executable section.
       Repair its PC-relative operands and callers. No game code is executed. */
    IMAGE_SECTION_HEADER *mapped_sections=IMAGE_FIRST_SECTION(mapped_nt);
    IMAGE_SECTION_HEADER *extra=&mapped_sections[mapped_nt->FileHeader.NumberOfSections];
    if((uintptr_t)(extra+1)-(uintptr_t)image>mapped_nt->OptionalHeader.SizeOfHeaders)return 15;
    uint32_t relocated=mapped_nt->OptionalHeader.SizeOfImage;
    uint32_t original=rvas[STS_TRANSFER_ITEM];
    const StsProfile *profile=&sts_profiles[STS_TRANSFER_ITEM];
    memcpy(image+relocated,image+original,profile->size);
    int32_t movement=(int32_t)(relocated-original);
    for(unsigned i=0;i<profile->mask_count;i++) {
        if(profile->masks[i].size!=4)return 16;
        int32_t value;memcpy(&value,image+relocated+profile->masks[i].offset,4);
        value-=movement;memcpy(image+relocated+profile->masks[i].offset,&value,4);
    }
    for(unsigned i=0;i<sizeof(sts_edges)/sizeof(*sts_edges);i++)if(sts_edges[i].to==STS_TRANSFER_ITEM) {
        const StsReference *ref=&sts_edges[i];int32_t value;
        memcpy(&value,image+rvas[ref->from]+ref->displacement,4);value+=movement;
        memcpy(image+rvas[ref->from]+ref->displacement,&value,4);
    }
    for(unsigned i=0;i<count;i++)if(entries[i].BeginAddress==original) {
        entries[i].BeginAddress=relocated;entries[i].EndAddress=relocated+profile->size;
    }
    memset(extra,0,sizeof(*extra));memcpy(extra->Name,".ststest",8);
    extra->VirtualAddress=relocated;extra->Misc.VirtualSize=0x10000;
    extra->Characteristics=IMAGE_SCN_MEM_EXECUTE|IMAGE_SCN_MEM_READ|IMAGE_SCN_CNT_CODE;
    mapped_nt->FileHeader.NumberOfSections++;mapped_nt->OptionalHeader.SizeOfImage+=0x10000;
    if(!sts_resolve((HMODULE)image,rvas,&manager,logger) || rvas[STS_TRANSFER_ITEM]!=relocated)return 17;
    /* A masked relative displacement still has to satisfy the call graph. */
    for(unsigned i=0;i<sizeof(sts_edges)/sizeof(*sts_edges);i++)if(sts_edges[i].to==STS_TRANSFER_ITEM) {
        const StsReference *ref=&sts_edges[i];
        image[rvas[ref->from]+ref->displacement]^=1;
        if(resolve(image))return 18;
        image[rvas[ref->from]+ref->displacement]^=1;
        break;
    }
    for(unsigned i=0;i<sizeof(sts_manager_refs)/sizeof(*sts_manager_refs);i++) {
        const StsReference *ref=&sts_manager_refs[i];int32_t value;
        memcpy(&value,image+rvas[ref->from]+ref->displacement,4);value+=16;
        memcpy(image+rvas[ref->from]+ref->displacement,&value,4);
    }
    uintptr_t old_manager=manager;
    if(!sts_resolve((HMODULE)image,rvas,&manager,logger) || manager!=old_manager+16)return 19;
    const StsReference *ref=&sts_manager_refs[0];
    image[rvas[ref->from]+ref->displacement]^=8;
    if(resolve(image))return 20;
    VirtualFree(image,0,MEM_RELEASE);
    puts("Resolver tests passed: identity, function/global relocation, altered code, ambiguity, call graph, manager agreement.");
    return 0;
}
