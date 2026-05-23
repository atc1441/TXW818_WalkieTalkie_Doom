/*
    用户自定义的内存管理,主要为了根据客户自己的需求,用独立的,可以避免内存在heap中碎片化
*/

#include "custom_mem.h"
static struct sys_csram_heap custom_sram_heap = { .name = "c_sram", .ops = &mmpool1_ops};
static void *g_custom_buf = NULL;

void custom_mem_init(void *buf,int custom_heap_size)
{
    if(!buf || g_custom_buf)
    {
        os_printf("%s:%d err,g_custom_buf:%X\n",__FUNCTION__,__LINE__,g_custom_buf);
        return;
    }
    g_custom_buf = buf;
    uint32_t flags = 0;
    #ifdef MEM_TRACE
        flags |= SYSHEAP_FLAGS_MEM_LEAK_TRACE | SYSHEAP_FLAGS_MEM_OVERFLOW_CHECK;
    #endif
        os_printf("custom_mem_init:%x\n",buf);
        sysheap_init(&custom_sram_heap, (void *)buf, custom_heap_size, flags);
}

void custom_mem_deinit()
{
    g_custom_buf = NULL;
}

void print_custom_sram()
{
    os_printf("custom mem sram:%d\n",sysheap_freesize(&custom_sram_heap));
}

void *custom_malloc(int size)
{
    //os_printf("custom mem:%d\n",sysheap_freesize(&custom_sram_heap));
    return sysheap_alloc(&custom_sram_heap, size, RETURN_ADDR(), 0);
}

void custom_free(void *ptr)
{
    if (ptr) {
        sysheap_free(&custom_sram_heap, ptr);
    }
}

void *custom_zalloc(int size)
{
    void *ptr = sysheap_alloc(&custom_sram_heap, size, RETURN_ADDR(), 0);
    if (ptr) {
        os_memset(ptr, 0, size);
    }
    return ptr;
}

void *custom_calloc(int nmemb, int size)
{
    void *ptr = sysheap_alloc(&custom_sram_heap, nmemb * size, RETURN_ADDR(), 0);
    if (ptr) {
        os_memset(ptr, 0, nmemb * size);
    }
    return ptr;
}

void *custom_realloc(void *ptr, int size)
{
    void *nptr = sysheap_alloc(&custom_sram_heap, size, RETURN_ADDR(), 0);
    if (nptr) {
        os_memcpy(nptr, ptr, size);
        sysheap_free(&custom_sram_heap, ptr);
    }
    return nptr;
}

#ifdef PSRAM_HEAP

static struct sys_cpsram_heap custom_psram_heap = { .name = "c_psram", .ops = &mmpool1_ops};
static void *g_custom_psram_buf = NULL;
void custom_mem_psram_init(void *buf,int custom_heap_size)
{
    if(!buf || g_custom_psram_buf)
    {
        os_printf("%s:%d err,g_custom__psram_buf:%X\n",__FUNCTION__,__LINE__,g_custom_psram_buf);
        return;
    }
    g_custom_psram_buf = buf;
    uint32_t flags = 0;
    #ifdef MEM_TRACE
        flags |= SYSHEAP_FLAGS_MEM_LEAK_TRACE | SYSHEAP_FLAGS_MEM_OVERFLOW_CHECK;
    #endif
    flags |= SYSHEAP_FLAGS_MEM_ALIGN_16;
    os_printf("%s :%x\n",__FUNCTION__,buf);
    sysheap_init(&custom_psram_heap, (void *)buf, custom_heap_size, flags);
}

void custom_mem_psram_deinit()
{
    g_custom_psram_buf = NULL;
}

void print_custom_psram()
{
    os_printf("custom mem psram:%d\n",sysheap_freesize(&custom_psram_heap));
}

void *custom_malloc_psram(int size)
{
	void *pt;
    //os_printf("custom mem:%d\n",sysheap_freesize(&custom_psram_heap));
    pt = sysheap_alloc(&custom_psram_heap, size, RETURN_ADDR(), 0);
	sys_dcache_clean_invalid_range(pt, size);
    return pt;
}

void custom_free_psram(void *ptr)
{
    if (ptr) {
        sysheap_free(&custom_psram_heap, ptr);
    }
}

void *custom_zalloc_psram(int size)
{
    void *ptr = sysheap_alloc(&custom_psram_heap, size, RETURN_ADDR(), 0);
    if (ptr) {
        os_memset(ptr, 0, size);
    }
    return ptr;
}

void *custom_calloc_psram(int nmemb, int size)
{
    void *ptr = sysheap_alloc(&custom_psram_heap, nmemb * size, RETURN_ADDR(), 0);
    if (ptr) {
        os_memset(ptr, 0, nmemb * size);
    }
    return ptr;
}

void *custom_realloc_psram(void *ptr, int size)
{
    void *nptr = sysheap_alloc(&custom_psram_heap, size, RETURN_ADDR(), 0);
    if (nptr) {
        os_memcpy(nptr, ptr, size);
        sysheap_free(&custom_psram_heap, ptr);
    }
    return nptr;
}
#endif
