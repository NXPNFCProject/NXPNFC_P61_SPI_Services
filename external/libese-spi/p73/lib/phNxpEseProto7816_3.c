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
#include <phNxpEseProto7816_3.h>

/**
 * \addtogroup ISO7816-3_protocol_lib
 *
 * @{ */
/******************************************************************************
\section Introduction Introduction

 * This module provide the 7816-3 protocol level implementation for ESE
 *
 ******************************************************************************/
static bool_t phNxpEseProto7816_ResetProtoParams(void);
static bool_t phNxpEseProto7816_SendRawFrame(uint32_t data_len, uint8_t *p_data);
static bool_t phNxpEseProto7816_GetRawFrame(uint32_t *data_len, uint8_t **pp_data);
static uint8_t phNxpEseProto7816_ComputeLRC(unsigned char *p_buff, uint32_t offset,
        uint32_t length);
static bool_t phNxpEseProto7816_CheckLRC(uint32_t data_len, uint8_t *p_data);
static uint8_t getMaxSupportedSendIFrameSize();
static bool_t phNxpEseProto7816_SendSFrame(sFrameInfo_t sFrameData);
static bool_t phNxpEseProto7816_SendIframe(iFrameInfo_t iFrameData);
static  bool_t phNxpEseProto7816_sendRframe(rFrameTypes_t rFrameType);
static bool_t phNxpEseProto7816_SetFirstIframeContxt(void);
static bool_t phNxpEseProto7816_SetNextIframeContxt(void);
static bool_t phNxpEseProro7816_SaveIframeData(uint8_t *p_data, uint32_t data_len);
static bool_t phNxpEseProto7816_ResetRecovery(void);
static bool_t phNxpEseProto7816_RecoverySteps(void);
static bool_t phNxpEseProto7816_DecodeFrame(uint8_t *p_data, uint32_t data_len);
static bool_t phNxpEseProto7816_ProcessResponse(void);
static bool_t TransceiveProcess(void);
static bool_t phNxpEseProto7816_RSync(void);
static bool_t phNxpEseProto7816_ResetProtoParams(void);

/******************************************************************************
 * Function         phNxpEseProto7816_SendRawFrame
 *
 * Description      This internal function is called send the data to ESE
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static bool_t phNxpEseProto7816_SendRawFrame(uint32_t data_len, uint8_t *p_data)
{
    bool_t status = FALSE;
    NXPLOG_ESELIB_D("Enter %s ", __FUNCTION__);
    status = phNxpEse_WriteFrame(data_len, p_data);
    if (ESESTATUS_SUCCESS != status)
    {
        NXPLOG_ESELIB_E("%s Error phNxpEse_WriteFrame\n", __FUNCTION__);
    }
    else
    {
        NXPLOG_ESELIB_D("%s phNxpEse_WriteFrame Success \n", __FUNCTION__);
        status = TRUE;
    }
    NXPLOG_ESELIB_D("Exit %s ", __FUNCTION__);
    return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_GetRawFrame
 *
 * Description      This internal function is called read the data from the ESE
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static bool_t phNxpEseProto7816_GetRawFrame(uint32_t *data_len, uint8_t **pp_data)
{
    bool_t bStatus = FALSE;
    ESESTATUS status = ESESTATUS_FAILED;

    status = phNxpEse_read(data_len, pp_data);
    if (ESESTATUS_SUCCESS != status)
    {
        NXPLOG_ESELIB_E("%s phNxpEse_read failed , status : 0x%x", __FUNCTION__, status);
    }
    else
    {
        bStatus = TRUE;
    }
    return bStatus;
}

/******************************************************************************
 * Function         phNxpEseProto7816_ComputeLRC
 *
 * Description      This internal function is called compute the LRC
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static uint8_t phNxpEseProto7816_ComputeLRC(unsigned char *p_buff, uint32_t offset,
        uint32_t length)
{
    uint32_t LRC = 0, i = 0;
    NXPLOG_ESELIB_D("Enter %s ", __FUNCTION__);
    for (i = offset; i < length; i++)
    {
        LRC = LRC ^ p_buff[i];
        //NXPLOG_ESELIB_E("%s data 0x%x", __FUNCTION__, p_buff[i]);
    }
    NXPLOG_ESELIB_D("Exit %s ", __FUNCTION__);
    return (uint8_t) LRC;
}

/******************************************************************************
 * Function         phNxpEseProto7816_CheckLRC
 *
 * Description      This internal function is called compute and compare the
 *                  received LRC of the received data
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static bool_t phNxpEseProto7816_CheckLRC(uint32_t data_len, uint8_t *p_data)
{
    bool_t status = TRUE;
    uint8_t calc_crc = 0;
    uint8_t recv_crc = 0;
    NXPLOG_ESELIB_D("Enter %s ", __FUNCTION__);
    recv_crc = p_data[data_len - 1];

    /* calculate the CRC after excluding CRC  */
    calc_crc = phNxpEseProto7816_ComputeLRC(p_data, 1, (data_len -1));
    NXPLOG_ESELIB_D("Received LRC:0x%x Calculated LRC:0x%x", recv_crc, calc_crc);
    if (recv_crc != calc_crc)
    {
        status = FALSE;
        NXPLOG_ESELIB_E("%s LRC failed", __FUNCTION__);
    }
    NXPLOG_ESELIB_D("Exit %s ", __FUNCTION__);
    return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_SendSFrame
 *
 * Description      This internal function is called to get the max supported
 *                  I-frame size
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static uint8_t getMaxSupportedSendIFrameSize()
{
    return IFSC_SIZE_SEND ;
}

/******************************************************************************
 * Function         phNxpEseProto7816_SendSFrame
 *
 * Description      This internal function is called to send S-frame with all
 *                   updated 7816-3 headers
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static bool_t phNxpEseProto7816_SendSFrame(sFrameInfo_t sFrameData)
{
    bool_t status = ESESTATUS_FAILED;
    uint32_t frame_len = 0;
    uint8_t *p_framebuff = NULL;
    uint8_t pcb_byte = 0;
    NXPLOG_ESELIB_D("Enter %s ", __FUNCTION__);
    sFrameInfo_t sframeData = sFrameData;

    switch(sframeData.sFrameType)
    {
        case RESYNCH_REQ:
            frame_len = (PH_PROTO_7816_HEADER_LEN + PH_PROTO_7816_CRC_LEN);
            p_framebuff = phNxpEse_memalloc(frame_len * sizeof(uint8_t));
            if (NULL == p_framebuff)
            {
                return FALSE;
            }
            p_framebuff[2] = 0;
            p_framebuff[3] = 0x00;

            pcb_byte |= PH_PROTO_7816_S_BLOCK_REQ; /* PCB */
            pcb_byte |= PH_PROTO_7816_S_RESYNCH;
            break;
        case INTF_RESET_REQ:
            frame_len = (PH_PROTO_7816_HEADER_LEN + PH_PROTO_7816_CRC_LEN);
            p_framebuff = phNxpEse_memalloc(frame_len * sizeof(uint8_t));
            if (NULL == p_framebuff)
            {
                return FALSE;
            }
            p_framebuff[2] = 0;
            p_framebuff[3] = 0x00;

            pcb_byte |= PH_PROTO_7816_S_BLOCK_REQ; /* PCB */
            pcb_byte |= PH_PROTO_7816_S_RESET;
            break;
        case PROP_END_APDU_REQ:
            frame_len = (PH_PROTO_7816_HEADER_LEN + PH_PROTO_7816_CRC_LEN);
            p_framebuff = phNxpEse_memalloc(frame_len * sizeof(uint8_t));
            if (NULL == p_framebuff)
            {
                return FALSE;
            }
            p_framebuff[2] = 0;
            p_framebuff[3] = 0x00;

            pcb_byte |= PH_PROTO_7816_S_BLOCK_REQ; /* PCB */
            pcb_byte |= PH_PROTO_7816_S_END_OF_APDU;
            break;
        case WTX_RSP:
            frame_len = (PH_PROTO_7816_HEADER_LEN + 1 + PH_PROTO_7816_CRC_LEN);
            p_framebuff = phNxpEse_memalloc(frame_len * sizeof(uint8_t));
            if (NULL == p_framebuff)
            {
                return FALSE;
            }
            p_framebuff[2] = 0x01;
            p_framebuff[3] = 0x01;

            pcb_byte |= PH_PROTO_7816_S_BLOCK_RSP;
            pcb_byte |= PH_PROTO_7816_S_WTX;
            break;
        default:
            NXPLOG_ESELIB_E("Invalid S-block");
            break;
    }
    if(NULL != p_framebuff)
    {
        /* frame the packet */
        p_framebuff[0] = 0x00; /* NAD Byte */
        p_framebuff[1] = pcb_byte; /* PCB */

        p_framebuff[frame_len - 1] = phNxpEseProto7816_ComputeLRC(p_framebuff, 0,
                (frame_len - 1));
        NXPLOG_ESELIB_D("S-Frame PCB: %x\n", p_framebuff[1]);
        status = phNxpEseProto7816_SendRawFrame(frame_len, p_framebuff);
        phNxpEse_free(p_framebuff);
    }
    else
    {
        NXPLOG_ESELIB_E("Invalid S-block or malloc for s-block failed");
    }
    NXPLOG_ESELIB_D("Exit %s ", __FUNCTION__);
    return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_sendRframe
 *
 * Description      This internal function is called to send R-frame with all
 *                   updated 7816-3 headers
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static  bool_t phNxpEseProto7816_sendRframe(rFrameTypes_t rFrameType)
{
    bool_t status = FALSE;
    uint8_t recv_ack[4]= {0x00,0x80,0x00,0x00};
    if(RNACK == rFrameType)
    {
        recv_ack[1] = 0x82;
    }
    else
    {
        /* Do Nothing */
    }
    recv_ack[1] |=((phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.seqNo^1) << 4);
    NXPLOG_ESELIB_D("%s recv_ack[1]:0x%x", __FUNCTION__, recv_ack[1]);
    recv_ack[3] = phNxpEseProto7816_ComputeLRC(recv_ack, 0x00, (sizeof(recv_ack) -1));
    status = phNxpEseProto7816_SendRawFrame(sizeof(recv_ack), recv_ack);
    return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_SendIframe
 *
 * Description      This internal function is called to send I-frame with all
 *                   updated 7816-3 headers
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static bool_t phNxpEseProto7816_SendIframe(iFrameInfo_t iFrameData)
{
    bool_t status = FALSE;
    uint32_t frame_len = 0;
    uint8_t *p_framebuff = NULL;
    uint8_t pcb_byte = 0;
    NXPLOG_ESELIB_D("Enter %s ", __FUNCTION__);
    if (0 == iFrameData.sendDataLen)
    {
        NXPLOG_ESELIB_E("I frame Len is 0, INVALID");
        return FALSE;
    }

    frame_len = (iFrameData.sendDataLen+ PH_PROTO_7816_HEADER_LEN + PH_PROTO_7816_CRC_LEN);

    p_framebuff = phNxpEse_memalloc(frame_len * sizeof(uint8_t));
    if (NULL == p_framebuff)
    {
        NXPLOG_ESELIB_E("Heap allocation failed");
        return FALSE;
    }

    /* frame the packet */
    p_framebuff[0] = 0x00; /* NAD Byte */

    if (iFrameData.isChained)
    {
        /* make B6 (M) bit high */
        pcb_byte |= PH_PROTO_7816_CHAINING;
    }

    /* Update the send seq no */
    pcb_byte |= (phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.seqNo << 6);

    /* store the pcb byte */
    p_framebuff[1] = pcb_byte;
    /* store I frame length */
    p_framebuff[2] = iFrameData.sendDataLen;
    /* store I frame */
    phNxpEse_memcpy(&(p_framebuff[3]), iFrameData.p_data + iFrameData.dataOffset, iFrameData.sendDataLen);

    p_framebuff[frame_len - 1] = phNxpEseProto7816_ComputeLRC(p_framebuff, 0,
            (frame_len - 1));

    status = phNxpEseProto7816_SendRawFrame(frame_len, p_framebuff);

    phNxpEse_free(p_framebuff);
    NXPLOG_ESELIB_D("Exit %s ", __FUNCTION__);
    return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_SetNextIframeContxt
 *
 * Description      This internal function is called to set the context for next I-frame.
 *                  Not applicable for the first I-frame of the transceive
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static bool_t phNxpEseProto7816_SetFirstIframeContxt(void)
{
    NXPLOG_ESELIB_D("Enter %s ", __FUNCTION__);
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.dataOffset = 0;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = IFRAME;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.seqNo = phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.seqNo ^ 1;
    phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_IFRAME;
    if (phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.totalDataLen > phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.maxDataLen)
    {
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.isChained = TRUE;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.sendDataLen = phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.maxDataLen;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.totalDataLen = phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.totalDataLen -
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.maxDataLen;
    }
    else
    {
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.sendDataLen = phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.totalDataLen;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.isChained = FALSE;
    }
    NXPLOG_ESELIB_D("I-Frame Data Len: %d Seq. no:%d", phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.sendDataLen, phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.seqNo);
    NXPLOG_ESELIB_D("Exit %s ", __FUNCTION__);
    return TRUE;
}

/******************************************************************************
 * Function         phNxpEseProto7816_SetNextIframeContxt
 *
 * Description      This internal function is called to set the context for next I-frame.
 *                  Not applicable for the first I-frame of the transceive
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static bool_t phNxpEseProto7816_SetNextIframeContxt(void)
{
    NXPLOG_ESELIB_D("Enter %s ", __FUNCTION__);
    /* Expecting to reach here only after first of chained I-frame is sent and before the last chained is sent */
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = IFRAME;
    phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_IFRAME;

    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.seqNo = phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.seqNo ^ 1;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.dataOffset = phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.dataOffset +
                                                                        phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.maxDataLen;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.p_data = phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.p_data;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.maxDataLen = phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.maxDataLen;

    //if  chained
    if (phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.totalDataLen >
            phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.maxDataLen)
    {
        NXPLOG_ESELIB_D("Process Chained Frame");
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.isChained = TRUE;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.sendDataLen = phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.maxDataLen;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.totalDataLen = phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.totalDataLen -
                                                                                phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.maxDataLen;
    }
    else
    {
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.isChained = FALSE;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.sendDataLen = phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.totalDataLen;
    }
    NXPLOG_ESELIB_D("I-Frame Data Len: %d", phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.sendDataLen);
    NXPLOG_ESELIB_D("Exit %s ", __FUNCTION__);
    return TRUE;
}

/******************************************************************************
 * Function         phNxpEseProto7816_ResetRecovery
 *
 * Description      This internal function is called to do reset the recovery pareameters
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static bool_t phNxpEseProro7816_SaveIframeData(uint8_t *p_data, uint32_t data_len)
{
    bool_t status = FALSE;
    NXPLOG_ESELIB_D("Enter %s ", __FUNCTION__);
    NXPLOG_ESELIB_D("Data[0]=0x%x len=%d Data[%d]=0x%x", p_data[0], data_len, data_len-1, p_data[data_len-1]);
    if (ESESTATUS_SUCCESS != phNxpEse_StoreDatainList(data_len, p_data))
    {
        NXPLOG_ESELIB_E("%s - Error storing chained data in list", __FUNCTION__);
    }
    else
    {
        status = TRUE;
    }
    NXPLOG_ESELIB_D("Exit %s ", __FUNCTION__);
    return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_ResetRecovery
 *
 * Description      This internal function is called to do reset the recovery pareameters
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static bool_t phNxpEseProto7816_ResetRecovery(void)
{
    phNxpEseProto7816_3_Var.recoveryCounter = 0;
    return TRUE;
}

/******************************************************************************
 * Function         phNxpEseProto7816_RecoverySteps
 *
 * Description      This internal function is called when 7816-3 stack failed to recover
 *                  after PH_PROTO_7816_FRAME_RETRY_COUNT, and the interface has to be
 *                  recovered
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static bool_t phNxpEseProto7816_RecoverySteps(void)
{
    if(phNxpEseProto7816_3_Var.recoveryCounter <= PH_PROTO_7816_FRAME_RETRY_COUNT)
    {
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType = INTF_RESET_REQ;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType= SFRAME;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.SframeInfo.sFrameType = INTF_RESET_REQ;
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_S_INTF_RST;
    }
    else
    { /* If recovery fails */
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = IDLE_STATE;
    }
    return TRUE;
}

/******************************************************************************
 * Function         phNxpEseProto7816_DecodeFrame
 *
 * Description      This internal function is used to
 *                  1. Identify the received frame
 *                  2. If the received frame is I-frame with expected sequence number, store it or else send R-NACK
                    3. If the received frame is R-frame,
                       3.1 R-ACK with expected seq. number: Send the next chained I-frame
                       3.2 R-ACK with different sequence number: Sebd the R-Nack
                       3.3 R-NACK: Re-send the last frame
                    4. If the received frame is S-frame, send back the correct S-frame response.
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static bool_t phNxpEseProto7816_DecodeFrame(uint8_t *p_data, uint32_t data_len)
{
    bool_t status = TRUE;
    uint8_t pcb;
    phNxpEseProto7816_PCB_bits_t pcb_bits;
    NXPLOG_ESELIB_D("Enter %s ", __FUNCTION__);
    pcb = p_data[PH_PROPTO_7816_PCB_OFFSET];
    //memset(&phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.rcvPcbBits, 0x00, sizeof(struct PCB_BITS));
    phNxpEse_memset(&pcb_bits, 0x00, sizeof(phNxpEseProto7816_PCB_bits_t));
    phNxpEse_memcpy(&pcb_bits, &pcb, sizeof(uint8_t));

    if (0x00 == pcb_bits.msb) /* I-FRAME decoded should come here */
    {
        NXPLOG_ESELIB_D("%s I-Frame Received", __FUNCTION__);
        phNxpEseProto7816_3_Var.wtx_counter = 0;
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdFrameType = IFRAME ;
        if (phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.seqNo != pcb_bits.bit7)       //   != pcb_bits->bit7)
        {
            NXPLOG_ESELIB_D("%s I-Frame lastRcvdIframeInfo.seqNo:0x%x", __FUNCTION__, pcb_bits.bit7);
            phNxpEseProto7816_ResetRecovery();
            phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.seqNo = 0x00;
            phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.seqNo |= pcb_bits.bit7;

            if (pcb_bits.bit6)
            {
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.isChained = TRUE;
                phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = RFRAME;
                phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.RframeInfo.errCode = NO_ERROR ;
                status = phNxpEseProro7816_SaveIframeData(&p_data[3], data_len-4);
                phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_R_ACK ;
            }
            else
            {
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.isChained = FALSE;
                phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = IDLE_STATE;
                status = phNxpEseProro7816_SaveIframeData(&p_data[3], data_len-4);
            }
        }
        else
        {
            phNxpEse_Sleep(DELAY_ERROR_RECOVERY);
            if(phNxpEseProto7816_3_Var.recoveryCounter < PH_PROTO_7816_FRAME_RETRY_COUNT)
            {
                phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = RFRAME;
                phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.RframeInfo.errCode= OTHER_ERROR ;
                phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_R_NACK ;
                phNxpEseProto7816_3_Var.recoveryCounter++;
            }
            else
            {
                phNxpEseProto7816_RecoverySteps();
                phNxpEseProto7816_3_Var.recoveryCounter++;
            }
        }
    }
    else if ((0x01 == pcb_bits.msb) && (0x00 == pcb_bits.bit7)) /* R-FRAME decoded should come here */
    {
        NXPLOG_ESELIB_D("%s R-Frame Received", __FUNCTION__);
        phNxpEseProto7816_3_Var.wtx_counter = 0;
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdFrameType = RFRAME;
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo.seqNo = 0; // = 0;
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo.seqNo |= pcb_bits.bit5;

        if ((pcb_bits.lsb == 0x00) && (pcb_bits.bit2 == 0x00))
        {
            phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo.errCode = NO_ERROR;
            phNxpEseProto7816_ResetRecovery();
            if(phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo.seqNo !=
                    phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.seqNo)
            {
                status = phNxpEseProto7816_SetNextIframeContxt();
                phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_IFRAME;
            }
            else
            {
                //error handling.
            }
        } /* Error handling 1 */
        else if ((pcb_bits.lsb == 0x01) && (pcb_bits.bit2 == 0x00))
        {
            phNxpEse_Sleep(DELAY_ERROR_RECOVERY);
            if(phNxpEseProto7816_3_Var.recoveryCounter < PH_PROTO_7816_FRAME_RETRY_COUNT)
            {
                if(phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.FrameType == IFRAME)
                {
                    phNxpEse_memcpy(&phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx,
                        &phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx,
                            sizeof(phNxpEseProto7816_NextTx_Info_t));
                    phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_IFRAME;
                    phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo.errCode = PARITY_ERROR;
                    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = IFRAME;
                }
                else if(phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.FrameType == RFRAME)
                {
                    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType= RFRAME;
                    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.RframeInfo.errCode = OTHER_ERROR ;
                    phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_R_NACK ;
                }
                else if(phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.FrameType == SFRAME)
                {
                    /* Copy the last S frame sent */
                    phNxpEse_memcpy(&phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx,
                        &phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx,
                            sizeof(phNxpEseProto7816_NextTx_Info_t));
                }
                phNxpEseProto7816_3_Var.recoveryCounter++;
            }
            else
            {
                phNxpEseProto7816_RecoverySteps();
                phNxpEseProto7816_3_Var.recoveryCounter++;
            }
            //resend previously send I frame
        } /* Error handling 2 */
        else if ((pcb_bits.lsb == 0x00) && (pcb_bits.bit2 == 0x01))
        {
            phNxpEse_Sleep(DELAY_ERROR_RECOVERY);
            if(phNxpEseProto7816_3_Var.recoveryCounter < PH_PROTO_7816_FRAME_RETRY_COUNT)
            {
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo.errCode = OTHER_ERROR;
                phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx = phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx;
                phNxpEseProto7816_3_Var.recoveryCounter++;
            }
            else
            {
                phNxpEseProto7816_RecoverySteps();
                phNxpEseProto7816_3_Var.recoveryCounter++;
            }

        } /* Error handling 3 */
        else if ((pcb_bits.lsb == 0x01) && (pcb_bits.bit2 == 0x01))
        {
            phNxpEse_Sleep(DELAY_ERROR_RECOVERY);
            if(phNxpEseProto7816_3_Var.recoveryCounter < PH_PROTO_7816_FRAME_RETRY_COUNT)
            {
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo.errCode = SOF_MISSED_ERROR;
                phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx = phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx;
                phNxpEseProto7816_3_Var.recoveryCounter++;
            }
            else
            {
                phNxpEseProto7816_RecoverySteps();
                phNxpEseProto7816_3_Var.recoveryCounter++;
            }
        }
        else /* Error handling 4 */
        {
            phNxpEse_Sleep(DELAY_ERROR_RECOVERY);
            if(phNxpEseProto7816_3_Var.recoveryCounter < PH_PROTO_7816_FRAME_RETRY_COUNT)
            {
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo.errCode = UNDEFINED_ERROR;
                phNxpEseProto7816_3_Var.recoveryCounter++;
            }
            else
            {
                phNxpEseProto7816_RecoverySteps();
                phNxpEseProto7816_3_Var.recoveryCounter++;
            }
        }
    }
    else if ((0x01 == pcb_bits.msb) && (0x01 == pcb_bits.bit7)) /* S-FRAME decoded should come here */
    {
        NXPLOG_ESELIB_D("%s S-Frame Received", __FUNCTION__);
        int32_t frameType = (int32_t)(pcb & 0x3F); /*discard upper 2 bits */
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdFrameType = SFRAME;
        if(frameType!=WTX_REQ)
        {
            phNxpEseProto7816_3_Var.wtx_counter = 0;
        }
        switch(frameType)
        {
            case RESYNCH_REQ:
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType = RESYNCH_REQ;
                break;
            case RESYNCH_RSP:
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType = RESYNCH_RSP;
                phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType= UNKNOWN;
                phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = IDLE_STATE;
                break;
            case IFSC_REQ:
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType = IFSC_REQ;
                break;
            case IFSC_RES:
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType = IFSC_RES;
                phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType= UNKNOWN;
                phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = IDLE_STATE ;
                break;
            case ABORT_REQ:
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType = ABORT_REQ;
                break;
            case ABORT_RES:
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType = ABORT_RES;
                phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType= UNKNOWN;
                phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = IDLE_STATE ;
                break;
            case WTX_REQ:
                phNxpEseProto7816_3_Var.wtx_counter++;
                NXPLOG_ESELIB_D("%s Wtx_counter value - %lu", __FUNCTION__, phNxpEseProto7816_3_Var.wtx_counter);
                NXPLOG_ESELIB_D("%s Wtx_counter wtx_counter_limit - %lu", __FUNCTION__, phNxpEseProto7816_3_Var.wtx_counter_limit);
                /* Previous sent frame is some S-frame but not WTX response S-frame */
                if(phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.SframeInfo.sFrameType != WTX_RSP &&
                    phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.FrameType == SFRAME)
                {   /* Goto recovery if it keep coming here for more than recovery counter max. value */
                    if(phNxpEseProto7816_3_Var.recoveryCounter < PH_PROTO_7816_FRAME_RETRY_COUNT)
                    {   /* Re-transmitting the previous sent S-frame */
                        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx = phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx;
                        phNxpEseProto7816_3_Var.recoveryCounter++;
                    }
                    else
                    {
                        phNxpEseProto7816_RecoverySteps();
                        phNxpEseProto7816_3_Var.recoveryCounter++;
                    }
                }
                else
                {   /* Checking for WTX counter with max. allowed WTX count */
                    if(phNxpEseProto7816_3_Var.wtx_counter == phNxpEseProto7816_3_Var.wtx_counter_limit)
                    {
                        phNxpEseProto7816_3_Var.wtx_counter = 0;
                        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType = INTF_RESET_REQ;
                        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType= SFRAME;
                        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.SframeInfo.sFrameType = INTF_RESET_REQ;
                        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_S_INTF_RST;
                        NXPLOG_ESELIB_E("%s Interface Reset to eSE wtx count reached!!!", __FUNCTION__);
                    }
                    else
                    {
                        phNxpEse_Sleep(DELAY_ERROR_RECOVERY);
                        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType = WTX_REQ;
                        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType= SFRAME;
                        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.SframeInfo.sFrameType = WTX_RSP;
                        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_S_WTX_RSP ;
                    }
                }
                break;
            case WTX_RSP:
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType= WTX_RSP;
                break;
            case INTF_RESET_REQ:
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType= INTF_RESET_REQ;
                break;
            case INTF_RESET_RSP:
                phNxpEseProto7816_ResetProtoParams();
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType= INTF_RESET_RSP;
                phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType= UNKNOWN;
                phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = IDLE_STATE;
                break;
            case PROP_END_APDU_REQ:
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType= PROP_END_APDU_REQ;
                break;
            case PROP_END_APDU_RSP:
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType= PROP_END_APDU_RSP;
                phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType= UNKNOWN;
                phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = IDLE_STATE;
                break;
            default:
                NXPLOG_ESELIB_E("%s Wrong S-Frame Received", __FUNCTION__);
                break;
        }
    }
    else
    {
        NXPLOG_ESELIB_E("%s Wrong-Frame Received", __FUNCTION__);
    }
    NXPLOG_ESELIB_D("Exit %s ", __FUNCTION__);
    return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_ProcessResponse
 *
 * Description      This internal function is used to
 *                  1. Check the LRC
 *                  2. Initiate decoding of received frame of data.
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static bool_t phNxpEseProto7816_ProcessResponse(void)
{
    uint32_t data_len = 0;
    uint8_t *p_data = NULL;
    bool_t status = FALSE;
    bool_t checkLrcPass = TRUE;
    NXPLOG_ESELIB_D("Enter %s ", __FUNCTION__);
    status = phNxpEseProto7816_GetRawFrame(&data_len, &p_data);
    NXPLOG_ESELIB_D("%s p_data ----> %p len ----> 0x%x", __FUNCTION__,p_data, data_len);
    if(TRUE == status)
    {
        /* Resetting the timeout counter */
        phNxpEseProto7816_3_Var.timeoutCounter = PH_PROTO_7816_VALUE_ZERO;
        /* LRC check followed */
        checkLrcPass = phNxpEseProto7816_CheckLRC(data_len, p_data);
        if(checkLrcPass == TRUE)
        {
            phNxpEseProto7816_DecodeFrame(p_data, data_len);
        }
        else
        {
            NXPLOG_ESELIB_E("%s LRC Check failed", __FUNCTION__);
            if((SFRAME == phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.FrameType) &&
                    ((WTX_RSP == phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.SframeInfo.sFrameType) ||
                     (RESYNCH_RSP == phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.SframeInfo.sFrameType)))
            {
                phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdFrameType = INVALID ;
                phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType= RFRAME;
                phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.RframeInfo.errCode = PARITY_ERROR ;
                phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.RframeInfo.seqNo =(!phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.seqNo) << 4;
                phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_R_NACK ;
            }
            else
            {
                NXPLOG_ESELIB_E("%s Re-transmitting the previous frame", __FUNCTION__);
                /* Re-transmit the frame */
                phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx = phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx;
            }
        }
    }
    else
    {
        NXPLOG_ESELIB_E("%s phNxpEseProto7816_GetRawFrame failed", __FUNCTION__);
        if((SFRAME == phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.FrameType) &&
                ((WTX_RSP == phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.SframeInfo.sFrameType) ||
                 (RESYNCH_RSP == phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.SframeInfo.sFrameType)))
        {
            phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdFrameType = INVALID ;
            phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType= RFRAME;
            phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.RframeInfo.errCode = OTHER_ERROR ;
            phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.RframeInfo.seqNo =(!phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.seqNo) << 4;
            phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_R_NACK ;
        }
        else
        {
            phNxpEse_Sleep(DELAY_ERROR_RECOVERY);
            /* re transmit the frame */
            if(phNxpEseProto7816_3_Var.timeoutCounter < PH_PROTO_7816_TIMEOUT_RETRY_COUNT)
            {
                phNxpEseProto7816_3_Var.timeoutCounter++;
                NXPLOG_ESELIB_E("%s re-transmitting the previous frame", __FUNCTION__);
                phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx = phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx ;
            }
            else
            {
                /* Re-transmission failed completely, Going to exit */
                phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = IDLE_STATE;
                phNxpEseProto7816_3_Var.timeoutCounter = PH_PROTO_7816_VALUE_ZERO;
            }
        }
    }
    NXPLOG_ESELIB_D("Exit %s Status 0x%x", __FUNCTION__, status);
    return status;
}

/******************************************************************************
 * Function         TransceiveProcess
 *
 * Description      This internal function is used to
 *                  1. Send the raw data received from application after computing LRC
 *                  2. Receive the the response data from ESE, decode, process and
 *                     store the data.
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static bool_t TransceiveProcess(void)
{
    bool_t status = FALSE;
    sFrameInfo_t sFrameInfo;

    NXPLOG_ESELIB_D("Enter %s ", __FUNCTION__);
    //status = phNxpEseProto7816_SetNextIframeContxt(); // TODO need to be changed to set first I-frame context
    while(phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState != IDLE_STATE)
    {
        NXPLOG_ESELIB_D("%s nextTransceiveState %x", __FUNCTION__, phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState);
        switch(phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState)
        {
            case SEND_IFRAME:
                status = phNxpEseProto7816_SendIframe(phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo);
                break;
            case SEND_R_ACK:
                status = phNxpEseProto7816_sendRframe(RACK);
                break;
            case SEND_R_NACK:
                status = phNxpEseProto7816_sendRframe(RNACK);
                break;
            case SEND_S_RSYNC:
                sFrameInfo.sFrameType = RESYNCH_REQ;
                status = phNxpEseProto7816_SendSFrame(sFrameInfo);
                break;
            case SEND_S_INTF_RST:
                sFrameInfo.sFrameType = INTF_RESET_REQ;
                status = phNxpEseProto7816_SendSFrame(sFrameInfo);
                break;
            case SEND_S_EOS:
                sFrameInfo.sFrameType = PROP_END_APDU_REQ;
                status = phNxpEseProto7816_SendSFrame(sFrameInfo);
                break;
            case SEND_S_WTX_RSP:
                sFrameInfo.sFrameType = WTX_RSP;
                status = phNxpEseProto7816_SendSFrame(sFrameInfo);
                break;
            default:
                phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = IDLE_STATE;
                break;
        }
        if(TRUE == status)
        {
            phNxpEse_memcpy(&phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx,
                &phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx,
                    sizeof(phNxpEseProto7816_NextTx_Info_t));
            status = phNxpEseProto7816_ProcessResponse();
        }
        else
        {
            NXPLOG_ESELIB_D("%s Transceive send failed, going to recovery!", __FUNCTION__);
            phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = IDLE_STATE;
        }
    };
    NXPLOG_ESELIB_D("Exit %s Status 0x%x", __FUNCTION__, status);
    return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_Transceive
 *
 * Description      This function is used to
 *                  1. Send the raw data received from application after computing LRC
 *                  2. Receive the the response data from ESE, decode, process and
 *                     store the data.
 *                  3. Get the final complete data and sent back to application
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
bool_t phNxpEseProto7816_Transceive(phNxpEse_data *pCmd, phNxpEse_data *pRsp)
{
    bool_t status = FALSE;
    ESESTATUS wStatus = ESESTATUS_FAILED;
    phNxpEse_data pRes;
    NXPLOG_ESELIB_D("Enter %s ", __FUNCTION__);
    if((NULL == pCmd) || (NULL == pRsp) ||
            (phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState != PH_NXP_ESE_PROTO_7816_IDLE))
        return status;
    phNxpEse_memset(&pRes, 0x00, sizeof(phNxpEse_data));
    /* Updating the transceive information to the protocol stack */
    phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState = PH_NXP_ESE_PROTO_7816_TRANSCEIVE;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.p_data = pCmd->p_data;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.totalDataLen = pCmd->len;
    NXPLOG_ESELIB_D("Transceive data ptr 0x%p len:%d", pCmd->p_data, pCmd->len);
    status = phNxpEseProto7816_SetFirstIframeContxt();
    status = TransceiveProcess();
    if(FALSE == status)
    {
        /* ESE hard reset to be done */
        NXPLOG_ESELIB_E("Transceive failed, hard reset to proceed");
    }
    else
    {
        //fetch the data info and report to upper layer.
        wStatus = phNxpEse_GetData(&pRes.len, &pRes.p_data);
        if (ESESTATUS_SUCCESS == wStatus)
        {
            NXPLOG_ESELIB_D("%s Data successfully received at 7816, packaging to send upper layers: DataLen = %d", __FUNCTION__, pRes.len);
            /* Copy the data to be read by the upper layer via transceive api */
            pRsp->len = pRes.len;
            pRsp->p_data = pRes.p_data;
        }
        else
            status = FALSE;
    }
    phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState = PH_NXP_ESE_PROTO_7816_IDLE;
    NXPLOG_ESELIB_D("Exit %s Status 0x%x", __FUNCTION__, status);
    return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_RSync
 *
 * Description      This function is used to send the RSync command
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static bool_t phNxpEseProto7816_RSync(void)
{
    bool_t status = FALSE;
    phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState = PH_NXP_ESE_PROTO_7816_TRANSCEIVE;
    /* send the end of session s-frame */
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType= SFRAME;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.SframeInfo.sFrameType = RESYNCH_REQ;
    phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_S_RSYNC;
    status = TransceiveProcess();
    phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState = PH_NXP_ESE_PROTO_7816_IDLE;
    return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_ResetProtoParams
 *
 * Description      This function is used to reset the 7816 protocol stack instance
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
static bool_t phNxpEseProto7816_ResetProtoParams(void)
{
    unsigned long int tmpWTXCountlimit = PH_PROTO_7816_VALUE_ZERO;
    tmpWTXCountlimit = phNxpEseProto7816_3_Var.wtx_counter_limit;
    phNxpEse_memset(&phNxpEseProto7816_3_Var, PH_PROTO_7816_VALUE_ZERO, sizeof(phNxpEseProto7816_t));
    phNxpEseProto7816_3_Var.wtx_counter_limit = tmpWTXCountlimit;
    phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState = PH_NXP_ESE_PROTO_7816_IDLE;
    phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = IDLE_STATE;
    phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdFrameType = INVALID;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = INVALID;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.maxDataLen = IFSC_SIZE_SEND;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.p_data = NULL;
    phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.FrameType = INVALID;
    phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.maxDataLen = IFSC_SIZE_SEND;
    phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.p_data = NULL;
    /* Initialized with sequence number of the last I-frame sent */
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.seqNo = PH_PROTO_7816_VALUE_ONE;
    /* Initialized with sequence number of the last I-frame received */
    phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.seqNo = PH_PROTO_7816_VALUE_ONE;
    /* Initialized with sequence number of the last I-frame received */
    phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.seqNo = PH_PROTO_7816_VALUE_ONE;
    phNxpEseProto7816_3_Var.recoveryCounter = PH_PROTO_7816_VALUE_ZERO;
    phNxpEseProto7816_3_Var.timeoutCounter = PH_PROTO_7816_VALUE_ZERO;
    phNxpEseProto7816_3_Var.wtx_counter = PH_PROTO_7816_VALUE_ZERO;
    return TRUE;
}


/******************************************************************************
 * Function         phNxpEseProto7816_Reset
 *
 * Description      This function is used to reset the 7816 protocol stack instance
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
bool_t phNxpEseProto7816_Reset(void)
{
    bool_t status = FALSE;
    /* Resetting host protocol instance */
    phNxpEseProto7816_ResetProtoParams();
    /* Resynchronising ESE protocol instance */
    status = phNxpEseProto7816_RSync();
    return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_Open
 *
 * Description      This function is used to open the 7816 protocol stack instance
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
bool_t phNxpEseProto7816_Open(phNxpEseProto7816InitParam_t initParam)
{
    bool_t status = FALSE;
    status = phNxpEseProto7816_ResetProtoParams();
    NXPLOG_ESELIB_D("%s: First open completed, Congratulations", __FUNCTION__);
    /* Update WTX max. limit */
    phNxpEseProto7816_3_Var.wtx_counter_limit = initParam.wtx_counter_limit;
    if(initParam.interfaceReset) /* Do interface reset */
    {
        status = phNxpEseProto7816_IntfReset();
    }
    else /* Do R-Sync */
    {
        status = phNxpEseProto7816_RSync();
    }
    return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_Close
 *
 * Description      This function is used to close the 7816 protocol stack instance
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
bool_t phNxpEseProto7816_Close(void)
{
    bool_t status = FALSE;
    if(phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState != PH_NXP_ESE_PROTO_7816_IDLE)
        return status;
    phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState = PH_NXP_ESE_PROTO_7816_DEINIT;
    phNxpEseProto7816_3_Var.recoveryCounter = 0;
    phNxpEseProto7816_3_Var.wtx_counter = 0;
    /* send the end of session s-frame */
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType= SFRAME;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.SframeInfo.sFrameType = PROP_END_APDU_REQ;
    phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_S_EOS;
    status = TransceiveProcess();
    phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState = PH_NXP_ESE_PROTO_7816_IDLE;
    return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_IntfReset
 *
 * Description      This function is used to reset just the current interface
 *
 * Returns          On success return TRUE or else FALSE.
 *
 ******************************************************************************/
bool_t phNxpEseProto7816_IntfReset(void)
{
    bool_t status = FALSE;
    NXPLOG_ESELIB_D("Enter %s ", __FUNCTION__);
    phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState = PH_NXP_ESE_PROTO_7816_TRANSCEIVE;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType= SFRAME;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.SframeInfo.sFrameType = INTF_RESET_REQ;
    phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_S_INTF_RST;
    status = TransceiveProcess();
    if(FALSE == status)
    {
        /* reset all the structures */
        NXPLOG_ESELIB_E("%s TransceiveProcess failed ", __FUNCTION__);
    }
    phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState = PH_NXP_ESE_PROTO_7816_IDLE;
    NXPLOG_ESELIB_D("Exit %s ", __FUNCTION__);
    return status ;
}

/******************************************************************************
 * Function         phNxpEseProto7816_SetIfscSize
 *
 * Description      This function is used to set the max T=1 data send size
 *
 * Returns          Always return TRUE (1).
 *
 ******************************************************************************/
bool_t phNxpEseProto7816_SetIfscSize(uint16_t IFSC_Size)
{
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.maxDataLen = IFSC_Size;
    return TRUE;
}
/** @} */
