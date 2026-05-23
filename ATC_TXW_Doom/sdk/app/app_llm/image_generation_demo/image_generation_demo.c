#include "image_generation_demo.h"
#include "sounds.h"

/*
* 功能说明：
*
* 按键方案：
*    长按设备开始监听，此时进行语音输入，
*    松开设备停止监听，思考后回复图片。
*/
#ifdef IMAGE_GENERATION_DEMO
/* Please add your server information */
#define STT_URL         "wss://openspeech.bytedance.com/api/v2/asr"
#define STT_APPID       "xxx"
#define STT_TOKEN       "xxx"
#define STT_CLUSTER     "xxx"

#define TTI_URL         "https://ark.cn-beijing.volces.com/api/v3/images/generations"
#define ARK_API_KEY     "xxx"
#define ENDPOINT_ID     "doubao-seedream-3-0-t2i-250415"

#ifndef UINT64_MAX
#define UINT64_MAX 0xffffffffffffffffULL
#endif

#define IG_SOUND_RB_SIZE      8192
#define IG_AUDIO_CHUNK_SIZE   2048
#define IG_EVENT_QUEUE_CNT    16
#define IG_WAITING_TIME       1000 //ms

struct image_generation_manage ig_mgr = {
    .key_triggered      = 0,
    .auadc_model_start  = 0,
    .time_stamp         = UINT64_MAX
};

extern void *llm_mem_alloc(uint32 size);
extern void llm_mem_free(void *ptr);

void *llm_malloc(uint32 size) {return llm_mem_alloc(size);}
void  llm_free(void *ptr) {llm_mem_free(ptr);}
void *llm_zalloc(size_t size) {
    void *ptr = llm_malloc(size);
    if (ptr) {
        os_memset(ptr, 0, size);
    }
    return ptr;
}
void *llm_calloc(size_t nmemb, size_t size) {return llm_zalloc(nmemb * size);}
void *llm_realloc(void *ptr, size_t size) {
    void *nptr = llm_malloc(size);
    if (nptr) {
        if(ptr){
            os_memcpy(nptr, ptr, size);
            llm_free(ptr);
        }
    }
    return nptr;
}
char *llm_strdup(const char *s) {
    size_t len;
    char *d;

    if (s == NULL) {
        return NULL;
    }

    len = os_strlen(s);
    d = llm_malloc(len + 1);
    if (d == NULL) {
        return NULL;
    }
    os_memcpy(d, s, len);
    d[len] = '\0';
    return d;
}

int32 ig_stt_event_cb(void *session, uint16 evt, uint32 param1, uint32 param2)
{
    uint8 process = 0;
    struct ig_event_msg event_msg = {
        .type       = IG_SESSION_STT,
        .event      = evt,
        .msg        = NULL,
        .msg_len    = 0,
    };

    switch (evt) {
        case LLM_EVENT_ERROR_MSG:
            event_msg.msg = llm_strdup((const char *)param1);
            if (event_msg.msg == NULL) {
                os_printf("stt err msg no memory!\r\n");
                break;
            }
            event_msg.msg_len = param2;
            process = 1;
            break;
        case LLM_EVENT_STT_RESULT:
            event_msg.msg = llm_strdup((const char *)param1);
            if (event_msg.msg == NULL) {
                os_printf("stt result no memory!\r\n");
                break;
            }
            event_msg.msg_len = param2;
            process = 1;
            break;
        case LLM_EVENT_CONN_ERR:
        case LLM_EVENT_TX_ERR:
        case LLM_EVENT_RX_ERR:
            process = 1;
        default:
            break;
    }
    if (process) {
        RB_INT_SET(&ig_mgr.event_queue, event_msg);
    }
    return RET_OK;
}

int32 ig_tti_event_cb(void *session, uint16 evt, uint32 param1, uint32 param2)
{
    uint8 process = 0;
    struct ig_event_msg event_msg = {
        .type       = IG_SESSION_TTI,
        .event      = evt,
        .msg        = NULL,
        .msg_len    = 0,
    };

    switch (evt) {
        case LLM_EVENT_ERROR_MSG:
            event_msg.msg = llm_strdup((const char *)param1);
            if (event_msg.msg == NULL) {
                os_printf("tti err msg no memory!\r\n");
                break;
            }
            event_msg.msg_len = param2;
            process = 1;
            break;
        case LLM_EVENT_TTI_RESULT:
            event_msg.msg = llm_malloc(param2);
            if (event_msg.msg == NULL) {
                os_printf("tti result no memory!\r\n");
                break;
            }
            os_memcpy(event_msg.msg, param1, param2);
            event_msg.msg_len = param2;
            process = 1;
            break;
        case LLM_EVENT_CONN_ERR:
        case LLM_EVENT_TX_ERR:
        case LLM_EVENT_RX_ERR:
            process = 1;
        default:
            break;
    }
    if (process) {
        RB_INT_SET(&ig_mgr.event_queue, event_msg);
    }
    return RET_OK;
}

////////////////////////////////////////////
const struct llm_model models[] = {
    {"doubao_asr_stt",  &doubao_asr_stt},
    {"doubao_llm_tti",  &doubao_llm_tti}
};

struct llm_global_param global = {
    .task_reuse     = 1,
    .model_count    = 2,
    .models         = models,
};

//STT
struct llm_stt_sparam ig_stt_session_cfg = {
    .qmsg_tx_cnt            = 32,
    .cb_max_size            = 2048,
    .evt_cb                 = ig_stt_event_cb,
};
struct Doubao_ASR_STT_Cfg ig_stt_platform_cfg = {
    .url = STT_URL,
    .app_appid = STT_APPID,
    .app_token = STT_TOKEN,
    .app_cluster = STT_CLUSTER,
    .user_uid = "41494922",
    .audio_format = "raw",
    .audio_codec = "raw",
    .audio_rate = "16000",
    .audio_bits = "16",
    .audio_channel = "1",
    .boosting_table_name = NULL,
    .correct_table_name = NULL
};
struct llm_trans_param ig_stt_trans_cfg = {
    .buffersize         = 2048,
    .upload_buffersize  = 2048,
    .low_speed_limit    = 10,
    .low_speed_time     = 5,
    .connect_timeout    = 5,
    .blob_len           = 0,
    .blob_data          = NULL
};

//TTI
struct llm_tti_sparam ig_tti_session_cfg = {
    .qmsg_tx_cnt            = 16,
    .evt_cb                 = ig_tti_event_cb,
};
struct Doubao_LLM_TTI_Cfg ig_tti_platform_cfg = {
    .url = TTI_URL,
    .ark_api_key = ARK_API_KEY,
    .model = ENDPOINT_ID,
    .size = "512x512",
    .seed = "1",
    .watermark = "false",
    .guidance_scale = "2.5"
};
struct llm_trans_param ig_tti_trans_cfg = {
    .buffersize         = 16384,
    .upload_buffersize  = 2048,
    .low_speed_limit    = 10,
    .low_speed_time     = 5,
    .connect_timeout    = 5,
    .blob_len           = 0,
    .blob_data          = NULL
};

/* -------------------------- 推屏代码 -------------------------- */
static int ig_tti_opcode_func(stream *s, void *priv, int opcode)
{
    int res = 0;
    switch (opcode) {
        case STREAM_OPEN_EXIT: {
            stream_data_dis_mem(s, 3);
            streamSrc_bind_streamDest(s, SR_OTHER_JPG_USB1);
        }
        break;
        case STREAM_DATA_DIS: {
            struct data_structure *data = (struct data_structure *)priv;
            //注册对应函数
            data->ops = NULL;
            data->data = NULL;
        }
        break;
        case STREAM_DATA_FREE: {
            //释放data->data里面的图片数据
            struct data_structure *data = (struct data_structure *)priv;
            llm_free(data->data);
            data->data = NULL;
        }
        break;
        default:
            //默认都返回成功
            break;
    }
    return res;
}

/* -------------------------- mic代码 -------------------------- */
static int32 ig_stt_opcode_func(stream *s, void *priv, int opcode)
{
    int32 res = 0;
    switch (opcode) {
        case STREAM_OPEN_ENTER:
            break;
        case STREAM_OPEN_EXIT: {
            enable_stream(s, 1);
        }
        break;
        case STREAM_OPEN_FAIL:
            break;
        default:
            break;
    }
    return res;
}

/* -------------------------- mp3代码 -------------------------- */
static int32 ig_mp3_read_func(void *ptr, uint32_t size)
{
    int32 read_len = 0;
    while (1) {
        read_len = rbuffer_get(&ig_mgr.sound_rb, ptr, size);
        if (read_len <= 0) {
            os_sleep_ms(10);
        } else {
            break;
        }
    }
    return read_len;
}

static void ig_audio_play_mp3(const char *audio, uint32 audio_len)
{
    uint32 written = 0;
    uint32 to_write = 0;
    uint32 wait_cnt = 0;

    while (written < audio_len) {
        to_write = (audio_len - written > IG_AUDIO_CHUNK_SIZE) ? IG_AUDIO_CHUNK_SIZE : (audio_len - written);
        if (RB_IDLE(&ig_mgr.sound_rb) < to_write) {
            os_sleep_ms(10);
            wait_cnt++;
            if (wait_cnt == 10) {
                wait_cnt = 0;
                os_printf("Waiting for the application to read audio!(%d:%d)\r\n", RB_IDLE(&ig_mgr.sound_rb), to_write);
            }
        } else {
            rbuffer_set(&ig_mgr.sound_rb, (void *)audio + written, to_write);
            written += to_write;
        }
    }
}

static void ig_audio_play_prompt_tone(const char *audio, uint32 audio_len)
{
    audac_disable_play();
    while (!audac_wait_empty()) {
        os_sleep_ms(1);
    }
    audac_enable_play();

    ig_audio_play_mp3(audio, audio_len);

    while (!audac_wait_empty()) {
        os_sleep_ms(1);
    }
}

/* -------------------------- 按键代码 -------------------------- */
uint32_t ig_intercom_push_key(struct key_callback_list_s *callback_list, uint32_t keyvalue, uint32_t extern_value)
{
    static uint8 key_state = 0;
    if ((keyvalue >> 8) != AD_PRESS)
    { return 0; }
    uint32 key_val = (keyvalue & 0xff);
    if ((key_val == KEY_EVENT_LDOWN) || (key_val == KEY_EVENT_REPEAT)) {
        if (key_state == 0) {
            os_printf("key start\r\n");
            key_state = 1;
            ig_mgr.key_triggered = 1;
        }
    } else if ((key_val == KEY_EVENT_LUP)) {
        if (key_state == 1) {
            os_printf("key stop\r\n");
            key_state = 0;
            ig_mgr.key_triggered = 0;
        }
    }
    return 0;
}

static int32 ig_event_msg_process(void)
{
    int send_ret = 0;
    struct data_structure  *data_s = NULL;
    struct ig_event_msg event_msg = {0};

    RB_INT_GET(&ig_mgr.event_queue, event_msg);
    if (event_msg.event == LLM_EVENT_UNKNOWN) { return RET_OK; }

    switch (event_msg.event) {
        case LLM_EVENT_TTI_RESULT: {
            ig_mgr.time_stamp = UINT64_MAX;
            os_printf("IG TTI: %d\r\n", event_msg.msg_len);
            data_s = get_src_data_f(ig_mgr.screen_stream);
            if (data_s) {
                data_s->data = (void *)event_msg.msg;
                data_s->len = event_msg.msg_len;
                data_s->ref = 0;

                data_s->type = SET_DATA_TYPE(JPEG, JPEG_FULL);
                send_ret = send_data_to_stream(data_s);
                data_s = NULL;
            } else {
                os_printf("Failed to obtain data_s\r\n");
                llm_free(event_msg.msg);
            }
            break;
        }
        case LLM_EVENT_STT_RESULT: {
            os_printf("IG STT(%d):", event_msg.msg_len);
            hgprintf_out(event_msg.msg, event_msg.msg_len, 0);
            _os_printf("\r\n");
            llm_tti_send(ig_mgr.tti_session, LLM_DATA_TYPE_TEXT, LLM_DATA_STATE_START, NULL, 0);
            llm_tti_send(ig_mgr.tti_session, LLM_DATA_TYPE_TEXT, LLM_DATA_STATE_END, event_msg.msg, event_msg.msg_len);
            llm_free(event_msg.msg);
            break;
        }
        case LLM_EVENT_ERROR_MSG: {
            if (event_msg.type == IG_SESSION_STT) {
                os_printf("IG recv STT err msg(%d):", event_msg.msg_len);
            } else if (event_msg.type == IG_SESSION_TTI) {
                os_printf("IG recv TTI err msg(%d):", event_msg.msg_len);
            } else {
                os_printf("IG recv UNKNOWN err msg(%d):", event_msg.msg_len);
            }
            hgprintf_out(event_msg.msg, event_msg.msg_len, 0);
            _os_printf("\r\n");
            llm_free(event_msg.msg);
            // TODO
            break;
        }
        case LLM_EVENT_CONN_ERR:
        case LLM_EVENT_TX_ERR:
        case LLM_EVENT_RX_ERR: {
            if (event_msg.type == IG_SESSION_STT) {
                os_printf("IG STT %s err!\r\n", event_msg.event == LLM_EVENT_CONN_ERR ? "CONN" : (event_msg.event == LLM_EVENT_TX_ERR ? "TX" : "RX"));
                //llm_stt_interrupt(ig_mgr.stt_session, 0);
            } else if (event_msg.type == IG_SESSION_TTI) {
                os_printf("IG TTI %s err!\r\n", event_msg.event == LLM_EVENT_CONN_ERR ? "CONN" : (event_msg.event == LLM_EVENT_TX_ERR ? "TX" : "RX"));
                //llm_tti_interrupt(ig_mgr.tti_session, 0);
            } else {
                os_printf("IG UNKNOWN %s err!\r\n", event_msg.event == LLM_EVENT_CONN_ERR ? "CONN" : (event_msg.event == LLM_EVENT_TX_ERR ? "TX" : "RX"));
            }
            ig_mgr.time_stamp = UINT64_MAX;
            ig_mgr.auadc_model_start = 0;
            break;
        }
        default: {
            os_printf("Not supported event msg: %d\r\n", event_msg.event);
            break;
        }
    }
    return RET_OK;
}

sysevt_hdl_res ig_wifi_event(uint32 event_id, uint32 data, uint32 priv)
{
    switch (event_id & 0xffff) {
        case SYSEVT_WIFI_DISCONNECT:
            os_printf("**网络异常!**\r\n", event_id);
            ig_audio_play_prompt_tone(yiduankai, yiduankai_size);
            ig_mgr.auadc_model_start = 0;
            ig_mgr.time_stamp = UINT64_MAX;
            llm_stt_interrupt(ig_mgr.stt_session, 0);
            llm_tti_interrupt(ig_mgr.tti_session, 0);
            break;
        case SYSEVT_WIFI_CONNECTTED:
            break;
    }
    return SYSEVT_CONTINUE;
}

sysevt_hdl_res ig_network_event(uint32 event_id, uint32 data, uint32 priv)
{
    struct netif *nif;

    switch (event_id) {
        case SYS_EVENT(SYS_EVENT_NETWORK, SYSEVT_LWIP_DHCPC_DONE):
            nif = netif_find("w0");
            gethostbyname_async("openspeech.bytedance.com");
            gethostbyname_async("ark.cn-beijing.volces.com");
            break;
    }
    return SYSEVT_CONTINUE;
}

static int32 ig_stt_app_init(char *llm_name)
{
    int32 ret = RET_OK;

    ig_mgr.stt_session = llm_stt_init(llm_name, &ig_stt_session_cfg);
    if (!ig_mgr.stt_session) {
        os_printf("stt session init fail!\r\n");
        return RET_ERR;
    }
    ret = llm_stt_config(ig_mgr.stt_session, LLM_CONFIG_TYPE_TRANS,
                         (void *)&ig_stt_trans_cfg, sizeof(ig_stt_trans_cfg));
    if (ret) {
        os_printf("stt_trans_cfg fail!\r\n");
        return RET_ERR;
    }
    ret = llm_stt_config(ig_mgr.stt_session, LLM_CONFIG_TYPE_MODEL,
                         (void *)&ig_stt_platform_cfg, sizeof(ig_stt_platform_cfg));
    if (ret) {
        os_printf("asr_stt_cfg fail!\r\n");
        return RET_ERR;
    }
    return ret;
}

static int32 ig_tti_app_init(char *llm_name)
{
    int32 ret = RET_OK;

    ig_mgr.tti_session = llm_tti_init(llm_name, &ig_tti_session_cfg);
    if (!ig_mgr.tti_session) {
        os_printf("tti session init fail!\r\n");
        return RET_ERR;
    }
    ret = llm_tti_config(ig_mgr.tti_session, LLM_CONFIG_TYPE_TRANS,
                         (void *)&ig_tti_trans_cfg, sizeof(ig_tti_trans_cfg));
    if (ret) {
        os_printf("tti_trans_cfg fail!\r\n");
        return RET_ERR;
    }
    ret = llm_tti_config(ig_mgr.tti_session, LLM_CONFIG_TYPE_MODEL,
                         (void *)&ig_tti_platform_cfg, sizeof(ig_tti_platform_cfg));
    if (ret) {
        os_printf("llm_tti_cfg fail!\r\n");
        return RET_ERR;
    }
    return ret;
}

static int32 ig_app_init(void)
{
    int32 ret = RET_OK;
    void *sound_rb_buff = NULL;
    struct ig_event_msg *event_queue_buf = NULL;
    llm_global_init(&global);

    ig_stt_app_init("doubao_asr_stt");
    ig_tti_app_init("doubao_llm_tti");

    sys_event_take(SYS_EVENT(SYS_EVENT_WIFI, 0), ig_wifi_event, 0);
    sys_event_take(SYS_EVENT(SYS_EVENT_NETWORK, 0), ig_network_event, 0);

    add_keycallback(ig_intercom_push_key, NULL);
    mp3_decode_init(NULL, ig_mp3_read_func);

    ig_mgr.mic_stream = open_stream_available(R_SPEECH_RECOGNITION, 0, 8, ig_stt_opcode_func, NULL);
    if (!ig_mgr.mic_stream) {
        os_printf("open mic stream err!\r\n");
        return RET_ERR;
    }

    ig_mgr.screen_stream = open_stream_available(S_NET_JPEG, 3, 0, ig_tti_opcode_func, NULL);
    if (!ig_mgr.screen_stream) {
        os_printf("open screen stream err!\r\n");
        return RET_ERR;
    }

    sound_rb_buff = llm_zalloc(IG_SOUND_RB_SIZE);
    if (!sound_rb_buff) {
        os_printf("sound_rb_buff malloc fail, no memory!\r\n");
        return RET_ERR;
    }
    rbuffer_init(&ig_mgr.sound_rb, IG_SOUND_RB_SIZE, sound_rb_buff);

    event_queue_buf = llm_malloc((IG_EVENT_QUEUE_CNT + 1) * sizeof(struct ig_event_msg));
    if (!event_queue_buf) {
        os_printf("event_queue_buf malloc fail, no memory!\r\n");
        return RET_ERR;
    }
    RB_INIT_R(&ig_mgr.event_queue, IG_EVENT_QUEUE_CNT, event_queue_buf);

    os_printf("Waiting for network connection...");
    while (!sys_status.wifi_connected || !sys_status.dhcpc_done) {
        _os_printf(".");
        os_sleep(1);
    }
    os_printf("Network connected!\r\n");
    ig_audio_play_prompt_tone(yilianjie, yilianjie_size);
    os_sleep_ms(1000);
    return ret;
}

void ig_main(void)
{
    int32 ret = RET_OK;
    uint64 jiff = 0;
    uint8 last_key_triggered = 0;   // 记录上一次按键状态；
    struct data_structure *data_s = NULL;
    char *data = NULL;
    uint32 data_len = 0;
    uint8 last_sta = 0;

    ret = ig_app_init();
    if (ret) {
        os_printf("stt main init failed: %d\r\n", ret);
        goto cleanup;
    }

    os_printf("**开始运行：**\r\n");
    while (1) {
        ig_event_msg_process();
        if (ig_mgr.time_stamp != UINT64_MAX) {
            jiff = os_jiffies() - ig_mgr.time_stamp;
            if (os_jiffies_to_msecs(jiff) > IG_WAITING_TIME) {
                ig_audio_play_prompt_tone(beep, beep_size);
                ig_mgr.time_stamp = os_jiffies();
            }
        }

        if (last_key_triggered != ig_mgr.key_triggered) {
            // 按键按下
            if (ig_mgr.key_triggered == 1) {
                llm_stt_interrupt(ig_mgr.stt_session, 0);
                llm_tti_interrupt(ig_mgr.tti_session, 0);
                llm_stt_send(ig_mgr.stt_session, LLM_DATA_TYPE_AUDIO, LLM_DATA_STATE_START, NULL, 0);
                os_printf("send start audio data!\r\n");
                os_printf("**请说话：**\r\n");
                ig_mgr.auadc_model_start = 1;
            }
            // 按键松开
            else {
                ig_mgr.auadc_model_start = 0;
                ig_audio_play_prompt_tone(fasong, fasong_size);
                os_sleep(1);
                os_printf("**正在生成**\r\n");
                ig_mgr.time_stamp = 0;
            }
            last_key_triggered = ig_mgr.key_triggered;
        }
        data_s = recv_real_data(ig_mgr.mic_stream);
        if (data_s) {
            data = get_stream_real_data(data_s);
            data_len = get_stream_real_data_len(data_s);
            if (ig_mgr.auadc_model_start) {
                llm_stt_send(ig_mgr.stt_session, LLM_DATA_TYPE_AUDIO, LLM_DATA_STATE_MIDDLE, data, data_len);
                os_printf("send audio data(%d)!\r\n", data_len);
            } else if ((last_sta) && (!ig_mgr.auadc_model_start)) {
                llm_stt_send(ig_mgr.stt_session, LLM_DATA_TYPE_AUDIO, LLM_DATA_STATE_END, data, data_len);
                os_printf("send end audio data(%d)!\r\n", data_len);
            }
            last_sta = ig_mgr.auadc_model_start;
            free_data(data_s);
            data_s = NULL;
        }
        os_sleep_ms(10);
    }

cleanup:
    if (ig_mgr.event_queue.rbq) { llm_free(ig_mgr.event_queue.rbq); }
    close_stream(ig_mgr.mic_stream);
    close_stream(ig_mgr.screen_stream);
    llm_stt_deinit(ig_mgr.stt_session);
    llm_tti_deinit(ig_mgr.tti_session);
    llm_global_deinit();
}

struct os_task ig_task;
void image_generation_demo(void)
{
    OS_TASK_INIT("IMAGE_GENERATION_DEMO", &ig_task, ig_main, NULL, OS_TASK_PRIORITY_NORMAL, 2048);
}
#endif

