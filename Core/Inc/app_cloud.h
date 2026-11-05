/* ============================================================
 * app_cloud.h —— 云端应用层（需求 12 / 13）
 *
 * 分工：
 *   bsp_wifi.c  只管"怎么连上"（AT 状态机、收发）
 *   app_cloud.c 管"连上之后干什么"（传什么、收什么怎么处理）
 * ============================================================ */

#ifndef __APP_CLOUD_H
#define __APP_CLOUD_H

#include <stdint.h>

/* ---- 生命周期 ---- */
void Cloud_Init(void);          /* 上电调一次：清手动覆盖状态 */

/* ---- 主循环每圈调，绝不阻塞 ---- */
void Cloud_Task(void);          /* 上传 + 收指令，内部按时间节流 */

void    Cloud_ManualExit(void);      /* 强制退出手动模式（进报警时调） */
uint8_t Cloud_IsFanOverridden(void); /* 1 = 风扇被手动接管 */
uint8_t Cloud_IsLedOverridden(void); /* 1 = LED 被手动接管 */
/* ---- 供外部查询 ---- */
uint8_t Cloud_IsManual(void);   /* 1 = 手动模式生效中 */

#endif /* __APP_CLOUD_H */