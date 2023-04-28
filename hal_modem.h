/*************************************************
Copyright (C), 1988-1999, Jc-LxTech. Co., Ltd.
File name: hal_modem.h
Author: jc-lx
Version: v1.0.0
Date: 2023.01.16
*************************************************/
#pragma once
#ifndef _HAL_MODEM_H
#define _HAL_MODEM_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef _cplusplus
extern "c"{
#endif

#define __LOGI__
#ifdef __LOGI__
#define LOGI(format,...) printf(format,##__VA_ARGS__);
#else
#define LOGI(format,...)
#endif

/* USART Control Block */
typedef struct usart_cb{
    struct usart_cb*        self;                       /* 指向usart tcb 本身 */
    bool                    flag;                       /* 指示数据发送或接收就位 */
    uint8_t                 data[1024+128];             /* 指示数据本体 */
    uint32_t                len;                        /* 指示接收或发送数据长度 */
}usart_t;

/* Definitions for error constants. */
#define HAL_MODEM_OK                0       /*!< esp_err_t value indicating success (no error) */
#define HAL_MODEM_FAIL              -1      /*!< Generic esp_err_t code indicating failure */

#define SOH                         (0x01)  /* start of 128-byte data packet */
#define STX                         (0x02)  /* start of 1024-byte data packet */
#define EOT                         (0x04)  /* end of transmission */
#define ACK                         (0x06)  /* acknowledge */
#define NAK                         (0x15)  /* negative acknowledge */
#define CAN                         (0x18)  /* two of these in succession aborts transfer */
#define CRC16                       (0x43)  /* 'C' == 0x43, request 16-bit CRC */

#define DEFAULT_RETRY               5
#define PKT_HEAD_LEN                3       /* SOH + BLK + 255 - BLK */
#define PKT_DATA_1K                 1024
#define PKT_DATA_128                128
#define IDLE_CHAR                   '\0'
#define CPMEOF                      (0x1A)
#define NO_TIME_WAIT                (0x00000000)
#define NO_CHAR_WAIT                (0xFF)

typedef enum {
    YOMODEM_1K_SENDER,
    YOMODEM_1K_RECEIVER,
}hal_modem_role_t;

typedef enum {
    HAL_MODEM_STATE_CONNECTING,
    HAL_MODEM_STATE_CONNECTED,
    HAL_MODEM_STATE_SENDER_SEND_DATA,
    HAL_MODEM_STATE_SENDER_SEND_EOT,
    HAL_MODEM_STATE_RECEIVER_RECEIVE_DATA,
    HAL_MODEM_STATE_RECEIVER_RECEIVE_EOT,
    HAL_MODEM_STATE_ON_FILE,
    HAL_MODEM_STATE_FINISH,
    HAL_MODEM_STATE_ERROR,
} hal_modem_state_t;

typedef enum {  
    HAL_MODEM_EVENT_INIT,
    HAL_MODEM_EVENT_ERROR,
    HAL_MODEM_EVENT_CONNECTED,
    HAL_MODEM_EVENT_FINISHED,
    HAL_MODEM_EVENT_ON_SEND_DATA,
    HAL_MODEM_EVENT_ON_RECEIVE_DATA,
    HAL_MODEM_EVENT_ON_FILE,
} hal_modem_event_id_t;

typedef int32_t modem_err_t;

typedef struct hal_modem_fcb {                  /* file control block*/
    char                            file_desc[PKT_DATA_128];
    size_t                          file_size;
    size_t                          file_offset;
    uint32_t                        pkt_id;
    uint8_t                         pkt_mode;
    uint32_t                        pkt_len;    /* 打包好的有效数据长度 */
    uint8_t*                        pkt_data;   /* 指向打包好的数据包 */
} hal_modem_fcb_t;

typedef modem_err_t (*hal_modem_event_handle_fun)(void* arg);

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

/* Custom config */
typedef struct hal_modem_config{
    hal_modem_role_t                role;
    uint32_t                        baud;
    uint32_t                        addr;
    size_t                          size;/* bytes */
    size_t                          offset;
    char*                           name;
    char*                           date;
}hal_modem_config_t;

/**
 * @brief      init and start hal modem
 *
 * @param[in]  config   hal modem config 
 *
 * @return
 *     - NULL failed
 */
hal_modem_handle_t hal_modem_start(hal_modem_config_t* config);

/**
 * @brief      hal modem machine run in a task or cycle
 *
 * @param[in]  handle   hal_modem_handle_t 
 *
 * @return
 *     - HAL_MODEM_OK ok
 *     - HAL_MODEM_FAIL wrang or stop
 */
modem_err_t hal_modem_machine_run(hal_modem_handle_t* handle_addr);

#ifdef _cplusplus
}
#endif
#endif
