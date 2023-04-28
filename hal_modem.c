#include "hal_modem.h"

/**********************************************************************************/
/*
**用户在此实现移植功能
**在此包含串口驱动需要引用的头文件
*/

/* Read file data */
static modem_err_t hal_modem_file_read(void* dst, size_t offset, size_t len)
{ 
    return HAL_MODEM_OK;  
}

/* Get time */
static uint32_t hal_modem_get_time(void)
{
    uint32_t ticks = 0;
    //ticks = xTaskGetTickCount();
    return ticks;
}

/* Send data to UART */
static int hal_modem_send_data(uint8_t* data, uint32_t len)
{
    return 0; 
}

/* Write file data to flash */
static modem_err_t hal_modem_file_write(uint32_t offset, uint8_t* dst, size_t len)
{
   return HAL_MODEM_FAIL;
}

/* Read data from UART */
static modem_err_t hal_modem_get_recvdata(hal_modem_handle_t handle)
{
    // if (usart.flag) {
    //     handle->fcb.pkt_len = usart.len;
    //     handle->fcb.pkt_data = usart.data;
    //     return HAL_MODEM_OK;
    // }else return HAL_MODEM_FAIL;   
}

static void hal_modem_clear_transport(void)
{
    /* clear uart */
}

/**********************************************************************************/

static uint16_t hal_modem_crc16(uint8_t *buffer, uint32_t len)
{
    uint16_t crc16 = 0;
    while(len != 0) {
        crc16  = (uint8_t)(crc16 >> 8) | (crc16 << 8);
        crc16 ^= *buffer;
        crc16 ^= (uint8_t)(crc16 & 0xff) >> 4;
        crc16 ^= (crc16 << 8) << 4;
        crc16 ^= ((crc16 & 0xff) << 4) << 1;
        buffer++;
        len--;
    }
    return crc16;
}

static modem_err_t hal_modem_check_crc(hal_modem_fcb_t* fcb)
{
    /* pack crc calculation */
    uint16_t crc = hal_modem_crc16(fcb->pkt_data + 3,fcb->pkt_len - 5);
    
    /* pack crc check */
    uint8_t crc_h = fcb->pkt_data[fcb->pkt_len - 2];
    uint8_t crc_l = fcb->pkt_data[fcb->pkt_len - 1];/* end is len-1 */
    if (crc_h == (uint8_t)(crc >> 8) && crc_l == (uint8_t)crc) {
        return HAL_MODEM_OK;
    }else return HAL_MODEM_FAIL;
}

static void hal_modem_clear_recv(hal_modem_handle_t handle)
{
    /* clear recv flag */
    hal_modem_clear_transport();
}

static modem_err_t hal_modem_check_recv(hal_modem_handle_t handle)
{
    if (!handle) {
        return HAL_MODEM_FAIL;
    }

    /* get recv flag */
    if (hal_modem_get_recvdata(handle) != HAL_MODEM_OK) {
        return HAL_MODEM_FAIL;
    }

    /* check hal modem no wait */
    if (handle->wait_ch == NO_CHAR_WAIT) {
        return HAL_MODEM_OK;
    }
    
    /* check hal modem wait ch */
    hal_modem_fcb_t* fcb = &handle->fcb;   
    if (handle->wait_ch != fcb->pkt_data[0]) {
        LOGI("ERR recv_data:%02x data_len:%d waitch:0x%02x\r\n",
                                    fcb->pkt_data[0],fcb->pkt_len,handle->wait_ch);
        hal_modem_clear_recv(handle);
        return HAL_MODEM_FAIL; /* 无效接收 */
    }else return HAL_MODEM_OK;
}

static modem_err_t hal_modem_check_timeout(hal_modem_handle_t handle)
{
    if (!handle) {
        return HAL_MODEM_FAIL;
    }

    /* 超时未处理或NO_TIME_WAIT不检查新的超时 */
    if (handle->_timeout || handle->waitime == NO_TIME_WAIT) {
        return HAL_MODEM_FAIL;
    }
    
    if(handle->waitime < hal_modem_get_time()) {
        handle->_timeout = 1;/* flag timeout */
        return HAL_MODEM_OK;
    }else return HAL_MODEM_FAIL;   
}

static void hal_modem_clear_timeout(hal_modem_handle_t handle)
{
    handle->_timeout = 0;
}

static void hal_modem_set_wait(hal_modem_handle_t handle , uint8_t ch , uint32_t s)
{
    handle->wait_ch = ch;

    if (s == NO_TIME_WAIT) { /* NO_TIME_WAIT 为不设置等待延时 */
        handle->waitime = NO_TIME_WAIT;
    }else {
        handle->waitime = hal_modem_get_time() + s*1000;
    }

    /* clear wait flag */  
    hal_modem_clear_timeout(handle);
}

static void hal_modem_stop(hal_modem_handle_t* p)
{
    free(*p);
    *p = NULL;
}

static modem_err_t hal_modem_dispatch_event(hal_modem_handle_t handle, 
                                                hal_modem_event_id_t evt_id)
{
    if (!handle || !handle->modem_event_handle_cb) {
        return HAL_MODEM_FAIL;
    }

    if (handle->modem_event_handle_cb) {
        handle->evt_id = evt_id;
        return handle->modem_event_handle_cb(handle);
    }
    
    return HAL_MODEM_OK;
}

static void hal_modem_set_state(hal_modem_handle_t handle , hal_modem_state_t state)
{
    handle->state = state;
}

static void hal_modem_send_ch(hal_modem_handle_t handle, uint8_t sendch,
                                                            uint8_t ch, uint32_t time)
{
    /* set ch */
    hal_modem_send_data(&sendch,1);

    /* set wait */
    hal_modem_set_wait(handle,ch,time);
}

static modem_err_t hal_modem_fill_pkt(hal_modem_fcb_t* fcb)
{  
    /* count file rem size */
    int32_t rem_size = fcb->file_size - fcb->file_offset;
    
    fcb->pkt_data[0] = fcb->pkt_mode;
    fcb->pkt_data[1] = fcb->pkt_id;
    fcb->pkt_data[2] = ~fcb->pkt_id;
    switch (fcb->pkt_mode)
    {
    case SOH:{
        if (!fcb->pkt_id) {
            if (rem_size == fcb->file_size) {
                memcpy(fcb->pkt_data+3,fcb->file_desc,PKT_DATA_128);/* desc pkt */
            }
            if (rem_size <= 0) {
                memset(fcb->pkt_data+3,IDLE_CHAR,PKT_DATA_128);/* ending pkt */
            }           
        }
        if ( fcb->pkt_id > 0 && rem_size <= PKT_DATA_128) {
            /* 128 < file data < 1024 */
            assert(HAL_MODEM_FAIL != hal_modem_file_read(fcb->pkt_data+3,fcb->file_offset,rem_size));
            memset(fcb->pkt_data+3+rem_size,CPMEOF,PKT_DATA_128-rem_size);
        }                
    }break;
    case STX:{
        if ( rem_size >= PKT_DATA_1K) {
            assert(HAL_MODEM_FAIL != hal_modem_file_read(fcb->pkt_data+3,fcb->file_offset,PKT_DATA_1K));
        }else {
            assert(HAL_MODEM_FAIL != hal_modem_file_read(fcb->pkt_data+3,fcb->file_offset,rem_size));
            memset(fcb->pkt_data+3+rem_size,CPMEOF,PKT_DATA_1K-rem_size);
        }
    }break;
    }  
    uint16_t crc = hal_modem_crc16(fcb->pkt_data+3,fcb->pkt_len-5);
    fcb->pkt_data[fcb->pkt_len - 2] = (uint8_t)(crc >> 8);
    fcb->pkt_data[fcb->pkt_len - 1] = (uint8_t)crc;

    return HAL_MODEM_OK;
}

static modem_err_t hal_modem_pkt_unpack(hal_modem_handle_t handle)
{
    hal_modem_fcb_t* fcb = &handle->fcb;

    /* pack crc check */
    modem_err_t err = hal_modem_check_crc(fcb);
    if (err != HAL_MODEM_OK) {
        return err;
    }

    /* save to flash */
    err = hal_modem_file_write(fcb->file_offset,fcb->pkt_data+3,fcb->pkt_len-5);
    if (err != HAL_MODEM_OK) {
        return err;
    }else {
        LOGI("HAL_MODEM WRITE DOWN AT: 0x%08x \r\n",fcb->file_offset);
        fcb->file_offset = fcb->file_offset + 1024;
        return HAL_MODEM_OK;
    }
}

static modem_err_t hal_modem_send_pkt(hal_modem_handle_t handle , uint8_t mode , 
                                                            uint8_t ch , uint32_t time)
{
    hal_modem_fcb_t* fcb = &handle->fcb;

    /* free pkt memery */
    if (fcb->pkt_data) {
        free(fcb->pkt_data);
    }

    /* malloc pkt memery */
    if (mode == SOH) {
        fcb->pkt_id = handle->pktid;
        fcb->pkt_mode = SOH;
        fcb->pkt_len = PKT_DATA_128+5;
        fcb->pkt_data = (uint8_t*)malloc((fcb->pkt_len)*sizeof(uint8_t));
    }else if (mode == STX) {
        fcb->pkt_id = handle->pktid;
        fcb->pkt_mode = STX;
        fcb->pkt_len = PKT_DATA_1K+5;
        fcb->pkt_data = (uint8_t*)malloc((fcb->pkt_len)*sizeof(uint8_t));
    }else return HAL_MODEM_FAIL;

    /* fill pkt */       
    if (HAL_MODEM_OK != hal_modem_fill_pkt(fcb)) {
        LOGI("err fill pkt\n");
        return HAL_MODEM_FAIL;
    }
    
    /* send pkt */
    if (fcb->pkt_len != hal_modem_send_data(fcb->pkt_data,fcb->pkt_len)) {
        LOGI("err uart send data , data len:%d\n",fcb->pkt_len);                
        return HAL_MODEM_FAIL;
    }

    /* update fcb */
    if (fcb->pkt_id) {
        fcb->file_offset = fcb->file_offset + fcb->pkt_len-5;
        LOGI("********id:%d mode:%d len:%d offest:%d size:%d*******\n",
            fcb->pkt_id,fcb->pkt_mode,fcb->pkt_len,fcb->file_offset,fcb->file_size);
    }

    /* set wait */
    hal_modem_set_wait(handle,ch,time);

    free(fcb->pkt_data);
    fcb->pkt_data = NULL;
    fcb->pkt_len = 0;

    return HAL_MODEM_OK;
}

static void hal_modem_ymodem1krecver_handle(hal_modem_handle_t handle)
{
    hal_modem_fcb_t* fcb = &handle->fcb;
    handle->cycle = DEFAULT_RETRY;
    
    LOGI("*******\r\n");
    for (size_t i = 0; i < fcb->pkt_len; i++){
        LOGI("%02x ",fcb->pkt_data[i]);
    }
    LOGI("\r\n*******\n");  

    if (handle->role == YOMODEM_1K_RECEIVER) {
        switch (fcb->pkt_data[0]) {
            case STX:{
                if (handle->state == HAL_MODEM_STATE_ON_FILE) {
                    hal_modem_pkt_unpack(handle);
                    hal_modem_send_ch(handle,ACK,NO_CHAR_WAIT,3);
                }
            }break;
            case SOH:{
                if (handle->state == HAL_MODEM_STATE_ON_FILE) {
                    hal_modem_pkt_unpack(handle);
                    hal_modem_send_ch(handle,ACK,NO_CHAR_WAIT,3);
                }
                if (handle->state == HAL_MODEM_STATE_CONNECTING) {
                    hal_modem_set_state(handle,HAL_MODEM_STATE_CONNECTED);
                    hal_modem_dispatch_event(handle,HAL_MODEM_EVENT_CONNECTED);
                    LOGI("******** HAL MODEM CONNECTED ********\r\n");
                    hal_modem_set_state(handle,HAL_MODEM_STATE_ON_FILE);
                    hal_modem_dispatch_event(handle,HAL_MODEM_EVENT_ON_FILE);
                    LOGI("******** HAL MODEM STARTING RECVING FILE ********\r\n");
                    hal_modem_send_ch(handle,ACK,NO_CHAR_WAIT,3);
                    hal_modem_send_ch(handle,CRC16,NO_CHAR_WAIT,3);
                }
                if (handle->state == HAL_MODEM_STATE_RECEIVER_RECEIVE_EOT) {
                    hal_modem_send_ch(handle,ACK,NO_CHAR_WAIT,3);
                    hal_modem_set_state(handle,HAL_MODEM_STATE_FINISH);
                    hal_modem_dispatch_event(handle,HAL_MODEM_EVENT_FINISHED);
                    LOGI("******** HAL MODEM FINISHED ********\r\n");
                }
            }break;
            case EOT:{
                if (handle->state == HAL_MODEM_STATE_RECEIVER_RECEIVE_EOT) {
                    hal_modem_send_ch(handle,ACK,NO_CHAR_WAIT,3);
                    hal_modem_send_ch(handle,CRC16,NO_CHAR_WAIT,3);
                }
                if (handle->state == HAL_MODEM_STATE_ON_FILE) {
                    hal_modem_set_state(handle,HAL_MODEM_STATE_RECEIVER_RECEIVE_EOT);
                    LOGI("******** HAL MODEM RECVING FILE COMPLETED ********\r\n");
                    hal_modem_send_ch(handle,NAK,EOT,3);
                }
            }break;
            default:
                break;
        }
    }
}


static modem_err_t hal_modem_ymodem1ksender_handle(hal_modem_handle_t handle)
{
    hal_modem_fcb_t* fcb = &handle->fcb;
    handle->cycle = DEFAULT_RETRY;
    
    // LOGI("*******\r\n");
    //     for (size_t i = 0; i < fcb->pkt_len; i++){
    //         LOGI("%02x ",fcb->pkt_data[i]);
    //     }
    // LOGI("\r\n*******\n");     
    
    if (handle->role == YOMODEM_1K_SENDER) {
        switch (fcb->pkt_data[0]) {
            case ACK:{
                if (fcb->pkt_data[1] == CRC16 && fcb->pkt_len == 2) {
                    if (handle->state == HAL_MODEM_STATE_SENDER_SEND_EOT) {
                        /* send ending pkt */
                        handle->pktid = 0;
                        hal_modem_send_pkt(handle,SOH,ACK,2);
                        break;
                    }
                    if (handle->state == HAL_MODEM_STATE_CONNECTING ) {
                        hal_modem_set_state(handle,HAL_MODEM_STATE_CONNECTED);
                        hal_modem_dispatch_event(handle,HAL_MODEM_EVENT_CONNECTED);
                        LOGI("******** HAL MODEM CONNECTED ********\r\n"); 
                        hal_modem_set_state(handle,HAL_MODEM_STATE_ON_FILE);
                        hal_modem_dispatch_event(handle,HAL_MODEM_EVENT_ON_FILE);
                        LOGI("******** HAL MODEM STARTING SENDING FILE ********\r\n");
                    }
                }
                if (handle->state == HAL_MODEM_STATE_SENDER_SEND_EOT) {
                    hal_modem_set_state(handle,HAL_MODEM_STATE_FINISH);
                    hal_modem_dispatch_event(handle,HAL_MODEM_EVENT_FINISHED);
                    LOGI("******** HAL MODEM FINISHED ********\r\n"); 
                }
                if (handle->state == HAL_MODEM_STATE_ON_FILE) {
                    int32_t  rem_size = fcb->file_size - fcb->file_offset;
                    if (rem_size > 0) {
                        /* 处理已发送数据索引 */
                        handle->pktid++;
                        if (rem_size <= PKT_DATA_128) {
                            hal_modem_send_pkt(handle,SOH,ACK,5); 
                        }else {
                            hal_modem_send_pkt(handle,STX,ACK,5);
                        }
                    }else{
                        /* last data pkt send down */
                        hal_modem_set_state(handle,HAL_MODEM_STATE_SENDER_SEND_EOT);
                        LOGI("******** HAL MODEM SENDING FILE DOWN ********\r\n");            
                        hal_modem_send_ch(handle,EOT,NAK,2);/* EOT 0x04 */
                    }
                    break;   
                }  
            }break;
            case CRC16:{
                if (handle->state == HAL_MODEM_STATE_CONNECTING) {
                    handle->pktid = 0;
                    hal_modem_send_pkt(handle,SOH,ACK,5);
                } 
            }break;
            case NAK:{
                if (handle->state == HAL_MODEM_STATE_SENDER_SEND_EOT) {
                    hal_modem_send_ch(handle,EOT,ACK,2);/* EOT 0x04 */
                }
            }break;
            case CAN:{

            }break;
            default:
                break;
        }
    }
    
    return HAL_MODEM_OK;
}

static modem_err_t hal_modem_timeout_handle(hal_modem_handle_t handle)
{
    if (!handle->cycle -- ) {
        hal_modem_set_state(handle,HAL_MODEM_STATE_ERROR);
        hal_modem_dispatch_event(handle,HAL_MODEM_EVENT_ERROR);
        LOGI("******** HAL MODEM ERROR ********\r\n");
        handle->waitime = NO_TIME_WAIT;/* stop immediately */
        return HAL_MODEM_FAIL;
    }

    if (handle->role == YOMODEM_1K_RECEIVER) {
        switch (handle->state) {
            case HAL_MODEM_STATE_CONNECTING:{
                hal_modem_send_ch(handle,CRC16,SOH,1);
            }break;

            default:
                break;
        }
    }

    if (handle->role == YOMODEM_1K_SENDER) {
        switch (handle->state) {
            case HAL_MODEM_STATE_CONNECTING:{
                if (handle->role == YOMODEM_1K_SENDER) {
                    hal_modem_send_pkt(handle,SOH,ACK,5);
                }
            }break;
            default:
                break;
        }
    }
    
    return HAL_MODEM_OK;
}

hal_modem_handle_t hal_modem_start(hal_modem_config_t* config)
{
    assert(config != NULL);

    hal_modem_handle_t handle = calloc(1,sizeof(hal_modem_t));
    if (!handle) {
        LOGI("malloc handle fail\r\n");
        return NULL;
    }

    if(config->role == YOMODEM_1K_RECEIVER) {
        handle->role = config->role;
        handle->fcb.file_offset = config->offset;
        handle->cycle = DEFAULT_RETRY;
        hal_modem_set_state(handle,HAL_MODEM_STATE_CONNECTING);
        hal_modem_clear_transport();
        hal_modem_send_ch(handle,CRC16,SOH,1);
    }
    
    if(config->role == YOMODEM_1K_SENDER) {
        handle->role = config->role;
        handle->fcb.file_offset = config->offset;
        handle->fcb.file_size = config->size;
        handle->cycle = DEFAULT_RETRY;

        /* fill file desc */
        char s_file_size[16] = {0};
        itoa(config->size,s_file_size,10);
        uint8_t file_desc_len = sprintf(handle->fcb.file_desc,"%s%c%s%c%s",config->name,'\0',
                                s_file_size,'\0',config->date);
        if (file_desc_len > PKT_DATA_128 - 1 ) {
            LOGI("file desc is too long\r\n");
        }else {
            memset(handle->fcb.file_desc+file_desc_len,IDLE_CHAR,PKT_DATA_128-file_desc_len);
        }
        hal_modem_set_state(handle,HAL_MODEM_STATE_CONNECTING);
        hal_modem_dispatch_event(handle,HAL_MODEM_STATE_CONNECTING);
        LOGI("******** HAL MODEM SENDER , WAIT CONNECTING ********\r\n");
        hal_modem_set_wait(handle,CRC16,NO_TIME_WAIT);
    }
    
    return handle;
}

modem_err_t hal_modem_machine_run(hal_modem_handle_t* handle_addr)
{   
    hal_modem_handle_t handle = *handle_addr;

    if (handle == NULL) {
        return HAL_MODEM_FAIL;
    }  

    if(hal_modem_check_recv(handle) == HAL_MODEM_OK) {
        if (handle->role == YOMODEM_1K_SENDER) {
            hal_modem_ymodem1ksender_handle(handle);
        }
        if (handle->role == YOMODEM_1K_RECEIVER) {
            hal_modem_ymodem1krecver_handle(handle);
        }
        hal_modem_clear_recv(handle);
        return HAL_MODEM_OK;
    }

    if(hal_modem_check_timeout(handle) == HAL_MODEM_OK) {
        hal_modem_timeout_handle(handle);
        hal_modem_clear_timeout(handle);
        return HAL_MODEM_OK;
    }

    if (handle->state == HAL_MODEM_STATE_FINISH || handle->state == HAL_MODEM_STATE_ERROR) {
        hal_modem_stop(handle_addr);
    }

    return HAL_MODEM_OK;
}


