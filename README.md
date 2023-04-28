# HalModem
## 简介
hal_modem是一个基于非死循环与状态机机制设计小巧简介的协议传输框架，可以方便的在裸机或RTOS中移植与使用，目前移植适配了YModem-1k协议，已实现协议标准对接，CRC数据效验，超时重传检查等功能。
## 特性
HalModem使用C语言实现，基于面向对象方式设计思路，独立数据结构管理。
```c
/* 文件传输控制块 */
typedef struct hal_modem_fcb {                  /* file control block*/
    char                            file_desc[PKT_DATA_128];
    size_t                          file_size;
    size_t                          file_offset;
    uint32_t                        pkt_id;
    uint8_t                         pkt_mode;
    uint32_t                        pkt_len;    /* 打包好的有效数据长度 */
    uint8_t*                        pkt_data;   /* 指向打包好的数据包 */
} hal_modem_fcb_t;

/* 核心handle */
typedef struct hal_modem {
    hal_modem_role_t                role;
    hal_modem_fcb_t                 fcb;
    hal_modem_state_t               state;
    hal_modem_event_id_t            evt_id;
    uint32_t                        cycle;
    hal_modem_event_handle_fun      modem_event_handle_cb;
    bool                            _timeout;               /* wait timeout flag */
    uint32_t                        waitime;                /* 0 设置不延时 */
    uint32_t                        pktid;
    char                            wait_ch;
}hal_modem_t,* hal_modem_handle_t;
```
## 使用方法
参考stm32F1xxIAP启动加载引导程序设计。
链接：<https://github.com/Jc-Lx/stm32F1xxIAP>