/* ============================================================================
 * ui_custom.c  —— 手写的 UI 逻辑，与 SquareLine 生成代码完全隔离
 *
 * 一块做四件事：
 *   1. 四屏手势 → 写 g_scr_req（标志位方案，绝不在回调里 lv_scr_load_anim）
 *   2. WiFi 页：点输入框升降键盘、点空白收键盘、点登录取字符串
 *   3. 底栏三格（系统状态 / 风扇 / WiFi）按真实数据刷新
 *   4. 顶栏时钟前的指示灯按系统状态变色
 *
 * 【键盘 Y 值怎么来的】
 *   SquareLine 里 kbWifi 是 pnlForm 的子对象，对齐 LV_ALIGN_CENTER，
 *   pnlForm 本身在屏幕正中（中心 y = 120）。
 *   导出值 y = 59  → 键盘中心 = 120 + 59 = 179，高 113，顶边 122.5、底边 235.5 ✅ 露在下半屏
 *   收起时键盘中心要 > 240 + 113/2 = 296.5 → 相对 pnlForm 中心 = 176.5
 *   取 180 留 3.5 像素余量，保证一格都看不见。
 *   ⚠ 别照抄 SquareLine 里配的 260 / 150，那两个数是按"坐标原点在屏幕左上"算的，
 *     而 lv_obj_set_y() 在 LV_ALIGN_CENTER 下是相对中心的偏移，两套坐标不能混。
 * ==========================================================================*/

#include "ui_custom.h"

#include "ui.h"             /* ui_ScreenMain / ui_taSsid ... 全部在这里 */
#include "app_state.h"      /* SysState_t, App_State_Get() */
#include "bsp_fan.h"        /* Fan_GetSpeed() */
#include "bsp_wifi.h"       /* connect_flag */
#include "alarm.h"          /* Alarm_IsSilenced / Alarm_ToggleSilence */
#include "debug_uart.h"

#include <string.h>

/* --------------------------------------------------------------- 对外变量 */
volatile uint8_t g_scr_req = SCR_REQ_NONE;

char g_ui_wifi_ssid[UI_WIFI_SSID_MAX];
char g_ui_wifi_pwd[UI_WIFI_PWD_MAX];

volatile uint8_t g_ui_wifi_connect_req = 0;
/* --------------------------------------------------------------- 内部变量 */
#define KB_Y_SHOW    59     /* 键盘露出时的 y（= SquareLine 导出值） */
#define KB_Y_HIDE    180    /* 键盘收起时的 y（完全在屏幕下方） */

static lv_obj_t *ui_dotStatus = NULL;   /* 顶栏时钟前那颗指示灯，代码创建 */
static lv_obj_t *ui_hotWifi   = NULL;   /* 底栏右侧 WiFi 透明热区，代码创建 */




/* ============================================================ 一、切屏标志 */
void Ui_ScreenSwitch_Handle(void)
{
    uint8_t req = g_scr_req;

    if (req == SCR_REQ_NONE) {
        return;
    }

    /* 先清零再切：切屏途中再来手势不会丢 */
    g_scr_req = SCR_REQ_NONE;

    /* 离开 WiFi 页统一把键盘收干净。
     * 原来只有 OnBackClicked（点在「< 返回」文字上）收键盘，
     * 点 ui_Button7 边缘空白走的是 OnNavButton，键盘就留在那儿了。
     * 这里是所有切屏的唯一出口，放这儿一次就够。 */
    if (lv_scr_act() == ui_ScreenWifi && ui_kbWifi != NULL) {
        lv_keyboard_set_textarea(ui_kbWifi, NULL);
        lv_obj_clear_state(ui_taSsid, LV_STATE_FOCUSED);
        lv_obj_clear_state(ui_taPwd,  LV_STATE_FOCUSED);
        lv_obj_set_y(ui_kbWifi, KB_Y_HIDE);
    }

    switch (req) {
    case SCR_REQ_SETTING:
        lv_scr_load_anim(ui_ScreenSetting, LV_SCR_LOAD_ANIM_MOVE_TOP,    300, 0, false);
        break;

    case SCR_REQ_ALARM:
        lv_scr_load_anim(ui_ScreenAlarm,   LV_SCR_LOAD_ANIM_MOVE_TOP,    300, 0, false);
        break;

    case SCR_REQ_MAIN:
        lv_scr_load_anim(ui_ScreenMain,    LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 300, 0, false);
        break;

    case SCR_REQ_WIFI:
        lv_scr_load_anim(ui_ScreenWifi,    LV_SCR_LOAD_ANIM_MOVE_TOP,    300, 0, false);
        break;

    default:
        break;
    }

}

/* ============================================================ 二、手势注册 */
static void OnGesture(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) {
        return;
    }

    lv_indev_t *indev = lv_indev_get_act();
    if (indev == NULL) {
        return;
    }

    lv_dir_t dir = lv_indev_get_gesture_dir(indev);

    /* ★ 探针（临时）：把位移分量一起打出来，判断「方向」和「物理动作」对不对得上。
     *   sum.x 大 = 横向位移为主；sum.y 大 = 纵向位移为主。
     *   物理上向上划，sum.y 应该是负数；向下划 sum.y 是正数。
     *   如果"向上划"打出 sum.x 很大而 sum.y 接近 0，就是触摸坐标 X/Y 装反了。测完删掉。 */
    {
        lv_point_t gs = indev->proc.types.pointer.gesture_sum;
        u1_USART("[GES] dir=%u sum=(%d,%d)\r\n", (unsigned)dir, (int)gs.x, (int)gs.y);
    }

    /* 等手指抬起再往下走：不然一次滑动会连发好几条 GESTURE */
    lv_indev_wait_release(indev);

    /* current_target 才是"事件挂在哪"，target 在冒泡时会变成原始子对象 */
    lv_obj_t *scr = lv_event_get_current_target(e);

    if (scr == ui_ScreenMain) {
        if (dir == LV_DIR_TOP) {
            g_scr_req = SCR_REQ_SETTING;
        }
    } else if (scr == ui_ScreenSetting) {
        if (dir == LV_DIR_TOP) {
            g_scr_req = SCR_REQ_ALARM;
        } else if (dir == LV_DIR_BOTTOM) {
            g_scr_req = SCR_REQ_MAIN;
        }
    } else if (scr == ui_ScreenAlarm) {
        if (dir == LV_DIR_BOTTOM) {
            g_scr_req = SCR_REQ_SETTING;
        }
    } else if (scr == ui_ScreenWifi) {
        if (dir == LV_DIR_BOTTOM) {
            g_scr_req = SCR_REQ_MAIN;
        }
    }
}

/* ============================================================ 三、键盘升降 */
static void KbSlide(uint8_t up)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ui_kbWifi);
    lv_anim_set_time(&a, 220);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, up ? KB_Y_HIDE : KB_Y_SHOW,
                           up ? KB_Y_SHOW : KB_Y_HIDE);
    lv_anim_start(&a);
}

/* ⚠ 这里用 CLICKED 而不是 FOCUSED。
 * LVGL 的 LV_STATE_FOCUSED 要靠 lv_group 驱动，本工程没建 group，
 * 点 textarea 永远不会进 FOCUSED 状态，挂 FOCUSED 回调等于没挂。
 * 而 lv_keyboard_set_textarea() 内部自己会给 ta 打上 FOCUSED，
 * 所以只要"点了"就够，焦点由 keyboard 替我们管。 */
static void TaClicked(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);

    if (ta != ui_taSsid && ta != ui_taPwd) {
        return;
    }

    lv_keyboard_set_textarea(ui_kbWifi, ta);   /* 绑对框，这句是关键 */
    lv_obj_clear_flag(ui_kbWifi, LV_OBJ_FLAG_HIDDEN);
    KbSlide(1);
}

static void TaDefocused(lv_event_t *e)
{
    (void)e;
    lv_keyboard_set_textarea(ui_kbWifi, NULL);
    KbSlide(0);
}

/* 点空白收键盘。
 * 挂在 ui_ScreenWifi 上而不是 pnlWifi：pnlWifi 只是顶栏（327×41），
 * 而 pnlForm 被清掉了 CLICKABLE 标志，两个都收不到"点空白"的点击。 */
static void WifiBgClicked(lv_event_t *e)
{
    lv_obj_t *t = lv_event_get_target(e);

    /* 点输入框时事件会冒泡上来，这里必须放行，否则键盘刚弹就被收回去 */
    if (t == ui_taSsid || t == ui_taPwd) {
        return;
    }

    lv_obj_t *ta = lv_keyboard_get_textarea(ui_kbWifi);
    lv_keyboard_set_textarea(ui_kbWifi, NULL);
    if (ta != NULL) {
        lv_obj_clear_state(ta, LV_STATE_FOCUSED);
    }
    KbSlide(0);
}

/* 主屏底栏点「WiFi 已连」那一格 → 进 WiFi 页
 * ⚠ label 默认不接收点击（LV_OBJ_FLAG_CLICKABLE 默认为关），
 *   所以这两个 label 必须显式打开 flag，否则四个字点了毫无反应。 */
static void OnWifiClicked(lv_event_t *e)
{
    (void)e;
    u1_USART("[UI] wifi clicked\r\n");   /* 点不动时先看这行有没有出来 */
    g_scr_req = SCR_REQ_WIFI;
}

/* WiFi 页左上角「< 返回」 */
static void OnBackClicked(lv_event_t *e)
{
    (void)e;

    /* 走的时候把键盘收干净，否则回来时它还挂在那儿 */
    if (ui_kbWifi != NULL) {
        lv_keyboard_set_textarea(ui_kbWifi, NULL);
        lv_obj_set_y(ui_kbWifi, KB_Y_HIDE);
    }
    lv_obj_clear_state(ui_taSsid, LV_STATE_FOCUSED);
    lv_obj_clear_state(ui_taPwd,  LV_STATE_FOCUSED);

    g_scr_req = SCR_REQ_MAIN;
}

/* ============================================================================
 * SquareLine 里新加的 7 个 Button，导出时全都挂了 ui_event_ButtonX
 * （里面是 _ui_screen_change）。老师在回调里不许切屏，所以：
 *   ① Ui_BindEvents() 里按函数指针摘掉
 *   ② 换成下面这套「只写标志位」的回调
 *
 * 映射表（2026-11-14 导出后核对）：
 *   ui_Button1  主屏「设置页」  → SCR_REQ_SETTING
 *   ui_Button2  主屏「警告页」  → SCR_REQ_ALARM
 *   ui_Button4  主屏「WiFi」    → SCR_REQ_WIFI
 *   ui_Button3  设置页「警告页」→ SCR_REQ_ALARM
 *   ui_Button5  设置页「< 首页」→ SCR_REQ_MAIN
 *   ui_Button6  报警页「< 返回」→ SCR_REQ_MAIN
 *   ui_Button7  WiFi 页「< 返回」→ SCR_REQ_MAIN
 * ==========================================================================*/

/* 这几个回调内容几乎一样，用一个带参数的统一处理：
 * 悬空指针存请求值，省得写七个函数。LVGL 的 user_data 正是干这个的。 */
static void OnNavButton(lv_event_t *e)
{
    uint32_t req = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

    if (req != SCR_REQ_NONE) {
        u1_USART("[UI] btn nav req=%u\r\n", (unsigned)req);
        g_scr_req = (uint8_t)req;
    }
}

/* ---- 设置页阈值数值显示（运行时创建，不依赖 SquareLine）---- */
static lv_obj_t *s_labTempVal = NULL;
static lv_obj_t *s_labHumiVal = NULL;

/* ---- 设置页顶栏状态灯 ----
 * SquareLine 那个 ui_imgBack 其实是 assets/电力_绿灯16.png —— 一个 16×16 的绿点，
 * 名字叫 Back（返回）纯属命名事故。它是静态图片，永远不会变色。
 * 这里用代码画一个圆点盖住它位置，交给 Ui_RefreshStatus 控色。 */
static lv_obj_t *s_dotSetting = NULL;

/* 设置页顶栏标题「设置」——存指针，以后要改文字（比如加状态后缀）不用遍历子对象 */
static lv_obj_t *s_labSetTitleHdr = NULL;

/* ==================== 设置页：温度阈值滑块 ==================== */
static void OnTempThChanged(lv_event_t *e)
{
    int32_t v = lv_slider_get_value(lv_event_get_target(e));

    /* 界面滑块给的是整数摄氏度 */
    App_State_SetThreshold(TH_TEMP_HIGH, (float)v);

    /* 迟滞自动跟着走：进 35 出 33，安全带 2℃ */
    App_State_SetThreshold(TH_TEMP_LOW, (float)v - 2.0f);

    /* 数值实时跟手 —— 没有这行，用户拖滑块不知道拖到多少 */
    if (s_labTempVal != NULL) {
        lv_label_set_text_fmt(s_labTempVal, "%dC", (int)v);
    }
}



/* ==================== 设置页：湿度阈值滑块 ==================== */
static void OnHumiThChanged(lv_event_t *e)
{
    int32_t v = lv_slider_get_value(lv_event_get_target(e));

    App_State_SetThreshold(TH_HUMI_HIGH, (float)v);
    App_State_SetThreshold(TH_HUMI_LOW,  (float)v - 5.0f);

    if (s_labHumiVal != NULL) {
        lv_label_set_text_fmt(s_labHumiVal, "%d%%", (int)v);
    }
}


/* ==================== 设置页：阈值数值 label（运行时建） ====================
 * SquareLine 里没做数值显示，这里补两个纯 label。
 * 位置对齐两个滑块右端上方：滑块 x=0 y=-74/-31，宽 303；
 * label 放 x=105（相对 pnlTempRow 中心）刚好在滑块右端外侧。
 * ------------------------------------------------------------------- */
static void CreateThValueLabels(void)
{
    if (ui_pnlTempRow == NULL) return;

    s_labTempVal = lv_label_create(ui_pnlTempRow);
    lv_obj_set_style_text_font(s_labTempVal, &ui_font_KaiTi12, 0);
    lv_obj_set_style_text_color(s_labTempVal, lv_color_hex(0x1565C0), 0);
    lv_obj_align(s_labTempVal, LV_ALIGN_CENTER, 120, -92);   /* 跟"温度阈值"同一行，靠右 */
    lv_label_set_text_fmt(s_labTempVal, "%dC", (int)App_State_GetThreshold(TH_TEMP_HIGH));

    s_labHumiVal = lv_label_create(ui_pnlTempRow);
    lv_obj_set_style_text_font(s_labHumiVal, &ui_font_KaiTi12, 0);
    lv_obj_set_style_text_color(s_labHumiVal, lv_color_hex(0x1565C0), 0);
    lv_obj_align(s_labHumiVal, LV_ALIGN_CENTER, 120, -50);   /* 跟"湿度阈值"同一行，靠右 */
    lv_label_set_text_fmt(s_labHumiVal, "%d%%", (int)App_State_GetThreshold(TH_HUMI_HIGH));
}


/* ==================== 设置页：报警代码上下箭头 ====================
 * ui_imgCodeUp / ui_imgCodeDown 是 lv_img —— 默认不可点，
 * 必须手动加 CLICKABLE 才会进命中判定。
 * roller 的索引范围 0~(cnt-1)，越界直接丢弃，别让 LVGL 内部数组跑飞。
 * ------------------------------------------------------------- */
static void RollerStep(int8_t dir)
{
    uint16_t sel = lv_roller_get_selected(ui_rolCodeList);

    if (dir > 0) {
        if (sel + 1u < 8u) lv_roller_set_selected(ui_rolCodeList, (uint16_t)(sel + 1u), LV_ANIM_ON);
    } else {
        if (sel > 0u)      lv_roller_set_selected(ui_rolCodeList, (uint16_t)(sel - 1u), LV_ANIM_ON);
    }
}

static void OnCodeUpClicked(lv_event_t *e)   { (void)e; RollerStep(-1); }
static void OnCodeDownClicked(lv_event_t *e) { (void)e; RollerStep(+1); }


/* ==================== 报警页：静音开关 ====================
 * 开关是「绝对状态」，Alarm_ToggleSilence() 是「相对翻转」。
 * 直接 toggle 会在 Alarm_Apply 把 s_silenced 清 0 之后永久错位，
 * 症状是"拨掉反而静音" —— 所以必须先读、再比、后写。
 * ------------------------------------------------------------- */
static void OnMuteSwChanged(lv_event_t *e)
{
    (void)e;
    uint8_t want = lv_obj_has_state(ui_swMute, LV_STATE_CHECKED) ? 1u : 0u;

    if (Alarm_IsSilenced() != want) {
        Alarm_ToggleSilence();
    }
}

static void OnMuteLabelClicked(lv_event_t *e)
{
    (void)e;
    if (Alarm_IsSilenced()) {
        Alarm_ToggleSilence();
        lv_obj_clear_state(ui_swMute, LV_STATE_CHECKED);
    } else {
        Alarm_ToggleSilence();
        lv_obj_add_state(ui_swMute, LV_STATE_CHECKED);
    }
}


/* ==================== 设置页：四类报警使能 ==================== */
static void OnEnableChanged(lv_event_t *e)
{
    lv_obj_t *chk = lv_event_get_target(e);

    /* 读勾选状态只有一个办法：查 CHECKED 标志 */
    uint8_t on = lv_obj_has_state(chk, LV_STATE_CHECKED) ? 1u : 0u;

    if (chk == ui_chkTempEn)      App_State_SetEnable(EN_TEMP, on);
    else if (chk == ui_chkHumiEn) App_State_SetEnable(EN_HUMI, on);
    else if (chk == ui_chkFireEn) App_State_SetEnable(EN_FIRE, on);
    else if (chk == ui_chkElecEn) App_State_SetEnable(EN_ELEC, on);
}

/* ==================== 设置页：报警代码说明文字 ====================
 * roller 里选中第几条，右下那行"03=明火报警"就跟着变。
 * 索引是 0~7，对应 01~08。
 * ------------------------------------------------------------- */
static const char *s_code_desc[8] = {
    "01=温度过高",
    "02=湿度过高",
    "03=明火报警",
    "04=用电异常",
    "05=风扇故障",
    "06=WiFi断连",
    "07=传感器故障",
    "08=系统重启",
};

static void OnCodeChanged(lv_event_t *e)
{
    uint16_t sel = lv_roller_get_selected(lv_event_get_target(e));

    /* roller 的索引是 uint16_t，越界会让 LVGL 内部数组跑飞，必须先挡 */
    if (sel < 8u) {
        lv_label_set_text(ui_labCodeHint, s_code_desc[sel]);
    }
}

/* ============================================================================
 * 设置页参数调试打印 —— 由 DisplayTask 每秒调用一次
 *
 * 三条纪律：
 *   ① 必须在函数体里。u1_USART 是宏，裸放在全局会被当成函数声明 → 一串报错
 *   ② 不能在 OnTempThChanged 里打。VALUE_CHANGED 拖动时每像素触发一次，
 *      303 像素的滑块最多 303 次，串口 115200 打一行 0.9ms，累计 270ms 界面卡死
 *   ③ 全用 %d 不用 %.0f。本工程是 MicroLIB，vsnprintf 不支持浮点格式
 * ========================================================================= */
void Ui_CfgDebugPrint(void)
{
    u1_USART("[CFG] t=%d/%d h=%d/%d en=%u%u%u%u\r\n",
             (int)App_State_GetThreshold(TH_TEMP_HIGH),   /* 温度进 WARN 门槛 */
             (int)App_State_GetThreshold(TH_TEMP_LOW),    /* 温度退 WARN 门槛 */
             (int)App_State_GetThreshold(TH_HUMI_HIGH),   /* 湿度进 WARN 门槛 */
             (int)App_State_GetThreshold(TH_HUMI_LOW),    /* 湿度退 WARN 门槛 */
             (unsigned)App_State_GetEnable(EN_TEMP),      /* 温度报警使能 */
             (unsigned)App_State_GetEnable(EN_HUMI),      /* 湿度报警使能 */
             (unsigned)App_State_GetEnable(EN_FIRE),      /* 明火报警使能 */
             (unsigned)App_State_GetEnable(EN_ELEC));     /* 用电报警使能 */
}



/* 点「登录」：取两个框的字符串，交给 bsp_wifi */
static void BtnConnectClicked(lv_event_t *e)
{
    (void)e;

    const char *s = lv_textarea_get_text(ui_taSsid);
    const char *p = lv_textarea_get_text(ui_taPwd);

    if (s == NULL) s = "";
    if (p == NULL) p = "";

    strncpy(g_ui_wifi_ssid, s, UI_WIFI_SSID_MAX - 1);
    g_ui_wifi_ssid[UI_WIFI_SSID_MAX - 1] = '\0';

    strncpy(g_ui_wifi_pwd, p, UI_WIFI_PWD_MAX - 1);
    g_ui_wifi_pwd[UI_WIFI_PWD_MAX - 1] = '\0';

    /* 收起键盘 */
    lv_keyboard_set_textarea(ui_kbWifi, NULL);
    lv_obj_clear_state(ui_taSsid, LV_STATE_FOCUSED);
    lv_obj_clear_state(ui_taPwd,  LV_STATE_FOCUSED);
    KbSlide(0);

    g_ui_wifi_connect_req = 1;

    u1_USART("[WIFI] ssid=%s pwd_len=%u\r\n", g_ui_wifi_ssid,
             (unsigned)strlen(g_ui_wifi_pwd));
}

/* ============================================ 四、底栏 WiFi 热区 + 顶栏指示灯 */

/* 底栏右侧 1/3 铺一块透明可点区域。
 * 单纯给 ui_labWifi 加 CLICKABLE 也能用，但那是 30×14 的一小块，
 * 手指落偏两个像素就没反应，用起来像坏的。
 * 热区 110×31，盖住整个"WiFi 已连"那一格，怎么点都中。
 * 不画任何东西（OPA_TRANSP + remove_style_all），纯逻辑热区。 */
static void CreateWifiHotzone(void)
{
    if (ui_hotWifi != NULL || ui_pnlBottom == NULL) {
        return;
    }

    ui_hotWifi = lv_obj_create(ui_pnlBottom);
    lv_obj_remove_style_all(ui_hotWifi);
    lv_obj_set_size(ui_hotWifi, 110, 31);
    lv_obj_set_align(ui_hotWifi, LV_ALIGN_RIGHT_MID);
    lv_obj_set_style_bg_opa(ui_hotWifi, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(ui_hotWifi, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_hotWifi, LV_OBJ_FLAG_SCROLLABLE);

    /* 按下反馈：平时全透明，按住时浮一层淡蓝，松手就消失。
     * 这块热区盖着 Button4，用户点 WiFi 全靠它，没反馈等于闭眼操作。 */
    lv_obj_set_style_bg_color(ui_hotWifi, lv_color_hex(0x1E88E5), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_hotWifi, 70, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(ui_hotWifi, 6, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_ext_click_area(ui_hotWifi, 4);

    lv_obj_add_event_cb(ui_hotWifi, OnWifiClicked, LV_EVENT_CLICKED, NULL);
}

/* 原图 ui_imgStatus 是一张固定颜色的 16px PNG，改不了颜色。
 * 这里画一个同位置的圆点盖上去，颜色代码里随便改。
 * 用圆点而不是换图：零素材、零 Flash、SquareLine 重新导出也不受影响。 */
static void CreateStatusDot(void)
{
    if (ui_dotStatus != NULL || ui_pnlTop == NULL) {
        return;
    }

    ui_dotStatus = lv_obj_create(ui_pnlTop);
    lv_obj_remove_style_all(ui_dotStatus);
    lv_obj_set_size(ui_dotStatus, 10, 10);
    lv_obj_set_style_radius(ui_dotStatus, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ui_dotStatus, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(ui_dotStatus, lv_color_hex(0x11EB31), 0);

    lv_obj_set_x(ui_dotStatus, 102);        /* 顶到原 imgStatus 的位置 */
    lv_obj_set_y(ui_dotStatus, 0);
    lv_obj_set_align(ui_dotStatus, LV_ALIGN_CENTER);

    lv_obj_clear_flag(ui_dotStatus, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* 原图藏掉，不删 —— SquareLine 重新导出还会把它加回来 */
    if (ui_imgStatus != NULL) {
        lv_obj_add_flag(ui_imgStatus, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ============================================================ 五、状态刷新 */
/* ==================== 报警页刷新 ====================
 * 报警页的 6 个 label 全是 SquareLine 写死的静态文字，从来不更新。
 * 这个函数每秒跑一次，把真实状态、真实数值、真实时刻灌进去。
 *
 * 没有 RTC：时间用"开机后累计到本次状态开始的秒数"，等价于发生时刻。
 * MicroLIB 不支持 %f：温湿度一律 (int) 取整。
 * ------------------------------------------------------------- */
static void RefreshAlarmPage(void)
{
    if (ui_labAlmReason == NULL || ui_labAlmVal == NULL) return;

    SysState_t    st = App_State_Get();
    AlarmReason_t rs = App_State_GetReason();
    lv_color_t    col;

    switch (st) {
    case SYS_ALARM:
        col = lv_color_hex(0xFF2020);
        if (rs == ALARM_FLAME) {
            lv_label_set_text(ui_labAlmReason, "明火报警");
            lv_label_set_text(ui_labAlmVal, "检测到明火，已全速排风");
        } else {
            lv_label_set_text(ui_labAlmReason, "用电异常");
            lv_label_set_text(ui_labAlmVal, "电压或电流超出安全范围");
        }
        break;

    case SYS_WARN:
        col = lv_color_hex(0xFFC107);
        if (rs == ALARM_HUMI_HIGH) {
            lv_label_set_text(ui_labAlmReason, "湿度过高");
            lv_label_set_text_fmt(ui_labAlmVal, "湿度 %d%% / 阈值 %d%%",
                                  (int)App_State_GetHumi(),
                                  (int)App_State_GetThreshold(TH_HUMI_HIGH));
        } else {
            lv_label_set_text(ui_labAlmReason, "温度过高");
            lv_label_set_text_fmt(ui_labAlmVal, "温度 %dC / 阈值 %dC",
                                  (int)App_State_GetTemp(),
                                  (int)App_State_GetThreshold(TH_TEMP_HIGH));
        }
        break;

    default:
        col = lv_color_hex(0x11EB31);
        lv_label_set_text(ui_labAlmReason, "系统正常");
        lv_label_set_text(ui_labAlmVal, "各项参数均在安全范围");
        break;
    }

    lv_obj_set_style_text_color(ui_labAlmReason, col, 0);

    /* 发生时间 = 本次状态开始的时刻（开机累计秒） */
    {
        uint32_t sec = App_State_GetStateMs() / 1000u;
        lv_label_set_text_fmt(ui_labAlmTime, "%02u:%02u:%02u",
                              (unsigned)(sec / 3600u),
                              (unsigned)((sec / 60u) % 60u),
                              (unsigned)(sec % 60u));
    }

    /* 底栏一句话 + 静音开关对账 */
    if (ui_labAlmState != NULL) {
        lv_label_set_text(ui_labAlmState,
                          (st == SYS_ALARM) ? "报警保持中"
                                            : ((st == SYS_WARN) ? "预警中" : "系统正常"));
    }

    if (ui_swMute != NULL) {
        uint8_t real = Alarm_IsSilenced();
        uint8_t show = lv_obj_has_state(ui_swMute, LV_STATE_CHECKED) ? 1u : 0u;

        if (real != show) {
            if (real) lv_obj_add_state(ui_swMute, LV_STATE_CHECKED);
            else      lv_obj_clear_state(ui_swMute, LV_STATE_CHECKED);
        }
    }
}


void Ui_RefreshStatus(void)
{
    /* ---------- 顶栏指示灯 ---------- */
    if (ui_dotStatus != NULL) {
        lv_color_t c;
        switch (App_State_Get()) {
        case SYS_ALARM: c = lv_color_hex(0xFF3030); break;   /* 红 */
        case SYS_WARN:  c = lv_color_hex(0xFFC107); break;   /* 黄 */
        default:        c = lv_color_hex(0x11EB31); break;   /* 绿 */
        }
        lv_obj_set_style_bg_color(ui_dotStatus, c, 0);
    }

    /* ---------- 设置页顶栏指示灯（跟主页同一个状态源）---------- */
    if (s_dotSetting != NULL) {
        lv_color_t c;
        switch (App_State_Get()) {
        case SYS_ALARM: c = lv_color_hex(0xFF3030); break;   /* 红 */
        case SYS_WARN:  c = lv_color_hex(0xFFC107); break;   /* 黄 */
        default:        c = lv_color_hex(0x11EB31); break;   /* 绿 */
        }
        lv_obj_set_style_bg_color(s_dotSetting, c, 0);
    }

    /* ---------- 底栏：系统状态 ---------- */
    if (ui_labSysState != NULL) {
        switch (App_State_Get()) {
        case SYS_ALARM:
            lv_label_set_text(ui_labSysState, "报警");
            lv_obj_set_style_text_color(ui_labSysState, lv_color_hex(0xFF2020), 0);
            break;
        case SYS_WARN:
            lv_label_set_text(ui_labSysState, "预警");
            lv_obj_set_style_text_color(ui_labSysState, lv_color_hex(0xFFC107), 0);
            break;
        default:
            lv_label_set_text(ui_labSysState, "正常");
            lv_obj_set_style_text_color(ui_labSysState, lv_color_hex(0x11EB31), 0);
            break;
        }
    }

    /* ---------- 底栏：风扇 ----------
     * 读真实转速，不从系统状态推导。
     * 因为云端能手动接管风扇（Cloud_IsFanOverridden），那时"报警 + 风扇关闭"
     * 是真实存在的组合，按状态推导等于骗人。 */
    if (ui_labFanState != NULL) {
        uint16_t sp = Fan_GetSpeed();
        if (sp == 0) {
            lv_label_set_text(ui_labFanState, "关闭");
        } else if (sp < 500) {
            lv_label_set_text(ui_labFanState, "低速");
        } else if (sp < 900) {
            lv_label_set_text(ui_labFanState, "中速");
        } else {
            lv_label_set_text(ui_labFanState, "全速");
        }
    }

    /* ---------- 底栏：WiFi ---------- */
    if (ui_labWifiState != NULL) {
        lv_label_set_text(ui_labWifiState, connect_flag ? "已连" : "未连");
    }

    /* ---------- 报警页：原因 / 数值 / 时刻 / 静音对账 ---------- */
    RefreshAlarmPage();
}

/* ============================================================ 六、统一注册 */
/* ui_custom.c —— Ui_BindEvents() 里，checkbox 回填之前加这段 */
void Ui_BindEvents(void)
{
	
	{
    /* 四个 checkbox 补 CHECKED 样式：SquareLine 只配了 DEFAULT 状态，
     * 而 ui_font_KaiTiXX 里没有 U+F00C（LV_SYMBOL_OK），勾选符号画不出来，
     * 四个框全是蓝块，肉眼分不清哪个开了。这里白底=关、蓝底=开。 */
    lv_obj_t *chks[4] = { ui_chkTempEn, ui_chkHumiEn, ui_chkFireEn, ui_chkElecEn };
    for (int i = 0; i < 4; i++) {
        if (chks[i] == NULL) continue;
        /* 未勾选：白底灰框 */
        lv_obj_set_style_bg_color(chks[i], lv_color_hex(0xFFFFFF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(chks[i], LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(chks[i], lv_color_hex(0x9AA0A6), LV_PART_INDICATOR);
        lv_obj_set_style_border_width(chks[i], 2, LV_PART_INDICATOR);

        /* 勾选：蓝底白框 —— 一眼分得出来 */
        lv_obj_set_style_bg_color(chks[i], lv_color_hex(0x29BFF3), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(chks[i], LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(chks[i], lv_color_hex(0x0D7FB0), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_border_width(chks[i], 2, LV_PART_INDICATOR | LV_STATE_CHECKED);

        lv_obj_set_style_pad_all(chks[i], 2, LV_PART_INDICATOR);
        lv_obj_set_style_radius(chks[i], 3, LV_PART_INDICATOR);
    }
}
	
    /* ---------------------------------------------------------------
     * 2026-11-14 更新：SquareLine 里已经改成「按钮级事件」了，
     * 屏幕级的 ui_event_ScreenMain / ui_event_ScreenSetting /
     * ui_event_ScreenAlarm / ui_event_taSsid 全部不存在了，所以不用再摘。
     *
     * 现在的 _ui_screen_change 全在七个 ui_event_ButtonX 里，
     * 在下面统一摘（见「七个导航按钮」那一段）。
     * ------------------------------------------------------------- */

    /* 键盘先拨到收起位：SquareLine 里配的是露出位，运行时要先从下面藏着 */
    if (ui_kbWifi != NULL) {
        lv_obj_set_y(ui_kbWifi, KB_Y_HIDE);
        lv_keyboard_set_textarea(ui_kbWifi, NULL);
    }

    /* 四屏手势 */
    /* 手势切屏已停用：只能靠按钮跳转。
     * OnGesture 留着不删（以后可能要用），只是不再挂到任何屏上。 */

    /* 主屏底栏：点 WiFi 那一格进 WiFi 页 */
    if (ui_labWifi != NULL) {
        lv_obj_add_flag(ui_labWifi, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui_labWifi, OnWifiClicked, LV_EVENT_CLICKED, NULL);
    }
    if (ui_labWifiState != NULL) {
        lv_obj_add_flag(ui_labWifiState, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui_labWifiState, OnWifiClicked, LV_EVENT_CLICKED, NULL);
    }

    /* WiFi 页：两个输入框点一下就弹键盘（用 CLICKED，不依赖 lv_group） */
    lv_obj_add_event_cb(ui_taSsid, TaClicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_taPwd,  TaClicked, LV_EVENT_CLICKED, NULL);

    /* SquareLine 里把「账号」框也勾了密码模式，账号不该打码 */
    lv_textarea_set_password_mode(ui_taSsid, false);

    /* ---------------------------------------------------------------
     * 摘掉 SquareLine 给两个输入框挂的 ui_event_taXxx。
     * 它里面配的是 _ui_flag_modify(ui_kbWifi, LV_OBJ_FLAG_HIDDEN,
     * _UI_MODIFY_FLAG_ADD) —— 点输入框把键盘「隐藏」，方向配反了。
     * 现在靠 TaClicked 里那句 clear_flag 在后面兜住才没出事，
     * 但那是赌事件注册顺序，靠不住。这里直接摘掉，只留 TaClicked 一套。
     * ------------------------------------------------------------- */
    lv_obj_remove_event_cb(ui_taSsid, ui_event_taSsid);
    lv_obj_remove_event_cb(ui_taPwd,  ui_event_taPwd);

    /* 占位文字清空：原来挂着 "admin" / "process"，空框里灰字容易误以为已填好。
     * 注意 get_text() 空的时候本来就返回 ""，占位字不参与逻辑，纯粹视觉问题。 */
    lv_textarea_set_placeholder_text(ui_taSsid, "");
    lv_textarea_set_placeholder_text(ui_taPwd,  "");

    /* 左上角「< 返回」回主屏 —— 免得只能靠下滑手势出去 */
    if (ui_labBack1 != NULL) {
        lv_obj_add_flag(ui_labBack1, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui_labBack1, OnBackClicked, LV_EVENT_CLICKED, NULL);
    }

    /* WiFi 页：点空白收键盘、点登录连接 */
    lv_obj_add_event_cb(ui_ScreenWifi, WifiBgClicked,    LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btnConnect, BtnConnectClicked, LV_EVENT_CLICKED, NULL);

    /* ---------------------------------------------------------------
     * 七个导航按钮：SquareLine 导出时给每个都挂了 ui_event_ButtonX，
     * 里面是 _ui_screen_change(...)。在回调里切屏会把后续 RELEASED
     * 发给已经换掉的屏 —— 老师明确不许。
     *
     * 做法同前面：不改生成文件，运行时按函数指针精确摘掉，换成写标志位。
     * 每次 SquareLine 重新导出它都会回来，所以这段必须留着。
     * ------------------------------------------------------------- */
    lv_obj_remove_event_cb(ui_Button1, ui_event_Button1);   /* 主屏 → 设置页 */
    lv_obj_remove_event_cb(ui_Button2, ui_event_Button2);   /* 主屏 → 警告页 */
    lv_obj_remove_event_cb(ui_Button4, ui_event_Button4);   /* 主屏 → WiFi  */
    lv_obj_remove_event_cb(ui_Button3, ui_event_Button3);   /* 设置页 → 警告页 */
    lv_obj_remove_event_cb(ui_Button5, ui_event_Button5);   /* 设置页 → 首页 */
    lv_obj_remove_event_cb(ui_Button6, ui_event_Button6);   /* 报警页 → 返回 */
    lv_obj_remove_event_cb(ui_Button7, ui_event_Button7);   /* WiFi 页 → 返回 */

    /* 换成标志位通路：user_data 直接塞请求码，七个按钮共用一个回调 */
    if (ui_Button1 != NULL) lv_obj_add_event_cb(ui_Button1, OnNavButton, LV_EVENT_CLICKED, (void *)(uintptr_t)SCR_REQ_SETTING);
    if (ui_Button2 != NULL) lv_obj_add_event_cb(ui_Button2, OnNavButton, LV_EVENT_CLICKED, (void *)(uintptr_t)SCR_REQ_ALARM);
    if (ui_Button4 != NULL) lv_obj_add_event_cb(ui_Button4, OnNavButton, LV_EVENT_CLICKED, (void *)(uintptr_t)SCR_REQ_WIFI);
    if (ui_Button3 != NULL) lv_obj_add_event_cb(ui_Button3, OnNavButton, LV_EVENT_CLICKED, (void *)(uintptr_t)SCR_REQ_ALARM);
    if (ui_Button5 != NULL) lv_obj_add_event_cb(ui_Button5, OnNavButton, LV_EVENT_CLICKED, (void *)(uintptr_t)SCR_REQ_MAIN);
    if (ui_Button6 != NULL) lv_obj_add_event_cb(ui_Button6, OnNavButton, LV_EVENT_CLICKED, (void *)(uintptr_t)SCR_REQ_MAIN);
    if (ui_Button7 != NULL) lv_obj_add_event_cb(ui_Button7, OnNavButton, LV_EVENT_CLICKED, (void *)(uintptr_t)SCR_REQ_MAIN);

    /* ---------------------------------------------------------------
     * 按下反馈 + 放大点击热区
     *
     * SquareLine 里一个 PRESSED 状态样式都没配，手指按下去界面纹丝不动，
     * 根本分不清是「没点到」还是「点到了但没跳」。这里在运行时补一套：
     *   - 按下 → 背景压深、边框加粗变蓝
     *   - 热区往外扩 8px，手指偏一点也能命中
     *
     * lv_obj_set_ext_click_area 只扩可点区域，不改绘制尺寸，
     * 所以按钮看起来还是 75×50，但实际能点的范围是 91×66。
     * ------------------------------------------------------------- */
    {
        lv_obj_t *nav[7] = { ui_Button1, ui_Button2, ui_Button3,
                             ui_Button4, ui_Button5, ui_Button6, ui_Button7 };
        lv_color_t press_bg     = lv_color_hex(0x1565C0);   /* 按下的深蓝，对比拉满 */
        lv_color_t press_border = lv_color_hex(0xFF9800);   /* 按下的橙边，一眼能看见 */
        uint8_t i;
        for (i = 0; i < 7; i++) {
            if (nav[i] == NULL) continue;
            lv_obj_set_style_bg_color(nav[i], press_bg, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_color(nav[i], press_border, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_width(nav[i], 3, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_ext_click_area(nav[i], 8);
        }
    }

    /* 登录按钮也给一样的按下反馈 */
    if (ui_btnConnect != NULL) {
        lv_obj_set_style_bg_color(ui_btnConnect, lv_color_hex(0x1565C0), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_color(ui_btnConnect, lv_color_hex(0x0D47A1), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_width(ui_btnConnect, 2, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_ext_click_area(ui_btnConnect, 6);
    }

    /* 底栏 WiFi 文字和「< 返回」都是 label 当按钮用，按下改字色 */
    if (ui_labWifi != NULL) {
        lv_obj_set_style_text_color(ui_labWifi, lv_color_hex(0x1E88E5), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_ext_click_area(ui_labWifi, 6);
    }
    if (ui_labBack1 != NULL) {
        lv_obj_set_style_text_color(ui_labBack1, lv_color_hex(0x1E88E5), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_ext_click_area(ui_labBack1, 8);
    }

    /* ===============================================================
     * 设置页：滑块 + 复选框 + 报警代码 roller
     *
     * 顺序：set_range → set_value → add_state → 挂事件。
     * 先挂事件再 set_value，设值会触发一次 VALUE_CHANGED，
     * 万一回调里有副作用就跑飞了。
     *
     * ★ 这一整块是"让前面那些回调真正活起来"的地方。
     *   少一行 add_event_cb，对应控件就永远没反应，
     *   而且编译 0 Error 0 Warning，一点提示都没有。
     * =============================================================== */

    /* ---- ① 滑块范围（LVGL 默认 0~100，温度必须改） ---- */
    lv_slider_set_range(ui_sldTempTh,    20, 60);      /* 温度 20~60℃ */
    lv_slider_set_range(ui_labTempThVal, 50, 100);     /* 湿度 50~100%RH（这个名字是滑块不是label） */

    /* ---- ② 回填：把 app_state 里的真实阈值灌进界面 ---- */
    lv_slider_set_value(ui_sldTempTh,    (int32_t)App_State_GetThreshold(TH_TEMP_HIGH), LV_ANIM_OFF);
    lv_slider_set_value(ui_labTempThVal, (int32_t)App_State_GetThreshold(TH_HUMI_HIGH), LV_ANIM_OFF);

    /* ---- ③ 复选框回填（不加 else-clear，切页回来会被旧状态污染） ---- */
    if (App_State_GetEnable(EN_TEMP)) lv_obj_add_state(ui_chkTempEn, LV_STATE_CHECKED);
    else                              lv_obj_clear_state(ui_chkTempEn, LV_STATE_CHECKED);

    if (App_State_GetEnable(EN_HUMI)) lv_obj_add_state(ui_chkHumiEn, LV_STATE_CHECKED);
    else                              lv_obj_clear_state(ui_chkHumiEn, LV_STATE_CHECKED);

    if (App_State_GetEnable(EN_FIRE)) lv_obj_add_state(ui_chkFireEn, LV_STATE_CHECKED);
    else                              lv_obj_clear_state(ui_chkFireEn, LV_STATE_CHECKED);

    if (App_State_GetEnable(EN_ELEC)) lv_obj_add_state(ui_chkElecEn, LV_STATE_CHECKED);
    else                              lv_obj_clear_state(ui_chkElecEn, LV_STATE_CHECKED);

    /* ---- ④ roller 补成 8 条（SquareLine 里只配了 "01\n02\n03\n04"） ---- */
    lv_roller_set_options(ui_rolCodeList,
        "01 温度过高\n"
        "02 湿度过高\n"
        "03 明火报警\n"
        "04 用电异常\n"
        "05 风扇故障\n"
        "06 WiFi断连\n"
        "07 传感器故障\n"
        "08 系统重启",
        LV_ROLLER_MODE_NORMAL);

    lv_label_set_text(ui_labCodeHint1, "共8条");     /* 跟 roller 实际条数对上 */

    /* ---- ⑤ 挂事件：没有这 8 行，上面写的回调全是死代码 ---- */
    lv_obj_add_event_cb(ui_sldTempTh,    OnTempThChanged, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_labTempThVal, OnHumiThChanged, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_add_event_cb(ui_chkTempEn, OnEnableChanged, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_chkHumiEn, OnEnableChanged, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_chkFireEn, OnEnableChanged, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_chkElecEn, OnEnableChanged, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_add_event_cb(ui_rolCodeList, OnCodeChanged, LV_EVENT_VALUE_CHANGED, NULL);

    /* ---- ⑥ 阈值数值 label（SquareLine 里没做，运行时补） ---- */
    CreateThValueLabels();

    /* ---- ⑦ 上下箭头：lv_img 默认不可点，必须手动加 CLICKABLE ---- */
    if (ui_imgCodeUp != NULL) {
        lv_obj_add_flag(ui_imgCodeUp, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(ui_imgCodeUp, 6);
        lv_obj_add_event_cb(ui_imgCodeUp, OnCodeUpClicked, LV_EVENT_CLICKED, NULL);
    }
    if (ui_imgCodeDown != NULL) {
        lv_obj_add_flag(ui_imgCodeDown, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(ui_imgCodeDown, 6);
        lv_obj_add_event_cb(ui_imgCodeDown, OnCodeDownClicked, LV_EVENT_CLICKED, NULL);
    }

    /* ---- ⑧ 界面修正：顶栏超宽 / roller 太矮 / 顶栏缺标题 ---- */
    if (ui_pnlSetting != NULL) {
        lv_obj_set_width(ui_pnlSetting, 320);      /* SquareLine 配的 331 超屏 11px */
    }
    if (ui_rolCodeList != NULL) {
        lv_obj_set_height(ui_rolCodeList, 60);     /* 42 只能显 2 行，看不到上下还有 7 条 */
    }
    if (ui_pnlSetting != NULL) {
        s_labSetTitleHdr = lv_label_create(ui_pnlSetting);
        lv_obj_set_style_text_font(s_labSetTitleHdr, &ui_font_KaiTi14, 0);
        lv_obj_set_style_text_color(s_labSetTitleHdr, lv_color_hex(0x202020), 0);
        lv_label_set_text(s_labSetTitleHdr, "设置");
        lv_obj_align(s_labSetTitleHdr, LV_ALIGN_CENTER, 0, 0);

        /* 顶栏右侧状态灯：盖掉那张永远绿的静态图 */
        if (ui_imgBack != NULL) {
            lv_obj_add_flag(ui_imgBack, LV_OBJ_FLAG_HIDDEN);
        }

        s_dotSetting = lv_obj_create(ui_pnlSetting);
        lv_obj_remove_style_all(s_dotSetting);
        lv_obj_set_size(s_dotSetting, 10, 10);
        lv_obj_set_style_radius(s_dotSetting, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s_dotSetting, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(s_dotSetting, lv_color_hex(0x11EB31), 0);
        lv_obj_align(s_dotSetting, LV_ALIGN_CENTER, 143, 0);
        lv_obj_clear_flag(s_dotSetting, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }

    /* ---- ⑨ 报警页：消音开关 + 「消音」文字 ---- */
    if (ui_swMute != NULL) {
        lv_obj_add_event_cb(ui_swMute, OnMuteSwChanged, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (ui_labMute != NULL) {
        lv_obj_add_flag(ui_labMute, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(ui_labMute, 8);
        lv_obj_set_style_text_color(ui_labMute, lv_color_hex(0x1565C0),
                                    LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_add_event_cb(ui_labMute, OnMuteLabelClicked, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_update_layout(ui_ScreenMain);      /* ★ 先排版，否则 coords 全是 (0,0)-(-1,-1) */
    lv_obj_update_layout(ui_ScreenSetting);
    lv_obj_update_layout(ui_ScreenAlarm);

    CreateWifiHotzone();
    CreateStatusDot();
}
