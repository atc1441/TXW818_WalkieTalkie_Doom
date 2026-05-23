#ifndef _SYS_HEAP_H_
#define _SYS_HEAP_H_

#include "lib/heap/mmpool.h"

enum SYSHEAP_FLAGS {
    SYSHEAP_FLAGS_MEM_LEAK_TRACE     = (1u << 0),
    SYSHEAP_FLAGS_MEM_OVERFLOW_CHECK = (1u << 1),
    SYSHEAP_FLAGS_MEM_ALIGN_16       = (1u << 2),
};

struct sys_heap {
    const char *name;
    const struct mmpool_ops *ops;
    struct mmpool_base pool;
};

struct sys_sramheap {
    const char *name;
    const struct mmpool_ops *ops;
    struct mmpool1 pool;
};

struct sys_psramheap {
    const char *name;
    const struct mmpool_ops *ops;
    struct mmpool1 pool;
};

////////////////////////////////////////////////////////////////////////
// 自定义 heap 示例
// 每个字段的位置不能修改，只能修改 pool 的类型，选择 mmpool1, mmpool2 或 mmpool3
// 可以添加任意多个 struct sys_xx_heap 类型，来实现各种自定义的heap
// 可以在 alloc.c 添加新的 自定义heap 分配函数，例如 os_xx_malloc/os_xx_free
struct sys_xx_heap {
    const char *name;              //初始化时赋值，参考system.c中的malloc_init函数
    const struct mmpool_ops *ops;  //初始化时赋值，参考system.c中的malloc_init函数
    //可以修改pool的类型
    struct mmpool1 pool;           //修改pool类型，选择不同的分配模块: mmpool1, mmpool2, mmpool3
    //struct mmpool2 pool;
    //struct mmpool3 pool;
};

struct sys_csram_heap {
    const char *name;              //初始化时赋值，参考system.c中的malloc_init函数
    const struct mmpool_ops *ops;  //初始化时赋值，参考system.c中的malloc_init函数
    //可以修改pool的类型
    struct mmpool1 pool;           //修改pool类型，选择不同的分配模块: mmpool1, mmpool2, mmpool3
    //struct mmpool2 pool;
    //struct mmpool3 pool;
};

struct sys_cpsram_heap {
    const char *name;              //初始化时赋值，参考system.c中的malloc_init函数
    const struct mmpool_ops *ops;  //初始化时赋值，参考system.c中的malloc_init函数
    //可以修改pool的类型
    struct mmpool1 pool;           //修改pool类型，选择不同的分配模块: mmpool1, mmpool2, mmpool3
    //struct mmpool2 pool;
    //struct mmpool3 pool;
};
///////////////////////////////////////////////////////////////////////

int32 _sysheap_init(struct sys_heap *heap, void *heap_start, unsigned int heap_size, unsigned int flags);
void *_sysheap_alloc(struct sys_heap *heap, int size, const char *func, int line);
int32 _sysheap_free(struct sys_heap *heap, void *ptr);
uint32 _sysheap_freesize(struct sys_heap *heap);
uint32 _sysheap_totalsize(struct sys_heap *heap);
void _sysheap_collect_init(struct sys_heap *heap);
int32 _sysheap_add(struct sys_heap *heap, uint32 start_addr, uint32 end_addr);
int32 _sysheap_of_check(struct sys_heap *heap, void *ptr, uint32 size);
void _sysheap_status(struct sys_heap *heap, uint32 *status_buf, int32 buf_size, uint32 mini_size);
int32 _sysheap_valid_addr(struct sys_heap *heap, void *ptr, uint8 first);
uint32 _sysheap_time(struct sys_heap *heap);
int32 _sysheap_used_list(struct sys_heap *heap, uint32 *list_buf, int32 buf_size);
void _sysheap_dump(struct sys_heap *heap);

#define sysheap_init(heap, heap_start, heap_size, flags) \
    _sysheap_init((struct sys_heap *)(heap), heap_start, heap_size, flags)

#define sysheap_time(heap)\
    _sysheap_time((struct sys_heap *)(heap))

#define sysheap_alloc(heap, size, func, line)\
    _sysheap_alloc((struct sys_heap *)(heap), size, func, line)

#define sysheap_free(heap, ptr)\
    _sysheap_free((struct sys_heap *)(heap), ptr)

#define sysheap_freesize(heap)\
    _sysheap_freesize((struct sys_heap *)(heap))

#define sysheap_totalsize(heap)\
    _sysheap_totalsize((struct sys_heap *)(heap))

#define sysheap_collect_init(heap)\
    _sysheap_collect_init((struct sys_heap *)(heap))

#define sysheap_add(heap, start_addr, end_addr)\
    _sysheap_add((struct sys_heap *)(heap), start_addr, end_addr)

#define sysheap_of_check(heap, ptr, size)\
    _sysheap_of_check((struct sys_heap *)(heap), ptr, size)

#define sysheap_status(heap, status_buf, buf_size, mini_size)\
    _sysheap_status((struct sys_heap *)(heap), status_buf, buf_size, mini_size)

#define sysheap_valid_addr(heap, ptr, first)\
    _sysheap_valid_addr((struct sys_heap *)(heap), ptr, first)

#define sysheap_time(heap)\
    _sysheap_time((struct sys_heap *)(heap))

#define sysheap_used_list(heap, list_buf, buf_size)\
    _sysheap_used_list((struct sys_heap *)(heap), list_buf, buf_size)

#define sysheap_dump(heap)\
    _sysheap_dump((struct sys_heap *)(heap))

extern struct sys_sramheap  sram_heap;
extern struct sys_psramheap psram_heap;
//extern struct sys_xx_heap  xx_heap; //xx模块的自定义heap

#endif

