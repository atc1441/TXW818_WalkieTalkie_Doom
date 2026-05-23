#include "lvgl/lvgl.h"
#include "ui/preview_ui.h"
#include "project_config.h"
#include "lvgl_ui.h"

#define MINI_DV_UI 	    0
#define IPC_UI		    1
#define BBM_UI 		    2
#define CHILDREN_UI     3
#define LLM_VISION_UI   4

#define DEFINE_UI    MINI_DV_UI

lv_indev_t * indev_keypad;
extern lv_style_t g_style;

lv_obj_t *main_Mini_DV_ui(lv_obj_t *base_ui,lv_group_t *group)
{
    lv_obj_t *btn;
    //可以打印系统一些信息
    btn = system_msg_ui(group,base_ui);
    //预览dvp的镜头数据
    btn = preview_ui(group,base_ui);
    //可以进行录像拍照
    btn = preview_encode_selct_ui(group,base_ui);
    //播放自己录制的视频文件
    btn = player2_ui(group,base_ui);
    return base_ui;
}


lv_obj_t *main_IPC_ui(lv_obj_t *base_ui,lv_group_t *group)
{
    lv_obj_t *btn;
    //可以打印系统一些信息
    btn = system_msg_ui(group,base_ui);
    //预览USB的镜头数据
    btn = preview_usb_ui(group,base_ui);
    btn = preview_usb_select_ui(group,base_ui);
    btn = photo_ui(group,base_ui);
    return base_ui;
}

lv_obj_t *main_BBM_ui(lv_obj_t *base_ui,lv_group_t *group)
{
    lv_obj_t *btn;
    //可以打印系统一些信息
    btn = system_msg_ui(group,base_ui);

    btn = preview_net_jpg_ui(group,base_ui);
    return base_ui;
}


lv_obj_t *main_children_ui(lv_obj_t *base_ui,lv_group_t *group)
{
    lv_obj_t *btn;
    btn = system_msg_ui(group,base_ui);
    btn = preview_ui(group,base_ui);
    btn = preview_encode_QVGA_ui(group,base_ui);
    btn = preview_encode_QQVGA_ui(group,base_ui);
    btn = preview_dvp_pip_ui(group,base_ui);
    btn = preview_usb_ui(group,base_ui);
    btn = preview_usb_encode_ui(group,base_ui);
    btn = preview_dvp_usb_ui(group,base_ui);
    btn = player_ui(group,base_ui);
    btn = player2_ui(group,base_ui);
    btn = preview_encode_takephoto_ui(group,base_ui);
    btn = preview_encode_960P_ui(group,base_ui);
    btn = preview_encode_selct_ui(group,base_ui);
    btn = zbar_ui(group,base_ui);
    btn = zbar_dvp_ui(group,base_ui);
    btn = zbar_usb_ui(group,base_ui);
    btn = preview_usb_select_ui(group,base_ui);
    btn = photo_ui(group,base_ui);
    return base_ui;
}

lv_obj_t *main_LLM_vision_ui(lv_obj_t *base_ui,lv_group_t *group)
{
    lv_obj_t *ui = lv_obj_create(lv_scr_act());  
    lv_obj_add_style(ui, &g_style, 0);
    lv_obj_set_size(ui, LV_PCT(100), LV_PCT(100));
    stream *decode_s = jpg_decode_stream_not_bind(S_JPG_DECODE);
    //将解码的数据推送到Video P0和Video P1显示
    streamSrc_bind_streamDest(decode_s,R_VIDEO_P0);
    streamSrc_bind_streamDest(decode_s,R_VIDEO_P1);
    enable_stream(decode_s,1);

    //接收LLM的jpg数据,配置需要解码的参数(320x240),然后给到S_JPG_DECODE进行解码,最后给到P0去显示(设置时P0的类型)
    stream *P0_jpg_s = other_jpg_stream_not_bind(SR_OTHER_JPG_USB1,320,240,320,240,YUV_P0);
    //将other_jpg的数据给到S_JPG_DECODE进行编码
    streamSrc_bind_streamDest(P0_jpg_s,S_JPG_DECODE);
    enable_stream(P0_jpg_s,1);

    return base_ui;
}

lv_obj_t *main_ui(lv_obj_t *base_ui)
{
    lv_group_t *group;
    group = lv_group_create();
    lv_indev_set_group(indev_keypad, group);
    lv_obj_t * ui = lv_list_create(lv_scr_act());
    lv_obj_add_style(ui,&g_style,0);
    lv_obj_set_size(ui, LV_PCT(100), LV_PCT(100));

    #if DEFINE_UI == MINI_DV_UI
        main_Mini_DV_ui(ui,group);
    #elif DEFINE_UI == IPC_UI
        main_IPC_ui(ui,group);
    #elif DEFINE_UI == BBM_UI
		main_BBM_ui(ui,group);
    #elif DEFINE_UI == CHILDREN_UI
        main_children_ui(ui,group);
    #elif DEFINE_UI == LLM_VISION_UI
        main_LLM_vision_ui(ui,group);
    #endif
    return ui;
}
