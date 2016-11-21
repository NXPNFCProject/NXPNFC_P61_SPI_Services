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
#include <phTmlEse_spi.h>
#include <phEseStatus.h>
#include <string.h>
#include <phNxpSpiHal_utils.h>

#define NORMAL_MODE_HEADER_LEN      3
#define NORMAL_MODE_LEN_OFFSET      2

/*******************************************************************************
**
** Function         phTmlEse_spi_close
**
** Description      Closes PN547 device
**
** Parameters       pDevHandle - device handle
**
** Returns          None
**
*******************************************************************************/
void phTmlEse_spi_close(void *pDevHandle)
{
    if (NULL != pDevHandle)
    {
        close((intptr_t)pDevHandle);
    }

    return;
}

/*******************************************************************************
**
** Function         phTmlEse_spi_open_and_configure
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
ESESTATUS phTmlEse_spi_open_and_configure(pphTmlEse_Config_t pConfig, void ** pLinkHandle)
{
    int nHandle;

    NXPLOG_TML_D("Opening port=%s\n", pConfig->pDevName);
    /* open port */
    nHandle = open((char const *)pConfig->pDevName, O_RDWR);
    if (nHandle < 0)
    {
        NXPLOG_TML_E("_spi_open() Failed: retval %x",nHandle);
        *pLinkHandle = NULL;
        return ESESTATUS_INVALID_DEVICE;
    }

    *pLinkHandle = (void*) ((intptr_t) nHandle);

    return ESESTATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         phTmlEse_spi_read
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
int phTmlEse_spi_read(void *pDevHandle, uint8_t * pBuffer, int nNbBytesToRead)
{

    int ret = -1;

    NXPLOG_TML_D("%s Read Requested %d bytes", __FUNCTION__, nNbBytesToRead);
    ret = read((intptr_t)pDevHandle, pBuffer, nNbBytesToRead);
    if (ret <= 0)
    {
        NXPLOG_TML_E("_spi_read() [HDR]errno : %x ret : %X", errno, ret);
        if (!ret)
            ret = -1;
    }
    NXPLOG_TML_E(" read returned= %d", ret);
    return ret;
}

/*******************************************************************************
**
** Function         phTmlEse_spi_write
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
int phTmlEse_spi_write(void *pDevHandle, uint8_t * pBuffer, int nNbBytesToWrite)
{
    int ret;
    int numWrote = 0;

    if (NULL == pDevHandle)
    {
        return -1;
    }

    while (numWrote < nNbBytesToWrite)
    {
        usleep(5000);
        ret = write((intptr_t)pDevHandle, pBuffer + numWrote, nNbBytesToWrite - numWrote);
        if (ret > 0)
        {
            numWrote += ret;
        }
        else if (ret == 0)
        {
            NXPLOG_TML_E("_spi_write() EOF");
            return -1;
        }
        else
        {
            NXPLOG_TML_E("_spi_write() errno : %x",errno);
            if (errno == EINTR || errno == EAGAIN)
            {
                continue;
            }
            return -1;
        }
    }

    return numWrote;
}

/*******************************************************************************
**
** Function         phTmlEse_spi_ioctl
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
int phTmlEse_spi_ioctl(phTmlEse_ControlCode_t eControlCode, void *pDevHandle, long level)
{
    int ret;
    NXPLOG_TML_D("phTmlEse_spi_ioctl(), ioctl %d , level %ld", eControlCode, level);

    if (NULL == pDevHandle)
    {
        return -1;
    }
    switch(eControlCode)
    {
    case phTmlEse_e_ResetDevice:
        ret = ioctl((intptr_t)pDevHandle, P61_SET_PWR, level);
        break;

    case phTmlEse_e_EnableLog:
        ret = ioctl((intptr_t)pDevHandle, P61_SET_DBG, level);
        break;

    case phTmlEse_e_EnablePollMode:
        ret = ioctl((intptr_t)pDevHandle, P61_SET_POLL, level);
        break;
    default:
        break;

    }

    return ret;
}
