#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
struct block_memory_alloc_s {
  void  *firstpool;
  size_t size;
  size_t perpool;
  int    tag;
  const char *desc;
};

#define DECLARE_BLOCK_MEMORY_ALLOC_ZONE(name) extern struct block_memory_alloc_s name
#define IMPLEMENT_BLOCK_MEMORY_ALLOC_ZONE(name, size, tag, num, desc) \
struct block_memory_alloc_s name = { NULL, size, num, tag, desc}
#define NULL_BLOCK_MEMORY_ALLOC_ZONE(name) name.firstpool = NULL

void* Z_BMalloc(struct block_memory_alloc_s *pzone);

__inline static void* Z_BCalloc(struct block_memory_alloc_s *pzone)
{ void *p = Z_BMalloc(pzone); memset(p,0,pzone->size); return p; }

void Z_BFree(struct block_memory_alloc_s *pzone, void* p);
