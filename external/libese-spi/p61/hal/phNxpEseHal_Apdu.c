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

#include <phNxpEseHal.h>
#include <phNxpLog.h>
#include <phNxpEseProtocol.h>
#include <phNxpEseHal_Apdu.h>
void ese_stack_data_callback(ESESTATUS status, phNxpEseP61_data *eventData);
STATIC ESESTATUS phNxpEseP61_7816_FrameCmd(pphNxpEseP61_7816_cpdu_t pCmd,
        uint8_t **pcmd_data, uint32_t *cmd_len);

/*********************** Global Variables *************************************/
static uint8_t* sTransceiveData = NULL;
static uint32_t sTransceiveDataLen = 0;
phNxpSpiHal_Sem_t cb_data;

/******************************************************************************
 * Function         phNxpEseP61_7816_Transceive
 *
 * Description      This function prepares C-APDU and sends to p61 and recives
 *                  response from the p61. also it parses all required fields of
 *                  the response PDU.
 *
 * Returns          On Success ESESTATUS_SUCCESS else proper error code
 *
 ******************************************************************************/
ESESTATUS phNxpEseP61_7816_Transceive(pphNxpEseP61_7816_cpdu_t pCmd, pphNxpEseP61_7816_rpdu_t pRsp)
{
    ESESTATUS status = ESESTATUS_FAILED;
    NXPLOG_SPIHAL_D(" %s Enter \n", __FUNCTION__);
    uint32_t cmd_len = 0;
    uint8_t *pCmd_data = NULL;

    if (phNxpSpiHal_init_cb_data(&cb_data, NULL) != ESESTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_D("%s Create cb data failed", __FUNCTION__);
        CONCURRENCY_UNLOCK();
        return status;
    }

    if (NULL == pCmd  || NULL == pRsp)
    {
        NXPLOG_SPIHAL_E(" %s Invalid prameter \n", __FUNCTION__);
        status = ESESTATUS_INVALID_PARAMETER;
    }
    else if(pCmd->cpdu_type > 1)
    {
        NXPLOG_SPIHAL_E(" %s Invalid cpdu type \n", __FUNCTION__);
        status = ESESTATUS_INVALID_CPDU_TYPE;
    }
    else if(0 < pCmd->le_type && NULL == pRsp->pdata)
    {
       /* if response is requested, but no valid res buffer
        * provided by application */
        NXPLOG_SPIHAL_E(" %s Invalid response buffer \n", __FUNCTION__);
        status = ESESTATUS_INVALID_BUFFER;
    }
    else
    {
        status = phNxpEseP61_7816_FrameCmd(pCmd, &pCmd_data, &cmd_len);
        if (ESESTATUS_SUCCESS == status)
        {
            status = phNxpEseP61_Transceive(cmd_len, pCmd_data);
            if (ESESTATUS_SUCCESS != status)
            {
                NXPLOG_SPIHAL_E(" %s phNxpEseP61_Transceive Failed \n", __FUNCTION__);
            }
            else
            {
                /* Wait for callback response */
                if (SEM_WAIT(cb_data))
                {
                    NXPLOG_SPIHAL_E("phNxpEseP61_Transceive semaphore error");
                    status = ESESTATUS_FAILED;
                }
                else
                {
                    if ((sTransceiveDataLen > 0) && (sTransceiveData != NULL))
                    {
                        pRsp->sw2 = *(sTransceiveData + (sTransceiveDataLen -1));
                        sTransceiveDataLen--;
                        pRsp->sw1 = *(sTransceiveData + (sTransceiveDataLen -1));
                        sTransceiveDataLen--;
                        pRsp->len = sTransceiveDataLen;
                        ALOGD("pRsp->len %d", pRsp->len);
                        if (sTransceiveDataLen > 0 && NULL != pRsp->pdata)
                        {
                            memcpy(pRsp->pdata, sTransceiveData, sTransceiveDataLen);
                            status = ESESTATUS_SUCCESS;
                        }
                        else if(sTransceiveDataLen == 0)
                        {
                            status = ESESTATUS_SUCCESS;
                        }
                        else
                        {
                           /* if application response buffer is null and data is present */
                            NXPLOG_SPIHAL_E("Invalid Res buffer");
                            status = ESESTATUS_FAILED;
                        }
                        NXPLOG_SPIHAL_D("Freeing memroy sTransceiveData");
                        free(sTransceiveData);
                        sTransceiveData = NULL;
                        sTransceiveDataLen = 0;
                    }
                    else
                    {
                        NXPLOG_SPIHAL_E("sTransceiveDataLen error = %d", sTransceiveDataLen);
                        status = ESESTATUS_FAILED;
                    }
                }
            }
            if (pCmd_data != NULL)
            {
                NXPLOG_SPIHAL_D("Freeing memroy pCmd_data");
                free(pCmd_data);
            }
        }
    }
    phNxpSpiHal_cleanup_cb_data(&cb_data);
    NXPLOG_SPIHAL_D(" %s Exit status 0x%x \n", __FUNCTION__, status);
    return status;
}

/**
 * \ingroup ISO7816-4_application_protocol_implementation
 * \brief Frames ISO7816-4 command.
 *                  pCmd: 7816 command structure.
 *                  pcmd_data: command buffer pointer.
 *
 * \param[in]       phNxpEseP61_7816_cpdu_t pCmd- Structure pointer passed from application
 * \param[in]       uint32_t *cmd_len - Hold the buffer length, update by this function
 * \param[in]       uint8_t **pcmd_data - Hold the allocated memory buffer for command.
 *
 * \retval  ESESTATUS_SUCCESS on Success else proper error code
 *
 */

STATIC ESESTATUS phNxpEseP61_7816_FrameCmd(pphNxpEseP61_7816_cpdu_t pCmd,
        uint8_t **pcmd_data, uint32_t *cmd_len)
{
    uint32_t cmd_total_len = MIN_HEADER_LEN;/* header is always 4 bytes */
    uint8_t *pbuff = NULL;
    uint32_t index = 0;
    uint8_t lc_len = 0;
    uint8_t le_len = 0;

    NXPLOG_SPIHAL_D("%s  pCmd->lc = %d, pCmd->le_type = %d",
            __FUNCTION__, pCmd->lc, pCmd->le_type);
    /* calculate the total buffer length */
    if (pCmd->lc > 0)
    {
        if (pCmd->cpdu_type == 0)
        {
            cmd_total_len += 1; /* 1 byte LC */
            lc_len = 1;
        }
        else
        {
            cmd_total_len += 3; /* 3 byte LC */
            lc_len = 3;
        }

        cmd_total_len += pCmd->lc; /* add data length */
        if (pCmd->pdata == NULL)
        {
            NXPLOG_SPIHAL_E("%s Invalide data buffer from application ", __FUNCTION__);
            return ESESTATUS_INVALID_BUFFER;
        }
    }
    else
    {
        lc_len = 0;
        NXPLOG_SPIHAL_D("%s lc (data) field is not present %d", __FUNCTION__, pCmd->lc);
    }

    if (pCmd->le_type > 0)
    {
        if (pCmd->le_type == 1)
        {
            cmd_total_len += 1; /* 1 byte LE */
            le_len = 1;
        }
        else if ((pCmd->le_type == 2 || pCmd->le_type == 3))
        {
            /* extended le */
            if ((pCmd->le_type == 3 )&& (lc_len == 0))
            {
                /* if data field not present, than only LE would be three bytes */
                cmd_total_len += 3; /* 3 byte LE */
                le_len = 3;
            }
            else if ((pCmd->le_type == 2 )&& (lc_len != 0))
            {
                /* if data field present, LE would be two bytes */
                cmd_total_len += 2; /* 2 byte LE */
                le_len = 2;
            }
            else
            {
                NXPLOG_SPIHAL_E("%s wrong LE type  %d", __FUNCTION__, pCmd->le_type);
                cmd_total_len += pCmd->le_type;
                le_len = pCmd->le_type;
            }
        }
        else
        {
            NXPLOG_SPIHAL_E("%s wrong cpdu_type value %d", __FUNCTION__, pCmd->cpdu_type);
            return ESESTATUS_INVALID_CPDU_TYPE;
        }
    }
    else
    {
        le_len = 0;
        NXPLOG_SPIHAL_D("%s le field is not present", __FUNCTION__);
    }
    NXPLOG_SPIHAL_D("%s cmd_total_len = %d, le_len = %d, lc_len = %d",
            __FUNCTION__, cmd_total_len, le_len, lc_len);

    pbuff = (uint8_t *)calloc(cmd_total_len, sizeof(uint8_t));
    if (pbuff == NULL)
    {
        NXPLOG_SPIHAL_D("%s Error allocating memory", __FUNCTION__);
        return ESESTATUS_INSUFFICIENT_RESOURCES;
    }
    *cmd_len = cmd_total_len;
    *pcmd_data = pbuff;
    index = 0;

    *(pbuff + index) = pCmd->cla;
    index++;

    *(pbuff + index) = pCmd->ins;
    index++;

    *(pbuff + index) = pCmd->p1;
    index++;

    *(pbuff + index) = pCmd->p2;
    index++;

    /* lc_len can be 0, 1 or 3 bytes */
    if (lc_len > 0)
    {
        if (lc_len == 1)
        {
            *(pbuff + index) = pCmd->lc;
            index++;
        }
        else
        {
            /* extended cmd buffer*/
            *(pbuff + index) = 0x00; /* three byte lc */
            index++;
            *(pbuff + index) = ((pCmd->lc & 0xFF00) >> 8);
            index++;
            *(pbuff + index) = (pCmd->lc & 0x00FF);
            index++;
        }
        /* copy the lc bytes data */
        memcpy((pbuff+index), pCmd->pdata, pCmd->lc);
        index += pCmd->lc;
    }
    /* le_len can be 0, 1, 2 or 3 bytes */
    if (le_len > 0)
    {
        if (le_len == 1)
        {
            if(pCmd->le == 256)
            {
                /* if le is 256 assign max value*/
                *(pbuff + index) = 0x00;
            }
            else
            {
                *(pbuff + index) = (uint8_t) pCmd->le;
            }
            index++;
        }
        else
        {
            if (pCmd->le == 65536)
            {
                if (le_len == 3)
                {
                    /* assign max value */
                    *(pbuff + index) = 0x00; /* three byte le */
                    index++;
                }
                *(pbuff + index) = 0x00;
                index++;
                *(pbuff + index) = 0x00;
                index++;
            }
            else
            {
                if (le_len == 3)
                {
                    *(pbuff + index) = 0x00; /* three byte le */
                    index++;
                }
                *(pbuff + index) = ((pCmd->le & 0x0000FF00) >> 8);
                index++;
                *(pbuff + index) = (pCmd->le & 0x000000FF);
                index++;
            }
        }
    }
    NXPLOG_SPIHAL_D("Exit %s cmd_total_len = %d, index = %d", __FUNCTION__, index, cmd_total_len);
    return ESESTATUS_SUCCESS;
}

/**
 * \ingroup spi_package
 * \brief This is local callback used if application layer provide NULL for phNxpEseP61_open() \n
 * This function receives connection-related events from stack.
 *                  connEvent: Event code.
 *                  eventData: Event data.
 *
 * \param[in]       ESESTATUS
 * \param[in]       phNxpEseP61_data*
 *
 * \retval void
 *
 */

void ese_stack_data_callback(ESESTATUS status, phNxpEseP61_data *eventData)
{
    switch (status)
    {
        case ESESTATUS_SUCCESS:
        {
            if(eventData != NULL)
            {
                NXPLOG_SPIHAL_D("%s: Status:ESESTATUS_SUCCESS", __FUNCTION__);
                if (eventData->len)
                {
                    if (NULL == (sTransceiveData = (uint8_t *) malloc (eventData->len)))
                    {
                        NXPLOG_SPIHAL_D("%s: memory allocation error", __FUNCTION__);
                    }
                    else
                    {
                        memcpy (sTransceiveData, eventData->p_data, sTransceiveDataLen = eventData->len);
                    }
                }
            }
            break;
        }
        case ESESTATUS_FAILED:
        {
            NXPLOG_SPIHAL_D("%s: Status:ESESTATUS_FAILED", __FUNCTION__);
            break;
        }

        case ESESTATUS_WTX_REQ:
        {
            NXPLOG_SPIHAL_D("%s: Status:ESESTATUS_WTX_REQ", __FUNCTION__);
            break;
        }

        case ESESTATUS_ABORTED:
        {
            NXPLOG_SPIHAL_D("%s: Status:ESESTATUS_ABORTED", __FUNCTION__);
            break;
        }

    }
    if (ESESTATUS_WTX_REQ != status)
    {
        NXPLOG_SPIHAL_D("Before SEM_POST");
        SEM_POST(&cb_data);
    }

    NXPLOG_SPIHAL_D("%s: exit", __FUNCTION__);
}
/** @} */
