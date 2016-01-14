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
#ifndef _PHNXPESEPROTOCOL_H_
#define _PHNXPESEPROTOCOL_H_
#include <phTmlEse.h>
#include <phNxpEseHal.h>
#include <phNxpLog.h>
#include <phNxpEseRecvMgr.h>


#define PH_SCAL_T1_CHAINING       0x20
#define PH_SCAL_T1_SINGLE_FRAME   0x00

/* Macro to enable SPM Module */
#define SPM_INTEGRATED
//#undef SPM_INTEGRATED
#ifdef SPM_INTEGRATED
#include "../spm/phNxpEseP61_Spm.h"
#endif
typedef enum PH_SCAL_T1_FRAME {
    PH_SCAL_I_FRAME,
    PH_SCAL_R_FRAME,
    PH_SCAL_S_FRAME,
    PH_SCAL_I_CHAINED_FRAME
} PH_SCAL_T1_FRAME_T;

typedef enum PH_SCAL_T1_IFRAME_ERROR {
    RESYNCH_REQ = 0x00,
    RESYNCH_RES = 0x20,
    IFS_REQ = 0x01,
    IFS_RES = 0x21,
    ABORT_REQ = 0x02,
    ABORT_RES = 0x22,
    WTX_REQ = 0x03,
    WTX_RES = 0x23,
    INVALID_REQ_RES
} PH_SCAL_T1_IFRAME_ERROR_T;

struct PCB_BITS {
    uint8_t lsb :1;
    uint8_t bit2 :1;
    uint8_t bit3 :1;
    uint8_t bit4 :1;
    uint8_t bit5 :1;
    uint8_t bit6 :1;
    uint8_t bit7 :1;
    uint8_t msb :1;

};

/* Timeout value to wait for response from P61 */
#define HAL_EXTNS_WRITE_RSP_TIMEOUT   (1000)


void phNxpEseP61_ProcessResponse(uint32_t data_len, uint8_t *p_data);
ESESTATUS phNxpEseP61_ProcessData(uint32_t data_len, uint8_t *p_data);
ESESTATUS phNxpEseP61_Action(ESESTATUS action_evt, uint32_t data_len, uint8_t *p_data);
ESESTATUS phNxpEseP61_SemInit(phNxpSpiHal_Sem_t *event_ack);
ESESTATUS phNxpEseP61_SemWait(phNxpSpiHal_Sem_t *event_ack);
void phNxpEseP61_SemUnlock(phNxpSpiHal_Sem_t *event_ack, ESESTATUS status);
#endif /* _PHNXPPROTOCOL_H_ */
