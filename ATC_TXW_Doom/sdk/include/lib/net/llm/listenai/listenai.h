#ifndef _LISTENAI_H_
#define _LISTENAI_H_

#ifdef __cplusplus
extern "C" {
#endif

struct Listenai_AIUI_Cfg {
    char    *token_url;                 //必填
    char    *gateway_url;               //必填

    char    *product_id;                //必填
    char    *secret_id;                 //必填
    char    *device_id;                 //必填

    char    *data_type;                 //必填
    char    *aue;                       //必填
    char    *speex_size;                //若音频数据格式为speex、speex-web时，则需设置
    char    *features[10];              //选填

    char    *asr_evad;                  //选填
    char    *asr_vad_eos;               //选填
    char    *asr_ent;                   //选填
    char    *asr_svad;                  //选填

    char    *tts_vcn;                   //选填
    char    *tts_ent;                   //选填
    char    *tts_volume;                //选填
    char    *tts_speed;                 //选填
    char    *tts_pitch;                 //选填

    char    *nlu_nlp_mode;              //选填
    char    *nlu_scene;                 //选填
    char    *nlu_clean_dialog_history;  //选填
};
extern const struct llm_model_data listenai_aiui_model;

#ifdef __cplusplus
}
#endif

#endif


