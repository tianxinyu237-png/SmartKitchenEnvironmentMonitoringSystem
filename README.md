# 智能厨房环境监测系统

> 基于 **STM32F407 + FreeRTOS + LVGL 8.3** 的厨房环境监测终端。
> 320×240 触摸屏，四屏交互界面，实时监测温度、湿度、光照、供电电压、负载电流与明火信号，
> 本地三态报警联动蜂鸣器 / 排风扇 / 马达，通过 ESP8266 上云接受远程下控。

---

## 功能特性

| 模块 | 说明 |
|---|---|
| **四屏界面** | 主页 / 设置页 / 报警页 / WiFi 配置页，按钮导航（禁用手势切屏，避免 `lv_indev_proc` 中途切屏崩溃） |
| **环境采集** | SHT20 温湿度、AP3216C 光照，走 I2C1，1 Hz 刷新 |
| **供电监测** | ADC 双通道采电压 / 电流，软件换算 + 分压比标定 |
| **明火检测** | 火焰传感器走 PB3 外部中断，双边沿触发 + 软件消抖 |
| **三态报警** | `NORMAL` / `WARN`（预警）/ `ALARM`（报警），带迟滞区间防抖动 |
| **报警联动** | ALARM 态：LED3 闪 1 Hz + 蜂鸣器间歇响 + 排风扇全速 + 马达启动；WARN 态：LED2 常亮 + 风扇中速 |
| **本地消音** | 报警页开关或 KEY3 消音，只掐声音、保留机械联锁（排风不停） |
| **WiFi 上云** | ESP8266 走 USART3，八步非阻塞状态机，断线自动重连 |
| **云端下控** | 服务器可下发 LED / 风扇 / 模式指令；**蜂鸣器与 ALARM 态风扇不接受远程关闭** |
| **阈值可调** | 设置页滑块实时调整温湿度门限与报警项使能，无需重新编译 |
| **报警代码查询** | 8 条常见故障码 roller 列表，可上下翻 |

---

## 硬件配置

**主控**：STM32F407IGH6 ｜ 1024 KB Flash ｜ 128 KB SRAM（112 + 16）｜ Cortex-M4 @ **168 MHz**（HSI 16 MHz / 8 × 168 / 2）

| 外设 | 型号 / 接口 | MCU 引脚 | 备注 |
|---|---|---|---|
| 显示屏 | ILI9341 320×240 RGB565 | PI0=DC / PI1=SCK / PI2=MISO / PI3=MOSI<br>PE2=RST / PE5=BL | 4 线 SPI，`chunk[512]` 分块刷新<br>**PI0 实为 DC 脚，任何像素段都不能拉低** |
| 触摸 | FT6336（I2C 从机 `0x38`） | PH4=SCL / PH5=SDA | I2C2，30 ms 轮询，非中断方案 |
| 温湿度 | SHT20（`0x40`） | PB6=SCL / PB7=SDA | I2C1 |
| 光照 | AP3216C（`0x1E`） | 同上（I2C1 共用） | 0~65535 lux |
| 明火 | 火焰传感器（LM393 型） | **PB3**（EXTI3） | 低电平 = 有火 |
| 人体红外 | RDA226 | PB4（EXTI4） | 辅助判断，不参与报警 |
| 槽型光电 | ITR9608 | PB2（EXTI2） | 备用通道 |
| 电压采样 | 电阻分压 → ADC1_CH9 | **PB1** | 分压比 `3.14`（实测标定） |
| 电流采样 | 采样电阻 → ADC1_CH8 | **PB0** | `1 V ↔ 100 mA`，满量程 330 mA |
| 排风扇 | PWM | **PC6**（TIM3_CH1） | ARR=999，三档：0 / 60% / 100% |
| 马达 | GPIO 推挽 | **PC7** | 仅 ALARM 态启动 |
| 蜂鸣器 | GPIO | PA15 | 无源，软件 PWM |
| 指示灯 | LED1 绿 / LED2 黄 / LED3 红 | PC4 / PC3 / PC13 | 正常绿常亮，预警黄常亮，报警红闪 |
| 按键 | KEY1 / KEY2 / KEY3 | PC9 / PC8 / PC5 | 20 ms 去抖 |
| 数码管 | 74HC595 锁存 | PB12=LATCH | 兼容旧课设外设 |
| WiFi 模块 | ESP8266 | **USART3**：PB10=TX / PB11=RX | RTOS 兼容性好于 DMA |
| 调试串口 | USB-TTL | **USART6**：PG14=TX / PG9=RX | 115200 8N1 |

> **PB3 / PB4 是 JTAG 引脚（JTDO / NJTRST）**。ST-Link 必须用 **SWD 模式**下载，否则这两个脚被 JTAG 占用，EXTI 收不到中断。

---

## 软件架构

### 任务划分（CMSIS-RTOS2）

| 任务 | 优先级 | 栈（字） | 职责 |
|---|---|---|---|
| `AlarmTask` | **Realtime** | 512 | 状态机推进 + `Alarm_Apply()` + 蜂鸣器节拍 |
| `WifiTask` | High | 1536 | ESP8266 八步状态机 + 云端收发 |
| `SensorTask` | AboveNormal | 1024 | I2C 读传感器 → 投递 `qSensorData` |
| `defaultTask` | Normal | 128 | 按键扫描 + 辅助逻辑 |
| `LogTask` | BelowNormal | 512 | 日志缓冲（当前为空壳） |
| `LCDTask` | **Low** | 1024 | LVGL 主循环 `lv_timer_handler()` + 界面刷新 |

### 数据流

```
SensorTask ──osMessageQueuePut(timeout=0)──▶ qSensorData (深度 4)
                                                   │
                                                   ▼
                                            LCDTask / DisplayTask
                                                   │
                                                   ▼
                                          App_State_SetEnv()  → s_temp_c / s_humi_rh / ...
                                                   │
                                            App_State_Task()
                                                   │
                                              SysState_t
                                                   │
                                            Alarm_Apply()
                                        ┌──────────┴──────────┐
                                    LED / 蜂鸣器        风扇 / 马达
```

### 三条铁律

1. **LVGL 不是线程安全的** —— 只有 `LCDTask` 能调 `lv_*` 接口。其它任务要改界面必须走队列或 `QueueConnection`。
2. **队列项大小必须是 `sizeof(SensorData_t)`** —— 类型不匹配会静默截断，字段全变垃圾值。
3. **`osMessageQueuePut` 的 timeout 给 0** —— 生产者是最高优先级任务，阻塞会饿死 `AlarmTask`。

---

## 界面说明

| 屏幕 | 内容 | 导航 |
|---|---|---|
| **主页** | 温湿度 / 光照 / 电压 / 电流 五项实时值 + 顶栏状态灯 + 底栏时间 | → 设置 / 报警 / WiFi |
| **设置页** | 温度阈值滑块（20~60℃）、湿度阈值滑块（50~100%）、四项报警使能勾选、报警代码 roller | → 报警 / 返回主页 |
| **报警页** | 报警原因、实测值、持续时长、消音开关 | → 返回主页 |
| **WiFi 页** | SSID / 密码输入、连接按钮、状态提示 | → 返回主页 |

切屏统一走 `g_scr_req` 请求标志，由 `Ui_ScreenSwitch_Handle()` 在 LVGL 主循环里消费。
**不使用 SquareLine 生成的 `_ui_screen_change()`** —— 它在 `lv_indev_proc()` 内部执行，
`act_obj` 还指向旧屏控件，切屏即崩溃。

---

## 报警逻辑

### 三态状态机

| 状态 | 触发条件 | 灯 | 蜂鸣器 | 风扇 | 马达 |
|---|---|---|---|---|---|
| `SYS_NORMAL` | 全部正常 | LED1 绿常亮 | 静音 | 停 | 停 |
| `SYS_WARN` | 温度或湿度超上限 | LED2 黄常亮 | 静音 | 60% 换气 | 停 |
| `SYS_ALARM` | 明火 **或** 用电异常 | LED3 红闪 1 Hz | 间歇两响 | **全速** | **启动** |

**优先级**：明火 > 用电 > 温湿度。明火 / 用电走锁存，迟滞 5 s 后自动解除。

### 门限与迟滞

温度 / 湿度门限可在设置页实时调整，默认值：

| 项 | 触发 | 退出 | 迟滞 |
|---|---|---|---|
| 温度 | > 33 ℃ | < 31 ℃ | 2 ℃ |
| 湿度 | > 60 %RH | < 55 %RH | 5 %RH |
| 明火 | PB3 读到低电平 | 火焰消失再等 5 s | `ALARM_HOLD_MS` |
| 用电 | 电压 < 4.5 V 或 > 5.5 V 或 电流 > 250 mA | 恢复正常再等 5 s | 同上 |

> **迟滞对必须成对修改，且满足 `LOW < HIGH`。** 只改上限会让门限附近每秒抖动进出 WARN，灯和蜂鸣器疯狂闪；
> 写反（进 33 出 35）则永远退不出 WARN。

### 报警项使能

四个使能位默认为 **`{1,1,1,1}`（全开）**，这是 **fail-safe 设计**：

```c
/* app_state.c —— s_en[] 默认全开是刻意的 */
if (s_en[EN_FIRE] && (Flame_IsActive() == 1u)) {   /* ← s_en 为 0 时短路，整路判定被删掉 */
    s_alarm_latch = 1;
```

**默认关闭意味着明火传感器被短路时屏幕毫无反应。** 要防误报应当调准门限，不能关掉报警能力。

> ⚠️ 使能状态 **不持久化**，每次上电 / 烧录回到全开。项目未实现 Flash 参数存储。

---

## 快速开始

### 环境要求

- **Keil MDK 5**（ARMCLANG V6.22 已验证）
- **STM32CubeMX**（仅修改外设配置时需要，`SmartKitchenEnvironmentMonitoringSystem.ioc`）
- **SquareLine Studio 1.3.x**（仅修改界面时需要，`SquareLinestidos/SquareLine_Project.spj`）
- ST-Link 驱动 + USB-TTL 串口模块
- 5 V / 2 A 电源适配器（**内正外负**）

### 编译

```bash
# GUI：双击打开
MDK-ARM/SmartKitchenEnvironmentMonitoringSystem.uvprojx

# 命令行
UV4.exe -b MDK-ARM/SmartKitchenEnvironmentMonitoringSystem.uvprojx -j0 -o build.log
```

预期输出：

```
0 Error(s), 0 Warning(s)
Program Size: Code=278216 RO-data=379344 RW-data=244 ZI-data=109800
```

### 烧录与验证

ST-Link 必须以 **SWD**（不是 JTAG）连接，目标板外部供电。
烧录后接 USART6（PG14=TX → USB-TTL 的 RX，**必须共地**），115200 8N1。

**正常启动的串口输出**：

```
[CFG] t=33/31 h=60/55 en=1111      ← 每秒一行，四个使能位应为 1111
```

`[CFG]` 中 `en=1111` 表示温度 / 湿度 / 明火 / 用电四项全部使能。
若出现 `[I2C1] diag rc=2`，说明 I2C1 总线异常，检查 SHT20 / AP3216C 供电与 PB6/PB7 接线。

---

## 关键配置点

### 1. WiFi 账号（必须改）

`Core/Inc/bsp_wifi.h`：

```c
#define ID          "你的路由器 SSID"
#define PASSWORD    "你的路由器密码"
#define ServerIP    "129.211.183.41"     /* 课堂调试台 IP，会变，以 8090 页面为准 */
```

运行时通过 WiFi 页输入的账号优先，宏定义仅作兜底。

### 2. 电压分压比标定（上板必做）

`Core/Inc/app_power.h`：

```c
#define VOLT_DIV_RATIO  3.14f   /* 实测标定：供电 5.02V ↔ PB1 1.600V ⇒ 3.1375 */
```

**标定方法**：万用表量 PB1 对 GND 得 `V_pin`，量 VBUS 得 `V_rail`，则 `ratio = V_rail / V_pin`。
无万用表时可临时按 `旧值 × 实际值 / 显示值` 反推。

### 3. 电流上限

```c
#define CURR_MAX_MA     250.0f  /* 硬件满量程仅 330mA */
```

> ⚠️ **已知设计缺陷**：报警动作（风扇全速 + 马达启动）本身会拉高电流。
> 若 `CURR_MAX_MA` 设得过低，会出现「电流超限 → 进 ALARM → 风扇全速 → 电流更高 → 永远退不出」的正反馈。
> 建议实测 ALARM 态峰值电流后留 30 mA 以上余量。

### 4. 字库子集（改界面时必看）

`LVGL_CUT/UI/fonts/` 下的 `ui_font_KaiTi*.c` 是 **SquareLine 扫描所有控件文本**生成的子集，
不是完整字库。需要新字时：

1. 在 SquareLine 里放一个 **隐藏 label**（`LV_OBJ_FLAG_HIDDEN`）当"字库饲料"
2. 把所有待用文字写进去
3. 导出后 **这个 label 必须保留不删** —— 删了再导出，字就又没了

---

## 目录结构

```
SmartKitchenEnvironmentMonitoringSystem/
├── Core/
│   ├── Inc/                     # 头文件
│   │   ├── app_state.h          # 状态机 + 阈值/使能接口
│   │   ├── app_power.h          # ADC 换算与门限宏
│   │   ├── app_cloud.h          # 云端协议
│   │   ├── bsp_*.h              # 板级驱动（led/beep/fan/flame/key/adc/smg/wifi/tick）
│   │   ├── drv_*.h              # 器件驱动（sht20/ap3216c）
│   │   ├── ui_custom.h          # 手写 UI 逻辑层
│   │   └── main.h               # 全部引脚宏定义
│   └── Src/
│       ├── main.c               # 初始化顺序 + 主循环
│       ├── freertos.c           # 任务创建与入口
│       ├── alarm.c              # 报警状态机执行层
│       ├── app_state.c          # 阈值 / 使能 / 三态判定
│       ├── app_power.c          # 电压电流换算
│       ├── ui_custom.c          # ★ 所有手写 UI 逻辑都在这
│       ├── bsp_*.c / drv_*.c    # 驱动实现
│       └── i2c.c                # 含 I2C1_DiagAndRecover() 总线故障自恢复
├── LVGL_CUT/
│   ├── src/                     # LVGL 8.3 内核
│   ├── UI/                      # SquareLine 导出（screens / fonts / images）
│   ├── examples/porting/        # ★ lv_port_indev.c 手改了触摸坐标
│   └── lv_conf.h
├── Middlewares/                 # FreeRTOS 内核
├── Drivers/                     # STM32F4 HAL
├── SquareLinestidos/            # SquareLine 工程源文件（.spj / assets / 字体原稿）
├── MDK-ARM/                     # Keil 工程（构建产物已 gitignore）
└── SmartKitchenEnvironmentMonitoringSystem.ioc
```

> **手写 UI 逻辑一行都不许放在 `LVGL_CUT/UI/` 里** —— SquareLine 重新导出会全部覆盖。
> 所有运行时修改集中在 `Core/Src/ui_custom.c`，用 `Ui_BindEvents()` 补事件、`Ui_RefreshStatus()` 补刷新。

---

## 资源占用

| 项 | 已用 | 总量 | 占比 |
|---|---|---|---|
| **Flash**（Code + RO-data） | 642.1 KB | 1024 KB | 62.7 % |
| **RAM**（RW + ZI） | 107.5 KB | 128 KB | **84.0 %** |

> RAM 只剩 **20.5 KB**。四个屏幕的控件树常驻内存（`lv_scr_load_anim(..., auto_del=false)`），
> 再加控件或加大 LVGL 内存池前请先测 `xPortGetFreeHeapSize()`。

---

## 已知限制

| 项 | 现状 | 影响 |
|---|---|---|
| **系统时钟用 HSI** | 内部 RC，±1 % 精度 | 长时间运行有漂移，WiFi 波特率误差偏大；有外部晶振建议切 HSE |
| **参数不持久化** | 无 Flash / EEPROM 存储 | 阈值与使能位每次上电回默认值 |
| **无 RTC** | 时间用"开机累计秒数"换算 | 断电即归零，时间显示无实际意义 |
| **SPI 阻塞传输** | `HAL_SPI_Transmit` 同步等待 | 满帧刷新约 15 fps，上 SPI DMA 可提升 |
| **I2C1 上电时序** | 有 `I2C1_DiagAndRecover()` 兜底 | 器件未就绪时仍可能需要重新上电 |
| **`LogTask` 为空壳** | 任务已创建但无实现 | 日志无法落盘 |
| **WWDG 超时 49.93 ms** | 任何阻塞重试都可能顶到看门狗 | I2C / WiFi 失败重试必须设上限 |

---

## 开发笔记

本项目排错过程整理成了二十余册知识库文档，记录每个坑的**症状 → 定位过程 → 根因 → 修法**，
放在本地知识库（不随仓库分发）。重点几册：

| 册 | 主题 |
|---|---|
| 17 | UI 卡顿排查（分块刷新的由来） |
| 18 | 触摸坐标错位（`x=319-p->y; y=p->x` 怎么试出来的） |
| 19 | 设置页功能落地（阈值 / 使能 / 报警代码） |
| 20 | 报警页消音按钮 |
| 21 | 报警页动态化 |
| 22 | 消音键管住风扇和马达 |
| 23 | 字库子集缺字 / 静态图冒充指示灯 |
| 24 | 电压读数虚高 / 上电就报警 / 滑块初值错 |

---

## License

课程设计项目，仅用于学习与答辩演示。
