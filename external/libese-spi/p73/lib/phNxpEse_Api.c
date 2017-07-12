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

#include <phNxpEse_Internal.h>
#include <phNxpEsePal.h>
#include <phNxpLog.h>
#include <phNxpEseProto7816_3.h>
#include <phNxpConfig.h>
#include <NXP_ESE_FEATURES.h>
#include <phNxpEsePal_spi.h>

#define RECIEVE_PACKET_SOF      0xA5
static int phNxpEse_readPacket(void *pDevHandle, uint8_t * pBuffer, int nNbBytesToRead);
static ESESTATUS phNxpEse_checkJcopDwnldState(void);
static ESESTATUS phNxpEse_setJcopDwnldState(phNxpEse_JcopDwnldState state);
static ESESTATUS phNxpEse_checkFWDwnldStatus(void);
static void phNxpEse_GetMaxTimer(unsigned long *pMaxTimer);
static unsigned char * phNxpEse_GgetTimerTlvBuffer(unsigned char *timer_buffer, unsigned int value);
/*********************** Global Variables *************************************/

/* ESE Context structure */
phNxpEse_Context_t nxpese_ctxt;

/******************************************************************************
 * Function         phNxpEse_init
 *
 * Description      This function is called by Jni/phNxpEse_open during the
 *                  initialization of the ESE. It initializes protocol stack instance variable
 *
 * Returns          This function return ESESTATUS_SUCCES (0) in case of success
 *                  In case of failure returns other failure value.
 *
 ******************************************************************************/
ESESTATUS phNxpEse_init(phNxpEse_initParams initParams)
{
    ESESTATUS wConfigStatus = ESESTATUS_SUCCESS;
    unsigned long int num;
    bool_t status = FALSE;
    unsigned long maxTimer = 0;
    phNxpEseProto7816InitParam_t protoInitParam;
    phNxpEse_memset(&protoInitParam, 0x00, sizeof(phNxpEseProto7816InitParam_t));
    /* STATUS_OPEN */
    nxpese_ctxt.EseLibStatus = ESE_STATUS_OPEN;

#ifdef ESE_DEBUG_UTILS_INCLUDED
    if (GetNxpNumValue (NAME_NXP_WTX_COUNT_VALUE, &num, sizeof(num)))
    {
        protoInitParam.wtx_counter_limit = num;
        NXPLOG_ESELIB_D("Wtx_counter read from config file - %lu", protoInitParam.wtx_counter_limit);
    }
    else
    {
        protoInitParam.wtx_counter_limit = PH_PROTO_WTX_DEFAULT_COUNT;
    }
    if(GetNxpNumValue (NAME_NXP_MAX_RNACK_RETRY, &num, sizeof(num)))
    {
        protoInitParam.rnack_retry_limit = num;
    }
    else
    {
        protoInitParam.rnack_retry_limit = MAX_RNACK_RETRY_LIMIT;
    }
#else
    protoInitParam.wtx_counter_limit = PH_PROTO_WTX_DEFAULT_COUNT;
#endif
    if (ESE_MODE_NORMAL == initParams.initMode) /* TZ/Normal wired mode should come here*/
    {
#ifdef ESE_DEBUG_UTILS_INCLUDED
        if (GetNxpNumValue (NAME_NXP_SPI_INTF_RST_ENABLE, &num, sizeof(num)))
        {
            protoInitParam.interfaceReset = (num == 1)?TRUE:FALSE;
        }
        else
#endif
        {
            protoInitParam.interfaceReset = TRUE;
        }
    }
    else /* OSU mode, no interface reset is required */
    {
        protoInitParam.interfaceReset = FALSE;
    }
    /* Sharing lib context for fetching secure timer values */
    protoInitParam.pSecureTimerParams = (phNxpEseProto7816SecureTimer_t *)&nxpese_ctxt.secureTimerParams;

    NXPLOG_ESELIB_D("%s secureTimer1 0x%x secureTimer2 0x%x secureTimer3 0x%x", __FUNCTION__,
            nxpese_ctxt.secureTimerParams.secureTimer1,
            nxpese_ctxt.secureTimerParams.secureTimer2,
            nxpese_ctxt.secureTimerParams.secureTimer3);

    phNxpEse_GetMaxTimer(&maxTimer);
#ifdef SPM_INTEGRATED
#if(NXP_SECURE_TIMER_SESSION == TRUE)
    status = phNxpEse_SPM_DisablePwrControl(maxTimer);
    if(status != ESESTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("%s phNxpEse_SPM_DisablePwrControl: failed", __FUNCTION__);
    }
#endif
#endif
    /* Delay before the first transceive */
    phPalEse_sleep(3000);

    /* T=1 Protocol layer open */
    status = phNxpEseProto7816_Open(protoInitParam);
    if(FALSE == status)
    {
        wConfigStatus = ESESTATUS_FAILED;
        NXPLOG_ESELIB_E("phNxpEseProto7816_Open failed");
    }
    return wConfigStatus;
}

/******************************************************************************
 * Function         phNxpEse_open
 *
 * Description      This function is called by Jni during the
 *                  initialization of the ESE. It opens the physical connection
 *                  with ESE and creates required client thread for
 *                  operation.
 * Returns          This function return ESESTATUS_SUCCES (0) in case of success
 *                  In case of failure returns other failure value.
 *
 ******************************************************************************/
ESESTATUS phNxpEse_open(phNxpEse_initParams initParams)
{
    phPalEse_Config_t tPalConfig;
    bool_t status = FALSE;
    ESESTATUS wConfigStatus = ESESTATUS_SUCCESS;
    unsigned long int num, tpm_enable = 0;
#ifdef SPM_INTEGRATED
    SPMSTATUS wSpmStatus = SPMSTATUS_SUCCESS;
    spm_state_t current_spm_state = SPM_STATE_INVALID;
#endif

    NXPLOG_ESELIB_E("phNxpEse_open Enter");
    /*When spi channel is already opened return status as FAILED*/
    if(nxpese_ctxt.EseLibStatus != ESE_STATUS_CLOSE)
    {
        ALOGD("already opened\n");
        return ESESTATUS_BUSY;
    }

    phNxpEse_memset(&nxpese_ctxt, 0x00, sizeof(nxpese_ctxt));
    phNxpEse_memset(&tPalConfig, 0x00, sizeof(tPalConfig));

    NXPLOG_ESELIB_E("MW SEAccessKit Version");
    NXPLOG_ESELIB_E("Android Version:0x%x", NXP_ANDROID_VER);
    NXPLOG_ESELIB_E("Major Version:0x%x", ESELIB_MW_VERSION_MAJ);
    NXPLOG_ESELIB_E("Minor Version:0x%x", ESELIB_MW_VERSION_MIN);

#ifdef ESE_DEBUG_UTILS_INCLUDED
    if (GetNxpNumValue (NAME_NXP_TP_MEASUREMENT, &tpm_enable, sizeof(tpm_enable)))
    {
        NXPLOG_ESELIB_E("SPI Throughput measurement enable/disable read from config file - %lu",num);
    }
    else
    {
        NXPLOG_ESELIB_E("SPI Throughput not defined in config file - %lu",num);
    }
#if(NXP_POWER_SCHEME_SUPPORT == TRUE)
    if (GetNxpNumValue (NAME_NXP_POWER_SCHEME, &num, sizeof(num)))
    {
        nxpese_ctxt.pwr_scheme = num;
        NXPLOG_ESELIB_E("Power scheme read from config file - %lu",num);
    }
    else
#endif
    {
        nxpese_ctxt.pwr_scheme = PN67T_POWER_SCHEME;
        NXPLOG_ESELIB_E("Power scheme not defined in config file - %lu",num);
    }
#else
    nxpese_ctxt.pwr_scheme = PN67T_POWER_SCHEME;
    tpm_enable  = 0x00;
#endif
#ifdef ESE_DEBUG_UTILS_INCLUDED
    /* reset config cache */
    resetNxpConfig();
#endif
    /* initialize trace level */
    phNxpLog_InitializeLogLevel();
    tPalConfig.pDevName = (int8_t *) "/dev/p73";

    /* Initialize PAL layer */
    wConfigStatus = phPalEse_open_and_configure(&tPalConfig);
    if (wConfigStatus != ESESTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phPalEse_Init Failed");
        goto clean_and_return;
    }
    /* Copying device handle to ESE Lib context*/
    nxpese_ctxt.pDevHandle = tPalConfig.pDevHandle;

#ifdef SPM_INTEGRATED
    /* Get the Access of ESE*/
    wSpmStatus = phNxpEse_SPM_Init(nxpese_ctxt.pDevHandle);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phNxpEse_SPM_Init Failed");
        wConfigStatus = ESESTATUS_FAILED;
        goto clean_and_return_2;
    }
    wSpmStatus = phNxpEse_SPM_SetPwrScheme(nxpese_ctxt.pwr_scheme);
    if( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        ALOGE(" %s : phNxpEse_SPM_SetPwrScheme Failed", __FUNCTION__);
        wConfigStatus = ESESTATUS_FAILED;
        goto clean_and_return_1;
    }
#if(NXP_NFCC_SPI_FW_DOWNLOAD_SYNC == TRUE)
    wConfigStatus = phNxpEse_checkFWDwnldStatus();
    if(wConfigStatus != ESESTATUS_SUCCESS)
    {
        ALOGD("Failed to open SPI due to VEN pin used by FW download \n");
        wConfigStatus = ESESTATUS_FAILED;
        goto clean_and_return_1;
    }
#endif
    wSpmStatus = phNxpEse_SPM_GetState(&current_spm_state);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E(" %s : phNxpEse_SPM_GetPwrState Failed", __FUNCTION__);
        wConfigStatus = ESESTATUS_FAILED;
        goto clean_and_return_1;
    }
    else
    {
        if ((current_spm_state & SPM_STATE_SPI) | (current_spm_state & SPM_STATE_SPI_PRIO))
        {
            NXPLOG_ESELIB_E(" %s : SPI is already opened...second instance not allowed", __FUNCTION__);
            wConfigStatus = ESESTATUS_FAILED;
            goto clean_and_return_1;
        }
    }
#if(NXP_ESE_JCOP_DWNLD_PROTECTION == TRUE)
    if(current_spm_state & SPM_STATE_JCOP_DWNLD)
    {
        ALOGE(" %s : Denying to open JCOP Download in progress", __FUNCTION__);
        wConfigStatus = ESESTATUS_FAILED;
        goto clean_and_return_1;
    }
#endif
    phNxpEse_memcpy(&nxpese_ctxt.initParams, &initParams, sizeof(phNxpEse_initParams));
    /* Updating ESE power state based on the init mode */
    if(ESE_MODE_OSU == nxpese_ctxt.initParams.initMode)
    {
        NXPLOG_ESELIB_E("%s Init mode ---->OSU", __FUNCTION__);
        wConfigStatus = phNxpEse_checkJcopDwnldState();
        if(wConfigStatus != ESESTATUS_SUCCESS)
        {
            NXPLOG_ESELIB_E("phNxpEse_checkJcopDwnldState failed");
            goto clean_and_return_1;
        }
    }
    wSpmStatus = phNxpEse_SPM_ConfigPwr(SPM_POWER_ENABLE);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phNxpEse_SPM_ConfigPwr: enabling power Failed");
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
        NXPLOG_ESELIB_D("nxpese_ctxt.spm_power_state TRUE");
        nxpese_ctxt.spm_power_state = TRUE;
    }
#endif

#ifndef SPM_INTEGRATED
    wConfigStatus = phPalEse_ioctl(phPalEse_e_ResetDevice, nxpese_ctxt.pDevHandle, 2);
    if (wConfigStatus != ESESTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phPalEse_IoCtl Failed");
        goto clean_and_return;
    }
#endif
    wConfigStatus = phPalEse_ioctl(phPalEse_e_EnableLog, nxpese_ctxt.pDevHandle, 0);
    if (wConfigStatus != ESESTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phPalEse_IoCtl Failed");
        goto clean_and_return;
    }
    wConfigStatus = phPalEse_ioctl(phPalEse_e_EnablePollMode, nxpese_ctxt.pDevHandle, 1);
    if(tpm_enable)
    {
        wConfigStatus = phPalEse_ioctl(phPalEse_e_EnableThroughputMeasurement, nxpese_ctxt.pDevHandle, 0);
        if (wConfigStatus != ESESTATUS_SUCCESS)
        {
            NXPLOG_ESELIB_E("phPalEse_IoCtl Failed");
            goto clean_and_return;
        }
    }
    if (wConfigStatus != ESESTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phPalEse_IoCtl Failed");
        goto clean_and_return;
    }

    NXPLOG_ESELIB_D("wConfigStatus %x", wConfigStatus);
    return wConfigStatus;

    clean_and_return:
#ifdef SPM_INTEGRATED
    wSpmStatus = phNxpEse_SPM_ConfigPwr(SPM_POWER_DISABLE);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phNxpEse_SPM_ConfigPwr: disabling power Failed");
    }
    clean_and_return_1:
    phNxpEse_SPM_DeInit();
    clean_and_return_2:
#endif
    if (NULL != nxpese_ctxt.pDevHandle)
    {
        phPalEse_close(nxpese_ctxt.pDevHandle);
        phNxpEse_memset (&nxpese_ctxt, 0x00, sizeof (nxpese_ctxt));
    }
    nxpese_ctxt.EseLibStatus = ESE_STATUS_CLOSE;
    nxpese_ctxt.spm_power_state = FALSE;
    return ESESTATUS_FAILED;
}

/******************************************************************************
 * Function         phNxpEse_openPrioSession
 *
 * Description      This function is called by Jni during the
 *                  initialization of the ESE. It opens the physical connection
 *                  with ESE () and creates required client thread for
 *                  operation.  This will get priority access to ESE for timeout duration.

 * Returns          This function return ESESTATUS_SUCCES (0) in case of success
 *                  In case of failure returns other failure value.
 *
 ******************************************************************************/
ESESTATUS phNxpEse_openPrioSession(phNxpEse_initParams initParams)
{
    phPalEse_Config_t tPalConfig;
    ESESTATUS wConfigStatus = ESESTATUS_SUCCESS;
    unsigned long int num, tpm_enable = 0;

    NXPLOG_ESELIB_E("phNxpEse_openPrioSession Enter");
#ifdef SPM_INTEGRATED
    SPMSTATUS wSpmStatus = SPMSTATUS_SUCCESS;
    spm_state_t current_spm_state = SPM_STATE_INVALID;
#endif
    phNxpEse_memset(&nxpese_ctxt, 0x00, sizeof(nxpese_ctxt));
    phNxpEse_memset(&tPalConfig, 0x00, sizeof(tPalConfig));

    NXPLOG_ESELIB_E("MW SEAccessKit Version");
    NXPLOG_ESELIB_E("Android Version:0x%x", NXP_ANDROID_VER);
    NXPLOG_ESELIB_E("Major Version:0x%x", ESELIB_MW_VERSION_MAJ);
    NXPLOG_ESELIB_E("Minor Version:0x%x", ESELIB_MW_VERSION_MIN);

#ifdef ESE_DEBUG_UTILS_INCLUDED
#if(NXP_POWER_SCHEME_SUPPORT == TRUE)
    if (GetNxpNumValue (NAME_NXP_POWER_SCHEME, &num, sizeof(num)))
    {
        nxpese_ctxt.pwr_scheme = num;
        NXPLOG_ESELIB_E("Power scheme read from config file - %lu",num);
    }
    else
#endif
    {
        nxpese_ctxt.pwr_scheme = PN67T_POWER_SCHEME;
        NXPLOG_ESELIB_E("Power scheme not defined in config file - %lu",num);
    }
    if (GetNxpNumValue (NAME_NXP_TP_MEASUREMENT, &tpm_enable, sizeof(tpm_enable)))
    {
        NXPLOG_ESELIB_E("SPI Throughput measurement enable/disable read from config file - %lu",num);
    }
    else
    {
        NXPLOG_ESELIB_E("SPI Throughput not defined in config file - %lu",num);
    }
#else
    nxpese_ctxt.pwr_scheme = PN67T_POWER_SCHEME;
#endif
#ifdef ESE_DEBUG_UTILS_INCLUDED
    /* reset config cache */
    resetNxpConfig();
#endif
    /* initialize trace level */
    phNxpLog_InitializeLogLevel();

    tPalConfig.pDevName = (int8_t *) "/dev/p73";


    /* Initialize PAL layer */
    wConfigStatus = phPalEse_open_and_configure(&tPalConfig);
    if (wConfigStatus != ESESTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phPalEse_Init Failed");
        goto clean_and_return;
    }
    /* Copying device handle to hal context*/
    nxpese_ctxt.pDevHandle = tPalConfig.pDevHandle;

#ifdef SPM_INTEGRATED
    /* Get the Access of ESE*/
    wSpmStatus = phNxpEse_SPM_Init(nxpese_ctxt.pDevHandle);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phNxpEse_SPM_Init Failed");
        wConfigStatus = ESESTATUS_FAILED;
        goto clean_and_return_2;
    }
    wSpmStatus = phNxpEse_SPM_SetPwrScheme(nxpese_ctxt.pwr_scheme);
    if( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        ALOGE(" %s : phNxpEse_SPM_SetPwrScheme Failed", __FUNCTION__);
        wConfigStatus = ESESTATUS_FAILED;
        goto clean_and_return_1;
    }
    wSpmStatus = phNxpEse_SPM_GetState(&current_spm_state);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        ALOGE(" %s : phNxpEse_SPM_GetPwrState Failed", __FUNCTION__);
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
#if(NXP_ESE_JCOP_DWNLD_PROTECTION == TRUE)
        if(current_spm_state & SPM_STATE_JCOP_DWNLD)
        {
            ALOGE(" %s : Denying to open JCOP Download in progress", __FUNCTION__);
            wConfigStatus = ESESTATUS_FAILED;
            goto clean_and_return_1;
        }
#endif
#if(NXP_NFCC_SPI_FW_DOWNLOAD_SYNC == TRUE)
    wConfigStatus = phNxpEse_checkFWDwnldStatus();
    if(wConfigStatus != ESESTATUS_SUCCESS)
    {
        ALOGD("Failed to open SPI due to VEN pin used by FW download \n");
        wConfigStatus = ESESTATUS_FAILED;
        goto clean_and_return_1;
    }
#endif
    }
    phNxpEse_memcpy(&nxpese_ctxt.initParams, &initParams.initMode, sizeof(phNxpEse_initParams));
    /* Updating ESE power state based on the init mode */
    if(ESE_MODE_OSU == nxpese_ctxt.initParams.initMode)
    {
        wConfigStatus = phNxpEse_checkJcopDwnldState();
        if(wConfigStatus != ESESTATUS_SUCCESS)
        {
            NXPLOG_ESELIB_E("phNxpEse_checkJcopDwnldState failed");
            goto clean_and_return_1;
        }
    }
    wSpmStatus = phNxpEse_SPM_ConfigPwr(SPM_POWER_PRIO_ENABLE);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phNxpEse_SPM_ConfigPwr: enabling power for spi prio Failed");
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
        NXPLOG_ESELIB_E("nxpese_ctxt.spm_power_state TRUE");
        nxpese_ctxt.spm_power_state = TRUE;
    }
#endif

#ifndef SPM_INTEGRATED
    wConfigStatus = phPalEse_ioctl(phPalEse_e_ResetDevice, nxpese_ctxt.pDevHandle, 2);
    if (wConfigStatus != ESESTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phPalEse_IoCtl Failed");
        goto clean_and_return;
    }
#endif
    wConfigStatus = phPalEse_ioctl(phPalEse_e_EnableLog, nxpese_ctxt.pDevHandle, 0);
    if (wConfigStatus != ESESTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phPalEse_IoCtl Failed");
        goto clean_and_return;
    }
    wConfigStatus = phPalEse_ioctl(phPalEse_e_EnablePollMode, nxpese_ctxt.pDevHandle, 1);
    if(tpm_enable)
    {
        wConfigStatus = phPalEse_ioctl(phPalEse_e_EnableThroughputMeasurement, nxpese_ctxt.pDevHandle, 0);
       if (wConfigStatus != ESESTATUS_SUCCESS)
       {
           NXPLOG_ESELIB_E("phPalEse_IoCtl Failed");
           goto clean_and_return;
       }
    }
    if (wConfigStatus != ESESTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phPalEse_IoCtl Failed");
        goto clean_and_return;
    }

    NXPLOG_ESELIB_E("wConfigStatus %x", wConfigStatus);

    return wConfigStatus;

    clean_and_return:
#ifdef SPM_INTEGRATED
    wSpmStatus = phNxpEse_SPM_ConfigPwr(SPM_POWER_DISABLE);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phNxpEse_SPM_ConfigPwr: disabling power Failed");
    }
    clean_and_return_1:
    phNxpEse_SPM_DeInit();
    clean_and_return_2:
#endif
    if (NULL != nxpese_ctxt.pDevHandle)
    {
        phPalEse_close(nxpese_ctxt.pDevHandle);
        phNxpEse_memset (&nxpese_ctxt, 0x00, sizeof (nxpese_ctxt));
    }
    nxpese_ctxt.EseLibStatus = ESE_STATUS_CLOSE;
    nxpese_ctxt.spm_power_state = FALSE;
    return ESESTATUS_FAILED;
}
#if(NXP_ESE_JCOP_DWNLD_PROTECTION == TRUE)
/******************************************************************************
 * Function         phNxpEse_setJcopDwnldState
 *
 * Description      This function is  used to check whether JCOP OS
 *                  download can be started or not.
 *
 * Returns          returns  ESESTATUS_SUCCESS or ESESTATUS_FAILED
 *
 ******************************************************************************/
static ESESTATUS phNxpEse_setJcopDwnldState(phNxpEse_JcopDwnldState state)
{
    SPMSTATUS wSpmStatus = SPMSTATUS_SUCCESS;
    ESESTATUS wConfigStatus = ESESTATUS_FAILED;
    NXPLOG_ESELIB_E("phNxpEse_setJcopDwnldState Enter");

    wSpmStatus = phNxpEse_SPM_SetJcopDwnldState(state);
    if(wSpmStatus == SPMSTATUS_SUCCESS)
    {
        wConfigStatus = ESESTATUS_SUCCESS;
    }

    return wConfigStatus;
}

/******************************************************************************
 * Function         phNxpEse_checkJcopDwnldState
 *
 * Description      This function is  used to check whether JCOP OS
 *                  download can be started or not.
 *
 * Returns          returns  ESESTATUS_SUCCESS or ESESTATUS_BUSY
 *
 ******************************************************************************/
static ESESTATUS phNxpEse_checkJcopDwnldState(void)
{
    NXPLOG_ESELIB_E("phNxpEse_checkJcopDwnld Enter");
    SPMSTATUS wSpmStatus = SPMSTATUS_SUCCESS;
    spm_state_t current_spm_state = SPM_STATE_INVALID;
    uint8_t ese_dwnld_retry = 0x00;
    ESESTATUS status = ESESTATUS_FAILED;

    wSpmStatus = phNxpEse_SPM_GetState(&current_spm_state);
    if (wSpmStatus == SPMSTATUS_SUCCESS)
    {
       /* Check current_spm_state and update config/Spm status*/
       if((current_spm_state & SPM_STATE_JCOP_DWNLD) || (current_spm_state & SPM_STATE_WIRED))
          return ESESTATUS_BUSY;

       wSpmStatus = phNxpEse_setJcopDwnldState(JCP_DWNLD_INIT);
       if(wSpmStatus == SPMSTATUS_SUCCESS)
       {
           while(ese_dwnld_retry < ESE_JCOP_OS_DWNLD_RETRY_CNT)
           {
               NXPLOG_ESELIB_E("ESE_JCOP_OS_DWNLD_RETRY_CNT retry count");
               wSpmStatus = phNxpEse_SPM_GetState(&current_spm_state);
               if(wSpmStatus == SPMSTATUS_SUCCESS)
               {
                   if((current_spm_state & SPM_STATE_JCOP_DWNLD))
                   {
                       status = ESESTATUS_SUCCESS;
                       break;
                    }
                }
                else
                {
                    status = ESESTATUS_FAILED;
                    break;
                }
                phNxpEse_Sleep(200000);    /*sleep for 200 ms checking for jcop dwnld status*/
                ese_dwnld_retry++;
            }
        }
    }

    NXPLOG_ESELIB_E("phNxpEse_checkJcopDwnldState status %x", status);
    return status;
}
#endif
/******************************************************************************
 * Function         phNxpEse_Transceive
 *
 * Description      This function update the len and provided buffer
 *
 * Returns          On Success ESESTATUS_SUCCESS else proper error code
 *
 ******************************************************************************/
ESESTATUS phNxpEse_Transceive( phNxpEse_data *pCmd, phNxpEse_data *pRsp)
{
    ESESTATUS status = ESESTATUS_FAILED;
    bool_t bStatus = FALSE;

    if((NULL == pCmd) || (NULL == pRsp))
        return ESESTATUS_INVALID_PARAMETER;

    if ((pCmd->len == 0) || pCmd->p_data == NULL )
    {
        NXPLOG_ESELIB_E(" phNxpEse_Transceive - Invalid Parameter no data\n");
        return ESESTATUS_INVALID_PARAMETER;
    }
    else if ((ESE_STATUS_CLOSE == nxpese_ctxt.EseLibStatus))
    {
        NXPLOG_ESELIB_E(" %s ESE Not Initialized \n", __FUNCTION__);
        return ESESTATUS_NOT_INITIALISED;
    }
    else if ((ESE_STATUS_BUSY == nxpese_ctxt.EseLibStatus))
    {
        NXPLOG_ESELIB_E(" %s ESE - BUSY \n", __FUNCTION__);
        return ESESTATUS_BUSY;
    }
    else
    {
        nxpese_ctxt.EseLibStatus = ESE_STATUS_BUSY;
        bStatus = phNxpEseProto7816_Transceive((phNxpEse_data*)pCmd, (phNxpEse_data*)pRsp);
        if(TRUE == bStatus)
        {
            status = ESESTATUS_SUCCESS;
        }
        else
        {
            status = ESESTATUS_FAILED;
        }

        if (ESESTATUS_SUCCESS != status)
        {
            NXPLOG_ESELIB_E(" %s phNxpEseProto7816_Transceive- Failed \n", __FUNCTION__);
        }
        nxpese_ctxt.EseLibStatus = ESE_STATUS_IDLE;

        NXPLOG_ESELIB_D(" %s Exit status 0x%x \n", __FUNCTION__, status);
        return status;
    }
}

/******************************************************************************
 * Function         phNxpEse_reset
 *
 * Description      This function reset the ESE interface and free all
 *
 * Returns          It returns ESESTATUS_SUCCESS (0) if the operation is successful else
 *                  ESESTATUS_FAILED(1)
 ******************************************************************************/
ESESTATUS phNxpEse_reset(void)
{
    ESESTATUS status = ESESTATUS_SUCCESS;
    unsigned long maxTimer = 0;
#ifdef SPM_INTEGRATED
    SPMSTATUS wSpmStatus = SPMSTATUS_SUCCESS;
#endif

    /* TBD : Call the ioctl to reset the ESE */
    NXPLOG_ESELIB_D(" %s Enter \n", __FUNCTION__);
    /* Do an interface reset, don't wait to see if JCOP went through a full power cycle or not */
    bool_t bStatus = phNxpEseProto7816_IntfReset((phNxpEseProto7816SecureTimer_t *)&nxpese_ctxt.secureTimerParams);
    if(!bStatus)
        status = ESESTATUS_FAILED;
    NXPLOG_ESELIB_D("%s secureTimer1 0x%x secureTimer2 0x%x secureTimer3 0x%x", __FUNCTION__,
            nxpese_ctxt.secureTimerParams.secureTimer1,
            nxpese_ctxt.secureTimerParams.secureTimer2,
            nxpese_ctxt.secureTimerParams.secureTimer3);
    phNxpEse_GetMaxTimer(&maxTimer);
#ifdef SPM_INTEGRATED
#if(NXP_SECURE_TIMER_SESSION == TRUE)
    status = phNxpEse_SPM_DisablePwrControl(maxTimer);
    if(status != ESESTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("%s phNxpEse_SPM_DisablePwrControl: failed", __FUNCTION__);
    }
#endif
    if((nxpese_ctxt.pwr_scheme == PN67T_POWER_SCHEME) || (nxpese_ctxt.pwr_scheme == PN80T_LEGACY_SCHEME))
    {
        wSpmStatus = phNxpEse_SPM_ConfigPwr(SPM_POWER_RESET);
        if ( wSpmStatus != SPMSTATUS_SUCCESS)
        {
            NXPLOG_ESELIB_E("phNxpEse_SPM_ConfigPwr: reset Failed");
        }
    }
#else
    /* if arg ==2 (hard reset)
     * if arg ==1 (soft reset)
     */
    status = phPalEse_ioctl(phPalEse_e_ResetDevice, nxpese_ctxt.pDevHandle, 2);
    if (status != ESESTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phNxpEse_reset Failed");
    }
#endif
    NXPLOG_ESELIB_D(" %s Exit \n", __FUNCTION__);
    return status;
}

/******************************************************************************
 * Function         phNxpEse_resetJcopUpdate
 *
 * Description      This function reset the ESE interface during JCOP Update
 *
 * Returns          It returns ESESTATUS_SUCCESS (0) if the operation is successful else
 *                  ESESTATUS_FAILED(1)
 ******************************************************************************/
ESESTATUS phNxpEse_resetJcopUpdate(void)
{
    ESESTATUS status = ESESTATUS_SUCCESS;
    bool_t bStatus = FALSE;

#ifdef SPM_INTEGRATED
    SPMSTATUS wSpmStatus = SPMSTATUS_SUCCESS;
    unsigned long int num = 0;
#endif

    /* TBD : Call the ioctl to reset the  */
    NXPLOG_ESELIB_D(" %s Enter \n", __FUNCTION__);

    /* Reset interface after every reset irrespective of
    whether JCOP did a full power cycle or not. */
    bStatus = phNxpEseProto7816_Reset();
    if(!bStatus)
        status = ESESTATUS_FAILED;

#ifdef SPM_INTEGRATED
#if (NXP_POWER_SCHEME_SUPPORT == TRUE)
    if (GetNxpNumValue (NAME_NXP_POWER_SCHEME, (void*)&num, sizeof(num)))
     {
        if((num == 1) || (num == 2))
        {
            NXPLOG_ESELIB_D(" %s Call Config Pwr Reset \n", __FUNCTION__);
            wSpmStatus = phNxpEse_SPM_ConfigPwr(SPM_POWER_RESET);
            if ( wSpmStatus != SPMSTATUS_SUCCESS)
            {
                NXPLOG_ESELIB_E("phNxpEse_resetJcopUpdate: reset Failed");
                status = ESESTATUS_FAILED;
            }
        }
        else if(num == 3)
        {
            NXPLOG_ESELIB_D(" %s Call eSE Chip Reset \n", __FUNCTION__);
            wSpmStatus = phNxpEse_chipReset();
            if ( wSpmStatus != SPMSTATUS_SUCCESS)
            {
                NXPLOG_ESELIB_E("phNxpEse_resetJcopUpdate: chip reset Failed");
                status = ESESTATUS_FAILED;
            }
        }
        else
        {
            NXPLOG_ESELIB_D(" %s Invalid Power scheme \n", __FUNCTION__);
        }
     }
#else
    {
        wSpmStatus = phNxpEse_SPM_ConfigPwr(SPM_POWER_RESET);
        if ( wSpmStatus != SPMSTATUS_SUCCESS)
        {
            NXPLOG_ESELIB_E("phNxpEse_SPM_ConfigPwr: reset Failed");
            status = ESESTATUS_FAILED;
        }
    }
#endif
#else
    /* if arg ==2 (hard reset)
     * if arg ==1 (soft reset)
     */
    status = phPalEse_ioctl(phPalEse_e_ResetDevice, nxpese_ctxt.pDevHandle, 2);
    if (status != ESESTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phNxpEse_resetJcopUpdate Failed");
    }
#endif

    NXPLOG_ESELIB_D(" %s Exit \n", __FUNCTION__);
    return status;
}
/******************************************************************************
 * Function         phNxpEse_EndOfApdu
 *
 * Description      This function is used to send S-frame to indicate END_OF_APDU
 *
 * Returns          It returns ESESTATUS_SUCCESS (0) if the operation is successful else
 *                  ESESTATUS_FAILED(1)
 *
 ******************************************************************************/
ESESTATUS phNxpEse_EndOfApdu(void)
{
    ESESTATUS status = ESESTATUS_SUCCESS;
#if(NXP_ESE_END_OF_SESSION == TRUE)
    bool_t bStatus = phNxpEseProto7816_Close((phNxpEseProto7816SecureTimer_t *)&nxpese_ctxt.secureTimerParams);
    if(!bStatus)
        status = ESESTATUS_FAILED;
#endif
    return status;
}


/******************************************************************************
 * Function         phNxpEse_chipReset
 *
 * Description      This function is used to reset the ESE.
  *
 * Returns          Always return ESESTATUS_SUCCESS (0).
 *
 ******************************************************************************/
ESESTATUS phNxpEse_chipReset(void)
{
    ESESTATUS status = ESESTATUS_SUCCESS;
    bool_t bStatus = FALSE;
    if(nxpese_ctxt.pwr_scheme == PN80T_EXT_PMU_SCHEME)
    {
        bStatus = phNxpEseProto7816_Reset();
        if(!bStatus)
        {
            status = ESESTATUS_FAILED;
            NXPLOG_ESELIB_E("Inside phNxpEse_chipReset, phNxpEseProto7816_Reset Failed");
        }
        status = phPalEse_ioctl(phPalEse_e_ChipRst, nxpese_ctxt.pDevHandle, 6);
        if (status != ESESTATUS_SUCCESS)
        {
            NXPLOG_ESELIB_E("phNxpEse_chipReset  Failed");
        }
    }
    else
    {
        NXPLOG_ESELIB_E("phNxpEse_chipReset is not supported in legacy power scheme");
        status = ESESTATUS_FAILED;
    }
    return status;
}

/******************************************************************************
 * Function         phNxpEse_deInit
 *
 * Description      This function de-initializes all the ESE protocol params
 *
 * Returns          Always return ESESTATUS_SUCCESS (0).
 *
 ******************************************************************************/
ESESTATUS phNxpEse_deInit(void)
{
    ESESTATUS status = ESESTATUS_SUCCESS;
    unsigned long maxTimer = 0;
    bool_t bStatus = phNxpEseProto7816_Close((phNxpEseProto7816SecureTimer_t *)&nxpese_ctxt.secureTimerParams);
    if(!bStatus)
    {
        status = ESESTATUS_FAILED;
    }
    else
    {
        NXPLOG_ESELIB_D("%s secureTimer1 0x%x secureTimer2 0x%x secureTimer3 0x%x", __FUNCTION__,
            nxpese_ctxt.secureTimerParams.secureTimer1,
            nxpese_ctxt.secureTimerParams.secureTimer2,
            nxpese_ctxt.secureTimerParams.secureTimer3);
        phNxpEse_GetMaxTimer(&maxTimer);
#ifdef SPM_INTEGRATED
#if(NXP_SECURE_TIMER_SESSION == TRUE)
        status = phNxpEse_SPM_DisablePwrControl(maxTimer);
        if(status != ESESTATUS_SUCCESS)
        {
            NXPLOG_ESELIB_E("%s phNxpEseP61_DisablePwrCntrl: failed", __FUNCTION__);
        }
#endif
#endif
    }
    return status;
}

/******************************************************************************
 * Function         phNxpEse_close
 *
 * Description      This function close the ESE interface and free all
 *                  resources.
 *
 * Returns          Always return ESESTATUS_SUCCESS (0).
 *
 ******************************************************************************/
ESESTATUS phNxpEse_close(void)
{
    ESESTATUS status = ESESTATUS_SUCCESS;

    if ((ESE_STATUS_CLOSE == nxpese_ctxt.EseLibStatus))
    {
        NXPLOG_ESELIB_E(" %s ESE Not Initialized \n", __FUNCTION__);
        return ESESTATUS_NOT_INITIALISED;
    }

#ifdef SPM_INTEGRATED
    SPMSTATUS wSpmStatus = SPMSTATUS_SUCCESS;
#endif

#ifdef SPM_INTEGRATED
    /* Release the Access of  */
    wSpmStatus = phNxpEse_SPM_ConfigPwr(SPM_POWER_DISABLE);
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phNxpEse_SPM_ConfigPwr: disabling power Failed");
    }
    else
    {
        nxpese_ctxt.spm_power_state = FALSE;
    }
    if(ESE_MODE_OSU == nxpese_ctxt.initParams.initMode)
    {
        status = phNxpEse_setJcopDwnldState(JCP_SPI_DWNLD_COMPLETE);
        if(status != ESESTATUS_SUCCESS)
        {
            NXPLOG_ESELIB_E("%s: phNxpEse_setJcopDwnldState failed", __FUNCTION__);
        }
    }
    wSpmStatus = phNxpEse_SPM_DeInit();
    if ( wSpmStatus != SPMSTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("phNxpEse_SPM_DeInit Failed");
    }

#endif
    if (NULL != nxpese_ctxt.pDevHandle)
    {
        phPalEse_close(nxpese_ctxt.pDevHandle);
        phNxpEse_memset (&nxpese_ctxt, 0x00, sizeof (nxpese_ctxt));
        NXPLOG_ESELIB_D("phNxpEse_close - ESE Context deinit completed");
    }
    /* Return success always */
    return status;
}

/******************************************************************************
 * Function         phNxpEse_read
 *
 * Description      This function write the data to ESE through physical
 *                  interface (e.g. I2C) using the  driver interface.
 *                  Before sending the data to ESE, phNxpEse_write_ext
 *                  is called to check if there is any extension processing
 *                  is required for the SPI packet being sent out.
 *
 * Returns          It returns ESESTATUS_SUCCESS (0) if read successful else
 *                  ESESTATUS_FAILED(1)
 *
 ******************************************************************************/
ESESTATUS phNxpEse_read(uint32_t *data_len, uint8_t **pp_data)
{
    ESESTATUS status = ESESTATUS_SUCCESS;
    int ret = -1;

    NXPLOG_ESELIB_D("%s Enter ..", __FUNCTION__);

    ret = phNxpEse_readPacket(nxpese_ctxt.pDevHandle, nxpese_ctxt.p_read_buff, MAX_DATA_LEN);
    if(ret < 0)
    {
        NXPLOG_ESELIB_E("PAL Read status error status = %x", status);
        status = ESESTATUS_FAILED;
    }
    else
    {
        phPalEse_print_packet("RECV", nxpese_ctxt.p_read_buff,
                            ret);
        *data_len = ret;
        *pp_data = nxpese_ctxt.p_read_buff;
        status = ESESTATUS_SUCCESS;
    }

    NXPLOG_ESELIB_D("%s Exit", __FUNCTION__);
    return status;
}

/******************************************************************************
 * Function         phNxpEse_readPacket
 *
 * Description      This function Reads requested number of bytes from
 *                  pn547 device into given buffer.
 *
 * Returns          nNbBytesToRead- number of successfully read bytes
 *                  -1        - read operation failure
 *
 ******************************************************************************/
static int phNxpEse_readPacket(void *pDevHandle, uint8_t * pBuffer, int nNbBytesToRead)
{
    int ret = -1;
    int sof_counter = 0;/* one read may take 1 ms*/
    int total_count = 0;

    NXPLOG_ESELIB_D("%s Enter", __FUNCTION__);
    do
    {
        sof_counter++;
        ret = -1;
        ret = phPalEse_read(pDevHandle, pBuffer, 1);
        if (ret <= 0)
        {
            /*Polling for read on spi, hence Debug log*/
            NXPLOG_PAL_D("_spi_read() [HDR]errno : %x ret : %X", errno, ret);
        }
        if (pBuffer[0] != RECIEVE_PACKET_SOF)
            phPalEse_sleep(WAKE_UP_DELAY); /*sleep for 1ms*/
    }while((pBuffer[0] != RECIEVE_PACKET_SOF) && (sof_counter < ESE_POLL_TIMEOUT));

    if (pBuffer[0] == RECIEVE_PACKET_SOF)
    {
        NXPLOG_ESELIB_D("SOF: 0x%x", pBuffer[0]);
        total_count = 1;
        /* Read the HEADR of Two bytes*/
        ret = phPalEse_read(pDevHandle, &pBuffer[1], 2);
        if (ret < 0)
        {
            NXPLOG_PAL_E("_spi_read() [HDR]errno : %x ret : %X", errno, ret);
        }
        else
        {
            total_count += 2;
            /* Get the data length */
            nNbBytesToRead = pBuffer[2];
            NXPLOG_ESELIB_D("lenght 0x%x", nNbBytesToRead);

            ret = phPalEse_read(pDevHandle, &pBuffer[3], (nNbBytesToRead+1));
            if (ret < 0)
            {
                NXPLOG_PAL_E("_spi_read() [HDR]errno : %x ret : %X", errno, ret);
            }
            else
            {
               ret = (total_count + (nNbBytesToRead+1));
            }
        }
    }
    else
    {
        ret = -1;
    }
    NXPLOG_ESELIB_D("%s Exit ret = %d", __FUNCTION__, ret);
    return ret;
}
/******************************************************************************
 * Function         phNxpEse_WriteFrame
 *
 * Description      This is the actual function which is being called by
 *                  phNxpEse_write. This function writes the data to ESE.
 *                  It waits till write callback provide the result of write
 *                  process.
 *
 * Returns          It returns ESESTATUS_SUCCESS (0) if write successful else
 *                  ESESTATUS_FAILED(1)
 *
 ******************************************************************************/
ESESTATUS phNxpEse_WriteFrame(uint32_t data_len, const uint8_t *p_data)
{
    ESESTATUS status = ESESTATUS_INVALID_PARAMETER;
    int32_t dwNoBytesWrRd = 0;
    NXPLOG_ESELIB_D("Enter %s ", __FUNCTION__);

    /* Create local copy of cmd_data */
    phNxpEse_memcpy(nxpese_ctxt.p_cmd_data, p_data, data_len);
    nxpese_ctxt.cmd_len = data_len;

    dwNoBytesWrRd = phPalEse_write(nxpese_ctxt.pDevHandle,
                        nxpese_ctxt.p_cmd_data,
                        nxpese_ctxt.cmd_len
                        );
    if (-1 == dwNoBytesWrRd)
    {
        NXPLOG_PAL_E(" - Error in SPI Write.....\n");
        status = ESESTATUS_FAILED;
    }
    else
    {
        status = ESESTATUS_SUCCESS;
        phPalEse_print_packet("SEND", nxpese_ctxt.p_cmd_data,
                            nxpese_ctxt.cmd_len);
    }

    NXPLOG_ESELIB_D("Exit %s status %x\n", __FUNCTION__, status);
    return status;
}

/******************************************************************************
 * Function         phNxpEse_setIfsc
 *
 * Description      This function sets the IFSC size to 240/254 support JCOP OS Update.
 *
 * Returns          Always return ESESTATUS_SUCCESS (0).
 *
 ******************************************************************************/
ESESTATUS phNxpEse_setIfsc(uint16_t IFSC_Size)
{
    /*SET the IFSC size to 240 bytes*/
    phNxpEseProto7816_SetIfscSize(IFSC_Size);
    return ESESTATUS_SUCCESS;
}


/******************************************************************************
 * Function         phNxpEse_Sleep
 *
 * Description      This function  suspends execution of the calling thread for
 *           (at least) usec microseconds
 *
 * Returns          Always return ESESTATUS_SUCCESS (0).
 *
 ******************************************************************************/
ESESTATUS phNxpEse_Sleep(uint32_t usec)
{
    phPalEse_sleep(usec);
    return ESESTATUS_SUCCESS;
}

/******************************************************************************
 * Function         phNxpEse_memset
 *
 * Description      This function updates destination buffer with val
 *                  data in len size
 *
 * Returns          Always return ESESTATUS_SUCCESS (0).
 *
 ******************************************************************************/
void* phNxpEse_memset(void *buff, int val, size_t len)
{
    return phPalEse_memset(buff, val, len);
}

/******************************************************************************
 * Function         phNxpEse_memcpy
 *
 * Description      This function copies source buffer to  destination buffer
 *                  data in len size
 *
 * Returns          Return pointer to allocated memory location.
 *
 ******************************************************************************/
void* phNxpEse_memcpy(void *dest, const void *src, size_t len)
{
    return phPalEse_memcpy(dest, src, len);
}

/******************************************************************************
 * Function         phNxpEse_Memalloc
 *
 * Description      This function allocation memory
 *
 * Returns          Return pointer to allocated memory or NULL.
 *
 ******************************************************************************/
void *phNxpEse_memalloc(uint32_t size)
{
    return phPalEse_memalloc(size);;
}

/******************************************************************************
 * Function         phNxpEse_calloc
 *
 * Description      This is utility function for runtime heap memory allocation
 *
 * Returns          Return pointer to allocated memory or NULL.
 *
 ******************************************************************************/
void* phNxpEse_calloc(size_t datatype, size_t size)
{
    return phPalEse_calloc(datatype, size);
}

/******************************************************************************
 * Function         phNxpEse_free
 *
 * Description      This function de-allocation memory
 *
 * Returns         void.
 *
 ******************************************************************************/
void phNxpEse_free(void* ptr)
{
    return phPalEse_free(ptr);
}

/******************************************************************************
 * Function         phNxpEse_GetMaxTimer
 *
 * Description      This function finds out the max. timer value returned from JCOP
 *
 * Returns          void.
 *
 ******************************************************************************/
static void phNxpEse_GetMaxTimer(unsigned long *pMaxTimer)
{
    /* Finding the max. of the timer value */
    *pMaxTimer = nxpese_ctxt.secureTimerParams.secureTimer1;
    if(*pMaxTimer < nxpese_ctxt.secureTimerParams.secureTimer2)
        *pMaxTimer = nxpese_ctxt.secureTimerParams.secureTimer2;
    *pMaxTimer = (*pMaxTimer < nxpese_ctxt.secureTimerParams.secureTimer3)?(nxpese_ctxt.secureTimerParams.secureTimer3):*pMaxTimer;

    /* Converting timer to millisecond from sec */
    *pMaxTimer = SECOND_TO_MILLISECOND(*pMaxTimer);
    /* Add extra 5% to the timer */
    *pMaxTimer += CONVERT_TO_PERCENTAGE(*pMaxTimer, ADDITIONAL_SECURE_TIME_PERCENTAGE);
    NXPLOG_ESELIB_E("%s Max timer value = %lu", __FUNCTION__, *pMaxTimer);
    return;
}

/******************************************************************************
 * Function         phNxpEseP61_DisablePwrCntrl
 *
 * Description      This function disables eSE GPIO power off/on control
 *                  when enabled
 *
 * Returns         SUCCESS/FAIL.
 *
 ******************************************************************************/
ESESTATUS phNxpEse_DisablePwrCntrl(void)
{
    ESESTATUS status = ESESTATUS_SUCCESS;
    unsigned long maxTimer = 0;
    NXPLOG_ESELIB_E("%s Enter", __FUNCTION__);
    phNxpEse_GetMaxTimer(&maxTimer);
#if(NXP_SECURE_TIMER_SESSION == TRUE)
    status = phNxpEse_SPM_DisablePwrControl(maxTimer);
    if(status != ESESTATUS_SUCCESS)
    {
        NXPLOG_ESELIB_E("%s phNxpEseP61_DisablePwrCntrl: failed", __FUNCTION__);
    }
#else
    NXPLOG_ESELIB_E("%s phNxpEseP61_DisablePwrCntrl: not supported", __FUNCTION__);
    status = ESESTATUS_FAILED;
#endif
    return status;
}

#if(NXP_NFCC_SPI_FW_DOWNLOAD_SYNC == TRUE)
/******************************************************************************
 * Function         phNxpEse_checkFWDwnldStatus
 *
 * Description      This function is  used to  check whether FW download
 *                  is completed or not.
 *
 * Returns          returns  ESESTATUS_SUCCESS or ESESTATUS_BUSY
 *
 ******************************************************************************/
static ESESTATUS phNxpEse_checkFWDwnldStatus(void)
{
    NXPLOG_ESELIB_E("phNxpEse_checkFWDwnldStatus Enter");
    SPMSTATUS wSpmStatus = SPMSTATUS_SUCCESS;
    spm_state_t current_spm_state = SPM_STATE_INVALID;
    uint8_t ese_dwnld_retry = 0x00;
    ESESTATUS status = ESESTATUS_FAILED;

    wSpmStatus = phNxpEse_SPM_GetState(&current_spm_state);
    if (wSpmStatus == SPMSTATUS_SUCCESS)
    {
       /* Check current_spm_state and update config/Spm status*/
           while(ese_dwnld_retry < ESE_FW_DWNLD_RETRY_CNT)
           {
               NXPLOG_ESELIB_E("ESE_FW_DWNLD_RETRY_CNT retry count");
               wSpmStatus = phNxpEse_SPM_GetState(&current_spm_state);
               if(wSpmStatus == SPMSTATUS_SUCCESS)
               {
                   if((current_spm_state & SPM_STATE_DWNLD))
                   {
                       status = ESESTATUS_FAILED;
                   }
                   else
                   {
                       NXPLOG_ESELIB_E("Exit polling no FW Download ..");
                       status = ESESTATUS_SUCCESS;
                       break;
                   }
                }
                else
                {
                    status = ESESTATUS_FAILED;
                    break;
                }
                phNxpEse_Sleep(500000);    /*sleep for 500 ms checking for fw dwnld status*/
                ese_dwnld_retry++;
            }
    }

    NXPLOG_ESELIB_E("phNxpEse_checkFWDwnldStatus status %x", status);
    return status;
}
#endif
/******************************************************************************
 * Function         phNxpEse_GetEseStatus(unsigned char *timer_buffer)
 *
 * Description      This function returns the all three timer
 * Timeout buffer length should be minimum 18 bytes. Response will be in below format:
 * <0xF1><Len><Timer Value><0xF2><Len><Timer Value><0xF3><Len><Timer Value>
 *
 * Returns         SUCCESS/FAIL.
 * ESESTATUS_SUCCESS if 0xF1 or 0xF2 tag timeout >= 0 & 0xF3 == 0
 * ESESTATUS_BUSY if 0xF3 tag timeout > 0
 * ESESTATUS_FAILED if any other error

 ******************************************************************************/
ESESTATUS phNxpEse_GetEseStatus(phNxpEse_data *timer_buffer)
{
    ESESTATUS status = ESESTATUS_FAILED;

    phNxpEse_SecureTimer_t secureTimerParams;
    uint8_t *temp_timer_buffer = NULL, len = 0;
    NXPLOG_ESELIB_D("%s Enter", __FUNCTION__);

    if(timer_buffer != NULL)
    {
        timer_buffer->len = (sizeof(secureTimerParams.secureTimer1) +
                             sizeof(secureTimerParams.secureTimer2) +
                             sizeof(secureTimerParams.secureTimer3)) + PH_PROPTO_7816_FRAME_LENGTH_OFFSET * PH_PROPTO_7816_FRAME_LENGTH_OFFSET;
        temp_timer_buffer = (uint8_t *)phNxpEse_memalloc(timer_buffer->len);
        timer_buffer->p_data = temp_timer_buffer;

#if(NXP_SECURE_TIMER_SESSION == TRUE)
        phNxpEse_memcpy(&secureTimerParams, &nxpese_ctxt.secureTimerParams, sizeof(phNxpEse_SecureTimer_t));

        NXPLOG_ESELIB_D("%s secureTimer1 0x%x secureTimer2 0x%x secureTimer3 0x%x len = %d", __FUNCTION__,
            secureTimerParams.secureTimer1,
            secureTimerParams.secureTimer2,
            secureTimerParams.secureTimer3, timer_buffer->len);

        *temp_timer_buffer++ = PH_PROPTO_7816_SFRAME_TIMER1;
        *temp_timer_buffer++ = sizeof(secureTimerParams.secureTimer1);
        temp_timer_buffer = phNxpEse_GgetTimerTlvBuffer(temp_timer_buffer, secureTimerParams.secureTimer1);
        if (temp_timer_buffer != NULL)
        {
            *temp_timer_buffer++ = PH_PROPTO_7816_SFRAME_TIMER2;
            *temp_timer_buffer++ = sizeof(secureTimerParams.secureTimer2);
            temp_timer_buffer = phNxpEse_GgetTimerTlvBuffer(temp_timer_buffer, secureTimerParams.secureTimer2);
            if (temp_timer_buffer != NULL)
            {
                *temp_timer_buffer++ = PH_PROPTO_7816_SFRAME_TIMER3;
                *temp_timer_buffer++ = sizeof(secureTimerParams.secureTimer3);
                temp_timer_buffer = phNxpEse_GgetTimerTlvBuffer(temp_timer_buffer, secureTimerParams.secureTimer3);
                if(temp_timer_buffer!= NULL)
                {
                    if(secureTimerParams.secureTimer3 > 0)
                    {
                        status = ESESTATUS_BUSY;
                    }
                    else
                    {
                        status = ESESTATUS_SUCCESS;
                    }
                }
            }
        }
    }
    else
    {
        NXPLOG_ESELIB_E("%s Invalid timer buffer ", __FUNCTION__);
    }
#endif
    NXPLOG_ESELIB_D("%s Exit status = 0x%x", __FUNCTION__, status);
    return status;
}

static unsigned char * phNxpEse_GgetTimerTlvBuffer(uint8_t *timer_buffer, unsigned int value)
{
    short int count =0, shift = 3;
    unsigned int mask = 0x000000FF;
    NXPLOG_ESELIB_D("value = %x \n", value);
    for(count = 0; count < 4; count++)
    {
         if(timer_buffer != NULL)
         {
              *timer_buffer = ( value >> (shift*8) & mask);
              NXPLOG_ESELIB_D("*timer_buffer=0x%x shift=0x%x", *timer_buffer, shift);
              timer_buffer++;
              shift--;
         }
         else
         {
             break;
         }
    }
    return timer_buffer;
}
