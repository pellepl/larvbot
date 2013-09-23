/*
 * heap.c
 *
 *  Created on: Jul 20, 2012
 *      Author: petera
 */

#include "heap.h"
#include "tinyheap.h"
#include "system.h"
#include "miniutils.h"

static u32_t _sram_heap_data[TH_BLOCKSIZE * (127) / sizeof(u32_t)];
static tinyheap _sram_heap;

static u32_t *_xram_heap_data = (u32_t*)XRAM_BEGIN;
#define XRAM_HEAP_SIZE (32*1024)
static tinyheap _xram_heap;

void *HEAP_malloc(unsigned int size) {
  void *p = th_malloc(&_sram_heap, size);
  if (p == NULL) {
    DBG(D_HEAP, D_FATAL, "HEAP alloc %i return 0\n", size);
  }
  return p;
}
void HEAP_free(void* p) {
  th_free(&_sram_heap, p);
}
void *HEAP_xmalloc(unsigned int size) {
  void *p = th_malloc(&_xram_heap, size);
  if (p == NULL) {
    DBG(D_HEAP, D_FATAL, "HEAP xalloc %i return 0\n", size);
  }
  return p;
}
void HEAP_xfree(void* p) {
  th_free(&_xram_heap, p);
}
void HEAP_init() {
  DBG(D_HEAP, D_DEBUG, "SRAM HEAP init\n");
  memset(&_sram_heap_data[0], 0xee, sizeof(_sram_heap_data));
  th_init(&_sram_heap, (void*)&_sram_heap_data[0], sizeof(_sram_heap_data));
#if TH_CALC_FREE
  DBG(D_HEAP, D_DEBUG, "  %i bytes free\n", th_freecount(&_sram_heap));
#endif

  DBG(D_HEAP, D_DEBUG, "XRAM HEAP init\n");
  memset(&_xram_heap_data[0], 0xee, XRAM_HEAP_SIZE);
  th_init(&_xram_heap, (void*)&_xram_heap_data[0], XRAM_HEAP_SIZE);
#if TH_CALC_FREE
  DBG(D_HEAP, D_DEBUG, "  %i bytes free\n", th_freecount(&_xram_heap));
#endif
}

void HEAP_dump() {
  print("SRAM heap\n");
  th_dump(&_sram_heap);
  print("XRAM heap\n");
  th_dump(&_xram_heap);
}
