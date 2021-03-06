/*
 * Copyright (C) 2012-2014 NXP Semiconductors
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

/**
 * \addtogroup ISO7816-4_application_protocol_implementation
 *
 * @{ */

#ifndef _PHNXPESEHAL_APDU_H
#define _PHNXPESEHAL_APDU_H
#include <phEseStatus.h>
#include <phEseTypes.h>

#define MIN_HEADER_LEN  4

typedef struct phNxpEseP61_7816_cpdu
{
    uint8_t cla; /* Class of instruction */
    uint8_t ins; /* Instruction code */
    uint8_t p1; /* Instruction parameter 1 */
    uint8_t p2; /* Instruction parameter 2 */
    uint16_t lc; /* No of data present in the data field of the command */
    uint8_t cpdu_type; /*0 - short len, 1 = extended len, this field is valid only if le > 0*/
    uint8_t  *pdata; /*application data*/
    uint8_t le_type; /*0 - Le absent ,1 - one byte le,2 - two byte le or 3 - 3  byte le*/
    uint32_t le; /* le value field */
} phNxpEseP61_7816_cpdu_t, *pphNxpEseP61_7816_cpdu_t;


typedef struct phNxpEseP61_7816_rpdu
{
    uint8_t sw1; /*Status byte most significant byte */
    uint8_t sw2; /*Status byte least significant byte */
    uint8_t  *pdata; /*Buffer allocated by caller*/
    uint16_t len; /* Lenght of the buffer, updated by calling api */
} phNxpEseP61_7816_rpdu_t, *pphNxpEseP61_7816_rpdu_t;


/**
 * \ingroup spi_libese
 * \brief This function prepares C-APDU and sends to p61 and recives response from the p61.
 * also it parses all required fields of the response PDU.
 *
 * \param[in]       pphNxpEseP61_7816_cpdu_t - CMD to p61
 * \param[out]      pphNxpEseP61_7816_rpdu_t  - RSP from p61(all required memory allocated by caller)
 *
 * \retval ESESTATUS_SUCCESS on Success #pphNxpEseP61_7816_rpdu_t all fields are filled correctly.
 *          else proper error code.
 *          ESESTATUS_INVALID_PARAMETER - If any invalid buffer passed from application \n
 *          ESESTATUS_INSUFFICIENT_RESOURCES - Any problem occurred during allocating the memory \n
 *          ESESTATUS_INVALID_BUFFER - If any invalid buffer received \n
 *          ESESTATUS_FAILED - Any other error occurred. \n
 */

ESESTATUS phNxpEseP61_7816_Transceive(pphNxpEseP61_7816_cpdu_t pCmd, pphNxpEseP61_7816_rpdu_t pRsp);

#endif  /*  _PHNXPESEHAL_APDU_H    */
/** @} */
