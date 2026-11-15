/* ============================================================================
 * ui_custom.h  —— 所有手写 UI 逻辑的唯一入口
 *
 * 【为什么要有这个文件】
 * LVGL_CUT/UI/ 下面的一切都是 SquareLine 生成的。实测：重新导出会
 *   · 清空 ui_ScreenMain.h 里 "// CUSTOM VARIABLES" 段之后的内容
 *   · 把 ui_ScreenXxx.c 里手写的事件回调恢复成生成时的样子
 *   · 把 _ui_screen_change 那套老写法重新塞回来
 * 所以：凡是自己写的，一行都不许放在 LVGL_CUT/UI/ 里面。
 *
 * 这个文件在 Core/ 下，SquareLine 永远碰不到。
 * ==========================================================================*/

#ifndef __UI_CUSTOM_H__
#define __UI_CUSTOM_H__

#include "lvgl.h"

/* ---------------------------------------------------------------- 切屏请求 */
/* 回调里只写这个标志，真正的 lv_scr_load_anim 在 DisplayTask 里做 */
#define SCR_REQ_NONE        0u
#define SCR_REQ_SETTING     1u
#define SCR_REQ_ALARM       2u
#define SCR_REQ_MAIN        3u
#define SCR_REQ_WIFI        4u

extern volatile uint8_t g_scr_req;

/* DisplayTask 每帧在 lv_timer_handler() 之后调一次 */
void Ui_ScreenSwitch_Handle(void);

/* ui_init() 之后调一次：挂四屏手势 + WiFi 页控件事件 + 建顶栏指示灯 */
void Ui_BindEvents(void);

/* DisplayTask 每 1 秒调一次：刷底栏三格 + 顶栏指示灯颜色 */
void Ui_RefreshStatus(void);

/* 设置页阈值/使能调试打印（由 DisplayTask 每秒调用一次） */
void Ui_CfgDebugPrint(void);

/* ------------------------------------------------------------------- WiFi */
/* WiFi 页上用户输入的账号密码，btnConnect 按下后填进来 */
#define UI_WIFI_SSID_MAX    33
#define UI_WIFI_PWD_MAX     65

extern char g_ui_wifi_ssid[UI_WIFI_SSID_MAX];
extern char g_ui_wifi_pwd[UI_WIFI_PWD_MAX];

/* 1 = 用户点了「登录」，bsp_wifi 看到就重走一遍 AT 流程，处理完自己清 0 */
extern volatile uint8_t g_ui_wifi_connect_req;

#endif /* __UI_CUSTOM_H__ */
