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
#ifndef _PHNXPSPILIB_H_
#define _PHNXPSPILIB_H_

#include <phNxpEse_Api.h>
#include <phNxpLog.h>

/* Macro to enable SPM Module */
#define SPM_INTEGRATED
//#undef SPM_INTEGRATED
#ifdef SPM_INTEGRATED
#include "../spm/phNxpEse_Spm.h"
#endif
/********************* Definitions and structures *****************************/

typedef enum
{
   ESE_STATUS_CLOSE = 0x00,
   ESE_STATUS_BUSY,
   ESE_STATUS_RECOVERY,
   ESE_STATUS_IDLE,
   ESE_STATUS_OPEN,
} phNxpEse_LibStatus;

typedef enum
{
  PN67T_POWER_SCHEME = 0x01,
  PN80T_LEGACY_SCHEME,
  PN80T_EXT_PMU_SCHEME,
}phNxpEse_PowerScheme;

/* Macros definition */
#define MAX_DATA_LEN      260
#if(NXP_ESE_JCOP_DWNLD_PROTECTION == TRUE)
#define ESE_JCOP_OS_DWNLD_RETRY_CNT   10 /* Maximum retry count for ESE JCOP OS Dwonload*/
#endif
#if(NXP_NFCC_SPI_FW_DOWNLOAD_SYNC == TRUE)
#define ESE_FW_DWNLD_RETRY_CNT        10 /* Maximum retry count for FW Dwonload*/
#endif

/* JCOP download states */
typedef enum jcop_dwnld_state{
    JCP_DWNLD_IDLE = SPM_STATE_JCOP_DWNLD,  /* jcop dwnld is not ongoing*/
    JCP_DWNLD_INIT=0x8010,                         /* jcop dwonload init state*/
    JCP_DWNLD_START=0x8020,                        /* download started */
    JCP_SPI_DWNLD_COMPLETE=0x8040,                 /* jcop download complete in spi interface*/
    JCP_DWP_DWNLD_COMPLETE=0x8080,                 /* jcop download complete */
} phNxpEse_JcopDwnldState;

/* SPI Control structure */
typedef struct phNxpEse_Context
{
    phNxpEse_LibStatus   EseLibStatus;      /* Indicate if Ese Lib is open or closed */
    void *pDevHandle;

    uint8_t p_read_buff[MAX_DATA_LEN];
    uint16_t cmd_len;
    uint8_t p_cmd_data[MAX_DATA_LEN];

    bool_t  spm_power_state;
    uint8_t pwr_scheme;
    phNxpEse_initParams initParams;
} phNxpEse_Context_t;

/* Timeout value to wait for response from
   Note: Timeout value updated from 1000 to 2000 to fix the JCOP delay (WTX)*/
#define HAL_EXTNS_WRITE_RSP_TIMEOUT   (2000)


#define SPILIB_CMD_CODE_LEN_BYTE_OFFSET         (2U)
#define SPILIB_CMD_CODE_BYTE_LEN                (3U)

ESESTATUS phNxpEse_WriteFrame(uint32_t data_len, const uint8_t *p_data);
ESESTATUS phNxpEse_read(uint32_t *data_len, uint8_t **pp_data);

#endif /* _PHNXPSPILIB_H_ */
