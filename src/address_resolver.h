#pragma once
#include <windows.h>
#include <stdint.h>
enum { STS_MENU_SET_DATA, STS_SELECT_ACTION, STS_LOOKUP_ITEM, STS_GET_PLAYER,
       STS_TRANSFER_ITEM, STS_TEXT_FROM_STRING, STS_GET_STORE,
       STS_STASH_LAYOUT_WITNESS, STS_TEXT_LAYOUT_WITNESS,
       STS_MENU_CONSTRUCTOR, STS_MENU_SHOW, STS_ROW_COPY,
       STS_TRANSFER_QUANTITY, STS_TRANSFER_LOWER, STS_PROFILE_COUNT };
typedef void (*StsLog)(const char *, ...);
/* Reads mapped PE memory only. Never calls game code or installs hooks. */
BOOL sts_resolve(HMODULE module, uint32_t rvas[STS_PROFILE_COUNT], uintptr_t *manager, StsLog log);

typedef struct { uint32_t offset, size; } StsMask;
typedef struct {
    const char *name;
    uint32_t size;
    unsigned char anchor[32], anchor_mask[32], digest[32];
    const StsMask *masks;
    uint32_t mask_count;
} StsProfile;
typedef struct { uint32_t from, displacement, next, to; } StsReference;

/* Selected only after complete startup validation; zero on refusal. */
extern uint32_t sts_container_pool_offset;
