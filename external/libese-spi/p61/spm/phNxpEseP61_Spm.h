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
 * \addtogroup SPI_Power_Management
 *
 * @{ */

#ifndef _PHNXPESEP61_SPM_H
#define _PHNXPESEP61_SPM_H

#include <phEseTypes.h>
/* SPI Power Manager (SPM) possible error codes */
typedef enum spmstatus_t
{
    SPMSTATUS_SUCCESS = 0x00, /* The requested operation is succeed */
    SPMSTATUS_DEVICE_BUSY, /* The p61 is busy with the wired operation */
    SPMSTATUS_DEVICE_DWNLD_BUSY, /* The NFCC is busy with the firmware download */
    SPMSTATUS_FAILED /* The requested operation is failed */
}SPMSTATUS;

typedef enum spm_power{
    SPM_POWER_DISABLE = 0,
    SPM_POWER_ENABLE,
    SPM_POWER_RESET,
    SPM_POWER_PRIO_ENABLE,
    SPM_POWER_PRIO_DISABLE
}spm_power_t;
typedef enum spm_state{
    SPM_STATE_INVALID = 0x0000, /* Nfc i2c driver misbehaving */
    SPM_STATE_IDLE = 0x0100, /* p61 is free to use */
    SPM_STATE_WIRED = 0x0200,  /* p61 is being accessed by DWP (NFCC)*/
    SPM_STATE_SPI = 0x0400, /* P61 is being accessed by SPI */
    SPM_STATE_DWNLD = 0x0800, /* NFCC fw download is in progress */
    SPM_STATE_SPI_PRIO = 0x1000, /*Start of p61 access by SPI on priority*/
    SPM_STATE_SPI_PRIO_END = 0x2000,  /*End of p61 access by SPI on priority*/
    SPM_STATE_JCOP_DOWNLOAD = 0x4000 /* Jcop download in progress */
}spm_state_t;

/**
 * \ingroup spm_module
 * \brief This function opens the nfc i2c driver to manage power and synchronization
 * for p61 secure element.
  *
 * \param[in]       none
 * \param[out]      none
 *
 * \retval SPMSTATUS_SUCCESS on Success #if nfc i2c driver closed successfully
           SPMSTATUS_FAILED - i2c driver close failed. \n
 */
SPMSTATUS phNxpEseP61_SPM_Init(void);

/**
 * \brief This function closes the nfc i2c driver node
  *
 * \param[in]       none
 * \param[out]      none
 *
 * \retval SPMSTATUS_SUCCESS on Success #if nfc i2c driver closed successfully.
 *         SPMSTATUS_FAILED - i2c driver close failed. \n
 */

SPMSTATUS phNxpEseP61_SPM_DeInit(void);

/**
 * \brief This function request to the nfc i2c driver
 * to enable power to p61/disable power to p61.
 * This api should be called before sending any apdu to p61 or
 * once apdu exchange is done.
 *
 * \param[in]       none
 * \param[out]      none
 *
 * \retval SPMSTATUS_SUCCESS on Success if nfc i2c driver
 *         configured the p61.
 *        SPMSTATUS_FAILED - Any other error \n
 */
SPMSTATUS phNxpEseP61_SPM_ConfigPwr(spm_power_t arg);

/**
 * \brief This function request to the nfc i2c driver
 * to enable power to p61. This api should be called before
 * sending any apdu to p61.
  *
 * \param[in]       none
 * \param[out]      none
 *
 * \retval SPMSTATUS_SUCCESS on Success #if nfc i2c driver
 *         provide access to P61.
 *         SPMSTATUS_DEVICE_BUSY - The p61 is busy with the wired operation
  *        SPMSTATUS_FAILED - Any other error \n
 */

SPMSTATUS phNxpEseP61_SPM_EnablePwr(void);

/**
 * \brief This function request to the nfc i2c driver
 * to disable power to p61. This api should be called
 * once apdu exchange is done.
  *
 * \param[in]       none
 * \param[out]      none
 *
 * \retval SPMSTATUS_SUCCESS on Success #if nfc i2c driver
 *         successfully switched off power to P61.
 *        SPMSTATUS_FAILED - if disable power request is failed \n
 */

SPMSTATUS phNxpEseP61_SPM_DisablePwr(void);

/**
 * \brief This function request to the nfc i2c driver
 * to get current P61 state
  *
 * \param[in]       none
 * \param[out]      Updates the current SPM state
 *
 * \retval SPMSTATUS_SUCCESS on Success #if nfc i2c driver
 *        successfully fulfill request.
 *        SPMSTATUS_FAILED - if reset request is failed \n
 */

SPMSTATUS phNxpEseP61_SPM_GetState(spm_state_t *current_state);

/**
 * \brief This function request to the nfc i2c driver
 * to reset p61.
  *
 * \param[in]       none
 * \param[out]      none
 *
 * \retval SPMSTATUS_SUCCESS on Success #if nfc i2c driver
 *        successfully reset P61.
 *        SPMSTATUS_FAILED - if reset request is failed \n
 */

SPMSTATUS phNxpEseP61_SPM_ResetPwr(void);
/**
 * \brief This function request to the nfc i2c driver
 * to get lock of p61.
  *
 * \param[in]       timeout - timeout to wait for ese access
 * \param[out]      none
 *
 * \retval SPMSTATUS_SUCCESS on Success #if nfc i2c driver
 *        successfully provides lock of P61.
 *        SPMSTATUS_FAILED - if timeout occured \n
 */

SPMSTATUS phNxpEseP61_SPM_GetAccess(long timeout);

/**
 * \brief This function request to the nfc i2c driver
 * to release p61 lock.
  *
 * \param[in]       none
 * \param[out]      none
 *
 * \retval SPMSTATUS_SUCCESS on Success #if nfc i2c driver
 *        successfully provides lock of P61.
 *        SPMSTATUS_FAILED - if reset request is failed \n
 */

SPMSTATUS phNxpEseP61_SPM_RelAccess(void);

/**
 * \brief This function request to the nfc i2c driver
 * to set current P61 state
  *
 * \param[in]       current_spm_state - state of P61
 *
 * \retval SPMSTATUS_SUCCESS on Success #if nfc i2c driver
 *        successfully fulfill request.
 *        SPMSTATUS_FAILED - if reset request is failed \n
 */
SPMSTATUS phNxpEseP61_SPM_SetState(spm_state_t current_spm_state);
#endif  /*  _PHNXPESEP61_SPM_H    */
/** @} */
