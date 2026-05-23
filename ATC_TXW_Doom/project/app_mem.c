#include "sys_config.h"
#include "basic_include.h"

/*
 * app_mem.c 用于控制 某些具有大内存需求的模块的 内存分配方式
 * 修改内存分配方式时，必须把对应的 alloc/free 2个API 一起修改。
 */

//////////////////////////////////////////////////////////////////////////////////////////////////////
// libCurl协议库的内存分配
void *libcurl_mem_alloc(uint32 size)
{
#ifndef PSRAM_HEAP
    return sysheap_alloc((struct sys_heap *)&sram_heap, size, RETURN_ADDR(), 0);    //从sram内存池分配
#else
    return sysheap_alloc((struct sys_heap *)&psram_heap, size, RETURN_ADDR(),0);    //从psram内存池分配
#endif
}
void libcurl_mem_free(void *ptr)
{
#ifndef PSRAM_HEAP
    sysheap_free((struct sys_heap *)&sram_heap, ptr);
#else
    sysheap_free((struct sys_heap *)&psram_heap, ptr);
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//mbedTLS 的内存分配
void *mbedtls_mem_alloc(uint32 size)
{
#ifndef PSRAM_HEAP
    return sysheap_alloc((struct sys_heap *)&sram_heap, size, RETURN_ADDR(), 0);    //从sram内存池分配
#else
    return sysheap_alloc((struct sys_heap *)&psram_heap, size, RETURN_ADDR(),0);    //从psram内存池分配
#endif
}
void mbedtls_mem_free(void *ptr)
{
#ifndef PSRAM_HEAP
    sysheap_free((struct sys_heap *)&sram_heap, ptr);
#else
    sysheap_free((struct sys_heap *)&psram_heap, ptr);
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//urlfile 的内存分配。 urlfile是网络下载协议模块，通常是网络缓冲区所需的buffer
void *uf_alloc(uint32 size)
{
    return sysheap_alloc((struct sys_heap *)&sram_heap, size, RETURN_ADDR(), 0);     //从sram内存池分配
    //return sysheap_alloc((struct sys_heap *)&psram_heap, size, RETURN_ADDR());  //从psram内存池分配
}
void uf_free(void *ptr)
{
    sysheap_free((struct sys_heap *)&sram_heap, ptr);     //从sram内存池分配
    //sysheap_free((struct sys_heap *)&psram_heap, ptr, RETURN_ADDR());  //从psram内存池分配
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//lwip 的内存分配。
//启用 MEM_LIBC_MALLOC 后，这2个API才会被使用
void *lwip_mem_alloc(uint32 size)
{
#ifndef PSRAM_HEAP
    return sysheap_alloc((struct sys_heap *)&sram_heap, size, RETURN_ADDR(), 0);    //从sram内存池分配
#else
    return sysheap_alloc((struct sys_heap *)&psram_heap, size, RETURN_ADDR(),0);    //从psram内存池分配
#endif
}
void lwip_mem_free(void *ptr)
{
#ifndef PSRAM_HEAP
    sysheap_free((struct sys_heap *)&sram_heap, ptr);
#else
    sysheap_free((struct sys_heap *)&psram_heap, ptr);
#endif
}

void *llm_mem_alloc(uint32 size)
{
#ifndef PSRAM_HEAP
    return sysheap_alloc((struct sys_heap *)&sram_heap, size, RETURN_ADDR(), 0);    //从sram内存池分配
#else
    return sysheap_alloc((struct sys_heap *)&psram_heap, size, RETURN_ADDR(),0);    //从psram内存池分配
#endif
}
void llm_mem_free(void *ptr)
{
#ifndef PSRAM_HEAP
    sysheap_free((struct sys_heap *)&sram_heap, ptr);
#else
    sysheap_free((struct sys_heap *)&psram_heap, ptr);
#endif
}

