// LVGL custom stdlib-malloc backend (LV_USE_STDLIB_MALLOC=LV_STDLIB_CUSTOM).
//
// Routes every LVGL heap allocation to PSRAM via ESP-IDF's heap_caps_*
// allocator (MALLOC_CAP_SPIRAM). The vendored LVGL builtin TLSF pool and
// stdlib CLIB backend both land small allocs on internal RAM on
// arduino-esp32, which starved mbedTLS during OTA HTTPS checks (55 KB free
// internal heap — see docs/decision-log.md 2026-07-17). This file implements
// the exact function contract LVGL's stdlib/lv_mem.c expects externally when
// LV_USE_STDLIB_MALLOC is set to LV_STDLIB_CUSTOM; no build_flags beyond the
// selector are required (see lvgl/src/stdlib/clib/lv_mem_core_clib.c for the
// reference shape of this contract).

#include <stddef.h>
#include "esp_heap_caps.h"
#include "lvgl.h"

void lv_mem_init(void)
{
    /*Nothing to init — heap_caps allocator is already up by the time LVGL boots*/
}

void lv_mem_deinit(void)
{
    /*Nothing to deinit*/
}

lv_mem_pool_t lv_mem_add_pool(void * mem, size_t bytes)
{
    /*Not supported — allocations go straight to heap_caps_malloc, no pool*/
    (void)mem;
    (void)bytes;
    return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool)
{
    /*Not supported*/
    (void)pool;
}

void * lv_malloc_core(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

void * lv_realloc_core(void * p, size_t new_size)
{
    return heap_caps_realloc(p, new_size, MALLOC_CAP_SPIRAM);
}

void lv_free_core(void * p)
{
    heap_caps_free(p);
}

void lv_mem_monitor_core(lv_mem_monitor_t * mon_p)
{
    /*Whole-PSRAM stats (includes esp_lcd framebuffers and every other SPIRAM
     *user, not just LVGL); max_used is current allocated, not a high-water mark*/
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);

    mon_p->total_size = info.total_free_bytes + info.total_allocated_bytes;
    mon_p->free_size = info.total_free_bytes;
    mon_p->free_biggest_size = info.largest_free_block;
    mon_p->free_cnt = 0;
    mon_p->used_cnt = 0;
    mon_p->max_used = info.total_allocated_bytes;
    mon_p->used_pct = mon_p->total_size ? (uint8_t)(100U * info.total_allocated_bytes / mon_p->total_size) : 0;
    mon_p->frag_pct = mon_p->free_size ? (uint8_t)(100U - (100U * mon_p->free_biggest_size / mon_p->free_size)) : 0;
}

lv_result_t lv_mem_test_core(void)
{
    /*Not supported — heap_caps has no LVGL-facing integrity walk*/
    return LV_RESULT_OK;
}
