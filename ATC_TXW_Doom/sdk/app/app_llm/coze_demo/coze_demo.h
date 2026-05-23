#ifndef __COZE_DEMO_H
#define __COZE_DEMO_H

#include "llm/llm_api.h"

#include "syscfg.h"
#include "keyWork.h"
#include "keyScan.h"
#include "audio_dac.h"
#include "stream_define.h"
#include "stream_frame.h"
#include "dev/audio/components/alaw/aualaw.h"
#include "src/opus_private.h"
#include "lib/net/urlfile/urlfile.h"
#include "lwip/netdb.h"

typedef enum {
    DIALOGUE_MODE_KEYBOARD       = 0,     // 按键触发模式
    DIALOGUE_MODE_VOICE          = 1,     // 语音触发模式
    DIALOGUE_MODE_IMAGE_UPDATED  = 2,     // 图像上传完毕自动触发模式  
} coze_demo_dialogue_mode;

typedef enum {
    COZE_DEMO_STATE_IDLE            = 0,    // 空闲状态
    COZE_DEMO_STATE_CONNECTED       = 1,    // 已连接状态
    COZE_DEMO_STATE_DISCONNECTED    = 2,    // 已断开状态
    COZE_DEMO_STATE_READY           = 3,    // 就绪状态
    COZE_DEMO_STATE_WAITING         = 4,    // 等待服务器进入监听状态
    COZE_DEMO_STATE_PUSHING         = 5,    // 推流状态
} coze_demo_state;

struct coze_demo_event_msg {
    uint16  event;
    char    *msg;
    uint32  msg_len;
};

struct coze_demo_manage {
    void                *sts_session;
    struct os_mutex     lock;
    RBUFFER_DEF_R(event_queue, struct coze_demo_event_msg);
    coze_demo_state     state;
    uint32  key_triggered:1,        // 按键触发标志位
            voice_triggered:1,      // 语音触发标志位
            finish: 1,              // 完成标志位
            connected: 1,           // 已连接标志位
            img_triggered:1,        // 图像识别完成触发标志位
            rev: 27;
    uint32  send_audio_cnt;         // 已发送音频包数量
    uint32  ip_addr;                // IP 地址
    uint32  dialogue_timeout_cnt;   // 对话超时次数
    uint64  time_stamp;             // 时间戳
    uint8   file_id[32];            // 文件 ID

#ifdef COZE_DEMO_URL_PLAY
	void    *uf_hdl;                // urlfile
	char    *uf_audio_buf;
    uint32  uf_audio_len;
#endif
    // audio
    stream                  *mic_stream;
    struct aualaw_device    *aualaw_dev;
    char                    *aualaw_buf;
    char                    *psram_copy_buff;
    stream                  *screen_stream;
};

extern struct coze_demo_manage coze_mgr;

void coze_main_awaken_demo(void);
uint32_t coze_main_intercom_push_key(struct key_callback_list_s *callback_list, uint32_t keyvalue, uint32_t extern_value);

#endif
