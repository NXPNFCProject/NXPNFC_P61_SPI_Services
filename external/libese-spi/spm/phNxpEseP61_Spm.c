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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/p61.h>
#include "phNxpEseP61_Spm.h"
#include <phNxpEseHal.h>
#include <phNxpEseProtocol.h>


#define debug
#ifdef debug
#include <cutils/log.h>
#define LOG_TAG "P61_SPM"
#else
#define ALOGE
#define ALOGD
#endif

/*********************** Global Variables *************************************/
#define DEVICE_NODE "/dev/p61"
static int fd_p61_device = -1;

extern phNxpEseP61_Control_t nxpesehal_ctrl;

/*********************** Function Declaration**********************************/
static spm_state_t phNxpEseP61_StateMaping(p61_access_state_t p61_current_state);
/**
 * \addtogroup SPI_Power_Management
 *
 * @{ */
/******************************************************************************
\section Introduction Introduction

 * This module provide power request to Pn54x nfc-i2c driver, it cheks if
 * wired access is already granted. It should have access to pn54x drive.
 * Below are the apis provided by the SPM module.
 ******************************************************************************/
/******************************************************************************
 * Function         phNxpEseP61_SPM_Init
 *
 * Description      This function opens the nfc i2c driver to manage power
 *                  and synchronization for p61 secure element.
 *
 * Returns          On Success SPMSTATUS_SUCCESS else proper error code
 *
 ******************************************************************************/
SPMSTATUS phNxpEseP61_SPM_Init(void)
{
    SPMSTATUS status = SPMSTATUS_SUCCESS;

    fd_p61_device = open(DEVICE_NODE, O_RDWR);
    if (fd_p61_device < 0)
    {
        ALOGE("%s : failed errno = 0x%x", __FUNCTION__, errno);
        status = SPMSTATUS_FAILED;
    }
    ALOGD("%s : exit status = %d", __FUNCTION__, status);

    return status;
}

/******************************************************************************
 * Function         phNxpEseP61_SPM_DeInit
 *
 * Description      This function closes the nfc i2c driver node.
 *
 * Returns          On Success SPMSTATUS_SUCCESS else proper error code
 *
 ******************************************************************************/
SPMSTATUS phNxpEseP61_SPM_DeInit(void)
{
    SPMSTATUS status = SPMSTATUS_SUCCESS;

    if (close(fd_p61_device) < 0)
    {
        ALOGE("%s : failed errno = 0x%x", __FUNCTION__, errno);
        status = SPMSTATUS_FAILED;
    }
    else
    {
        status = SPMSTATUS_SUCCESS;
        fd_p61_device = -1;
    }

    return status;
}

/******************************************************************************
 * Function         phNxpEseP61_SPM_ConfigPwr
 *
 * Description      This function request to the nfc i2c driver
 *                  to enable/disable power to p61. This api should be called before
 *                  sending any apdu to p61/once apdu exchange is done.
 *
 * Returns          On Success SPMSTATUS_SUCCESS else proper error code
 *
 ******************************************************************************/
SPMSTATUS phNxpEseP61_SPM_ConfigPwr(spm_power_t arg)
{
    int32_t ret = -1;
    SPMSTATUS wSpmStatus = SPMSTATUS_SUCCESS;
    spm_state_t current_spm_state = SPM_STATE_INVALID;

    if((nxpesehal_ctrl.cmd_rsp_state == STATE_TRANS_ONGOING) &&
      ((arg == SPM_POWER_DISABLE)||(arg == SPM_POWER_RESET)))
    {
        ALOGD("Reset performed while pending APDU response state = STATE_RESET_BLOCKED");

        nxpesehal_ctrl.cmd_rsp_state = STATE_RESET_BLOCKED;
        phNxpEseP61_SemInit(&nxpesehal_ctrl.cmd_rsp_ack);
        phNxpEseP61_SemWait(&nxpesehal_ctrl.cmd_rsp_ack);
        ALOGD("%s : After APDU response received perform ioctl", __FUNCTION__);
    }

    ret = ioctl(fd_p61_device, P61_SET_SPM_PWR, arg);

    if(nxpesehal_ctrl.cmd_rsp_state == STATE_RESET_BLOCKED)
        phNxpEseP61_SemUnlock(&nxpesehal_ctrl.reset_ack, ESESTATUS_SUCCESS);

    switch(arg){
    case SPM_POWER_DISABLE:
    {
        if(ret < 0)
        {
            ALOGE("%s : failed errno = 0x%x", __FUNCTION__, errno);
            wSpmStatus = SPMSTATUS_FAILED;
        }
    }
    break;
    case SPM_POWER_ENABLE:
    {
        if(ret < 0)
        {
        ALOGE("%s : failed errno = 0x%x", __FUNCTION__, errno);
            if(errno == -EBUSY)
            {
                wSpmStatus = phNxpEseP61_SPM_GetState(&current_spm_state);
                if ( wSpmStatus != SPMSTATUS_SUCCESS)
                {
                    ALOGE(" %s : phNxpEseP61_SPM_GetPwrState Failed", __FUNCTION__);
                    return wSpmStatus;
                }
                else
                {
                    if (current_spm_state & SPM_STATE_DWNLD)
                    {
                        wSpmStatus = SPMSTATUS_DEVICE_DWNLD_BUSY;
                    }
                    else
                    {
                        wSpmStatus = SPMSTATUS_DEVICE_BUSY;
                    }
                }

            }
            else
            {
                wSpmStatus = SPMSTATUS_FAILED;
            }
        }
    }
    break;
    case SPM_POWER_RESET:
    {
        if(ret < 0)
        {
            ALOGE("%s : failed errno = 0x%x", __FUNCTION__, errno);
            if(errno == -EBUSY)
            {
                wSpmStatus = phNxpEseP61_SPM_GetState(&current_spm_state);
                if ( wSpmStatus != SPMSTATUS_SUCCESS)
                {
                    ALOGE(" %s : phNxpEseP61_SPM_GetPwrState Failed", __FUNCTION__);
                    return wSpmStatus;
                }
                else
                {
                    if (current_spm_state & SPM_STATE_DWNLD)
                    {
                        wSpmStatus = SPMSTATUS_DEVICE_DWNLD_BUSY;
                    }
                    else
                    {
                        wSpmStatus = SPMSTATUS_DEVICE_BUSY;
                    }
                }

            }
            else
            {
                wSpmStatus = SPMSTATUS_FAILED;
            }
        }
    }
    break;
    case SPM_POWER_PRIO_ENABLE:
    {
        if(ret < 0)
        {
            ALOGE("%s : failed errno = 0x%x", __FUNCTION__, errno);
            if(errno == -EBUSY)
            {
                wSpmStatus = phNxpEseP61_SPM_GetState(&current_spm_state);
                if ( wSpmStatus != SPMSTATUS_SUCCESS)
                {
                    ALOGE(" %s : phNxpEseP61_SPM_GetPwrState Failed", __FUNCTION__);
                    return wSpmStatus;
                }
                else
                {
                    if (current_spm_state & SPM_STATE_DWNLD)
                    {
                        wSpmStatus = SPMSTATUS_DEVICE_DWNLD_BUSY;
                    }
                    else
                    {
                        wSpmStatus = SPMSTATUS_DEVICE_BUSY;
                    }
                }

            }
            else
            {
                wSpmStatus = SPMSTATUS_FAILED;
            }
        }
    }
    break;
    case SPM_POWER_PRIO_DISABLE:
    {
        if(ret < 0)
        {
            ALOGE("%s : failed errno = 0x%x", __FUNCTION__, errno);
            wSpmStatus = SPMSTATUS_FAILED;
        }
    }
    break;
    }
    return wSpmStatus;
}

/******************************************************************************
 * Function         phNxpEseP61_SPM_EnablePwr
 *
 * Description      This function request to the nfc i2c driver
 *                  to enable power to p61. This api should be called before
 *                  sending any apdu to p61.
 *
 * Returns          On Success SPMSTATUS_SUCCESS else proper error code
 *
 ******************************************************************************/
SPMSTATUS phNxpEseP61_SPM_EnablePwr(void)
{
    int32_t ret = -1;
    SPMSTATUS wSpmStatus = SPMSTATUS_SUCCESS;
    spm_state_t current_spm_state = SPM_STATE_INVALID;

    ret = ioctl(fd_p61_device, P61_SET_SPM_PWR, 1);
    if(ret < 0)
    {
        ALOGE("%s : failed errno = 0x%x", __FUNCTION__, errno);
        if(errno == -EBUSY)
        {
            wSpmStatus = phNxpEseP61_SPM_GetState(&current_spm_state);
            if ( wSpmStatus != SPMSTATUS_SUCCESS)
            {
                ALOGE(" %s : phNxpEseP61_SPM_GetPwrState Failed", __FUNCTION__);
                return wSpmStatus;
            }
            else
            {
                if (current_spm_state == SPM_STATE_DWNLD)
                {
                    wSpmStatus = SPMSTATUS_DEVICE_DWNLD_BUSY;
                }
                else
                {
                    wSpmStatus = SPMSTATUS_DEVICE_BUSY;
                }
            }

        }
        else
        {
            wSpmStatus = SPMSTATUS_FAILED;
        }
    }

    return wSpmStatus;
}

/******************************************************************************
 * Function         phNxpEseP61_SPM_DisablePwr
 *
 * Description      This function request to the nfc i2c driver
 *                  to disable power to p61. This api should be called
 *                  once apdu exchange is done.
 *
 * Returns          On Success SPMSTATUS_SUCCESS else proper error code
 *
 ******************************************************************************/
SPMSTATUS phNxpEseP61_SPM_DisablePwr(void)
{
    int32_t ret = -1;
    SPMSTATUS status = SPMSTATUS_SUCCESS;

    ret = ioctl(fd_p61_device, P61_SET_SPM_PWR, 0);
    if(ret < 0)
    {
        ALOGE("%s : failed errno = 0x%x", __FUNCTION__, errno);
        status = SPMSTATUS_FAILED;
    }

    return status;
}

/******************************************************************************
 * Function         phNxpEseP61_SPM_GetState
 *
 * Description      This function gets the current power state of P61
 *
 * Returns          On Success SPMSTATUS_SUCCESS else proper error code
 *
 ******************************************************************************/
SPMSTATUS phNxpEseP61_SPM_GetState(spm_state_t *current_state)
{
    int32_t ret = -1;
    SPMSTATUS status = SPMSTATUS_SUCCESS;
    p61_access_state_t p61_current_state;

    if (current_state == NULL)
    {
        ALOGE("%s : failed Invalid argument", __FUNCTION__);
        return SPMSTATUS_FAILED;
    }
    ret = ioctl(fd_p61_device, P61_GET_SPM_STATUS, (int32_t )&p61_current_state);
    if(ret < 0)
    {
        ALOGE("%s : failed errno = 0x%x", __FUNCTION__, errno);
        status = SPMSTATUS_FAILED;
    }
    else
    {
        *current_state = p61_current_state; /*phNxpEseP61_StateMaping(p61_current_state);*/
    }

    return status;
}

/******************************************************************************
 * Function         phNxpEseP61_SPM_ResetPwr
 *
 * Description      This function request to the nfc i2c driver
 *                  to reset p61.
 *
 * Returns          On Success SPMSTATUS_SUCCESS else proper error code
 *
 ******************************************************************************/
SPMSTATUS phNxpEseP61_SPM_ResetPwr(void)
{
    int32_t ret = -1;
    SPMSTATUS wSpmStatus = SPMSTATUS_SUCCESS;
    spm_state_t current_spm_state = SPM_STATE_INVALID;

    /* reset the p61 */
    ret = ioctl(fd_p61_device, P61_SET_SPM_PWR, 2);
    if(ret < 0)
    {
        ALOGE("%s : failed errno = 0x%x", __FUNCTION__, errno);
        if(errno == -EBUSY)
        {
            wSpmStatus = phNxpEseP61_SPM_GetState(&current_spm_state);
            if ( wSpmStatus != SPMSTATUS_SUCCESS)
            {
                ALOGE(" %s : phNxpEseP61_SPM_GetPwrState Failed", __FUNCTION__);
                return wSpmStatus;
            }
            else
            {
                if (current_spm_state == SPM_STATE_DWNLD)
                {
                    wSpmStatus = SPMSTATUS_DEVICE_DWNLD_BUSY;
                }
                else
                {
                    wSpmStatus = SPMSTATUS_DEVICE_BUSY;
                }
            }

        }
        else
        {
            wSpmStatus = SPMSTATUS_FAILED;
        }
    }

    return wSpmStatus;
}
/** @} */

static spm_state_t phNxpEseP61_StateMaping(p61_access_state_t p61_current_state)
{
    spm_state_t current_spm_state = SPM_STATE_INVALID;

    switch(p61_current_state)
    {
    case P61_STATE_IDLE:
        current_spm_state = SPM_STATE_IDLE;
        ALOGD("%s : SPM_STATE_IDLE", __FUNCTION__);
        break;
    case P61_STATE_WIRED:
        current_spm_state = SPM_STATE_WIRED;
        ALOGD("%s : SPM_STATE_WIRED", __FUNCTION__);
        break;
    case P61_STATE_SPI:
        current_spm_state = SPM_STATE_SPI;
        ALOGD("%s : SPM_STATE_SPI", __FUNCTION__);
        break;
    case P61_STATE_DWNLD:
        current_spm_state = SPM_STATE_DWNLD;
        ALOGD("%s : P61_STATE_DWNLD", __FUNCTION__);
        break;
    case P61_STATE_SPI_PRIO:
        current_spm_state = SPM_STATE_SPI_PRIO;
        ALOGD("%s : P61_STATE_SPI_PRIO", __FUNCTION__);
        break;
    case P61_STATE_SPI_PRIO_END:
        current_spm_state = SPM_STATE_SPI_PRIO_END;
        ALOGD("%s : P61_STATE_SPI_PRIO_END", __FUNCTION__);
        break;
    case P61_STATE_INVALID:
    default:
        current_spm_state = SPM_STATE_INVALID;
        ALOGD("%s : SPM_STATE_INVALID", __FUNCTION__);
        break;
    }

    return current_spm_state;
}
