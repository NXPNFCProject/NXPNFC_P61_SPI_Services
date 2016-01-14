/*
 * Copyright (C) 2010-2014 NXP Semiconductors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _PHNXPSPIHAL_H_
#define _PHNXPSPIHAL_H_

#include <phNxpSpiHal_utils.h>
#include <phNxpEseHal_Api.h>
#include <phTmlEse.h>

/********************* Definitions and structures *****************************/

typedef enum
{
   ESE_STATUS_CLOSE = 0x00,
   ESE_STATUS_BUSY,
   ESE_STATUS_RECOVERY,
   ESE_STATUS_IDLE,
   ESE_STATUS_OPEN,
} phNxpEse_HalStatus;

typedef enum
{
   STATE_IDLE = 0x00,
   STATE_TRANS_ONGOING = 0x01,
   STATE_RESET_BLOCKED =0x02,
}transceive_state;


/* Macros to enable and disable extensions */
#define HAL_ENABLE_EXT()    (nxpesehal_ctrl.hal_ext_enabled = 1)
#define HAL_DISABLE_EXT()   (nxpesehal_ctrl.hal_ext_enabled = 0)

/* SPI Control structure */
typedef struct phNxpEseP61_Control
{
    phNxpEse_HalStatus   halStatus;      /* Indicate if hal is open or closed */
    pthread_t client_thread;  /* Integration thread handle */
    uint8_t   thread_running; /* Thread running if set to 1, else set to 0 */
    phLibEse_sConfig_t   gDrvCfg; /* Driver config data */
    uint32_t timeoutTimerId;   /*Timer for read/write */
    uint32_t timeoutPrioSessionTimerId; /*Timer for priority session read/write */

    /* Rx data */
    uint8_t  *p_rx_data;
    uint16_t rx_data_len;

    /* libp61 callbacks */
    ese_stack_data_callback_t *p_ese_stack_data_cback;

    /* HAL extensions */
    //uint8_t hal_ext_enabled;

    /* Waiting semaphore */
    phNxpSpiHal_Sem_t ack_cb_data;

    uint8_t p_read_buff[MAX_DATA_LEN];
    uint16_t cmd_len;
    uint8_t p_cmd_data[MAX_DATA_LEN];
    uint16_t rsp_len;
    uint8_t p_rsp_data[MAX_DATA_LEN];

    uint16_t last_frame_len;
    uint8_t p_last_frame[MAX_DATA_LEN];
    /* retry count used to retry write*/
    uint16_t retry_cnt;

    /* send seq counter */
    uint8_t seq_counter;

    /* receive seq counter */
    uint8_t recv_seq_counter;

    uint8_t recovery_counter;

    /* stroe the last action*/
    ESESTATUS last_state;
    /*store the current ongoing operation*/
    ESESTATUS current_operation;
    ESESTATUS status_code;
    bool_t  spm_power_state;

    /*Whether last transmission a R-Frame*/
    uint8_t isRFrame;

    /* WTX  Set counter value*/
    unsigned long int wtx_counter_value;

    /* WTX  counter */
    unsigned long int wtx_counter;

    /*Whether last transmission a R-Frame*/
    phNxpSpiHal_Sem_t cmd_rsp_ack;

    transceive_state cmd_rsp_state;

    /*Reset API response ack*/
    phNxpSpiHal_Sem_t reset_ack;
} phNxpEseP61_Control_t;



#define SPIHAL_CMD_CODE_LEN_BYTE_OFFSET         (2U)
#define SPIHAL_CMD_CODE_BYTE_LEN                (3U)
ESESTATUS phNxpEseP61_WriteFrame(uint32_t data_len, const uint8_t *p_data);
ESESTATUS phNxpEseP61_InternalWriteFrame(uint32_t data_len, const uint8_t *p_data);
ESESTATUS phNxpEseP61_read(void);

#endif /* _PHNXPSPIHAL_H_ */
