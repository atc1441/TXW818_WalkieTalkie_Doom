#include "coze_demo.h"
#include "lib/net/llm/llm_trans.h"
#include "sounds.h"
#include "cJSON.h"
#include "devid.h"
#include "osal_file.h"

#ifdef COZE_DEMO
/* -------------------------- 宏 -------------------------- */
/* Please add your server information */
#define COZE_URL        "wss://ws.coze.cn/v1/chat?bot_id="
#define COZE_BOT_ID     "xxx"
#define COZE_PAT_KEY    "xxx"
#define COZE_UPDATE_URL "https://api.coze.cn/v1/files/upload"

#ifndef STRINGIFY
#define STRINGIFY(x) #x
#endif

#ifndef MACRO_TO_STR
#define MACRO_TO_STR(x) STRINGIFY(x)
#endif

#ifndef UINT64_MAX
#define UINT64_MAX 0xffffffffffffffffULL
#endif
#define COZE_DEMO_DIALOGUE_TIMEOUT_MAX_CNT 5
#define COZE_EVENT_QUEUE_CNT    16
#define COZE_IDLE_TIMEOUT       20000
#define SD_CACHE_SIZE           (4*1024)

#define COZE_DEMO_DEC_OPUS_BITRATE          8000
#define COZE_DEMO_DEC_OPUS_FRAME_SIZE_MS    60
#define COZE_DEMO_DEC_OPUS_FRAME_LEN  ((COZE_DEMO_DEC_OPUS_BITRATE * COZE_DEMO_DEC_OPUS_FRAME_SIZE_MS / 1000) / 8)

/* -------------------------- 函数声明 -------------------------- */
extern void *llm_mem_alloc(uint32 size);
extern void llm_mem_free(void *ptr);

void *llm_malloc(uint32 size) {return llm_mem_alloc(size);}
void  llm_free(void *ptr) {llm_mem_free(ptr);}
void *llm_zalloc(size_t size)
{
    void *ptr = llm_malloc(size);
    if (ptr) {
        os_memset(ptr, 0, size);
    }
    return ptr;
}
void *llm_calloc(size_t nmemb, size_t size) {return llm_zalloc(nmemb * size);}
void *llm_realloc(void *ptr, size_t size)
{
    void *nptr = llm_malloc(size);
    if (nptr) {
        if (ptr) {
            os_memcpy(nptr, ptr, size);
            llm_free(ptr);
        }
    }
    return nptr;
}
char *llm_strdup(const char *s)
{
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

static int32 coze_main_event_cb(void *session, uint16 evt, uint32 param1, uint32 param2);
static void coze_main_set_state(coze_demo_state new_state);
static void coze_main_timeout_warn(void);
static void coze_main_timeout_reset(void);
static void coze_main_conversation_id_init();

/* -------------------------- 全局变量 -------------------------- */
/*
 * @brief 模型列表
*/
const struct llm_model models[] = {
    {"coze_sts", (struct llm_model_data *) &coze_sts_model},
};
/*
 * @brief llm 库全局参数
*/
struct llm_global_param global = {
    .task_reuse     = 0,
    .model_count    = 1,
    .models         = models,
};

/*
 * @brief 会话层参数
*/
struct llm_sts_sparam coze_sts_session_cfg = {
    .qmsg_tx_cnt            = 16,       //audio sample msg num
    .rx_buff_size           = 4096,     //audeo recv buff
    .cb_max_size            = 2048,
    .evt_cb                 = coze_main_event_cb,
};

/*
 * @brief 平台层参数
*/
struct coze_chat_platform_cfg coze_sts_platform_cfg = {
    .host_url               = COZE_URL,
    .pat_key                = COZE_PAT_KEY,
    .bot_id                 = COZE_BOT_ID,
    .update_url             = COZE_UPDATE_URL,

    .chat_config_auto_save_history    = "TRUE",

    .input_audio_format     = "pcm",
    .input_audio_codec      = "g711a",

    .output_audio_voice_id      = "7426725529589596187",
    .output_audio_limit_config_max_frame_num    = "32",
    .output_audio_codec                         = "opus",
    .output_audio_opus_config_bitrate           = MACRO_TO_STR(COZE_DEMO_DEC_OPUS_BITRATE),
    .output_audio_opus_config_use_cbr           = "true",
    .output_audio_opus_config_sample_rate       = "8000",
    .output_audio_opus_config_frame_size_ms     = MACRO_TO_STR(COZE_DEMO_DEC_OPUS_FRAME_SIZE_MS),
};

/*
 * @brief 传输层参数
*/
struct llm_trans_param coze_sts_trans_cfg = {
    .buffersize         = 2048,
    .upload_buffersize  = 2048,
    .low_speed_limit    = 10,
    .low_speed_time     = 5,
    .connect_timeout    = 10,
    .blob_len           = 0,
    .blob_data          = NULL,
};

/*
 * @brief 应用全局管理结构体
*/
struct coze_demo_manage coze_mgr = {
    .key_triggered      = 0,
    .voice_triggered    = 0,
    .finish             = 0,
    .connected          = 0,
    .send_audio_cnt     = 0,
    .time_stamp         = UINT64_MAX,
};

#ifdef COZE_DEMO_URL_PLAY
static const struct ufprotocol coze_ufprotos[] = {
    {"http", &uf_curl_http},
};
#endif
/* -------------------------- 音频代码 -------------------------- */
/*
 * @brief 获取音频数据长度
 * @param data 音频数据指针
 * @return 音频数据长度
*/
static uint32_t coze_audio_get_sound_data_len(void *data)
{
    struct data_structure  *d = (struct data_structure *)data;
    return (uint32_t)d->priv;
}

/*
 * @brief 设置音频数据长度
 * @param data 音频数据指针
 * @param len 音频数据长度
 * @return 音频数据长度
*/
static uint32_t coze_audio_set_sound_data_len(void *data, uint32_t len)
{
    struct data_structure  *d = (struct data_structure *)data;
    d->priv = (void *)len;
    return (uint32_t)len;
}

static const stream_ops_func stream_sound_ops = {
    .get_data_len = coze_audio_get_sound_data_len,
    .set_data_len = coze_audio_set_sound_data_len,
};

/*
 * @brief 处理音频播放操作码
 * @param s 音频流指针
 * @param priv 私有数据指针
 * @param opcode 操作码
 * @return 操作结果，0表示成功，其他值表示失败
*/
static int coze_audio_play_opcode_func(stream *s, void *priv, int opcode)
{
    static uint8_t *audio_buf = NULL;
    int res = 0;
    switch (opcode) {
        case STREAM_OPEN_EXIT: {
            audio_buf = llm_malloc(3 * 60 * 8 * 2);  //帧长 * 每毫秒采样率 * 两个byte
            if (audio_buf) {
                stream_data_dis_mem(s, 3);
            }
            streamSrc_bind_streamDest(s, R_SPEAKER);
        }
        break;
        case STREAM_DATA_DIS: {
            struct data_structure *data = (struct data_structure *)priv;
            int data_num = (int)data->priv;
            data->ops = (stream_ops_func *)&stream_sound_ops;
            data->data = audio_buf + (data_num) * 60 * 16;
        }
        break;
        case STREAM_DATA_DESTORY: {
            if (audio_buf) {
                llm_free(audio_buf);
                audio_buf = NULL;
            }
        }
        case STREAM_DATA_FREE:
            //_os_printf("%s:%d\n",__FUNCTION__,__LINE__);
            break;


        //数据发送完成,可以选择唤醒对应的任务
        case STREAM_RECV_DATA_FINISH:
            break;

        default:
            //默认都返回成功
            break;
    }
    return res;
}

/*
 * @brief 音频播放线程函数
 * @param arg 线程参数指针
 * @return 无
*/
void coze_audio_play_main(void *arg)
{
    struct data_structure *data_s = NULL;
    stream *s = NULL;
    OpusDecoder *decoder = NULL;
    uint32_t dec_len = 0;
    int32 recv_len = 0;
    char *audio_buf = llm_malloc(COZE_DEMO_DEC_OPUS_FRAME_LEN);


    if (!audio_buf) {
        os_printf("audio_buf alloc fail!\r\n");
        goto cleanup;
    }

    s = open_stream_available("file_audio", 3, 0, coze_audio_play_opcode_func, NULL);
    audio_dac_set_filter_type(SOUND_FILE);

    int err = 0;;
    if (!(decoder = opus_decoder_create(8000, 1, &err))) {
        os_printf("Opus decoder init failed: %d\n", err);
        goto cleanup;
    }

    while (1) {
        os_sleep_ms(10);

        /* 数据获取阶段 */
        data_s = get_src_data_f(s);
        if (data_s) {
            void *pcm_data = get_stream_real_data(data_s);
            if (!pcm_data) {
                os_printf("Invalid pcm data buffer\n");
                goto data_cleanup;
            }

            recv_len = llm_sts_recv(coze_mgr.sts_session, audio_buf, COZE_DEMO_DEC_OPUS_FRAME_LEN);
            if (recv_len == COZE_DEMO_DEC_OPUS_FRAME_LEN) {
                /* 音频解码阶段 */
                if ((dec_len = opus_decode(decoder, (const unsigned char *)audio_buf, recv_len,
                                           pcm_data, 60 * 8, 0)) <= 0) {
                    os_printf("Decode error: %d\n", dec_len);
                    goto data_cleanup;
                }
                /* 数据发送阶段 */
                data_s->type = SET_DATA_TYPE(SOUND, SOUND_FILE);
                coze_audio_set_sound_data_len(data_s, 60 * 8 * 2);
                send_data_to_stream(data_s);
                data_s = NULL;
                continue;
            } else {
                if (recv_len != LLME_AGAIN) {
                    os_printf("audio recv fail, ret=%d\r\n", recv_len);
                }
            }
data_cleanup:
            force_del_data(data_s);
            data_s = NULL;
        }
    }

cleanup:
    if (audio_buf) { llm_free(audio_buf); }
    if (decoder) { opus_decoder_destroy(decoder); }
}

/*
 * @brief 播放提示音
 * @param audio 音频数据指针
 * @param audio_len 音频数据长度
 * @param frame_len 音频帧长度
*/
static void coze_audio_play_prompt_tone(const char *audio, uint32 audio_len)
{
    llm_sts_play_audio(coze_mgr.sts_session, (char *)audio, audio_len);
    while (!audac_wait_empty()) {
        os_sleep_ms(1);
    }
}

/*
 * @brief 释放音频数据流
 * @param s 音频数据流指针
*/
void coze_audio_free_sample_stream(stream *s)
{
    struct data_structure *data_s = NULL;
    do {
        data_s = recv_real_data(s);
        if (data_s) {
            free_data(data_s);
        }
    } while (data_s != NULL);
}

/*
 * @brief 发送音频数据到服务器
 * @param s 音频数据流指针
 * @param aualaw_dev 音频编码设备指针
 * @return 发送结果，0表示成功，其他值表示失败
*/
int32 coze_audio_send_data(stream *s, struct aualaw_device *aualaw_dev)
{
    int32 ret = RET_OK;
    static uint32 timeout = 0;

    struct data_structure *data_s = NULL;
    char *data = NULL;
    uint32 data_len = 0;

    data_s = recv_real_data(s);
    if (data_s) {
        data = get_stream_real_data(data_s);
        data_len = get_stream_real_data_len(data_s);
        if (!coze_mgr.aualaw_buf)
        { coze_mgr.aualaw_buf = (char *)llm_malloc(data_len / 2); }
#ifdef PSRAM_HEAP
        if (!coze_mgr.psram_copy_buff) {
            coze_mgr.psram_copy_buff = llm_malloc(data_len);
        }
        if (coze_mgr.psram_copy_buff) {
            os_memcpy(coze_mgr.psram_copy_buff, data, data_len);
            ret = aualaw_encode(aualaw_dev, coze_mgr.psram_copy_buff, data_len, coze_mgr.aualaw_buf);
        }
#else
        ret = aualaw_encode(aualaw_dev, data, data_len, coze_mgr.aualaw_buf);
#endif
        if (ret != 0) {
            os_printf("aualaw encode err!\n");
            free_data(data_s);
            data_s = NULL;
            return RET_ERR;
        }

        if (data && coze_mgr.aualaw_buf) {
            ret = llm_sts_send(coze_mgr.sts_session, LLM_DATA_TYPE_AUDIO, LLM_DATA_STATE_MIDDLE, coze_mgr.aualaw_buf, data_len / 2);
            //os_printf("send audio len:%d\r\n", data_len / 2);
            if (ret == LLME_AGAIN) {
                timeout++;
                if (timeout == 100) {
                    coze_main_timeout_warn();
                }
                if (timeout > 500) {
                    timeout = 0;
                    coze_main_timeout_reset();
                }
            } else if (ret == RET_OK) {
                timeout = 0;
                coze_mgr.send_audio_cnt++;
            } else {
                timeout = 0;
                os_printf("send audio err!\n");
                coze_main_set_state(COZE_DEMO_STATE_IDLE);
                llm_sts_reconnect(coze_mgr.sts_session);
            }
        }
        free_data(data_s);
        data_s = NULL;
    }
    return ret;
}

/*
 * @brief 处理音频流操作码
 * @param s 音频流指针
 * @param priv 私有数据指针
 * @param opcode 操作码
 * @return 操作结果，0表示成功，其他值表示失败
*/
static int32 coze_audio_opcode_func(stream *s, void *priv, int opcode)
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

static int coze_screen_opcode_func(stream *s, void *priv, int opcode)
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

/* -------------------------- 主要代码 -------------------------- */
const char *coze_main_state_str(coze_demo_state state)
{
    switch (state) {
        case COZE_DEMO_STATE_IDLE:
            return "IDLE";
        case COZE_DEMO_STATE_CONNECTED:
            return "CONNECTED";
        case COZE_DEMO_STATE_DISCONNECTED:
            return "DISCONNECTED";
        case COZE_DEMO_STATE_READY:
            return "READY";
        case COZE_DEMO_STATE_WAITING:
            return "WAITING";
        case COZE_DEMO_STATE_PUSHING:
            return "PUSHING";
        default:
            return "INVALID";
    }
    return "INVALID";
}

/*
 * @brief 获取当前状态
 * @return 当前状态
*/
static coze_demo_state coze_main_get_state(void)
{
    coze_demo_state state = COZE_DEMO_STATE_IDLE;
    os_mutex_lock(&coze_mgr.lock, osWaitForever);
    state = coze_mgr.state;
    os_mutex_unlock(&coze_mgr.lock);
    return state;
}

/*
 * @brief 设置当前状态
 * @param new_state 新状态
*/
static void coze_main_set_state(coze_demo_state new_state)
{
    int32 changed = 0;
    if (coze_mgr.state == new_state) { return; }
    os_mutex_lock(&coze_mgr.lock, osWaitForever);
    switch (new_state) {
        case COZE_DEMO_STATE_IDLE:
            changed = 1;
            break;
        case COZE_DEMO_STATE_CONNECTED:
            changed = 1;
            break;
        case COZE_DEMO_STATE_DISCONNECTED:
            coze_mgr.time_stamp = UINT64_MAX;
            changed = 1;
            break;
        case COZE_DEMO_STATE_READY:
            if (coze_mgr.connected) {
                changed = 1;
            }
            break;
        case COZE_DEMO_STATE_WAITING:
            coze_mgr.time_stamp = UINT64_MAX;
            if (coze_mgr.connected) {
                changed = 1;
            }
            break;
        case COZE_DEMO_STATE_PUSHING:
            if (coze_mgr.connected) {
                changed = 1;
            }
            break;
        default:
            break;
    }
    if (changed) {
        os_printf("New State:%s -> %s\r\n", coze_main_state_str(coze_mgr.state), coze_main_state_str(new_state));
        coze_mgr.state = new_state;
    } else {
        os_printf("invalid state: %s -> %s\r\n", coze_main_state_str(coze_mgr.state), coze_main_state_str(new_state));
    }
    os_mutex_unlock(&coze_mgr.lock);
}

/*
 * @brief 超时警告函数
 * @param 无
*/
static void coze_main_timeout_warn(void)
{
    os_printf("**当前网络较差，请稍等!**\n");
    coze_audio_play_prompt_tone(wangluocha, wangluocha_size);
    os_sleep_ms(3000);
}

/*
 * @brief 超时重连函数
 * @param 无
*/
static void coze_main_timeout_reset(void)
{
    os_printf("**网络超时，即将重连!**\n");
    coze_audio_play_prompt_tone(chaoshi, chaoshi_size);
    os_sleep_ms(3000);
    coze_main_set_state(COZE_DEMO_STATE_IDLE);
    llm_sts_reconnect(coze_mgr.sts_session);
}

/*
 * @brief 等待进入监听函数
 * @param 无
 * @return 操作结果，0表示成功，其他值表示失败
*/
static int32 coze_main_waiting(void)
{
    int32 ret = RET_OK;
    uint32 timeout = 0;
    do {
        ret = llm_sts_wait(coze_mgr.sts_session, 500);
        if (ret == RET_ERR) {
            os_printf("This can be confirmed to be a disconnection!\r\n");
            break;
        } else if (ret == LLME_WAIT_TIMEOUT) {
            timeout++;
            if (timeout == 6) {
                coze_main_timeout_warn();
            }
            if (timeout > 10) {
                coze_main_timeout_reset();
                break;
            }
        }
    } while (ret == LLME_WAIT_TIMEOUT);
    return ret;
}

/*
 * @brief 事件回调函数(不可阻塞)
 * @param session 会话指针
 * @param evt 事件类型
 * @param param1 事件参数1
 * @param param2 事件参数2
 * @return 操作结果，0表示成功，其他值表示失败
*/
int32 coze_main_event_cb(void *session, uint16 evt, uint32 param1, uint32 param2)
{
    int ret = RET_OK;
    uint8 process = 0;
    struct coze_demo_event_msg event_msg = {
        .event      = evt,
        .msg        = NULL,
        .msg_len    = 0,
    };
    switch (evt) {
        case LLM_EVENT_CONNECTED: {
            process = 1;
            break;
        }
        case LLM_EVENT_DISCONNECT: {
            process = 1;
            break;
        }
        case LLM_EVENT_STT_RESULT: {
            event_msg.msg = llm_strdup((const char *)param1);
            if (event_msg.msg == NULL) {
                os_printf("stt result no memory!\r\n");
                break;
            }
            event_msg.msg_len = param2;
            process = 1;
            break;
        }
        case LLM_EVENT_TTS_RESULT: {
            event_msg.msg = llm_strdup((const char *)param1);
            if (event_msg.msg == NULL) {
                os_printf("tts result no memory!\r\n");
                break;
            }
            event_msg.msg_len = param2;
            process = 1;
            break;
        }
        case LLM_EVENT_VAD_RESULT: {
            event_msg.msg = llm_strdup((const char *)(param1 == COZE_SERVER_VAD_START ? "START" : "STOP"));
            if (event_msg.msg == NULL) {
                os_printf("vad result no memory!\r\n");
                break;
            }
            event_msg.msg_len = os_strlen(event_msg.msg);
            process = 1;
            break;
        }
        case LLM_EVENT_DIALOGUE_END: {
            process = 1;
            break;
        }
        case LLM_EVENT_DIALOGUE_TIMEOUT: {
            process = 1;
            break;
        }
        case LLM_EVENT_ERROR_MSG: {
            event_msg.msg = llm_strdup((const char *)param1);
            if (event_msg.msg == NULL) {
                os_printf("err msg no memory!\r\n");
                break;
            }
            event_msg.msg_len = param2;
            process = 1;
            break;
        }
        case LLM_EVENT_CUSTOMIZE: {
            event_msg.msg = llm_strdup((const char *)param1);
            if (event_msg.msg == NULL) {
                os_printf("customize no memory!\r\n");
                break;
            }
            event_msg.msg_len = param2;
            process = 1;
            break;
        }
        case LLM_EVENT_UPLOAD_FILE_RESULT: {
            event_msg.msg = llm_strdup((const char *)param1);
            if (event_msg.msg == NULL) {
                os_printf("customize no memory!\r\n");
                break;
            }
            event_msg.msg_len = param2;
            process = 1;
            break;
        }
        case LLM_EVENT_TTI_RESULT: {
            event_msg.msg = llm_strdup((const char *)param1);
            if (event_msg.msg == NULL) {
                os_printf("customize no memory!\r\n");
                break;
            }
            event_msg.msg_len = param2;
            process = 1;
            break;
        }
        case LLM_EVENT_CONVERSATION_ID: {
            event_msg.msg = llm_strdup((const char *)param1);
            if (event_msg.msg == NULL) {
                os_printf("customize no memory!\r\n");
                break;
            }
            event_msg.msg_len = param2;
            process = 1;
            break;
        }
        default:
            break;
    }
    if (process) {
        RB_INT_SET(&coze_mgr.event_queue, event_msg);
    }
    return ret;
}

/*
 * @brief 系统无线事件回调函数
 * @param event_id 事件ID
 * @param data 事件数据
 * @param priv 私有数据
 * @return 操作结果，0表示成功，其他值表示失败
*/
sysevt_hdl_res coze_main_wifi_event(uint32 event_id, uint32 data, uint32 priv)
{
    switch (event_id & 0xffff) {
        case SYSEVT_WIFI_DISCONNECT:
            os_printf("**网络异常!**\r\n", event_id);
            break;
        case SYSEVT_WIFI_CONNECTTED:
            coze_main_set_state(COZE_DEMO_STATE_IDLE);
            llm_sts_reconnect(coze_mgr.sts_session);
            break;
    }
    return SYSEVT_CONTINUE;
}

/*
 * @brief 系统网络事件回调函数
 * @param event_id 事件ID
 * @param data 事件数据
 * @param priv 私有数据
 * @return 操作结果，0表示成功，其他值表示失败
*/
sysevt_hdl_res coze_main_network_event(uint32 event_id, uint32 data, uint32 priv)
{
    struct netif *nif;

    switch (event_id) {
        case SYS_EVENT(SYS_EVENT_NETWORK, SYSEVT_LWIP_DHCPC_DONE):
            nif = netif_find("w0");
            gethostbyname_async("ws.coze.cn");
            gethostbyname_async("lf-bot-studio-plugin-resource.coze.cn");
            if (coze_mgr.ip_addr != nif->ip_addr.addr) {
                coze_mgr.ip_addr = nif->ip_addr.addr;
                coze_main_set_state(COZE_DEMO_STATE_IDLE);
                llm_sts_reconnect(coze_mgr.sts_session);
            }
            break;
    }
    return SYSEVT_CONTINUE;
}

/*
 * @brief 应用初始化函数
 * @param llm_name LLM名称
 * @return 操作结果，0表示成功，其他值表示失败
*/
struct os_task coze_awaken_task;
struct os_task coze_audio_play_task;
int32 coze_main_app_init(char *llm_name)
{
    int ret = 0;
    struct coze_demo_event_msg *event_queue_buf = NULL;
    llm_global_init(&global);

    coze_mgr.sts_session = llm_sts_init(llm_name, &coze_sts_session_cfg);
    if (!coze_mgr.sts_session) {
        os_printf("sts session init fail!\r\n");
        return RET_ERR;
    }

    ret = llm_sts_config(coze_mgr.sts_session, LLM_CONFIG_TYPE_TRANS,
                         (void *)&coze_sts_trans_cfg, sizeof(coze_sts_trans_cfg));
    if (ret) {
        os_printf("sts_trans_cfg fail!\r\n");
        return RET_ERR;
    }

    coze_sts_platform_cfg.turn_detection_type = "client_interrupt";
    ret = llm_sts_config(coze_mgr.sts_session, LLM_CONFIG_TYPE_MODEL,
                         (void *)&coze_sts_platform_cfg, sizeof(coze_sts_platform_cfg));
    if (ret) {
        os_printf("sts_trans_cfg fail!\r\n");
        return RET_ERR;
    }

    add_keycallback(coze_main_intercom_push_key, NULL);

    coze_mgr.mic_stream = open_stream_available(R_SPEECH_RECOGNITION, 0, 8, coze_audio_opcode_func, NULL);
    if (!coze_mgr.mic_stream) {
        os_printf("open speech_recognition stream err!\r\n");
        return RET_ERR;
    }

    coze_mgr.screen_stream = open_stream_available(S_NET_JPEG, 3, 0, coze_screen_opcode_func, NULL);
    if (!coze_mgr.screen_stream) {
        os_printf("open screen stream err!\r\n");
        return RET_ERR;
    }

    coze_mgr.aualaw_dev = (struct aualaw_device *)dev_get(HG_AUALAW_DEVID);
    ret = aualaw_open(coze_mgr.aualaw_dev);
    if (ret != 0) {
        os_printf("aualaw open err!\n");
        return RET_ERR;
    }

    cJSON_Hooks hooks = {0};
    hooks.malloc_fn = llm_malloc;
    hooks.free_fn = llm_free;
    cJSON_InitHooks(&hooks);

    os_mutex_init(&coze_mgr.lock);

#ifdef COZE_DEMO_URL_PLAY
    urlfile_init(coze_ufprotos, ARRAY_SIZE(coze_ufprotos));
#endif

    event_queue_buf = llm_malloc((COZE_EVENT_QUEUE_CNT + 1) * sizeof(struct coze_demo_event_msg));
    if (!event_queue_buf) {
        os_printf("event_queue_buf malloc fail, no memory!\r\n");
        return RET_ERR;
    }
    RB_INIT_R(&coze_mgr.event_queue, COZE_EVENT_QUEUE_CNT, event_queue_buf);

#ifdef LLM_SPV12XX
    OS_TASK_INIT("COZE_AWAKEN", &coze_awaken_task, coze_awaken_main, NULL, OS_TASK_PRIORITY_NORMAL + 1, 1024);
#endif
    OS_TASK_INIT("COZE_AUPLAY", &coze_audio_play_task, coze_audio_play_main, NULL, OS_TASK_PRIORITY_NORMAL, 8192);

    coze_main_conversation_id_init();

    coze_audio_play_prompt_tone(kaiji, kaiji_size);
    os_sleep_ms(1000);

    os_printf("Waiting for network connection...");
    while (!sys_status.wifi_connected || !sys_status.dhcpc_done) {
        _os_printf(".");
        os_sleep(1);
    }
    os_printf("Network connected!\r\n");
    struct netif *nif = netif_find("w0");
    coze_mgr.ip_addr = nif->ip_addr.addr;

    sys_event_take(SYS_EVENT(SYS_EVENT_WIFI, 0), coze_main_wifi_event, 0);
    sys_event_take(SYS_EVENT(SYS_EVENT_NETWORK, 0), coze_main_network_event, 0);

    coze_main_set_state(COZE_DEMO_STATE_IDLE);
    llm_sts_reconnect(coze_mgr.sts_session);
    return ret;
}

/*
 * @brief: 处理Coze AI错误消息
 * @param: char *error 错误消息
 * @return: int32
*/
static int32 coze_main_error_process(char *error)
{
    int32 err_code = 0;
    char *err_msg = NULL;

    os_printf("**设备异常**\n");
    if (error) {
        cJSON *root = cJSON_Parse(error);
        if (root == NULL) {
            os_printf("Error before: %s\n", error);
            return RET_ERR;
        }

        cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
        if (cJSON_IsObject(data)) {
            cJSON *code_item = cJSON_GetObjectItemCaseSensitive(data, "code");
            cJSON *msg_item = cJSON_GetObjectItemCaseSensitive(data, "msg");

            if (cJSON_IsNumber(code_item) && cJSON_IsString(msg_item)) {
                err_code = code_item->valueint;
                err_msg = msg_item->valuestring;
            }
        }

        if (err_code == 0 && err_msg == NULL) {
            os_printf("Error after: %s\n", error);
            cJSON_Delete(root);
            return RET_ERR;
        }
        os_printf("\"%d\": \"%s\"\r\n", err_code, err_msg);
        switch (err_code) {
            case 4302: {
                os_printf("**没听清楚，您再说一遍!**\n");
                coze_mgr.finish = 1;
                coze_main_set_state(COZE_DEMO_STATE_WAITING);
                break;
            }
            default: {
                llm_sts_stop(coze_mgr.sts_session);
                // 注意：所有调用 llm_sts_reconnect 前必须将状态设置为 COZE_DEMO_STATE_IDLE
                //coze_main_set_state(COZE_DEMO_STATE_IDLE);
                //llm_sts_reconnect(coze_mgr.sts_session);
                break;
            }
        }
        cJSON_Delete(root);
    }
    return RET_OK;
}

#ifdef COZE_DEMO_URL_PLAY
/*
 * @brief: 获取Coze AI音频URL
 * @param: char *data 音频数据
 * @param: uint32 data_len 音频数据长度
 * @return: char * 音频URL
*/
static char *coze_main_get_audio_url(char *data, uint32 data_len)
{
    // TODO
    return NULL;
}
#endif

static int32 coze_main_customize_process(char *data, uint32 data_len)
{
#ifdef COZE_DEMO_URL_PLAY
    char *audio_url = coze_main_get_audio_url(data, data_len);
    if (audio_url) {
        coze_mgr.uf_hdl = uf_open(audio_url, "ra", 0);
        coze_mgr.finish = 0;
        coze_mgr.time_stamp = os_jiffies();
        llm_free(audio_url);
    }
#endif
    return RET_OK;
}

//生成手动提交对话内容：conversation.message.create
static char *coze_main_gen_manual_conversation_msg(uint64 id, const char *content)
{
    // 计算所需的字符串长度
    size_t content_length = os_strlen(content);

    // 计算最终字符串的总长度
    size_t json_length = content_length + 256; // 200是额外空间，用于其他固定部分

    char *json_string = (char *)llm_zalloc(json_length);
    if (json_string == NULL) {
        os_printf("No memory\n");
        return NULL; // 处理内存分配失败
    }

    // 构造 JSON 字符串
    os_sprintf(json_string, "{\"id\":\"%lld\",\"event_type\":\"conversation.message.create\",\"data\":{\"role\":\"user\",\"content_type\":\"text\",\"content\":\"%s\"}}", id, content);
    return json_string; // 返回生成的 JSON 字符串
}

//构建update包中的chat_config.parameters的json obj
static char *coze_main_gen_chat_config_parameters(const char *command, const char *file_id)
{
    // 创建一个 JSON 对象
    char *json_str = NULL;

    cJSON *json_obj = cJSON_CreateObject();

    // 添加 command 字段
    //cJSON_AddStringToObject(json_obj, "command", command);

    // 创建嵌套的 JSON 字符串
    char inner_image[256]; // 确保足够的大小
    os_snprintf(inner_image, sizeof(inner_image), "{\"file_id\":\"%s\"}", file_id);

    // 将嵌套的字符串添加到 image 字段
    cJSON_AddStringToObject(json_obj, "image", inner_image);

    char *json_str_temp = cJSON_PrintUnformatted(json_obj);
    json_str = llm_strdup(json_str_temp);
    if (json_str_temp) { cJSON_free(json_str_temp); }
    //hgprintf_out(json_str, os_strlen(json_str), 0);

    return json_str;
}
//提交手动对话内容
static int32 coze_main_send_manual_conversation_msg(uint64 id, char *content)
{
    char *str = NULL;
    int32 ret = 0;

    if (content == NULL) {
        os_printf("Input param error\n");
        return RET_ERR;
    }

    str = coze_main_gen_manual_conversation_msg(id, content);
    if (!str) {
        os_printf("Error,no memory!\n");
        return LLME_NOMEM;
    }
    ret = llm_sts_send(coze_mgr.sts_session, LLM_DATA_TYPE_RAW, LLM_DATA_STATE_START,
                       str, (os_strlen(str) + 1));
    if (ret != RET_OK) {
        os_printf("send mannual conversation.message.create failed!\n");
        llm_free(str);
        return RET_ERR;
    }
    llm_free(str);
    return RET_OK;
}

static void coze_main_clear_chat_config(void)
{
    if (coze_sts_platform_cfg.chat_config_meta_data) {
        llm_free(coze_sts_platform_cfg.chat_config_meta_data);
        coze_sts_platform_cfg.chat_config_meta_data = NULL;
    }
    if (coze_sts_platform_cfg.chat_config_custom_variables) {
        llm_free(coze_sts_platform_cfg.chat_config_custom_variables);
        coze_sts_platform_cfg.chat_config_custom_variables = NULL;
    }
    if (coze_sts_platform_cfg.chat_config_extra_params) {
        llm_free(coze_sts_platform_cfg.chat_config_extra_params);
        coze_sts_platform_cfg.chat_config_extra_params = NULL;
    }
    if (coze_sts_platform_cfg.chat_config_parameters) {
        llm_free(coze_sts_platform_cfg.chat_config_parameters);
        coze_sts_platform_cfg.chat_config_parameters = NULL;
    }
}

static int32 coze_main_file_upload_process(char *data, uint32 data_len)
{
    cJSON *json = cJSON_Parse(data);
    int32 ret = RET_OK;

    if (json == NULL) {
        const char *errorPtr = cJSON_GetErrorPtr();
        if (errorPtr != NULL) {
            os_printf("%s:Json error before: %s\n", errorPtr);
        }
        return RET_ERR;
    }

    cJSON *pdata = cJSON_GetObjectItem(json, "data");
    if (pdata == NULL) {
        os_printf("No 'data' found in JSON.\n");
        cJSON_Delete(json);
        return RET_ERR;
    }

    cJSON *id = cJSON_GetObjectItem(pdata, "id");
    if (id != NULL && cJSON_IsString(id)) {
        os_printf("get file id: [%s]\n", id->valuestring);//获取file_ID字符串
        char *param = coze_main_gen_chat_config_parameters("1", id->valuestring);
        if (param == NULL) {
            os_printf("create chat_config parameter JSON failed!\n");
            cJSON_Delete(json);
            return RET_ERR;
        }
        coze_sts_platform_cfg.chat_config_parameters = param;
        ret = llm_sts_config(coze_mgr.sts_session, LLM_CONFIG_TYPE_MODEL,
                             (void *)&coze_sts_platform_cfg, sizeof(coze_sts_platform_cfg));//填入config并update到llm库中
        if (ret) {
            os_printf("sts_model_cfg fail!\r\n");
            cJSON_Delete(json);
            return RET_ERR;
        }
        coze_mgr.img_triggered = 1;//如果想要传完图像后，语音问问题, 就把这行注释掉即可。
    } else {
        os_printf("'id' field not found or is not a string\n");
    }

    cJSON_Delete(json);
    return RET_OK;
}
//文生图官方文档做法，返回的是URL
static int32 coze_main_tti_url_process(char *data, uint32 data_len)
{
    //目前只是将分析的image_url打印出来，还需要用户通过https去该url拉数据。
    cJSON *json = cJSON_Parse(data);
    if (json == NULL) {
        os_printf("parse json error!\n");
        return RET_ERR;
    }

    // 查找 "data" 对象
    cJSON *json_data = cJSON_GetObjectItem(json, "data");
    if (json_data == NULL || !cJSON_IsObject(json_data)) {
        os_printf("can not find data obj!\n");
        cJSON_Delete(json);
        return RET_ERR;
    }

    // 查找 "content" 字段
    cJSON *content_item = cJSON_GetObjectItem(json_data, "content");
    if (content_item == NULL || !cJSON_IsString(content_item) || (content_item->valuestring == NULL)) {
        os_printf("can not find content obj!\n");
        cJSON_Delete(json);
        return RET_ERR;
    }

    // 解析 content 的 JSON 字符串
    cJSON *content_json = cJSON_Parse(content_item->valuestring);
    if (content_json == NULL) {
        os_printf("can not find content val str!\n");
        cJSON_Delete(json);
        return RET_ERR;
    }

    // 查找 "data" 对象中的 "image_urls" 数组
    cJSON *image_urls = cJSON_GetObjectItem(content_json, "data");
    if (image_urls == NULL || !cJSON_IsObject(image_urls)) {
        os_printf("can not find content data!\n");
        cJSON_Delete(content_json);
        cJSON_Delete(json);
        return RET_ERR;
    }

    // 从 "data" 对象中获取 "image_urls" 数组
    cJSON *urls_array = cJSON_GetObjectItem(image_urls, "image_urls");
    if (urls_array == NULL || !cJSON_IsArray(urls_array)) {
        os_printf("can not find image url array!\n");
        cJSON_Delete(content_json);
        cJSON_Delete(json);
        return RET_ERR;
    }

    // 遍历数组，提取 HTTPS 链接
    int array_size = cJSON_GetArraySize(urls_array);
    for (int i = 0; i < array_size; i++) {
        cJSON *url_item = cJSON_GetArrayItem(urls_array, i);
        if (cJSON_IsString(url_item) && (url_item->valuestring != NULL)) {
            os_printf("get coze gen image url:%s\n", url_item->valuestring);
        }
    }
    // 释放 JSON 对象
    cJSON_Delete(content_json);
    cJSON_Delete(json);
    return RET_OK;
}
//文生图：生成的图片数据通过base64的格式发送过来，此为从json分离base64数据
static char *coze_main_tti_extract_base64_from_json(const char *json_str, uint32 json_tot_len, uint32 *b64_len)
{
    cJSON *root = NULL;
    cJSON *data_obj = NULL;
    cJSON *content = NULL;
    cJSON *content_json = NULL;
    cJSON *data_field = NULL;
    char *base64_content = NULL;
    const char *base64_marker = "base64,";
    char *marker_pos = NULL;
    char *content_end = NULL;
    uint32 str_len = 0;
    int ret = RET_ERR;

    if (!json_str || !b64_len || json_tot_len == 0) {
        return NULL;
    }

    *b64_len = 0;

    // 检查JSON字符串长度是否超过限制
    str_len = os_strlen(json_str);
    if (str_len > json_tot_len) {
        os_printf("JSON string length %u exceeds total length %u\n", str_len, json_tot_len);
        str_len = json_tot_len;
    }

    // 解析外层JSON
    root = cJSON_Parse(json_str);
    if (!root) {
        os_printf("JSON parse error: %s\n", cJSON_GetErrorPtr());
        goto exit;
    }

    // 获取data对象
    data_obj = cJSON_GetObjectItem(root, "data");
    if (!data_obj || !cJSON_IsObject(data_obj)) {
        os_printf("JSON structure error: data object not found or not an object\n");
        goto exit;
    }

    // 获取content字段
    content = cJSON_GetObjectItem(data_obj, "content");
    if (!content || !cJSON_IsString(content) || !content->valuestring) {
        os_printf("JSON structure error: content not found, not a string, or empty\n");
        goto exit;
    }

    // 尝试解析content字段为JSON
    content_json = cJSON_Parse(content->valuestring);
    if (content_json) {
        // content 是 JSON 格式，从 data 字段获取
        data_field = cJSON_GetObjectItem(content_json, "data");
        if (!data_field || !cJSON_IsString(data_field) || !data_field->valuestring) {
            os_printf("JSON structure error: data field not found in content JSON, not a string, or empty\n");
            goto exit;
        }
        // 查找base64标记
        marker_pos = os_strstr((char *)data_field->valuestring, base64_marker);
    } else {
        // content 不是 JSON 格式，直接从 content 字符串中提取
        os_printf("content is not JSON format, extracting directly from content string\n");
        marker_pos = os_strstr((char *)content->valuestring, base64_marker);
    }

    if (!marker_pos) {
        os_printf("base64 marker not found\n");
        goto exit;
    }

    // 移动到base64内容开始位置
    marker_pos += os_strlen(base64_marker);

    // 查找base64内容的结束位置
    content_end = marker_pos;
    while (*content_end && *content_end != '"') {
        content_end++;
    }

    // 计算base64内容长度
    *b64_len = content_end - marker_pos;
    if (*b64_len == 0) {
        os_printf("base64 content is empty\n");
        goto exit;
    }

    // 分配内存并清空
    base64_content = llm_malloc(*b64_len + 1);
    if (!base64_content) {
        os_printf("Memory allocation failed\n");
        goto exit;
    }
    os_memset(base64_content, 0, *b64_len + 1);

    // 复制base64内容
    os_memcpy(base64_content, marker_pos, *b64_len);
    base64_content[*b64_len] = '\0';

    ret = RET_OK;

exit:
    // 统一释放JSON对象
    if (content_json) {
        cJSON_Delete(content_json);
    }
    if (root) {
        cJSON_Delete(root);
    }

    return (ret == RET_OK) ? base64_content : NULL;
}
//文生图：生成的图片数据通过base64的格式发送过来，
static int32 coze_main_tti_base64_process(char *data, uint32 data_len)
{
    struct data_structure  *data_s = NULL;
    unsigned char *decoded_image_buf = NULL;
    uint32 decoded_image_len = 0;
    char *base64_content = NULL;
    uint32 base64_content_len = 0;
    int ret = 0;

    base64_content = coze_main_tti_extract_base64_from_json(data, data_len, &base64_content_len);
    if (!base64_content) {
        os_printf("Extract base64 content failed!\n");
        return RET_ERR;
    }

    os_printf("Get TTI base64 data %p,len=%d\r\n", base64_content, base64_content_len);

    ret = llm_base64_decode(base64_content, &decoded_image_buf, &decoded_image_len);
    llm_free(base64_content);

    if (ret != RET_OK || decoded_image_buf == NULL || decoded_image_len == 0) {
        os_printf("Base64 decode failed (ret=%d, len=%d)\r\n", ret, decoded_image_len);
        return RET_ERR;
    }

    data_s = get_src_data_f(coze_mgr.screen_stream);
    if (data_s) {
        os_printf("success to obtain data_s,data_buff:%p,len:%d\r\n", decoded_image_buf, decoded_image_len);
        data_s->data = (void *)decoded_image_buf;
        data_s->len = decoded_image_len;
        data_s->ref = 0;
        data_s->type = SET_DATA_TYPE(JPEG, JPEG_FULL);
        ret = send_data_to_stream(data_s);
        data_s = NULL;
    } else {
        os_printf("Failed to obtain data_s\r\n");
        llm_free(decoded_image_buf);
    }

    return RET_OK;
}

static int32 coze_main_tti_process(char *data, uint32 data_len)
{
    const char *base64_key = ";base64,";
    if (NULL != os_strstr(data, base64_key)) {
        return coze_main_tti_base64_process(data, data_len);//智能体通过base64压缩过的图片数据
    } else {
        return coze_main_tti_url_process(data, data_len);//官方文档做法,需要解析URL并通过https获取图片数据
    }
}

//对llm库上传的conversation_id处理
extern struct sys_config sys_cfgs;
static int32 coze_main_conversation_id_process(char *data, uint32 data_len)
{
    int32 ret = 0;
    if (os_strlen(data) > sizeof(sys_cfgs.coze_conversation_id)) {
        os_printf("Error:conversation_id too long!\n");
        return RET_ERR;
    }
    if (os_strcmp(sys_cfgs.coze_conversation_id, data) == 0) {
        return RET_OK;
    }
    //保存到conversation_id到flash
    os_printf("Modify conversation_id to %s\n", data);
    memset(sys_cfgs.coze_conversation_id, 0, sizeof(sys_cfgs.coze_conversation_id));
    os_strncpy(sys_cfgs.coze_conversation_id, data, os_strlen(data) + 1);
    syscfg_save();

    coze_sts_platform_cfg.chat_config_conversation_id = (char *)&sys_cfgs.coze_conversation_id;
    ret = llm_sts_config(coze_mgr.sts_session, LLM_CONFIG_TYPE_MODEL,
                         (void *)&coze_sts_platform_cfg, sizeof(coze_sts_platform_cfg));//填入config并update到llm库中
    if (ret) {
        os_printf("update conversation_id fail!\r\n");
        return RET_ERR;
    }
    return RET_OK;
}

static void coze_main_conversation_id_init()
{
    char header[64];
    memset(&header, 0xFF, sizeof(header));
    if (os_memcmp(sys_cfgs.coze_conversation_id,
                  header, sizeof(sys_cfgs.coze_conversation_id)) != 0) {
        coze_sts_platform_cfg.chat_config_conversation_id = (char *)&sys_cfgs.coze_conversation_id;
        os_printf("use syscfg conversation_id:%s\n", coze_sts_platform_cfg.chat_config_conversation_id);
    }
}

/*
 * @brief: 处理Coze AI事件消息
 * @param: void
 * @return: int32
*/
static int32 coze_main_event_msg_process(void)
{
    struct coze_demo_event_msg event_msg = {0};

    RB_INT_GET(&coze_mgr.event_queue, event_msg);
    if (event_msg.event == LLM_EVENT_UNKNOWN) { return RET_OK; }

    switch (event_msg.event) {
        case LLM_EVENT_CONNECTED: {
            os_printf("Coze AI connected!\r\n");
            coze_main_set_state(COZE_DEMO_STATE_CONNECTED);
            break;
        }
        case LLM_EVENT_DISCONNECT: {
            os_printf("Coze AI disconnect!\r\n");
            coze_main_set_state(COZE_DEMO_STATE_DISCONNECTED);
            break;
        }
        case LLM_EVENT_STT_RESULT: {
            os_printf("COZE AI STT(%d):", event_msg.msg_len);
            hgprintf_out(event_msg.msg, event_msg.msg_len, 0);
            _os_printf("\r\n");
            llm_free(event_msg.msg);
            break;
        }
        case LLM_EVENT_TTS_RESULT: {
            os_printf("COZE AI TTS(%d):", event_msg.msg_len);
            hgprintf_out(event_msg.msg, event_msg.msg_len, 0);
            _os_printf("\r\n");
            llm_free(event_msg.msg);
            coze_mgr.time_stamp = os_jiffies();
            break;
        }
        case LLM_EVENT_VAD_RESULT: {
            os_printf("Coze AI recv server vad result:%s\r\n", event_msg.msg);
            if (os_strncmp(event_msg.msg, "STOP", event_msg.msg_len) == 0) {
                coze_main_set_state(COZE_DEMO_STATE_READY);
            }
            llm_free(event_msg.msg);
            break;
        }
        case LLM_EVENT_DIALOGUE_END: {
            os_printf("COZE AI check finish!\r\n");
            coze_mgr.finish = 1;
            coze_mgr.time_stamp = os_jiffies();
            break;
        }
        case LLM_EVENT_DIALOGUE_TIMEOUT: {
            os_printf("COZE AI dialogue timeout!\r\n");
            coze_mgr.dialogue_timeout_cnt++;    //1s
            if (coze_mgr.dialogue_timeout_cnt > COZE_DEMO_DIALOGUE_TIMEOUT_MAX_CNT) {
                coze_mgr.dialogue_timeout_cnt = 0;
                llm_sts_interrupt(coze_mgr.sts_session, 0);
                coze_mgr.finish = 1;
                coze_mgr.time_stamp = os_jiffies();
                os_printf("End the current conversation and start a new one!\r\n");
            }
            break;
        }
        case LLM_EVENT_ERROR_MSG: {
            os_printf("COZE AI recv err msg(%d):", event_msg.msg_len);
            hgprintf_out(event_msg.msg, event_msg.msg_len, 0);
            _os_printf("\r\n");
            coze_main_error_process(event_msg.msg);
            llm_free(event_msg.msg);
            break;
        }
        case LLM_EVENT_CUSTOMIZE: {
            os_printf("COZE AI recv customize(%d):", event_msg.msg_len);
            hgprintf_out(event_msg.msg, event_msg.msg_len, 0);
            _os_printf("\r\n");
            coze_main_customize_process(event_msg.msg, event_msg.msg_len);
            llm_free(event_msg.msg);
            break;
        }
        case LLM_EVENT_UPLOAD_FILE_RESULT: {
            os_printf("COZE AI recv upload file result(%d):", event_msg.msg_len);
            hgprintf_out(event_msg.msg, event_msg.msg_len, 0);
            _os_printf("\r\n");
            coze_main_file_upload_process(event_msg.msg, event_msg.msg_len);
            llm_free(event_msg.msg);
            break;
        }
        case LLM_EVENT_TTI_RESULT: {
            os_printf("COZE AI recv tti result(%d):", event_msg.msg_len);
            //hgprintf_out(event_msg.msg, event_msg.msg_len, 0);
            //_os_printf("\r\n");
            coze_main_tti_process(event_msg.msg, event_msg.msg_len);
            llm_free(event_msg.msg);
            break;
        }
        case LLM_EVENT_CONVERSATION_ID: {
            os_printf("Get conversation id:[%s]\n", event_msg.msg);
            coze_main_conversation_id_process(event_msg.msg, event_msg.msg_len);
            llm_free(event_msg.msg);
            break;
        }
        default: {
            os_printf("Not supported event msg: %d\r\n", event_msg.event);
            break;
        }
    }

    return RET_OK;
}

/*
 * @brief 主线程
*/
void coze_main(void)
{
    int32 ret = RET_OK;
    coze_demo_state state = COZE_DEMO_STATE_IDLE;
    uint64 jiff = 0;
    uint8 last_key_triggered = 0;   // 记录上一次按键状态；
    uint8 dialogue_mode = 0;        // 对话模式，0为按键模式，1为语音模式；
    uint8 send_complete = 0;        // 按键模式下，需发送完成帧告知服务器音频数据上传结束；

    ret = coze_main_app_init("coze_sts");
    if (ret) {
        os_printf("coze main init failed: %d\r\n", ret);
        goto cleanup;
    }

    while (1) {
        coze_main_event_msg_process();

        if (coze_mgr.connected == 1) {
            if (coze_mgr.img_triggered) {
                coze_mgr.img_triggered = 0;
                if (dialogue_mode != DIALOGUE_MODE_IMAGE_UPDATED) {
                    dialogue_mode = DIALOGUE_MODE_IMAGE_UPDATED;
                }
                coze_main_set_state(COZE_DEMO_STATE_WAITING);
            }
            // 处理语音触发事件
            if (coze_mgr.voice_triggered) {
                coze_mgr.voice_triggered = 0;
                if (dialogue_mode != DIALOGUE_MODE_VOICE) {
                    coze_sts_platform_cfg.turn_detection_type = "server_vad";
                    llm_sts_config(coze_mgr.sts_session, LLM_CONFIG_TYPE_MODEL,
                                   (void *)&coze_sts_platform_cfg, sizeof(coze_sts_platform_cfg));
                    dialogue_mode = DIALOGUE_MODE_VOICE;
                }
                coze_mgr.finish = 0;
#ifdef COZE_DEMO_URL_PLAY
                uf_close(coze_mgr.uf_hdl);
                coze_mgr.uf_hdl = NULL;
#endif
                coze_main_set_state(COZE_DEMO_STATE_WAITING);
            }
            // 处理按键触发事件，优先级：按键 > 语音
            if (last_key_triggered != coze_mgr.key_triggered) {
                // 按键按下
                if (coze_mgr.key_triggered == 1) {
                    if (dialogue_mode != DIALOGUE_MODE_KEYBOARD) {
                        // 按键配置与语音配置不同（turn_detection_type），需更新
                        coze_sts_platform_cfg.turn_detection_type = "client_interrupt";
                        llm_sts_config(coze_mgr.sts_session, LLM_CONFIG_TYPE_MODEL,
                                       (void *)&coze_sts_platform_cfg, sizeof(coze_sts_platform_cfg));
                        dialogue_mode = DIALOGUE_MODE_KEYBOARD;
                    }
#ifdef COZE_DEMO_URL_PLAY
                    uf_close(coze_mgr.uf_hdl);
                    coze_mgr.uf_hdl = NULL;
#endif
                    coze_main_set_state(COZE_DEMO_STATE_WAITING);
                }
                // 按键松开
                else {
                    // 若发送过音频数据需发送完成帧
                    if (coze_mgr.send_audio_cnt > 0) {
                        send_complete = 1;
                    }
                    coze_main_set_state(COZE_DEMO_STATE_READY);
                }
                last_key_triggered = coze_mgr.key_triggered;
            }
            // 处理语音触发模式下自动退出多轮对话
            if (dialogue_mode == DIALOGUE_MODE_VOICE) {
                jiff = os_jiffies() - coze_mgr.time_stamp;
                if (os_jiffies_to_msecs(jiff) > COZE_IDLE_TIMEOUT && coze_mgr.time_stamp != UINT64_MAX && llm_sts_check_audio(coze_mgr.sts_session)) {
                    llm_sts_interrupt(coze_mgr.sts_session, 0);
                    os_printf("**自动退出**\r\n");
                    coze_audio_play_prompt_tone(tuixia, tuixia_size);
                    coze_mgr.finish = 0;
                    coze_mgr.time_stamp = UINT64_MAX;
#ifdef COZE_DEMO_URL_PLAY
                    uf_close(coze_mgr.uf_hdl);
                    coze_mgr.uf_hdl = NULL;
#endif
                    coze_main_set_state(COZE_DEMO_STATE_READY);
                }
            }
        }
        state = coze_main_get_state();
        switch (state) {
            case COZE_DEMO_STATE_IDLE:
                break;
            case COZE_DEMO_STATE_CONNECTED: {
                os_printf("**已连接!**\n");
                coze_mgr.connected = 1;
                coze_audio_play_prompt_tone(yilianjie, yilianjie_size);
                os_sleep_ms(1000);
                // 连接成功后，需更新一次配置
                if (dialogue_mode == DIALOGUE_MODE_VOICE) {
                    coze_sts_platform_cfg.turn_detection_type = "server_vad";
                } else if (dialogue_mode == DIALOGUE_MODE_KEYBOARD) {
                    coze_sts_platform_cfg.turn_detection_type = "client_interrupt";
                }
                llm_sts_config(coze_mgr.sts_session, LLM_CONFIG_TYPE_MODEL,
                               (void *)&coze_sts_platform_cfg, sizeof(coze_sts_platform_cfg));
                coze_main_set_state(COZE_DEMO_STATE_READY);
                break;
            }
            case COZE_DEMO_STATE_READY: {
#ifdef COZE_DEMO_URL_PLAY
                if (coze_mgr.uf_hdl) {
                    if (uf_eof(coze_mgr.uf_hdl)) {
                        os_printf("UF EOF! close it!\r\n");
                        uf_close(coze_mgr.uf_hdl);
                        coze_mgr.uf_hdl = NULL;
                        coze_mgr.finish = 1;
                    } else {
                        coze_mgr.uf_audio_len = 0;
                        uf_ioctl(coze_mgr.uf_hdl, UF_IOCTL_CMD_GET_DATASIZE, (uint32)&coze_mgr.uf_audio_len, 0);
                        if (coze_mgr.uf_audio_len > 0) {
                            coze_mgr.uf_audio_buf = llm_malloc(coze_mgr.uf_audio_len + 1);
                            if (coze_mgr.uf_audio_buf) {
                                ret = uf_read(coze_mgr.uf_audio_buf, coze_mgr.uf_audio_len, 1, coze_mgr.uf_hdl);
                                if (ret > 0) {
                                    coze_audio_play_prompt_tone(coze_mgr.uf_audio_buf, coze_mgr.uf_audio_len);
                                }
                                llm_free(coze_mgr.uf_audio_buf);
                            } else {
                                os_printf("uf_audio_buf malloc fail, no memory!\r\n");
                            }
                        }
                    }
                }
#endif
                // 按键触发模式
                if (dialogue_mode == DIALOGUE_MODE_KEYBOARD) {
                    if (send_complete) {
                        ret = llm_sts_send(coze_mgr.sts_session, LLM_DATA_TYPE_AUDIO, LLM_DATA_STATE_END, NULL, 0);
                        if (ret == RET_OK) {
                            send_complete = 0;
                            coze_audio_play_prompt_tone(fasong, fasong_size);
                        } else if (ret != LLME_AGAIN) {
                            os_printf("send_complete fail!\r\n");
                            coze_main_set_state(COZE_DEMO_STATE_IDLE);
                            llm_sts_reconnect(coze_mgr.sts_session);
                            break;
                        }
                    }
                }
                // 语音触发模式
                else if (dialogue_mode == DIALOGUE_MODE_VOICE) {
                    // 自动触发多轮对话
                    if (coze_mgr.finish && llm_sts_check_audio(coze_mgr.sts_session)) {
                        coze_main_set_state(COZE_DEMO_STATE_WAITING);
                        os_printf("**新对话：**\r\n");
                    }
                }
                break;
            }
            case COZE_DEMO_STATE_WAITING: {
                // 打断上一次对话
                audac_disable_play();
                llm_sts_interrupt(coze_mgr.sts_session, 0);
                while (!audac_wait_empty()) {
                    os_sleep_ms(1);
                }
                audac_enable_play();
                // 等待服务器进入监听状态
                ret = coze_main_waiting();
                if (ret != RET_OK) {
                    break;
                }
                if (dialogue_mode == DIALOGUE_MODE_VOICE) {
                    if (coze_mgr.finish == 0) {
                        os_printf("**我在，请说：**\r\n");
                        coze_audio_play_prompt_tone(wozai, wozai_size);
                        os_sleep_ms(1000);
                    }
                    coze_mgr.finish = 0;
                    coze_mgr.time_stamp = os_jiffies();
                } else if (dialogue_mode == DIALOGUE_MODE_IMAGE_UPDATED) {
                    coze_main_send_manual_conversation_msg(os_jiffies(), "这张图片里有什么");
                    coze_main_set_state(COZE_DEMO_STATE_READY);
                    coze_main_clear_chat_config();
                }
                if (dialogue_mode != DIALOGUE_MODE_IMAGE_UPDATED) {
                    coze_audio_play_prompt_tone(beep, beep_size);
                    coze_audio_free_sample_stream(coze_mgr.mic_stream);
                    coze_main_set_state(COZE_DEMO_STATE_PUSHING);
                    coze_mgr.send_audio_cnt = 0;
                }

                break;
            }
            case COZE_DEMO_STATE_PUSHING: {
                // 发送音频数据
                coze_audio_send_data(coze_mgr.mic_stream, coze_mgr.aualaw_dev);
                break;
            }
            case COZE_DEMO_STATE_DISCONNECTED: {
                os_printf("**已断开!**\n");
                coze_mgr.connected = 0;
                coze_audio_play_prompt_tone(yiduankai, yiduankai_size);
                os_sleep_ms(1000);
                coze_mgr.finish = 0;
                coze_mgr.key_triggered = 0;
                last_key_triggered = 0;
                send_complete = 0;
                coze_mgr.voice_triggered = 0;
                coze_mgr.img_triggered   = 0;
                coze_main_set_state(COZE_DEMO_STATE_IDLE);
                break;
            }
            default:
                break;
        }
        os_sleep_ms(10);
    }

cleanup:
    close_stream(coze_mgr.mic_stream);
    close_stream(coze_mgr.screen_stream);
    if (coze_mgr.psram_copy_buff) { llm_free(coze_mgr.psram_copy_buff); }
    if (coze_mgr.aualaw_buf) { llm_free(coze_mgr.aualaw_buf); }
#ifdef COZE_DEMO_URL_PLAY
    if (coze_mgr.uf_audio_buf) { llm_free(coze_mgr.uf_audio_buf); }
    uf_close(coze_mgr.uf_hdl);
#endif
    llm_free(coze_mgr.event_queue.rbq);
    llm_sts_deinit(coze_mgr.sts_session);
    llm_global_deinit();
}

/*******************************************************
从SD卡上传图片sample code,若要使用LLM视觉处理功能:
1.需要在project_config.h打开如下宏：
#define DVP_EN                          1
#define SDH_EN                          1   //SD卡，可以不要
#define FS_EN                           1   //文件系统，可以不要
#define OPENDML_EN                      1   //摄像头

2.将config-taixin.h中将#define CURL_DISABLE_MIME 屏蔽掉

3.确保coze智能体搭建支持视觉处理

4.请确定打开了PSRAM_HEAP以开启psram,并把libcurl的malloc/free/realloc/calloc/strdup使用的都是psram,不然内存可能不够

5.上传完成后，llm库会返回event:LLM_EVENT_UPLOAD_FILE_RESULT
从event的参数解析file_id(可参考coze_main_file_upload_process)，
用户可以保存fileid,并将需要跟智能体交互的fileid通过llm_sts_config更新到llm库

*******************************************************/

int32 coze_atcmd_upload_photo(const char *cmd, char *argv[], uint32 argc)
{
    char *cache_buf = NULL;
    const char *update_filename = NULL;
    void *fp = NULL;
    uint32_t filesize = 0;
    uint32_t file_tot_size = 0;
    uint32_t readsize = SD_CACHE_SIZE;
    uint32_t bytes_read = 0;
    int32 ret = 0;

    if (argc < 1) {
        os_printf("argv is too less\n");
        return RET_ERR;
    }
    update_filename = argv[0];
    fp = osal_fopen(update_filename, "rb");
    if (!fp) {
        os_printf("%s file not exist\n", update_filename);
        ret = RET_ERR;
        goto __atcmd_update_file_end;
    }

    filesize = osal_fsize(fp);
    file_tot_size = filesize;
    cache_buf = llm_malloc(SD_CACHE_SIZE);
    if (!cache_buf) {
        ret = RET_ERR;
        goto __atcmd_update_file_end;
    }
    os_printf("%s:filesize:%d,cache_buff:0x%x\n", __FUNCTION__, filesize, cache_buf);

    while (filesize) {
        if (filesize < SD_CACHE_SIZE) {
            readsize = filesize;
        } else {
            readsize = SD_CACHE_SIZE;
        }
        bytes_read = osal_fread(cache_buf, 1, readsize, fp);
        if (bytes_read == 0) {
            os_printf("Read file error\n");
            ret = RET_ERR;
            goto __atcmd_update_file_end;
        }

        if (filesize == file_tot_size) { // First chunk
            llm_sts_upload_file(coze_mgr.sts_session, LLM_DATA_TYPE_FILE, LLM_DATA_STATE_START, NULL, file_tot_size);
        }
        llm_sts_upload_file(coze_mgr.sts_session, LLM_DATA_TYPE_FILE, LLM_DATA_STATE_MIDDLE, cache_buf, bytes_read);
        filesize -= bytes_read;
    }
    llm_sts_upload_file(coze_mgr.sts_session, LLM_DATA_TYPE_FILE, LLM_DATA_STATE_END, NULL, 0);
    ret = RET_OK;

__atcmd_update_file_end:
    if (fp) {
        osal_fclose(fp);
    }
    if (cache_buf) {
        llm_free(cache_buf);
    }
    return ret;
}

struct os_task coze_main_task;
void coze_demo(void)
{
    OS_TASK_INIT("COZE_DEMO", &coze_main_task, coze_main, NULL, OS_TASK_PRIORITY_NORMAL, 2048);
}
#endif
