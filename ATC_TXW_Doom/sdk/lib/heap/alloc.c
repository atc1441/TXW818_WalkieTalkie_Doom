#include "sys_config.h"
#include "typesdef.h"
#include "osal/string.h"
#include "osal/irq.h"
#include "lib/heap/sysheap.h"
#include "lib/common/rbuffer.h"

/***********************************************************************
MEM_RECLIST - 内存 申请/释放 操作流水记录。
使用方法：
  1. 添加宏定义： 
     #define MEM_RECLIST_CNT 128  - 表示记录最后128条内存访问记录

  2. 在需要检测的 申请/释放 API里面添加：mem_alloc_rec 和 mem_free_rec
      -申请内存成功时调用：mem_alloc_rec
      -释放内存时调用：mem_free_rec
     __malloc和__free函数有示例代码。
     使用 RETURN_ADDR() 获取 申请/释放 的代码返回地址。
     对于嵌套调用 alloc的API，需要在第一级alloc API添加：mem_alloc_rec 和 mem_free_rec，
     这样才能使 RETURN_ADDR() 获取到真实的调用者地址

  3. 执行 mem_reclist_dump API 打印被记录的内存访问信息
     SDK在检测到内存越界或内存信息损坏时，也会自动执行 mem_reclist_dump
***********************************************************************/
//#define MEM_RECLIST_CNT 128
#ifdef MEM_RECLIST_CNT
struct mem_reclist {
    void  *addr;
    void  *lr;
    uint64 tick;
};
static uint16 g_mfreelist_pos, g_malloclist_pos;
static struct mem_reclist g_mfreelist[MEM_RECLIST_CNT];
static struct mem_reclist g_malloclist[MEM_RECLIST_CNT];
#endif

void mem_free_rec(void *addr, void *lr)
{
#ifdef MEM_RECLIST_CNT
    uint32 flag = disable_irq();
    g_mfreelist[g_mfreelist_pos].addr = addr;
    g_mfreelist[g_mfreelist_pos].lr   = lr;
    g_mfreelist[g_mfreelist_pos].tick = os_jiffies();
    g_mfreelist_pos++;
    if (g_mfreelist_pos >= MEM_RECLIST_CNT) {
        g_mfreelist_pos = 0;
    }
    enable_irq(flag);
#endif
}

void mem_alloc_rec(void *addr, void *lr)
{
#ifdef MEM_RECLIST_CNT
    uint32 flag = disable_irq();
    g_malloclist[g_malloclist_pos].addr = addr;
    g_malloclist[g_malloclist_pos].lr   = lr;
    g_malloclist[g_malloclist_pos].tick = os_jiffies();
    g_malloclist_pos++;
    if (g_malloclist_pos >= MEM_RECLIST_CNT) {
        g_malloclist_pos = 0;
    }
    enable_irq(flag);
#endif
}

void mem_reclist_dump(void)
{
#ifdef MEM_RECLIST_CNT
    uint32 i = 0;
    uint32 flag = disable_irq();
    os_printf("------------------------------------------\r\n");
    os_printf("MEM ALLOC Reclist:\r\n");
    for (i = 0; i < MEM_RECLIST_CNT; i++) {
        if (g_malloclist[i].addr) {
            os_printf("     MEM:%p, LR:%p, tick:%lld\r\n", g_malloclist[i].addr, g_malloclist[i].lr, g_malloclist[i].tick);
        }
    }
    os_printf("MEM FREE Reclist:\r\n");
    for (i = 0; i < MEM_RECLIST_CNT; i++) {
        if (g_mfreelist[i].addr) {
            os_printf("     MEM:%p, LR:%p, tick:%lld\r\n", g_mfreelist[i].addr, g_mfreelist[i].lr, g_mfreelist[i].tick);
        }
    }
    os_printf("------------------------------------------\r\n");
    enable_irq(flag);
#endif
}

#if MPOOL_ALLOC

__bobj struct sys_sramheap sram_heap;

void *_os_malloc(int size)
{
    void *ptr = sysheap_alloc(&sram_heap, size, RETURN_ADDR(), 0);
    if (ptr) {
        ASSERT((uint32)ptr == ALIGN((uint32)ptr, sram_heap.pool.align));
        if (IS_DCACHE_ADDR(ptr)) {
            sys_dcache_clean_invalid_range(ptr, size);
        }
    } else {
        // os_printf(KERN_WARNING"malloc sram fail, size=%d [LR:%p]\r\n", size, RETURN_ADDR());
    }
    return ptr;
}

void _os_free(void *ptr)
{
    if(ptr){
        ASSERT((uint32)ptr == ALIGN((uint32)ptr, sram_heap.pool.align));
        if (sysheap_free(&sram_heap, ptr)) {
            os_printf(KERN_WARNING"free sram error, ptr=%p, [LR:%p]\r\n", ptr, RETURN_ADDR());
        }
    }
}

void *_os_zalloc(int size)
{
    void *ptr = sysheap_alloc(&sram_heap, size, RETURN_ADDR(), 0);
    if (ptr) {
        ASSERT((uint32)ptr == ALIGN((uint32)ptr, sram_heap.pool.align));
        if (IS_DCACHE_ADDR(ptr)) {
            sys_dcache_clean_invalid_range(ptr, size);
        }
        os_memset(ptr, 0, size);
    } else {
        // os_printf(KERN_WARNING"malloc sram fail, size=%d [LR:%p]\r\n", size, RETURN_ADDR());
    }
    return ptr;
}

void *_os_calloc(size_t nmemb, size_t size)
{
    void *ptr = sysheap_alloc(&sram_heap, nmemb * size, RETURN_ADDR(), 0);
    if (ptr) {
        ASSERT((uint32)ptr == ALIGN((uint32)ptr, sram_heap.pool.align));
        if (IS_DCACHE_ADDR(ptr)) {
            sys_dcache_clean_invalid_range(ptr, nmemb * size);
        }
        os_memset(ptr, 0, nmemb * size);
    } else {
        // os_printf(KERN_WARNING"malloc sram fail, size=%d [LR:%p]\r\n", size, RETURN_ADDR());
    }
    return ptr;
}

void *_os_realloc(void *ptr, int size)
{
    void *nptr = sysheap_alloc(&sram_heap, size, RETURN_ADDR(), 0);
    if (nptr) {
        ASSERT((uint32)nptr == ALIGN((uint32)nptr, sram_heap.pool.align));
        if (IS_DCACHE_ADDR(nptr)) {
            sys_dcache_clean_invalid_range(nptr, size);
        }
        if(ptr){
            os_memcpy(nptr, ptr, size);
            _os_free(ptr);
        }
    } else {
        os_printf(KERN_WARNING"Realloc failed, be careful of memory leaks! (size=%d, LR:%p)\r\n", size, RETURN_ADDR());
    }

    return nptr;
}

void *_os_malloc_t(int size, const char *func, int line)
{
    void *ptr = sysheap_alloc(&sram_heap, size, func, line);
    if (ptr) {
        ASSERT((uint32)ptr == ALIGN((uint32)ptr, sram_heap.pool.align));
        if (IS_DCACHE_ADDR(ptr)) {
            sys_dcache_clean_invalid_range(ptr, size);
        }
    } else {
        // os_printf(KERN_WARNING"malloc sram fail, size=%d [%s:%d]\r\n", size, func, line);
    }
    return ptr;
}

void _os_free_t(void *ptr, const char *func, int line)
{
    if(ptr){
        ASSERT((uint32)ptr == ALIGN((uint32)ptr, sram_heap.pool.align));
        if (sysheap_free(&sram_heap, ptr)) {
            os_printf(KERN_WARNING"free sram error, ptr=%p, [%s:%d]\r\n", ptr, func, line);
        }
    }
}

void *_os_zalloc_t(int size, const char *func, int line)
{
    void *ptr = _os_malloc_t(size, func, line);
    if (ptr) {
        os_memset(ptr, 0, size);
    }
    return ptr;
}

void *_os_calloc_t(size_t nmemb, size_t size, const char *func, int line)
{
    return _os_zalloc_t(nmemb * size, func, line);
}

void *_os_realloc_t(void *ptr, int size, const char *func, int line)
{
    void *nptr = _os_malloc_t(size, func, line);
    if (nptr) {
        if(ptr){
            os_memcpy(nptr, ptr, size);
            _os_free_t(ptr, func, line);
        }
    }else{
        os_printf(KERN_WARNING"Realloc failed, be careful of memory leaks! (size=%d, %s:%d)\r\n", size, func, line);
    }
    return nptr;
}

#ifdef MALLOC_IN_PSRAM
void *malloc(uint32 size)               __alias(_os_malloc_psram);
void  free(void *ptr)                   __alias(_os_free_psram);
void *zalloc(size_t size)               __alias(_os_zalloc_psram);
void *calloc(size_t nmemb, size_t size) __alias(_os_calloc_psram);
void *realloc(void *ptr, size_t size)   __alias(_os_realloc_psram);
#else
void *malloc(uint32 size)               __alias(_os_malloc);
void  free(void *ptr)                   __alias(_os_free);
void *zalloc(size_t size)               __alias(_os_zalloc);
void *calloc(size_t nmemb, size_t size) __alias(_os_calloc);
void *realloc(void *ptr, size_t size)   __alias(_os_realloc);
#endif

//////////////////////////////////////////////////////////////////////////
#ifdef PSRAM_HEAP // PSRAM heap 相关API
__bobj struct sys_psramheap psram_heap;

void *_os_malloc_psram(int size)
{
    void *ptr = sysheap_alloc(&psram_heap, size, RETURN_ADDR(), 0);
    if (ptr) {
        ASSERT((uint32)ptr == ALIGN((uint32)ptr, psram_heap.pool.align));
        if (IS_DCACHE_ADDR(ptr)) {
            sys_dcache_clean_invalid_range(ptr, size);
        }
    } else {
        // os_printf(KERN_WARNING"malloc psram fail, size=%d [LR:%p]\r\n", size, RETURN_ADDR());
    }
    return ptr;
}

void _os_free_psram(void *ptr)
{
    if(ptr){
        ASSERT((uint32)ptr == ALIGN((uint32)ptr, psram_heap.pool.align));
        if (sysheap_free(&psram_heap, ptr)) {
            os_printf(KERN_WARNING"free psram error, ptr=%p [LR:%p]\r\n", ptr, RETURN_ADDR());
        }
    }
}

void *_os_zalloc_psram(size_t size)
{
    void *ptr = sysheap_alloc(&psram_heap, size, RETURN_ADDR(), 0);
    if (ptr) {
        ASSERT((uint32)ptr == ALIGN((uint32)ptr, psram_heap.pool.align));
        if (IS_DCACHE_ADDR(ptr)) {
            sys_dcache_clean_invalid_range(ptr, size);
        }
        os_memset(ptr, 0, size);
    } else {
        // os_printf(KERN_WARNING"malloc psram fail, size=%d [LR:%p]\r\n", size, RETURN_ADDR());
    }
    return ptr;
}

void *_os_calloc_psram(size_t nmemb, size_t size)
{
    void *ptr = sysheap_alloc(&psram_heap, nmemb * size, RETURN_ADDR(), 0);
    if (ptr) {
        ASSERT((uint32)ptr == ALIGN((uint32)ptr, psram_heap.pool.align));
        if (IS_DCACHE_ADDR(ptr)) {
            sys_dcache_clean_invalid_range(ptr, nmemb * size);
        }
        os_memset(ptr, 0, nmemb * size);
    } else {
        // os_printf(KERN_WARNING"malloc psram fail, size=%d [LR:%p]\r\n", size, RETURN_ADDR());
    }
    return ptr;
}

void *_os_realloc_psram(void *ptr, int size)
{
    void *nptr = sysheap_alloc(&psram_heap, size, RETURN_ADDR(), 0);
    if (nptr) {
        ASSERT((uint32)nptr == ALIGN((uint32)nptr, psram_heap.pool.align));
        if (IS_DCACHE_ADDR(nptr)) {
            sys_dcache_clean_invalid_range(nptr, size);
        }
        if(ptr){
            os_memcpy(nptr, ptr, size);
            _os_free_psram(ptr);
        }
    } else {
        os_printf(KERN_WARNING"Realloc failed, be careful of memory leaks! (size=%d, LR:%p)\r\n", size, RETURN_ADDR());
    }

    return nptr;
}

void *_os_malloc_psram_t(int size, const char *func, int line)
{
    void *ptr = sysheap_alloc(&psram_heap, size, func, line);
    if (ptr) {
        ASSERT((uint32)ptr == ALIGN((uint32)ptr, psram_heap.pool.align));
        if (IS_DCACHE_ADDR(ptr)) {
            sys_dcache_clean_invalid_range(ptr, size);
        }
    } else {
        // os_printf(KERN_WARNING"malloc psram fail, size=%d [%s:%d]\r\n", size, func, line);
    }
    return ptr;
}

void _os_free_psram_t(void *ptr, const char *func, int line)
{
    if(ptr){
        ASSERT((uint32)ptr == ALIGN((uint32)ptr, psram_heap.pool.align));
        if (sysheap_free(&psram_heap, ptr)) {
            os_printf(KERN_WARNING"_os_free_psram error, ptr=%p, [%s:%d]\r\n", ptr, func, line);
        }
    }
}

void *_os_zalloc_psram_t(int size, const char *func, int line)
{
    void *ptr = _os_malloc_psram_t(size, func, line);
    if (ptr) {
        os_memset(ptr, 0, size);
    }
    return ptr;
}

void *_os_calloc_psram_t(size_t nmemb, size_t size, const char *func, int line)
{
    return _os_zalloc_psram_t(nmemb * size, func, line);
}

void *_os_realloc_psram_t(void *ptr, int size, const char *func, int line)
{
    void *nptr = _os_malloc_psram_t(size, func, line);
    if (nptr) {
        if(ptr){
            os_memcpy(nptr, ptr, size);
            _os_free_psram_t(ptr, func, line);
        }
    }else{
        os_printf(KERN_WARNING"Realloc failed, be careful of memory leaks! (size=%d, %s:%d)\r\n", size, func, line);
    }
    return nptr;
}

void *malloc_psram(uint32 size)                   __alias(_os_malloc_psram);
void  free_psram(void *ptr)                       __alias(_os_free_psram);
void *zalloc_psram(size_t size)                   __alias(_os_zalloc_psram);
void *calloc_psram(size_t nmemb, size_t size)     __alias(_os_calloc_psram);
void *realloc_psram(void *ptr, size_t size)       __alias(_os_realloc_psram);
#else
void *_os_malloc_psram(int size)                  __alias(_os_malloc);
void  _os_free_psram(void *ptr)                   __alias(_os_free);
void *_os_zalloc_psram(size_t size)               __alias(_os_zalloc);
void *_os_calloc_psram(size_t nmemb, size_t size) __alias(_os_calloc);
void *_os_realloc_psram(void *ptr, int size)      __alias(_os_realloc);

void *_os_malloc_psram_t(int size, const char *func, int line)                  __alias(_os_malloc_t);
void  _os_free_psram_t(void *ptr, const char *func, int line)                   __alias(_os_free_t);
void *_os_zalloc_psram_t(int size, const char *func, int line)                  __alias(_os_zalloc_t);
void *_os_calloc_psram_t(size_t nmemb, size_t size, const char *func, int line) __alias(_os_calloc_t);
void *_os_realloc_psram_t(void *ptr, int size, const char *func, int line)      __alias(_os_realloc_t);
#endif
//////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////
// 在这里可以 继续添加 自定义heap 的 API.


#endif

