#include "sys_config.h"
#include "typesdef.h"
#include "dev.h"
#include "devid.h"
#include "osal/string.h"
#include "osal/sleep.h"
#include "osal/task.h"
#include "osal/work.h"
#include "hal/gpio.h"
#include "lib/common/common.h"
#include "lib/heap/sysheap.h"
#include "custom_mem/custom_mem.h"

#if AUDIO_EN
#include "audio_app/audio_adc.h"
#endif
#if AUDIO_DAC_EN
#include "audio_app/audio_dac.h"
#endif

#include "keyWork.h"
#include "display.h"
#include "buttons.h"

extern uint8_t get_psram_status();

static struct os_work main_wk;

/* -----------------------------------------------------------------------
 * Camera power supply
 * --------------------------------------------------------------------- */
uint8 vcam_en()
{
    uint8 ret = TRUE;
#if VCAM_EN
    pmu_vcam_dis();
    os_sleep_ms(1);
    pmu_set_vcam_vol(VCAM_VOL_2V80);
    pmu_vcam_lc_en();
    pmu_vcam_oc_detect_dis();
    pmu_vcam_oc_int_dis();
    pmu_vcam_discharge_dis();
    pmu_vcam_pg_dis();
#ifdef VCAM_33
    pmu_set_vcam_vol(VCAM_VOL_3V25);
    pmu_vcam_en();
    os_sleep_ms(1);
    pmu_vcam_pg_en();
#else
    pmu_vcam_en();
    os_sleep_ms(1);
#endif
    pmu_vcam_lc_dis();
#ifndef VCAM_33
    pmu_vcam_oc_detect_en();
#endif
    if (PMU_VCAM_OC_PENDING) {
        return FALSE;
    }
    pmu_vcam_oc_detect_dis();
    pmu_vcam_oc_int_dis();
    pmu_lvd_oe_en();
#endif
    return ret;
}

/* Weak hooks for board-specific overrides */
__weak void user_io_preconfig() {}
__weak void user_hardware_config() {}

/* -----------------------------------------------------------------------
 * Hardware initialisation — camera, LCD, display, buttons, audio
 * --------------------------------------------------------------------- */
void hardware_init(uint8 vcam)
{
    if (vcam == FALSE) {
        os_printf("vcam err\n");
        return;
    }

#if DCDC_EN
    pmu_dcdc_open();
#endif

    buttons_hw_detect();   /* Flash-ID lesen, hw_4mb setzen — vor keyWork_init */

#if KEY_MODULE_EN == 1
    keyWork_init(10);
    /* buttons_init() omitted: keyWork already opened ADC0 on PA_3 */
#endif

#if DVP_EN
    bool csi_cfg();
    bool csi_ret = csi_cfg();
#endif

#if LCD_EN
    gpio_iomap_output(PA_5, GPIO_IOMAP_OUTPUT);
    gpio_set_val(PA_5, 1);
    {
        uint16_t w, h;
        uint8_t rotate;
        void lcd_module_run(uint16_t *w, uint16_t *h, uint8_t *rotate);
        lcd_module_run(&w, &h, &rotate);
    }
    display_init();   /* initialises lcd_sema + disables OSD before ISR fires */
#endif

#if DVP_EN
    bool csi_open();
    if (csi_ret)
        csi_open();
#endif

#if AUDIO_EN
    audio_adc_init();
#endif
#if AUDIO_DAC_EN
    audio_da_init();
#endif
}

/* -----------------------------------------------------------------------
 * Main loop — heartbeat work-queue item, ~1 s period
 * --------------------------------------------------------------------- */
static int32 main_loop(struct os_work *work)
{
    os_printf("freemem:%d\r\n", sysheap_freesize(&sram_heap));
#ifdef PSRAM_HEAP
    print_custom_psram();
#endif
    print_custom_sram();
    os_run_work_delay(&main_wk, 1000);
    return 0;
}

/* Custom memory pool size (SRAM) */
#ifndef CUSTOM_SIZE
#ifdef PSRAM_HEAP
    #define CUSTOM_SIZE (5*1024)
#else
    #if DVP_EN == 1
        #define CUSTOM_SIZE (60*1024)
    #else
        #define CUSTOM_SIZE (5*1024)
    #endif
#endif
#endif

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */
int main(void)
{
    os_printf("freemem:%d\r\n", sysheap_freesize(&sram_heap));

#ifdef PSRAM_HEAP
    extern uint8 audio_ana_detect_dwa(void);
    while (!get_psram_status()) {
        os_printf("psram not ready:%d\r\n", get_psram_status());
        os_sleep_ms(1000);
        mcu_reset();
    }
#endif

#ifdef PSRAM_HEAP
#ifdef CUSTOM_PSRAM_SIZE
#if CUSTOM_PSRAM_SIZE > 0
    {
        /* Use as much psram_heap as possible, capped at CUSTOM_PSRAM_SIZE.
         * A fixed request silently returns NULL when the static WAD buffer
         * leaves less space than requested. */
        int psram_avail = (int)sysheap_freesize(&psram_heap) - 32*1024;
        int pool_sz = (psram_avail < CUSTOM_PSRAM_SIZE) ? psram_avail : CUSTOM_PSRAM_SIZE;
        os_printf("psram_heap free:%d, custom pool:%d\r\n", psram_avail + 32*1024, pool_sz);
        void *custom_buf = (pool_sz > 0) ? (void*)os_malloc_psram(pool_sz) : NULL;
        if (!custom_buf) {
            os_printf("ERROR: custom psram pool alloc failed! pool_sz=%d\r\n", pool_sz);
        }
        custom_mem_psram_init(custom_buf, pool_sz > 0 ? pool_sz : 0);
        print_custom_psram();
    }
#endif
#endif
#endif

#ifdef CUSTOM_SIZE
#if CUSTOM_SIZE > 0
    {
        void *custom_buf = (void*)os_malloc(CUSTOM_SIZE);
        custom_mem_init(custom_buf, CUSTOM_SIZE);
        print_custom_sram();
    }
#endif
#endif

    uint8 vcam = vcam_en();
    user_io_preconfig();
    hardware_init(vcam);
    display_start();   /* lcd_sema already initialised inside hardware_init */

    mcu_watchdog_timeout(5);
    pmu_watchdog_timeout(5);
    OS_WORK_INIT(&main_wk, main_loop, 0);
    os_run_work_delay(&main_wk, 1000);

    pmu_clr_deadcode_pending();
    return 0;
}
