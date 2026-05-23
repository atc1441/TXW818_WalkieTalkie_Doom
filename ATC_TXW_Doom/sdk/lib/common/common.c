#include "sys_config.h"
#include "typesdef.h"
#include "errno.h"
#include "list.h"
#include "dev.h"
#include "devid.h"
#include "osal/task.h"
#include "osal/sleep.h"
#include "osal/string.h"
#include "osal/irq.h"
#include "hal/dma.h"
#include "hal/crc.h"
#include "lib/common/common.h"
#include "lib/heap/sysheap.h"

extern void *krhino_mm_alloc(size_t size, void *caller);
extern void  krhino_mm_free(void *ptr);
void cpu_loading_api_time(char *api, uint32 time, uint32 diff_tick);

__bobj uint32 sdk_version;
__bobj uint32 svn_version;
__bobj uint32 app_version;
__bobj uint64 cpu_loading_tick;
#ifdef M2M_DMA
uint32 m2mdam_time = 0;
__bobj struct dma_device *m2mdma;
#endif

void cpu_loading_api_time(char *api, uint32 time, uint32 diff_tick);
#define CPU_TIME_API(api) extern uint32 api##_time(void); \
                          _time_ = api##_time(); \
                          cpu_loading_api_time(#api, _time_, diff_tick);

void cpu_loading_api_time(char *api, uint32 time, uint32 diff_tick)
{
    if(time > 0){
        uint32 count = 100 * os_msecs_to_jiffies(time/1000);
        os_printf("[%s] Time: %dms, %d%%\r\n", api, (time/1000), (count/diff_tick));
    }
}

void module_version_show(void)
{
    extern uint32 __modver_start;
    extern uint32 __modver_end;
    uint32 *start = (uint32 *)&__modver_start;
    uint32 *end   = (uint32 *)&__modver_end;
    while(start < end){
        _os_printf("**   lib%s\r\n", (char *)*start++);
    }
    _os_printf("------------------------------------------------------------------\r\n");
}

typedef void (*__ctor_func_)(void);
extern __ctor_func_ __CTOR_LIST__[];
extern __ctor_func_ __DTOR_LIST__[];
void do_global_ctors(void)
{
    ulong i;
    ulong nptrs = (ulong)__CTOR_LIST__[0];
    if (nptrs == (ulong) -1) {
        for (nptrs = 0; __CTOR_LIST__[nptrs + 1] != 0; nptrs++) ;
    }
    for (i = nptrs; i >= 1; i--) {
        __CTOR_LIST__[i]();
    }
}
void do_global_dtors(void)
{
    ulong i;
    ulong nptrs = (ulong)__DTOR_LIST__[0];

    if (nptrs == (ulong) -1) {
        for (nptrs = 0; __DTOR_LIST__[nptrs + 1] != 0; nptrs++) ;
    }
    for (i = 1; i <= nptrs; i++) {
        __DTOR_LIST__[i]();
    }
}

void cpu_loading_print(uint8 all, struct os_task_info *tsk_info, uint32 size)
{
    uint32 i = 0;
    uint32 diff_tick = 0;
    uint32 _time_ = 0;
    uint32 count;
    uint64 jiff = os_jiffies();

    if(tsk_info == NULL) return;
    diff_tick = DIFF_JIFFIES(cpu_loading_tick, jiff);
    cpu_loading_tick = jiff;

    os_printf("--------------------------------------------------------------------\r\n");
    os_printf("Task Runtime Statistic, interval:%dms\r\n", os_jiffies_to_msecs(diff_tick));
    os_printf("PID     Name            %%CPU(Time)    Stack  Prio              Status\r\n");
    os_printf("--------------------------------------------------------------------\r\n");

    cpu_loading_api_time("sram_heap", sysheap_time(&sram_heap), diff_tick);
    #ifdef PSRAM_HEAP
    cpu_loading_api_time("sram_heap", sysheap_time(&psram_heap), diff_tick);
    #endif

    CPU_TIME_API(sysirq);
    CPU_TIME_API(skbpool);
    CPU_TIME_API(hw_memcpy);

    os_printf("--------------------------------------------------------------------\r\n");

    count = os_task_runtime(tsk_info, size);
    for (i = 0; i < count; i++) {
        if (tsk_info[i].time > 0 || all) {
            os_printf("%2d     %-12s\t%2d%%(%6d)   %4d  %2d (%08x)  %s\r\n",
                      tsk_info[i].id,
                      tsk_info[i].name ? tsk_info[i].name : "----",
#ifdef CSKY_OS
                      (tsk_info[i].time * 100) / diff_tick,
#elif defined(OHOS)
                      tsk_info[i].time,
#endif

                      tsk_info[i].time,
                      tsk_info[i].stack * 4,
                      tsk_info[i].prio,
                      tsk_info[i].arg,
                      tsk_info[i].status);
        }
    }
    os_printf("--------------------------------------------------------------------\r\n");
}

int strncasecmp(const char *s1, const char *s2, size_t n)
{
    size_t i = 0;

    for (i = 0; i < n && s1[i] && s2[i]; i++) {
        if (s1[i] == s2[i] || s1[i] + 32 == s2[i] || s1[i] - 32 == s2[i]) {
        } else {
            break;
        }
    }
    return (i != n);
}

int strcasecmp(const char *s1, const char *s2)
{
    while (*s1 || *s2) {
        if (*s1 == *s2 || *s1 + 32 == *s2 || *s1 - 32 == *s2) {
            s1++; s2++;
        } else {
            return -1;
        }
    }
    return 0;
}

#ifdef M2M_DMA
void hw_memcpy(void *dest, const void *src, uint32 size)
{
    uint32 __t__ = cpu_runtime_meas(0);
    if (dest && src) {
        if (m2mdma && size > 45) {
#ifdef MEM_TRACE
#ifdef PSRAM_HEAP
            void *heap = IS_PSRAM_ADDR(dest) ? (void *)(&psram_heap) : (void *)(&sram_heap);
#else
            void *heap = &sram_heap;
#endif
            int32 ret = sysheap_of_check(heap, dest, size);
            if (ret == -1) {
                os_printf("%s: WARING: OF CHECK 0x%x\r\n", __FUNCTION__, dest);
            } else {
                if (!ret) {
                    os_printf("check addr fail: %x, size:%d \r\n", dest, size);
                }
                ASSERT(ret == 1);
            }
#endif
            dma_memcpy(m2mdma, dest, src, size);
        } else {
            os_memcpy(dest, src, size);
        }
    }
    m2mdam_time += cpu_runtime_meas(__t__);
}

void hw_memcpy0(void *dest, const void *src, uint32 size)
{
    uint32 __t__ = cpu_runtime_meas(0);
    if (m2mdma && size > 45) {
#ifdef MEM_TRACE
#ifdef PSRAM_HEAP
        void *heap = IS_PSRAM_ADDR(dest) ? (void *)(&psram_heap) : (void *)(&sram_heap);
#else
        void *heap = &sram_heap;
#endif
        int32 ret = sysheap_of_check(heap, dest, size);
        if (ret == -1) {
            os_printf("%s: WARING: OF CHECK 0x%x\r\n", __FUNCTION__, dest);
        } else {
            if (!ret) {
                os_printf("check addr fail: %x, size:%d \r\n", dest, size);
            }
            ASSERT(ret == 1);
        }
#endif
        dma_memcpy(m2mdma, dest, src, size);
    } else {
        os_memcpy(dest, src, size);
    }
    m2mdam_time += cpu_runtime_meas(__t__);
}

void hw_memset(void *dest, uint8 val, uint32 n)
{
    uint32 __t__ = cpu_runtime_meas(0);
    if (dest) {
        if (m2mdma && n > 12) {
#ifdef MEM_TRACE
#ifdef PSRAM_HEAP
            void *heap = IS_PSRAM_ADDR(dest) ? (void *)(&psram_heap) : (void *)(&sram_heap);
#else
            void *heap = &sram_heap;
#endif
            int32 ret = sysheap_of_check(heap, dest, n);
            if (ret == -1) {
                os_printf("%s: WARING: OF CHECK 0x%x\r\n", __FUNCTION__, dest);
            } else {
                if (!ret) {
                    os_printf("check addr fail: %x, size:%d \r\n", dest, n);
                }
                ASSERT(ret == 1);
            }
#endif
            dma_memset(m2mdma, dest, val, n);
        } else {
            os_memset(dest, val, n);
        }
    }
    m2mdam_time += cpu_runtime_meas(__t__);
}

uint32 hw_memcpy_time(void)
{
    uint32 v = m2mdam_time;
    m2mdam_time = 0;
    return v/1000;
}

#endif

void *os_memdup(const void *ptr, uint32 len)
{
    void *p;
    if (!ptr || len == 0) {
        return NULL;
    }
    p = os_malloc(len);
    if (p) {
        hw_memcpy(p, ptr, len);
    }
    return p;
}


int32 os_random_bytes(uint8 *data, int32 len)
{
    int32 i = 0;
    int32 seed;
#ifdef TXW4002ACK803
    seed = csi_coret_get_value() ^ (csi_coret_get_value() << 8) ^ (csi_coret_get_value() >> 8);
#else
    seed = csi_coret_get_value() ^ sysctrl_get_trng() ^ (sysctrl_get_trng() >> 8);
#endif
    for (i = 0; i < len; i++) {
        seed = seed * 214013L + 2531011L;
        data[i] = (uint8)(((seed >> 16) & 0x7fff) & 0xff);
    }
    return 0;
}

uint32 hw_crc(enum CRC_DEV_TYPE type, uint8 *data, uint32 len)
{
    uint32 crc = 0xffff;
    struct crc_dev_req req;
    struct crc_dev *crcdev = (struct crc_dev *)dev_get(HG_CRC_DEVID);
    if (crcdev) {
        req.type = type;
        req.data = data;
        req.len  = len;
        //ASSERT((uint32)data % 4 == 0); // crc模块数据地址必须4字节对齐检查
        crc_dev_calc(crcdev, &req, &crc, 0);
    } else {
        os_printf("no crc dev\r\n");
        crc = (uint32)os_jiffies();
    }
    return crc;
}

int ffs(int x)
{
    int r = 1;

    if (!x) {
        return 0;
    }

    if (!(x & 0xffff)) {
        x >>= 16;
        r += 16;
    }
    if (!(x & 0xff)) {
        x >>= 8;
        r += 8;
    }
    if (!(x & 0xf)) {
        x >>= 4;
        r += 4;
    }
    if (!(x & 3)) {
        x >>= 2;
        r += 2;
    }
    if (!(x & 1)) {
        x >>= 1;
        r += 1;
    }
    return r;
}

int fls(int x)
{
    int r = 32;

    if (!x) {
        return 0;
    }

    if (!(x & 0xffff0000u)) {
        x <<= 16;
        r -= 16;
    }
    if (!(x & 0xff000000u)) {
        x <<= 8;
        r -= 8;
    }
    if (!(x & 0xf0000000u)) {
        x <<= 4;
        r -= 4;
    }
    if (!(x & 0xc0000000u)) {
        x <<= 2;
        r -= 2;
    }
    if (!(x & 0x80000000u)) {
        x <<= 1;
        r -= 1;
    }
    return r;
}

uint32 scatter_size(scatter_data *data, uint32 count)
{
    uint32 size = 0;
    uint32 i = 0;
    for (i = 0; i < count; i++) {
        size += data[i].size;
    }
    return size;
}

uint8 *scatter_offset(scatter_data *data, uint32 count, uint32 off)
{
    uint8 i;
    for (i = 0; i < count; i++) {
        if (off < data[i].size) {
            return data[i].addr + off;
        }
        off -= data[i].size;
    }
    return NULL;
}

extern int *__errno_location(void);

void set_errno(int32 err)
{
    *__errno_location() = err;
}

int32 get_errno(void)
{
    return *__errno_location();
}

