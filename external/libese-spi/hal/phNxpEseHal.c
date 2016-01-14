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

#include <sys/stat.h>
#include <phNxpEseHal.h>
#include <phTmlEse.h>
#include <phDal4Ese_messageQueueLib.h>
#include <phNxpLog.h>
#include <phNxpEseProtocol.h>
#include <phNxpConfig.h>

/* Macro to enable SPM Module */
#define SPM_INTEGRATED
//#undef SPM_INTEGRATED
#ifdef SPM_INTEGRATED
#include "../spm/phNxpEseP61_Spm.h"
#endif

/*********************** Global Variables *************************************/

/* SPI HAL Control structure */
phNxpEseP61_Control_t nxpesehal_ctrl;

/* TML Context */
extern phTmlEse_Context_t *gpphTmlEse_Context;
extern void ese_stack_data_callback(ESESTATUS status, phNxpEseP61_data *eventData);
/**************** local methods used in this file only ************************/

STATIC void phNxpEseP61_write_complete(void *pContext, phTmlEse_TransactInfo_t *pInfo);
STATIC void phNxpEseP61_read_complete(void *pContext, phTmlEse_TransactInfo_t *pInfo);
STATIC void phNxpEseP61_kill_client_thread(phNxpEseP61_Control_t *p_nxpesehal_ctrl);
STATIC void *phNxpEseP61_client_thread(void *arg);
STATIC void phNxpEseP61_WaitForAckCb(uint32_t timerId, void *pContext);
STATIC void phNxpEseP61_PrioSessionTimer(uint32_t timerId, uint32_t timeout);
STATIC void phNxpEseP61_PrioSessionTimeout(uint32_t timerId, void *pContext);
STATIC void phNxpEseP61_internal_write_complete(void *pContext, phTmlEse_TransactInfo_t *pInfo);
/******************************************************************************
 * Function         phNxpEseP61_open
 *
 * Description      This function is called by Jni during the
 *                  initialization of the ESE. It opens the physical connection
 *                  with ESE (P61) and creates required client thread for
 *                  operation.
 * Returns          This function return ESESTATUS_SUCCES (0) in case of success
 *                  In case of failure returns other failure value.
 *
 ******************************************************************************/
ESESTATUS phNxpEseP61_open( ese_stack_data_callback_t *p_data_cback)
{
    phOsalEse_Config_t tOsalConfig;
    phTmlEse_Config_t tTmlConfig;
    ESESTATUS wConfigStatus = ESESTATUS_SUCCESS;
    unsigned long int num = 0;
#ifdef SPM_INTEGRATED
    SPMSTATUS wSpmStatus = SPMSTATUS_SUCCESS;
    spm_state_t current_spm_state = SPM_STATE_INVALID;
#endif

    /*When spi channel is already opened return status as FAILED*/
    if(nxpesehal_ctrl.halStatus != ESE_STATUS_CLOSE)
    {
        ALOGD("already opened\n");
        return ESESTATUS_FAILED;
    }
    memset(&nxpesehal_ctrl, 0x00, sizeof(nxpesehal_ctrl));
    memset(&tOsalConfig, 0x00, sizeof(tOsalConfig));
    memset(&tTmlConfig, 0x00, sizeof(tTmlConfig));

#if(NFC_NXP_ESE_VER == JCOP_VER_3_2)
    if (GetNxpNumValue (NAME_NXP_WTX_COUNT_VALUE, &num, sizeof(num)))
    {
        nxpesehal_ctrl.wtx_counter_value = num;
        NXPLOG_SPIHAL_E("Wtx_counter read from config file - %lu",num);
    }
    else
    {
        nxpesehal_ctrl.wtx_counter_value = 0;
        NXPLOG_SPIHAL_E("Wtx_counter not defined in config file - %lu",num);
    }
#endif
    /* make sequence counter 1*/
    nxpesehal_ctrl.seq_counter = 0x01;
    nxpesehal_ctrl.isRFrame = 0;
    nxpesehal_ctrl.cmd_rsp_state = STATE_IDLE;
    /* reset config cache */
    resetNxpConfig();

    /* initialize trace level */
    phNxpLog_InitializeLogLevel();

    /*Create the timer for extns write response*/
    nxpesehal_ctrl.timeoutTimerId = phOsalEse_Timer_Create();

    if (phNxpEseHal_init_monitor() == NULL)
    {
        NXPLOG_SPIHAL_E("Init monitor failed");
        return ESESTATUS_FAILED;
    }

    CONCURRENCY_LOCK();
    /* Configure hardware link */
    nxpesehal_ctrl.gDrvCfg.nClientId = phDal4Ese_msgget(0, 0600);
    nxpesehal_ctrl.gDrvCfg.nLinkType = ENUM_LINK_TYPE_SPI;/* For P61 */
    tTmlConfig.pDevName = (int8_t *) "/dev/p61";
    tOsalConfig.dwCallbackThreadId
    = (uintptr_t) nxpesehal_ctrl.gDrvCfg.nClientId;
    tOsalConfig.pLogFile = NULL;
    tTmlConfig.dwGetMsgThreadId = (uintptr_t) nxpesehal_ctrl.gDrvCfg.nClientId;

#ifdef SPM_INTEGRATED
    /* Get the Access of P61 */
    wSpmStatus = phNxpEseP61_SPM_Init();
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phNxpEseP61_SPM_Init Failed");
        wConfigStatus = ESESTATUS_FAILED;
        goto clean_and_return_2;
    }

    wSpmStatus = phNxpEseP61_SPM_GetState(&current_spm_state);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        ALOGE(" %s : phNxpEseP61_SPM_GetPwrState Failed", __FUNCTION__);
        wConfigStatus = ESESTATUS_FAILED;
        goto clean_and_return_1;
    }
    else
    {
        if ((current_spm_state & SPM_STATE_SPI) | (current_spm_state & SPM_STATE_SPI_PRIO))
        {
            ALOGE(" %s : SPI is already opened...second instance not allowed", __FUNCTION__);
            wConfigStatus = ESESTATUS_FAILED;
            goto clean_and_return_1;
        }
#if(NFC_NXP_ESE_VER == JCOP_VER_3_1)
        if (current_spm_state & SPM_STATE_WIRED)
        {
            ALOGE(" %s : SPI open failed because NFC wired mode already open", __FUNCTION__);
            wConfigStatus = ESESTATUS_FAILED;
            goto clean_and_return_1;
        }
#endif
    }

    wSpmStatus = phNxpEseP61_SPM_ConfigPwr(SPM_POWER_ENABLE);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phNxpEseP61_SPM_ConfigPwr: enabling power Failed");
        if (wSpmStatus == SPMSTATUS_DEVICE_BUSY)
        {
            wConfigStatus = ESESTATUS_BUSY;
        }
        else if(wSpmStatus == SPMSTATUS_DEVICE_DWNLD_BUSY)
        {
            wConfigStatus = ESESTATUS_DWNLD_BUSY;
        }
        else
        {
            wConfigStatus = ESESTATUS_FAILED;
        }
        goto clean_and_return;
    }
    else
    {
        NXPLOG_SPIHAL_E("nxpesehal_ctrl.spm_power_state TRUE");
        nxpesehal_ctrl.spm_power_state = TRUE;
    }
#endif

    /* Initialize TML layer */
    wConfigStatus = phTmlEse_Init(&tTmlConfig);
    if (wConfigStatus != ESESTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phTmlEse_Init Failed");
        nxpesehal_ctrl.p_ese_stack_data_cback = NULL;
        goto clean_and_return;
    }

    if (pthread_create(&nxpesehal_ctrl.client_thread, NULL,
            phNxpEseP61_client_thread, &nxpesehal_ctrl) != 0)
    {
        NXPLOG_SPIHAL_E("pthread_create failed");
        wConfigStatus = phTmlEse_Shutdown();
        nxpesehal_ctrl.p_ese_stack_data_cback = NULL;
        goto clean_and_return;
    }
#ifndef SPM_INTEGRATED
    wConfigStatus = phTmlEse_IoCtl(phTmlEse_e_ResetDevice, 2);
    if (wConfigStatus != ESESTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phTmlEse_IoCtl Failed");
        nxpesehal_ctrl.p_ese_stack_data_cback = NULL;
        goto clean_and_return;
    }
#endif
    wConfigStatus = phTmlEse_IoCtl(phTmlEse_e_EnableLog, 0);
    if (wConfigStatus != ESESTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phTmlEse_IoCtl Failed");
        nxpesehal_ctrl.p_ese_stack_data_cback = NULL;
        goto clean_and_return;
    }
    wConfigStatus = phTmlEse_IoCtl(phTmlEse_e_EnablePollMode, 1);

    if (wConfigStatus != ESESTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phTmlEse_IoCtl Failed");
        nxpesehal_ctrl.p_ese_stack_data_cback = NULL;
        goto clean_and_return;
    }
    if (p_data_cback == NULL)
    {
        NXPLOG_SPIHAL_D("%s p_data_cback is null", __FUNCTION__);
        /* if callback is null handle it internally */
        nxpesehal_ctrl.p_ese_stack_data_cback = ese_stack_data_callback;
    }
    else
    {
        NXPLOG_SPIHAL_D("%s p_data_cback is not null", __FUNCTION__);
        nxpesehal_ctrl.p_ese_stack_data_cback = p_data_cback;
    }
    /* STATUS_OPEN */
     nxpesehal_ctrl.halStatus = ESE_STATUS_OPEN;

    CONCURRENCY_UNLOCK();

    NXPLOG_SPIHAL_E("wConfigStatus %x", wConfigStatus);
    return wConfigStatus;

    clean_and_return:
#ifdef SPM_INTEGRATED
    wSpmStatus = phNxpEseP61_SPM_ConfigPwr(SPM_POWER_DISABLE);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phNxpEseP61_SPM_ConfigPwr: disabling power Failed");
    }
    clean_and_return_1:
    phNxpEseP61_SPM_DeInit();
    clean_and_return_2:
#endif
    CONCURRENCY_UNLOCK();
    phNxpSpiHal_cleanup_monitor();
    nxpesehal_ctrl.halStatus = ESE_STATUS_CLOSE;
    nxpesehal_ctrl.spm_power_state = FALSE;
    return ESESTATUS_FAILED;
}
/******************************************************************************
 * Function         phNxpEseP61_openPrioSession
 *
 * Description      This function is called by Jni during the
 *                  initialization of the ESE. It opens the physical connection
 *                  with ESE (P61) and creates required client thread for
 *                  operation.  This will get priority access to ESE for timeout duration.

 * Returns          This function return ESESTATUS_SUCCES (0) in case of success
 *                  In case of failure returns other failure value.
 *
 ******************************************************************************/
ESESTATUS phNxpEseP61_openPrioSession(ese_stack_data_callback_t *p_data_cback, uint32_t timeOut)
{
    phOsalEse_Config_t tOsalConfig;
    phTmlEse_Config_t tTmlConfig;
    ESESTATUS wConfigStatus = ESESTATUS_SUCCESS;
    unsigned long int num = 0;
#ifdef SPM_INTEGRATED
    SPMSTATUS wSpmStatus = SPMSTATUS_SUCCESS;
    spm_state_t current_spm_state = SPM_STATE_INVALID;
#endif
    memset(&nxpesehal_ctrl, 0x00, sizeof(nxpesehal_ctrl));
    memset(&tOsalConfig, 0x00, sizeof(tOsalConfig));
    memset(&tTmlConfig, 0x00, sizeof(tTmlConfig));
#if(NFC_NXP_ESE_VER == JCOP_VER_3_2)
    if (GetNxpNumValue (NAME_NXP_WTX_COUNT_VALUE, &num, sizeof(num)))
    {
        nxpesehal_ctrl.wtx_counter_value = num;
        NXPLOG_SPIHAL_E("Wtx_counter read from config file - %lu",num);
    }
    else
    {
        nxpesehal_ctrl.wtx_counter_value = 0;
        NXPLOG_SPIHAL_E("Wtx_counter not defined in config file - %lu",num);
    }
#endif
    /* make sequence counter 1*/
    nxpesehal_ctrl.seq_counter = 0x01;
    /* reset config cache */
    resetNxpConfig();

    /* initialize trace level */
    phNxpLog_InitializeLogLevel();

    /*Create the timer for extns write response*/
    nxpesehal_ctrl.timeoutTimerId = phOsalEse_Timer_Create();

    /*Create the timer for priority based session*/
    nxpesehal_ctrl.timeoutPrioSessionTimerId = phOsalEse_Timer_Create();

    if (phNxpEseHal_init_monitor() == NULL)
    {
        NXPLOG_SPIHAL_E("Init monitor failed");
        return ESESTATUS_FAILED;
    }

    CONCURRENCY_LOCK();

    /* Configure hardware link */
    nxpesehal_ctrl.gDrvCfg.nClientId = phDal4Ese_msgget(0, 0600);
    nxpesehal_ctrl.gDrvCfg.nLinkType = ENUM_LINK_TYPE_SPI;/* For P61 */
    tTmlConfig.pDevName = (int8_t *) "/dev/p61";
    tOsalConfig.dwCallbackThreadId
    = (uintptr_t) nxpesehal_ctrl.gDrvCfg.nClientId;
    tOsalConfig.pLogFile = NULL;
    tTmlConfig.dwGetMsgThreadId = (uintptr_t) nxpesehal_ctrl.gDrvCfg.nClientId;

#ifdef SPM_INTEGRATED
    /* Get the Access of P61 */
    wSpmStatus = phNxpEseP61_SPM_Init();
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phNxpEseP61_SPM_Init Failed");
        wConfigStatus = ESESTATUS_FAILED;
        goto clean_and_return_2;
    }

    wSpmStatus = phNxpEseP61_SPM_GetState(&current_spm_state);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        ALOGE(" %s : phNxpEseP61_SPM_GetPwrState Failed", __FUNCTION__);
        wConfigStatus = ESESTATUS_FAILED;
        goto clean_and_return_1;
    }
    else
    {
        if ((current_spm_state & SPM_STATE_SPI) | (current_spm_state & SPM_STATE_SPI_PRIO))
        {
            ALOGE(" %s : SPI is already opened...second instance not allowed", __FUNCTION__);
            wConfigStatus = ESESTATUS_FAILED;
            goto clean_and_return_1;
        }
    }
    nxpesehal_ctrl.isRFrame = 0;
    nxpesehal_ctrl.cmd_rsp_state = STATE_IDLE;

    wSpmStatus = phNxpEseP61_SPM_ConfigPwr(SPM_POWER_PRIO_ENABLE);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phNxpEseP61_SPM_ConfigPwr: enabling power for spi prio Failed");
        if (wSpmStatus == SPMSTATUS_DEVICE_BUSY)
        {
            wConfigStatus = ESESTATUS_BUSY;
        }
        else if(wSpmStatus == SPMSTATUS_DEVICE_DWNLD_BUSY)
        {
            wConfigStatus = ESESTATUS_DWNLD_BUSY;
        }
        else
        {
            wConfigStatus = ESESTATUS_FAILED;
        }
        goto clean_and_return;
    }
    else
    {
        NXPLOG_SPIHAL_E("nxpesehal_ctrl.spm_power_state TRUE");
        nxpesehal_ctrl.spm_power_state = TRUE;
    }
#endif

    /* Initialize TML layer */
    wConfigStatus = phTmlEse_Init(&tTmlConfig);
    if (wConfigStatus != ESESTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phTmlEse_Init Failed");
        nxpesehal_ctrl.p_ese_stack_data_cback = NULL;
        goto clean_and_return;
    }

    if (pthread_create(&nxpesehal_ctrl.client_thread, NULL,
            phNxpEseP61_client_thread, &nxpesehal_ctrl) != 0)
    {
        NXPLOG_SPIHAL_E("pthread_create failed");
        wConfigStatus = phTmlEse_Shutdown();
        nxpesehal_ctrl.p_ese_stack_data_cback = NULL;
        goto clean_and_return;
    }
#ifndef SPM_INTEGRATED
    wConfigStatus = phTmlEse_IoCtl(phTmlEse_e_ResetDevice, 2);
    if (wConfigStatus != ESESTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phTmlEse_IoCtl Failed");
        nxpesehal_ctrl.p_ese_stack_data_cback = NULL;
        goto clean_and_return;
    }
#endif
    wConfigStatus = phTmlEse_IoCtl(phTmlEse_e_EnableLog, 0);
    if (wConfigStatus != ESESTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phTmlEse_IoCtl Failed");
        nxpesehal_ctrl.p_ese_stack_data_cback = NULL;
        goto clean_and_return;
    }
    wConfigStatus = phTmlEse_IoCtl(phTmlEse_e_EnablePollMode, 1);

    if (wConfigStatus != ESESTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phTmlEse_IoCtl Failed");
        nxpesehal_ctrl.p_ese_stack_data_cback = NULL;
        goto clean_and_return;
    }
    if (p_data_cback == NULL)
    {
        NXPLOG_SPIHAL_D("%s p_data_cback is null", __FUNCTION__);
        /* if callback is null handle it internally */
        nxpesehal_ctrl.p_ese_stack_data_cback = ese_stack_data_callback;
    }
    else
    {
        NXPLOG_SPIHAL_D("%s p_data_cback is not null", __FUNCTION__);
        nxpesehal_ctrl.p_ese_stack_data_cback = p_data_cback;
    }
    /* STATUS_OPEN */
     nxpesehal_ctrl.halStatus = ESE_STATUS_OPEN;

    CONCURRENCY_UNLOCK();

    NXPLOG_SPIHAL_E("wConfigStatus %x", wConfigStatus);
    if(wConfigStatus == ESESTATUS_SUCCESS)
    {
        /*start the prio session time if the timeout value is non-zero otherwise prio session continues till it closes */
        if(timeOut)
            phNxpEseP61_PrioSessionTimer(nxpesehal_ctrl.timeoutPrioSessionTimerId, timeOut);
    }
    return wConfigStatus;

    clean_and_return:
#ifdef SPM_INTEGRATED
    wSpmStatus = phNxpEseP61_SPM_ConfigPwr(SPM_POWER_DISABLE);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phNxpEseP61_SPM_ConfigPwr: disabling power Failed");
    }
    clean_and_return_1:
    phNxpEseP61_SPM_DeInit();
    clean_and_return_2:
#endif

    CONCURRENCY_UNLOCK();
    phNxpSpiHal_cleanup_monitor();
    nxpesehal_ctrl.halStatus = ESE_STATUS_CLOSE;
    nxpesehal_ctrl.spm_power_state = FALSE;
    return ESESTATUS_FAILED;
}
/******************************************************************************
 * Function         phNxpEseP61_Transceive
 *
 * Description      This function update the len and provided buffer
 *
 * Returns          On Success ESESTATUS_SUCCESS else proper error code
 *
 ******************************************************************************/
ESESTATUS phNxpEseP61_Transceive(uint16_t data_len, const uint8_t *p_data)
{
    ESESTATUS status = ESESTATUS_FAILED;

#ifdef SPM_INTEGRATED
    if (nxpesehal_ctrl.spm_power_state == FALSE)
    {
        NXPLOG_SPIHAL_E(" phNxpEseP61_Transceive - spm_power_state FALSE \n");
        return ESESTATUS_INVALID_STATE;
    }
#endif

    if ((data_len == 0) || p_data == NULL )
    {
        NXPLOG_SPIHAL_E(" phNxpEseP61_Transceive - Invalid Parameter \n");
        return ESESTATUS_INVALID_PARAMETER;
    }
    else if ((ESE_STATUS_CLOSE == nxpesehal_ctrl.halStatus))
    {
        NXPLOG_SPIHAL_E(" %s P61 Not Initialized \n", __FUNCTION__);
        return ESESTATUS_NOT_INITIALISED;
    }
    else if ((ESE_STATUS_BUSY == nxpesehal_ctrl.halStatus))
    {
        NXPLOG_SPIHAL_E(" %s P61 - BUSY \n", __FUNCTION__);
        return ESESTATUS_BUSY;
    }
    else
    {
        nxpesehal_ctrl.halStatus = ESE_STATUS_BUSY;
        status = phNxpEseP61_ProcessData(data_len, (uint8_t *)p_data);
        nxpesehal_ctrl.cmd_rsp_state = STATE_TRANS_ONGOING;
        NXPLOG_SPIHAL_D("%s state = STATE_TRANS_ONGOING", __FUNCTION__);

        if (ESESTATUS_SUCCESS != status)
        {
            NXPLOG_SPIHAL_E(" %s phNxpEseP61_ProcessData- Failed \n", __FUNCTION__);
            nxpesehal_ctrl.halStatus = ESE_STATUS_IDLE;
        }

        NXPLOG_SPIHAL_E(" %s Exit status 0x%x \n", __FUNCTION__, status);
        return status;
    }
}

/******************************************************************************
 * Function         phNxpEseP61_reset
 *
 * Description      This function reset the ESE interface and free all
  *
 * Returns          Always return ESESTATUS_SUCCESS (0).
 *
 ******************************************************************************/
ESESTATUS phNxpEseP61_reset(void)
{
    ESESTATUS status = ESESTATUS_SUCCESS;

#ifdef SPM_INTEGRATED
    SPMSTATUS wSpmStatus = SPMSTATUS_SUCCESS;
#endif

    /* TBD : Call the ioctl to reset the P61 */
    NXPLOG_SPIHAL_D(" %s Enter \n", __FUNCTION__);
    /* if arg ==2 (hard reset)
     * if arg ==1 (soft reset)
     */

#ifdef SPM_INTEGRATED
    wSpmStatus = phNxpEseP61_SPM_ConfigPwr(SPM_POWER_RESET);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phNxpEseP61_SPM_ConfigPwr: reset Failed");
        status = ESESTATUS_FAILED;
    }
#else
    status = phTmlEse_IoCtl(phTmlEse_e_ResetDevice, 2);
    if (status != ESESTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phNxpEseP61_reset Failed");
    }
#endif

    NXPLOG_SPIHAL_D(" %s Exit \n", __FUNCTION__);
    return status;
}
/******************************************************************************
 * Function         phNxpEseP61_close
 *
 * Description      This function close the ESE interface and free all
 *                  resources.
 *
 * Returns          Always return ESESTATUS_SUCCESS (0).
 *
 ******************************************************************************/
ESESTATUS phNxpEseP61_close(void)
{
    ESESTATUS status = ESESTATUS_SUCCESS;
#ifdef SPM_INTEGRATED
    SPMSTATUS wSpmStatus = SPMSTATUS_SUCCESS;
#endif
    CONCURRENCY_LOCK();

#ifdef SPM_INTEGRATED
    /* Release the Access of P61 */
    wSpmStatus = phNxpEseP61_SPM_ConfigPwr(SPM_POWER_DISABLE);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phNxpEseP61_SPM_ConfigPwr: disabling power Failed");
    }
    else
    {
        nxpesehal_ctrl.spm_power_state = FALSE;
    }
    wSpmStatus = phNxpEseP61_SPM_DeInit();
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("phNxpEseP61_SPM_DeInit Failed");
    }

#endif

    if (NULL != gpphTmlEse_Context->pDevHandle)
    {
        /* Abort any pending read and write */
        status = phTmlEse_ReadAbort();
        status = phTmlEse_WriteAbort();

        phNxpEseP61_kill_client_thread(&nxpesehal_ctrl);

        phOsalEse_Timer_Cleanup();

        status = phTmlEse_Shutdown();

        phDal4Ese_msgrelease(nxpesehal_ctrl.gDrvCfg.nClientId);


        memset (&nxpesehal_ctrl, 0x00, sizeof (nxpesehal_ctrl));

        NXPLOG_SPIHAL_D("phNxpEseP61_close - phOsalEse_DeInit completed");
    }

    CONCURRENCY_UNLOCK();

    phNxpSpiHal_cleanup_monitor();

    /* Return success always */
    return status;
}

/******************************************************************************
 * Function         phNxpEseP61_client_thread
 *
 * Description      This function is a thread handler which handles all TML and
 *                  SPI messages.
 *
 * Returns          void
 *
 ******************************************************************************/
STATIC void *phNxpEseP61_client_thread(void *arg)
{
    phNxpEseP61_Control_t *p_nxpesehal_ctrl = (phNxpEseP61_Control_t *) arg;
    phLibEse_Message_t msg;

    NXPLOG_SPIHAL_D("thread started");

    p_nxpesehal_ctrl->thread_running = 1;

    while (p_nxpesehal_ctrl->thread_running == 1)
    {
        /* Fetch next message from the ESE stack message queue */
        if (phDal4Ese_msgrcv(p_nxpesehal_ctrl->gDrvCfg.nClientId,
                &msg, 0, 0) == -1)
        {
            NXPLOG_SPIHAL_E("ESE client received bad message");
            continue;
        }

        if(p_nxpesehal_ctrl->thread_running == 0){
            break;
        }

        switch (msg.eMsgType)
        {
            case PH_LIBESE_DEFERREDCALL_MSG:
            {
                phLibEse_DeferredCall_t *deferCall =
                        (phLibEse_DeferredCall_t *) (msg.pMsgData);

                REENTRANCE_LOCK();
                deferCall->pCallback(deferCall->pParameter);
                REENTRANCE_UNLOCK();
            }
            break;
        }

    }

    NXPLOG_SPIHAL_D("NxpSpiHal thread stopped");

    return NULL;
}

/******************************************************************************
 * Function         phNxpEseP61_kill_client_thread
 *
 * Description      This function safely kill the client thread and clean all
 *                  resources.
 *
 * Returns          void.
 *
 ******************************************************************************/
STATIC void phNxpEseP61_kill_client_thread(phNxpEseP61_Control_t *p_nxpesehal_ctrl)
{
    NXPLOG_SPIHAL_D("Terminating phNxpEseP61 client thread...");

    p_nxpesehal_ctrl->p_ese_stack_data_cback = NULL;
    p_nxpesehal_ctrl->thread_running = 0;

    return;
}

/******************************************************************************
 * Function         phNxpEseP61_read
 *
 * Description      This function write the data to ESE through physical
 *                  interface (e.g. I2C) using the P61 driver interface.
 *                  Before sending the data to ESE, phNxpEseP61_write_ext
 *                  is called to check if there is any extension processing
 *                  is required for the SPI packet being sent out.
 *
 * Returns          It returns number of bytes successfully written to ESE.
 *
 ******************************************************************************/
ESESTATUS phNxpEseP61_read(void)
{
    ESESTATUS status = ESESTATUS_SUCCESS;
    int32_t time_out = HAL_EXTNS_WRITE_RSP_TIMEOUT;

    NXPLOG_SPIHAL_D("%s Enter", __FUNCTION__);
    /* Start timer */
    status = phOsalEse_Timer_Start(nxpesehal_ctrl.timeoutTimerId, time_out,
            &phNxpEseP61_WaitForAckCb, NULL);
    if (ESESTATUS_SUCCESS == status)
    {
        NXPLOG_SPIHAL_D("Response timer started");
    }
    else
    {
        NXPLOG_SPIHAL_E("Response timer not started!!!");
        return ESESTATUS_FAILED;
    }
#if(NFC_NXP_ESE_VER == JCOP_VER_3_2)
    if(nxpesehal_ctrl.wtx_counter_value != 0)
    {
        if(nxpesehal_ctrl.wtx_counter == nxpesehal_ctrl.wtx_counter_value)
        {
            NXPLOG_SPIHAL_E("Avoid read pending on wtx_counter reached!!!");
            return status;
        }
    }
#endif
    /* call read pending */
    status = phTmlEse_Read(
            nxpesehal_ctrl.p_read_buff,
            MAX_DATA_LEN,
            (pphTmlEse_TransactCompletionCb_t) &phNxpEseP61_read_complete,
            NULL);
    if (status != ESESTATUS_PENDING)
    {
        NXPLOG_SPIHAL_E("TML Read status error status = %x", status);
        status = ESESTATUS_FAILED;

    }
    else
    {
        status = ESESTATUS_SUCCESS;
    }
    NXPLOG_SPIHAL_D("%s Exit", __FUNCTION__);
    return status;
}
/******************************************************************************
 * Function         phNxpEseP61_WaitForAckCb
 *
 * Description      Timer call back function
 *
 * Returns          None
 *
 ******************************************************************************/
STATIC void phNxpEseP61_WaitForAckCb(uint32_t timerId, void *pContext)
{
    NXPLOG_SPIHAL_E("%s TIMEOUT !!!", __FUNCTION__);
    bool_t wtx_flag = FALSE;
    UNUSED(timerId);
    UNUSED(pContext);

#if(NFC_NXP_ESE_VER == JCOP_VER_3_2)
    if(nxpesehal_ctrl.wtx_counter_value != 0)
    {
        wtx_flag = TRUE;
    }
#endif
    if(wtx_flag)
    {
        if(nxpesehal_ctrl.wtx_counter == nxpesehal_ctrl.wtx_counter_value)
        {
            nxpesehal_ctrl.wtx_counter = 0;
            phNxpEseP61_SPM_EnablePwr();
            NXPLOG_SPIHAL_D("Enable power to P61 and  reset wtx count !!! ");
            phNxpEseP61_Action(ESESTATUS_FAILED, 0, NULL);
            return;
        }
    }
    if (nxpesehal_ctrl.retry_cnt < 3)
    {
        nxpesehal_ctrl.retry_cnt++;
        phNxpEseP61_read();
    }
    else
    {
        nxpesehal_ctrl.retry_cnt = 0;
        phNxpEseP61_Action(ESESTATUS_RESPONSE_TIMEOUT, 0, NULL);
    }
}

STATIC void phNxpEseP61_PrioSessionTimer(uint32_t timerId, uint32_t timeout)
{
    ESESTATUS wConfigStatus = ESESTATUS_SUCCESS;
    NXPLOG_SPIHAL_E("Enter phNxpEseP61_PrioSessionTimer \n");
    wConfigStatus = phOsalEse_Timer_Start(timerId, timeout, &phNxpEseP61_PrioSessionTimeout, NULL);
    if(wConfigStatus != ESESTATUS_SUCCESS){
        NXPLOG_SPIHAL_E("phOsalEse_Timer_Start failed \n");
    }
    NXPLOG_SPIHAL_E("Exit phNxpEseP61_PrioSessionTimer \n");
}

STATIC void phNxpEseP61_PrioSessionTimeout(uint32_t timerId, void *pContext)
{
    ESESTATUS status = ESESTATUS_SUCCESS;
    NXPLOG_SPIHAL_E("%s TIMEOUT !!!", __FUNCTION__);
    UNUSED(timerId);
    UNUSED(pContext);
    status = phNxpEseP61_SPM_ConfigPwr(SPM_POWER_PRIO_DISABLE);
    if(status != ESESTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_E("%s phNxpEseP61_SPM_ConfigPwr: spi prio timeout failed", __FUNCTION__);
    }
}
/******************************************************************************
 * Function         phNxpEseP61_read_complete
 *
 * Description      This function is called whenever there is an SPI packet
 *                  received from ESE. It could be RSP or NTF packet. This
 *                  function provide the received SPI packet to libese-spi
 *                  using data callback of libese-spi.
 *                  There is a pending read called from each
 *                  phNxpEseP61_read_complete so each a packet received from
 *                  ESE can be provide to libese-spi.
 *
 * Returns          void.
 *
 ******************************************************************************/
STATIC void phNxpEseP61_read_complete(void *pContext, phTmlEse_TransactInfo_t *pInfo)
{
    UNUSED(pContext);
    ESESTATUS status = ESESTATUS_FAILED;

    nxpesehal_ctrl.retry_cnt = 0;
    if (pInfo->wStatus == ESESTATUS_SUCCESS)
    {
        /* Stop Timer */
        status = phOsalEse_Timer_Stop(nxpesehal_ctrl.timeoutTimerId);

        if (ESESTATUS_SUCCESS != status)
        {
            NXPLOG_SPIHAL_E("%s Response timer stop ERROR!!!", __FUNCTION__);
            status = ESESTATUS_FAILED;
            phNxpEseP61_Action(ESESTATUS_FAILED, 0, NULL);
        }

        NXPLOG_SPIHAL_D("%s read successful status = 0x%x",  __FUNCTION__, pInfo->wStatus);

        nxpesehal_ctrl.p_rx_data = pInfo->pBuff;
        nxpesehal_ctrl.rx_data_len = pInfo->wLength;
        phNxpEseP61_ProcessResponse(nxpesehal_ctrl.rx_data_len, &nxpesehal_ctrl.p_rx_data[0]);
    }
    else
    {
        if (nxpesehal_ctrl.halStatus != ESE_STATUS_RECOVERY)
        {
            NXPLOG_SPIHAL_E("%s read error status = 0x%x",  __FUNCTION__, pInfo->wStatus);
            usleep(5000);
            /* call read pending */
            status = phTmlEse_Read(
                    nxpesehal_ctrl.p_read_buff,
                    MAX_DATA_LEN,
                    (pphTmlEse_TransactCompletionCb_t) &phNxpEseP61_read_complete,
                    NULL);
            if (status != ESESTATUS_PENDING)
            {
                NXPLOG_SPIHAL_E("read status error status = %x", status);
                return;
            }
        }
        else
        {
            NXPLOG_SPIHAL_E("read status error status(Recovery in Progress) = %x", status);
        }

    }

    return;
}

/******************************************************************************
 * Function         phNxpEseP61_write_timeout_cb
 *
 * Description      Timer call back function
 *
 * Returns          None
 *
 ******************************************************************************/
STATIC void phNxpEseP61_write_timeout_cb(uint32_t timerId, void *pContext)
{
    UNUSED(timerId);
    UNUSED(pContext);
    NXPLOG_SPIHAL_E("%s Write Timeout Occured", __FUNCTION__);
    phNxpEseP61_Action(ESESTATUS_WRITE_TIMEOUT, 0, NULL);
}

/******************************************************************************
 * Function         phNxpEseP61_WriteFrame
 *
 * Description      This is the actual function which is being called by
 *                  phNxpEseP61_write. This function writes the data to ESE.
 *                  It waits till write callback provide the result of write
 *                  process.
 *
 * Returns          It returns number of bytes successfully written to ESE.
 *
 ******************************************************************************/
ESESTATUS phNxpEseP61_WriteFrame(uint32_t data_len, const uint8_t *p_data)
{
    ESESTATUS status = ESESTATUS_INVALID_PARAMETER;
    phNxpSpiHal_Sem_t cb_data;
    nxpesehal_ctrl.retry_cnt = 0;

    CONCURRENCY_LOCK();
    /* Create the local semaphore */
    if (phNxpSpiHal_init_cb_data(&cb_data, NULL) != ESESTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_D("phNxpEseP61_write_unlocked Create cb data failed");
        data_len = 0;
        goto clean_and_return;
    }

    /* Create local copy of cmd_data */
    memcpy(nxpesehal_ctrl.p_cmd_data, p_data, data_len);
    nxpesehal_ctrl.cmd_len = data_len;

    retry:

    status = phTmlEse_Write( (uint8_t *) nxpesehal_ctrl.p_cmd_data,
            (uint16_t) nxpesehal_ctrl.cmd_len,
            (pphTmlEse_TransactCompletionCb_t) &phNxpEseP61_write_complete,
            (void *) &cb_data);
    if (status != ESESTATUS_PENDING)
    {
        NXPLOG_SPIHAL_E("write_unlocked status error");
        data_len = 0;
        goto clean_and_return;
    }

    /* Start timer */
    status = phOsalEse_Timer_Start(nxpesehal_ctrl.timeoutTimerId,
            HAL_EXTNS_WRITE_RSP_TIMEOUT,
            &phNxpEseP61_write_timeout_cb,
            NULL);
    if (ESESTATUS_SUCCESS == status)
    {
        NXPLOG_SPIHAL_D("Write Response timer started");
    }
    else
    {
        NXPLOG_SPIHAL_E("Write Response timer not started!!!");
        status  = ESESTATUS_FAILED;
        goto clean_and_return;
    }

    /* Wait for callback response */
    if (SEM_WAIT(cb_data))
    {
        NXPLOG_SPIHAL_E("write_unlocked semaphore error");
        status  = ESESTATUS_FAILED;
        goto clean_and_return;
    }
    /* Stop Timer */
    status = phOsalEse_Timer_Stop(nxpesehal_ctrl.timeoutTimerId);

    if (ESESTATUS_SUCCESS != status)
    {
        NXPLOG_SPIHAL_E("Write Response timer stop ERROR!!!");
        status  = ESESTATUS_FAILED;
    }
    else
    {
        NXPLOG_SPIHAL_D("Write Response timer stoped");
    }

    if (cb_data.status != ESESTATUS_SUCCESS)
    {
        data_len = 0;
        if(nxpesehal_ctrl.retry_cnt++ < MAX_RETRY_COUNT)
        {
            NXPLOG_SPIHAL_E("write_unlocked failed - P61 Maybe in Standby Mode - Retry");
            /* 1ms delay to give ESE wake up delay */
            usleep(1000);
            goto retry;
        }
        else
        {

            NXPLOG_SPIHAL_E("write_unlocked failed - P61 Maybe in Standby Mode (max count = 0x%x)", nxpesehal_ctrl.retry_cnt);

        }
    }
    else
    {
        status  = ESESTATUS_SUCCESS;

    }

    clean_and_return:
    phNxpSpiHal_cleanup_cb_data(&cb_data);
    CONCURRENCY_UNLOCK();
    NXPLOG_SPIHAL_E("Exit %s status %x\n", __FUNCTION__, status);
    return status;
}

/******************************************************************************
 * Function         phNxpEseP61_write_complete
 *
 * Description      This function handles write callback.
 *
 * Returns          void.
 *
 ******************************************************************************/
STATIC void phNxpEseP61_write_complete(void *pContext, phTmlEse_TransactInfo_t *pInfo)
{
    phNxpSpiHal_Sem_t *p_cb_data = (phNxpSpiHal_Sem_t*) pContext;

    if (pInfo->wStatus == ESESTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_D("write successful status = 0x%x", pInfo->wStatus);
    }
    else
    {
        NXPLOG_SPIHAL_E("write error status = 0x%x", pInfo->wStatus);
    }

    p_cb_data->status = pInfo->wStatus;

    SEM_POST(p_cb_data);

    return;
}
/******************************************************************************
 * Function         phNxpEseP61_InternalWriteFrame
 *
 * Description      This is the actual function which is being called by
 *                  phNxpEseP61_write. This function post msg to writer thread.
  *
 * Returns          It returns success or failure
 *
 ******************************************************************************/
ESESTATUS phNxpEseP61_InternalWriteFrame(uint32_t data_len, const uint8_t *p_data)
{
    ESESTATUS status = ESESTATUS_FAILED;
    nxpesehal_ctrl.retry_cnt = 0;

    CONCURRENCY_LOCK();
     /* Create local copy of cmd_data */
    memcpy(nxpesehal_ctrl.p_cmd_data, p_data, data_len);
    nxpesehal_ctrl.cmd_len = data_len;

    /* Start timer */
    status = phOsalEse_Timer_Start(nxpesehal_ctrl.timeoutTimerId,
            HAL_EXTNS_WRITE_RSP_TIMEOUT,
            &phNxpEseP61_write_timeout_cb,
            NULL);
    if (ESESTATUS_SUCCESS == status)
    {
        NXPLOG_SPIHAL_D("%s Write Response timer started",  __FUNCTION__);
    }
    else
    {
        NXPLOG_SPIHAL_E("%s Write Response timer not started!!!",  __FUNCTION__);
        status  = ESESTATUS_FAILED;
        goto clean_and_return;
    }


    status = phTmlEse_Write( (uint8_t *) nxpesehal_ctrl.p_cmd_data,
            (uint16_t) nxpesehal_ctrl.cmd_len,
            (pphTmlEse_TransactCompletionCb_t) &phNxpEseP61_internal_write_complete,
            (void *) NULL);
    if (status != ESESTATUS_PENDING)
    {
        NXPLOG_SPIHAL_E("%s write_unlocked status error", __FUNCTION__);
        goto clean_and_return;
    }
    else
    {
        status = ESESTATUS_SUCCESS;
    }

clean_and_return:
    CONCURRENCY_UNLOCK();
    NXPLOG_SPIHAL_E("Exit %s status %x\n", __FUNCTION__, status);
    return status;
}

/******************************************************************************
 * Function         phNxpEseP61_internal_write_complete
 *
 * Description      This function handles internal write callback.
 *
 * Returns          void.
 *
 ******************************************************************************/
STATIC void phNxpEseP61_internal_write_complete(void *pContext, phTmlEse_TransactInfo_t *pInfo)
{
    UNUSED(pInfo);
    UNUSED(pContext);
    /* Stop Timer */
    if (ESESTATUS_SUCCESS == phOsalEse_Timer_Stop(nxpesehal_ctrl.timeoutTimerId))
    {
        NXPLOG_SPIHAL_D("%s Write Response timer stopped", __FUNCTION__);
    }
    else
    {
        NXPLOG_SPIHAL_E("%s Write Response timer stop ERROR!!!", __FUNCTION__);
    }

    if (pInfo->wStatus == ESESTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_D("%s write successful status = 0x%x", __FUNCTION__, pInfo->wStatus);
        /* TBD may required some delay, if interrupt not used */
        //usleep(100000);
        if (ESESTATUS_SUCCESS != phNxpEseP61_read())
        {
            NXPLOG_SPIHAL_E("%s phNxpEseP61_read failed", __FUNCTION__);
        }
    }
    else
    {
        NXPLOG_SPIHAL_E("%s write error status = 0x%x", __FUNCTION__, pInfo->wStatus);
        phNxpEseP61_Action(ESESTATUS_FAILED, 0, NULL);
    }

    return;
}
