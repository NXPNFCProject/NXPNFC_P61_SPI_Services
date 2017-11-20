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

/*
 * DAL spi port implementation for linux
 *
 * Project: Trusted ESE Linux
 *
 */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <phNxpLog.h>
#include <phNxpEsePal_spi.h>
#include <phNxpEsePal.h>
#include <phEseStatus.h>
#include <string.h>
#include <phNxpConfig.h>

#define MAX_RETRY_CNT   10

/*******************************************************************************
**
** Function         phPalEse_spi_close
**
** Description      Closes PN547 device
**
** Parameters       pDevHandle - device handle
**
** Returns          None
**
*******************************************************************************/
void phPalEse_spi_close(void *pDevHandle)
{
    if (NULL != pDevHandle)
    {
        close((intptr_t)pDevHandle);
    }

    return;
}

/*******************************************************************************
**
** Function         phPalEse_spi_open_and_configure
**
** Description      Open and configure pn547 device
**
** Parameters       pConfig     - hardware information
**                  pLinkHandle - device handle
**
** Returns          ESE status:
**                  ESESTATUS_SUCCESS            - open_and_configure operation success
**                  ESESTATUS_INVALID_DEVICE     - device open operation failure
**
*******************************************************************************/
ESESTATUS phPalEse_spi_open_and_configure(pphPalEse_Config_t pConfig)
{
    int nHandle;
    int retryCnt = 0;

    NXPLOG_PAL_D("Opening port=%s\n", pConfig->pDevName);
    /* open port */

retry:
    nHandle = open((char const *)pConfig->pDevName, O_RDWR);
    if (nHandle < 0){
        NXPLOG_PAL_E("%s : failed errno = 0x%x", __FUNCTION__, errno);
        if (errno == -EBUSY || errno == EBUSY) {
            retryCnt++;
            NXPLOG_PAL_E("Retry open eSE driver, retry cnt : %d", retryCnt);
            if (retryCnt < MAX_RETRY_CNT) {
                phPalEse_sleep(1000000);
                goto retry;
            }
        }
        NXPLOG_PAL_E("_spi_open() Failed: retval %x",nHandle);
        pConfig->pDevHandle = NULL;
        return ESESTATUS_INVALID_DEVICE;
    }
    NXPLOG_PAL_D("eSE driver opened :: fd = [%d]", nHandle);
    pConfig->pDevHandle = (void*) ((intptr_t) nHandle);
    return ESESTATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         phPalEse_spi_read
**
** Description      Reads requested number of bytes from pn547 device into given buffer
**
** Parameters       pDevHandle       - valid device handle
**                  pBuffer          - buffer for read data
**                  nNbBytesToRead   - number of bytes requested to be read
**
** Returns          numRead   - number of successfully read bytes
**                  -1        - read operation failure
**
*******************************************************************************/
int phPalEse_spi_read(void *pDevHandle, uint8_t * pBuffer, int nNbBytesToRead)
{
    int ret = -1;
    NXPLOG_PAL_D("%s Read Requested %d bytes", __FUNCTION__, nNbBytesToRead);
    ret = read((intptr_t)pDevHandle, (void *)pBuffer, (nNbBytesToRead));
    NXPLOG_PAL_D("Read Returned = %d", ret);
    return ret;
}

/*******************************************************************************
**
** Function         phPalEse_spi_write
**
** Description      Writes requested number of bytes from given buffer into pn547 device
**
** Parameters       pDevHandle       - valid device handle
**                  pBuffer          - buffer for read data
**                  nNbBytesToWrite  - number of bytes requested to be written
**
** Returns          numWrote   - number of successfully written bytes
**                  -1         - write operation failure
**
*******************************************************************************/
int phPalEse_spi_write(void *pDevHandle, uint8_t * pBuffer, int nNbBytesToWrite)
{
    int ret = -1, retryCount = 0;
    int numWrote = 0;
    unsigned long int configNum1;
    if (NULL == pDevHandle)
    {
        return -1;
    }
#ifdef ESE_DEBUG_UTILS_INCLUDED
    if (GetNxpNumValue (NAME_NXP_SOF_WRITE, &configNum1, sizeof(configNum1)))
    {
        if(configNum1 == 1)
        {
            /* Appending SOF for SPI write */
            pBuffer[0] = SEND_PACKET_SOF;
        }
        else
        {
            /* Do Nothing */
        }
    }
#else
    pBuffer[0] = SEND_PACKET_SOF;
#endif
    while (numWrote < nNbBytesToWrite)
    {
        //usleep(5000);
        ret = write((intptr_t)pDevHandle, pBuffer + numWrote, nNbBytesToWrite - numWrote);
        if (ret > 0)
        {
            numWrote += ret;
        }
        else if (ret == 0)
        {
            NXPLOG_PAL_E("_spi_write() EOF");
            return -1;
        }
        else
        {
            NXPLOG_PAL_E("_spi_write() errno : %x",errno);
            if ((errno == EINTR || errno == EAGAIN) || (retryCount < MAX_RETRY_COUNT))
            {
                retryCount++;
                /* 5ms delay to give ESE wake up delay */
                phPalEse_sleep(WAKE_UP_DELAY);
                NXPLOG_PAL_E("_spi_write() failed. Going to retry, counter:%d !", retryCount);
                continue;
            }
            return -1;
        }
    }
    return numWrote;
}

/*******************************************************************************
**
** Function         phPalEse_spi_ioctl
**
** Description      Exposed ioctl by p61 spi driver
**
** Parameters       pDevHandle     - valid device handle
**                  level          - reset level
**
** Returns           0   - ioctl operation success
**                  -1   - ioctl operation failure
**
*******************************************************************************/
int phPalEse_spi_ioctl(phPalEse_ControlCode_t eControlCode, void *pDevHandle, long level)
{
    int ret;
    NXPLOG_PAL_D("phPalEse_spi_ioctl(), ioctl %x , level %lx", eControlCode, level);

    if (NULL == pDevHandle)
    {
        return -1;
    }
    switch(eControlCode)
    {
    case phPalEse_e_ResetDevice:
        ret = ioctl((intptr_t)pDevHandle, P61_SET_PWR, level);
        break;

    case phPalEse_e_EnableLog:
        ret = ioctl((intptr_t)pDevHandle, P61_SET_DBG, level);
        break;

    case phPalEse_e_EnablePollMode:
        ret = ioctl((intptr_t)pDevHandle, P61_SET_POLL, level);
        break;

    case phPalEse_e_ChipRst:
        ret = ioctl((intptr_t)pDevHandle, P61_SET_SPM_PWR, level);
        break;

    case phPalEse_e_EnableThroughputMeasurement:
        ret = ioctl((intptr_t)pDevHandle, P61_SET_THROUGHPUT, level);
        break;

    case phPalEse_e_SetPowerScheme:
        ret = ioctl((intptr_t)pDevHandle, P61_SET_POWER_SCHEME, level);
        break;

    case phPalEse_e_GetSPMStatus:
        ret = ioctl((intptr_t)pDevHandle, P61_GET_SPM_STATUS, level);
        break;

    case phPalEse_e_GetEseAccess:
        ret = ioctl((intptr_t)pDevHandle, P61_GET_ESE_ACCESS, level);
        break;
#if(NXP_ESE_JCOP_DWNLD_PROTECTION == TRUE)
    case phPalEse_e_SetJcopDwnldState:
        ret = ioctl((intptr_t)pDevHandle, P61_SET_DWNLD_STATUS, level);
        break;
#endif
    case phPalEse_e_DisablePwrCntrl:
        ret = ioctl((intptr_t)pDevHandle, P61_INHIBIT_PWR_CNTRL, level);
        break;
    default:
        break;
    }
    return ret;
}
