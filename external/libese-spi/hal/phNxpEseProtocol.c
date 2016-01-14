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
 * \addtogroup spi_t1_protocol_implementation
 *
 * @{ */
#include <phNxpEseProtocol.h>

#define SPI_RECOVERY_SUPPORTED 1
uint8_t PH_SCAL_T1_MAXLEN = 254;
const uint8_t PH_SCAL_T1_HEADER_LEN = 0x03;
const uint8_t PH_SCAL_T1_CRC_LEN = 0x01;
const uint8_t PH_SCAL_T1_PCB_OFFSET = 0x01;
const uint8_t PH_SCAL_T1_DATA_LEN_OFFSET = 0x02;
uint8_t recv_chained_frame = 0x00;
static uint8_t Rframe_send=0;
static uint8_t CRC_SEQ=0;
extern phNxpEseP61_Control_t nxpesehal_ctrl;
STATIC ESESTATUS phNxpEseP61_SendFrame(uint8_t mode,uint32_t data_len, uint8_t *p_data);
STATIC uint8_t phNxpEseP61_ComputeLRC(unsigned char *p_buff, uint32_t offset,
        uint32_t length);
STATIC ESESTATUS phNxpEseP61_ProcessChainedFrame(uint32_t data_len, uint8_t *p_data);
STATIC ESESTATUS phNxpEseP61_ProcessAck(int32_t data_len, uint8_t *p_data);
STATIC void phNxpEseP61_SendtoUpper(ESESTATUS status, void *data);
STATIC ESESTATUS phNxpEseP61_CheckPCB(uint8_t pcb);
STATIC void phNxpEseP61_DecodePcb(uint8_t pcb, struct PCB_BITS *pcbbits);
STATIC ESESTATUS phNxpEseP61_DecodeStatus(PH_SCAL_T1_FRAME_T frame_type, uint8_t pcb,
        struct PCB_BITS *pcb_bits);
STATIC ESESTATUS phNxpEseP61_CheckLRC(uint32_t data_len, uint8_t *p_data);
STATIC ESESTATUS phNxpEseP61_SendRawFrame(uint32_t data_len, uint8_t *p_data);
STATIC void phNxpEseP61_UnlockAck(ESESTATUS status);
STATIC ESESTATUS phNxpEseP61_WaitForAck(void);
STATIC ESESTATUS phNxpEseP61_InitWaitAck(void);
STATIC void phNxpEseP61_ResetRecovery(void);
STATIC ESESTATUS pnNxpEseP61_sendRFrame(void);
STATIC void pnNxpEseP61_resetCmdRspState(ESESTATUS status);
/**
 * \ingroup spi_t1_protocol_implementation
 * \brief It is used to process the request from JNI. \n
 * Based on the length, it will decide whether to processed as chained frame or as normal frame.
 *
 * \param[in]       uint32_t
 * \param[in]       uint8_t*
 *
 * \retval On Success ESESTATUS_SUCCESS else proper error code
 *
*/

ESESTATUS phNxpEseP61_ProcessData(uint32_t data_len, uint8_t *p_data)
{
    ESESTATUS status = ESESTATUS_FAILED;
    if (data_len > PH_SCAL_T1_MAXLEN)
    {
        NXPLOG_SPIHAL_D("Process Chained Frame");
        status = phNxpEseP61_ProcessChainedFrame(data_len, p_data);

    }
    else
    {
        NXPLOG_SPIHAL_D("Process Single Frame");

        status = phNxpEseP61_SendFrame(PH_SCAL_T1_SINGLE_FRAME, data_len, p_data);
        if (ESESTATUS_SUCCESS == status)
        {
            nxpesehal_ctrl.current_operation = PH_SCAL_I_FRAME;

        }
    }
    return status;

}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function is used to send the frame to TML. \n
 * It calculates the frame length and also updates frame with the sequence number, PCB field & data length etc.
 * \param[in]       uint8_t
 * \param[in]       uint32_t
 * \param[in]       uint8_t*
 *
 * \retval On Success ESESTATUS_SUCCESS else proper error code
 *
*/

STATIC ESESTATUS phNxpEseP61_SendFrame(uint8_t mode, uint32_t data_len, uint8_t *p_data)
{
    ESESTATUS status = ESESTATUS_FAILED;
    ESESTATUS wstatus = ESESTATUS_FAILED;
    uint32_t frame_len = 0;
    uint8_t *p_framebuff = NULL;
    uint8_t pcb_byte = 0;
    Rframe_send = 0;
    if (0 == data_len || NULL == p_data)
    {
        NXPLOG_SPIHAL_E("%s Error in buffer/len", __FUNCTION__);
        return status;
    }

    frame_len = (data_len + PH_SCAL_T1_HEADER_LEN + PH_SCAL_T1_CRC_LEN);

    p_framebuff = malloc(frame_len * sizeof(uint8_t));
    if (NULL == p_framebuff)
    {
        NXPLOG_SPIHAL_E("%s Error allocating memory\n", __FUNCTION__);
        return ESESTATUS_NOT_ENOUGH_MEMORY;
    }

    /* frame the packet */
    p_framebuff[0] = 0x00; /* NAD Byte */

    if (PH_SCAL_T1_CHAINING == mode)
    {
        /* make B6 (M) bit high */
        pcb_byte |= PH_SCAL_T1_CHAINING;
    }

    /* Update the send seq no */
    nxpesehal_ctrl.seq_counter = (nxpesehal_ctrl.seq_counter ^ 1);
    pcb_byte |= (nxpesehal_ctrl.seq_counter << 6);

    /* store the pcb byte */
    p_framebuff[1] = pcb_byte;
    /* store I frame length */
    p_framebuff[2] = data_len;
    /* store I frame */
    memcpy(&(p_framebuff[3]), p_data, data_len);
    p_framebuff[frame_len - 1] = phNxpEseP61_ComputeLRC(p_framebuff, 0,
            (frame_len - 1));
    /* store last frame */
    nxpesehal_ctrl.last_frame_len = frame_len;
    memcpy(nxpesehal_ctrl.p_last_frame, p_framebuff, frame_len);

    status = phNxpEseP61_WriteFrame(frame_len, p_framebuff);
    if (ESESTATUS_SUCCESS != status)
    {
        NXPLOG_SPIHAL_E("%s Error phNxpEseP61_WriteFrame\n", __FUNCTION__);
    }
    else
    {
        wstatus = phNxpEseP61_read();
        if (ESESTATUS_SUCCESS != wstatus)
        {
            NXPLOG_SPIHAL_E("%s phNxpEseP61_read failed \n", __FUNCTION__);
        }
    }
    free(p_framebuff);
    return status;
}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function is used to calculate the LRC (one byte).  \n
 * LRC will be part the epilogue field conveys the error detection code of the block. \n
 * When LRC is used, exclusive-oring of all the bytes of the block from NAD to LRC inclusive shall give '00'. Any other value is invalid.
 *
 * \param[in]       unsigned char*
 * \param[in]       uint32_t
 * \param[in]       uint32_t
 *
 * \retval Calculated LRC (uint8_t)
 *
*/

STATIC uint8_t phNxpEseP61_ComputeLRC(unsigned char *p_buff, uint32_t offset,
        uint32_t length)
{
    uint32_t LRC = 0, i = 0;

    for (i = offset; i < length; i++)
    {
        LRC = LRC ^ p_buff[i];
    }
    return (uint8_t) LRC;
}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function is used to process chained frame. \n
 * It will process the frame in a loop by sending max of 254 bytes at a time.
 *
 * \param[in]       uint32_t
 * \param[in]       uint8_t*
 *
 * \retval On Success ESESTATUS_SUCCESS else proper error code
 *
*/

STATIC ESESTATUS phNxpEseP61_ProcessChainedFrame(uint32_t data_len, uint8_t *p_data)
{
    ESESTATUS status = ESESTATUS_FAILED;
    uint32_t total_length = data_len;
    uint32_t offset = 0;

    NXPLOG_SPIHAL_D("%s Enter data_len %d", __FUNCTION__, data_len);

    do
    {
        nxpesehal_ctrl.current_operation = PH_SCAL_I_CHAINED_FRAME;

        status = phNxpEseP61_InitWaitAck();
        if (ESESTATUS_SUCCESS != status)
        {
            NXPLOG_SPIHAL_E("%s phNxpEseP61_InitWaitAck Failed", __FUNCTION__);
            break;
        }
        status = phNxpEseP61_SendFrame(PH_SCAL_T1_CHAINING,
                PH_SCAL_T1_MAXLEN, (p_data + offset));
        if (ESESTATUS_SUCCESS == status)
        {
            NXPLOG_SPIHAL_E("%s phNxpEseP61_SendFrame Success", __FUNCTION__);
            status = phNxpEseP61_WaitForAck();
            if (ESESTATUS_SUCCESS != status)
            {
                NXPLOG_SPIHAL_E("%s phNxpEseP61_WaitForAck Failed", __FUNCTION__);
                break;
            }
        }
        else
        {
            NXPLOG_SPIHAL_E("%s Error status %x", __FUNCTION__, status);
            break;
        }

        total_length = total_length - PH_SCAL_T1_MAXLEN;
        offset = offset + PH_SCAL_T1_MAXLEN;

    } while (total_length > PH_SCAL_T1_MAXLEN);

    if (ESESTATUS_SUCCESS == status)
    {
        nxpesehal_ctrl.current_operation = PH_SCAL_I_FRAME;
        NXPLOG_SPIHAL_E("%s Sending last frame", __FUNCTION__);
        status = phNxpEseP61_SendFrame(PH_SCAL_T1_SINGLE_FRAME,
                total_length, (p_data + offset));
        if (ESESTATUS_SUCCESS != status)
        {
            NXPLOG_SPIHAL_E("%s Error in last phNxpEseP61_SendFrame", __FUNCTION__);
        }

    }

    return status;
}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function checks the LRC byte of the received frame. \n
 * It is for error detection.
 *
 * \param[in]       uint32_t
 * \param[in]       uint8_t*
 *
 * \retval On Success ESESTATUS_SUCCESS else proper error code \n
 * ESESTATUS_CRC_ERROR ---> if received LRC is different from calculated LRC
 *
*/

STATIC ESESTATUS phNxpEseP61_CheckLRC(uint32_t data_len, uint8_t *p_data)
{
    ESESTATUS status = ESESTATUS_SUCCESS;
    uint8_t calc_crc = 0;
    uint8_t recv_crc = 0;

    recv_crc = p_data[data_len - 1];

    NXPLOG_SPIHAL_D("%s Received CRC   0x%X", __FUNCTION__, recv_crc);
    /* calculate the CRC after excluding CRC  */
    calc_crc = phNxpEseP61_ComputeLRC(p_data, 0, (data_len -1));

    NXPLOG_SPIHAL_D("%s Calculated CRC 0x%X", __FUNCTION__, calc_crc);
    if (recv_crc != calc_crc)
    {
        NXPLOG_SPIHAL_E("%s Error CRC Check", __FUNCTION__);
        status = ESESTATUS_CRC_ERROR;
    }
    return status;
}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function decodes the status byte
 *
 * I-blocks are denoted as follows. \n
 * I(N(S),M)     I-block where N(S) is the send-sequence number and M is
 *               the more-data bit (see 11.6.2.2) \n
 * Na(S),Nb(S)   send-sequence numbers of I-blocks where indices a and b
 *              distinguish sources A and B \n\n
 *
 * R-blocks are denoted as follows. \n
 * R(N(R))       R-block where N(R) is the send-sequence number of the expected I-block \n\n
 *
 * S-blocks are denoted as follows. \n
 * S(RESYNCH request)  S-block requesting a resynchronization. \n
 * S(RESYNCH response) S-block acknowledging the resynchronization. \n
 * S(IFS request)      S-block offering a maximum size of the information field. \n
 * S(IFS response)     S-block acknowledging IFS. \n
 * S(ABORT request)    S-block requesting a chain abortion. \n
 * S(ABORT response)   S-block acknowledging the chain abortion. \n
 * S(WTX request)      S-block requesting a waiting time extension. \n
 * S(WTX response)     S-block acknowledging the waiting time extension.
 *
 * \param[in]       uint32_t
 * \param[in]       uint8_t*
 *
 * \retval On Success ESESTATUS_SUCCESS else proper error code
 *
*/

STATIC ESESTATUS phNxpEseP61_DecodeStatus(PH_SCAL_T1_FRAME_T frame_type, uint8_t pcb,
        struct PCB_BITS *pcb_bits)
{
    ESESTATUS status = ESESTATUS_SUCCESS;
    int32_t error_code = (int32_t)(pcb & 0x3F); /*discard upper 2 bits */

    switch (frame_type)
    {
    case PH_SCAL_I_FRAME:
        NXPLOG_SPIHAL_D("Recv Seq bit  = 0x%x", pcb_bits->bit7);

        NXPLOG_SPIHAL_D("More Data bit = 0x%x", pcb_bits->bit6);
        if (pcb_bits->bit6)
        {
            NXPLOG_SPIHAL_D("Chained Frame - RECV");
            recv_chained_frame = 0x00 | pcb_bits->bit7;
            status = ESESTATUS_MORE_FRAME;
        } else
        {
            NXPLOG_SPIHAL_D("Last Frame ");
            status = ESESTATUS_LAST_FRAME;
        }
#ifdef SPI_RECOVERY_SUPPORTED
        if (ESE_STATUS_RECOVERY == nxpesehal_ctrl.halStatus)
        {
            phNxpEseP61_ResetRecovery();
        }
#endif
        break;

    case PH_SCAL_R_FRAME:
        NXPLOG_SPIHAL_D("Don't care  = 0x%x", pcb_bits->bit6);
        NXPLOG_SPIHAL_D("N(R)  = 0x%x", pcb_bits->bit5);

        nxpesehal_ctrl.recv_seq_counter = 0;
        nxpesehal_ctrl.recv_seq_counter |= pcb_bits->bit5;

        NXPLOG_SPIHAL_D("nxpesehal_ctrl.recv_seq_counter = 0x%x",nxpesehal_ctrl.recv_seq_counter);
        NXPLOG_SPIHAL_D("nxpesehal_ctrl.seq_counter = 0x%x", nxpesehal_ctrl.seq_counter);
        if ((pcb_bits->lsb == 0x00) && (pcb_bits->bit2 == 0x00))
        {
            NXPLOG_SPIHAL_D("%s Received ACK", __FUNCTION__);
            nxpesehal_ctrl.isRFrame = 0;
        }
        else if ((pcb_bits->lsb == 0x01) && (pcb_bits->bit2 == 0x00))
        {
            NXPLOG_SPIHAL_E(" Error : redundancy code error or a character parity = 0x%x", pcb_bits->lsb);
            status = ESESTATUS_PARITY_ERROR;
        }
        else if ((pcb_bits->lsb == 0x00) && (pcb_bits->bit2 == 0x01))
        {
            status = ESESTATUS_CRC_ERROR;
            nxpesehal_ctrl.isRFrame = 0;
        }
        else
        {
            NXPLOG_SPIHAL_E(" Error : Undefined");
            status = ESESTATUS_CRC_ERROR;
            nxpesehal_ctrl.isRFrame = 0;
        }

        if ((nxpesehal_ctrl.recv_seq_counter == nxpesehal_ctrl.seq_counter) || (status == ESESTATUS_PARITY_ERROR))
        {
            if(Rframe_send == 1)
            {
                status=ESESTATUS_CRC_ERROR;
                Rframe_send = 0;
                NXPLOG_SPIHAL_E(" Resend the last R-frame as rec_seq counter is equal to send_seq_counter");
            }
            else
            {
                /*If parity error detected for previous sent R-Frame*/
                if((status == ESESTATUS_PARITY_ERROR) && (nxpesehal_ctrl.isRFrame == 1))
                {
                    break;
                }
                else
                {
                    status = ESESTATUS_FRAME_RESEND;
                    NXPLOG_SPIHAL_E(" Resend the last frame as rec_seq counter is equal to send_seq_counter");
                }
            }
        }
        else if (PH_SCAL_I_CHAINED_FRAME == nxpesehal_ctrl.current_operation
                && PH_SCAL_R_FRAME != nxpesehal_ctrl.last_state
                && nxpesehal_ctrl.recv_seq_counter != nxpesehal_ctrl.seq_counter)
        {
            /* if chained send is going on move to next frame */
                NXPLOG_SPIHAL_D("Send Next Chained frame");
                status = ESESTATUS_SEND_NEXT_FRAME;
        }
        else if (PH_SCAL_R_FRAME == nxpesehal_ctrl.last_state
                && nxpesehal_ctrl.recv_seq_counter != nxpesehal_ctrl.seq_counter)
        {
            NXPLOG_SPIHAL_D("R frame Response received ");
            if (PH_SCAL_I_CHAINED_FRAME == nxpesehal_ctrl.current_operation)
            {
                nxpesehal_ctrl.last_state = PH_SCAL_I_FRAME;
                NXPLOG_SPIHAL_D(" Send Next Chained frame");
                status = ESESTATUS_SEND_NEXT_FRAME;
            }
            else
            {
#ifdef SPI_RECOVERY_SUPPORTED
                if (nxpesehal_ctrl.halStatus == ESE_STATUS_RECOVERY)
                {
                    NXPLOG_SPIHAL_D(" R Frame Received Resent the Last I Frame");
                    status = ESESTATUS_FRAME_RESEND;

                }
                else
                {
                    NXPLOG_SPIHAL_D(" Resend R frame");
                    status = ESESTATUS_FRAME_RESEND_R_FRAME;
                }
#else
                NXPLOG_SPIHAL_D(" Resend R frame");
                status = ESESTATUS_FRAME_RESEND_R_FRAME;
#endif
            }

        }
        else
        {
            NXPLOG_SPIHAL_E("Undefined behaviour");
            if(pcb_bits->bit2 == 0x01 && Rframe_send != 1)
            {
                status=ESESTATUS_FRAME_RESEND;
                NXPLOG_SPIHAL_E("Send the last frame as undefined behaviour with other indicated error");
            }
            else if(Rframe_send == 1)
            {
                status=ESESTATUS_CRC_ERROR;
                NXPLOG_SPIHAL_E("Undefined behaviour CRC ERROR case");
            }
            else
            {
                NXPLOG_SPIHAL_E("Undefined behaviour:ESESTATUS_INVALID_PARAMETER case entered");
                status=ESESTATUS_INVALID_PARAMETER;
            }
        }
#ifdef SPI_RECOVERY_SUPPORTED
        if ((nxpesehal_ctrl.halStatus == ESE_STATUS_RECOVERY) &&
                ((status == ESESTATUS_FRAME_RESEND) || ESESTATUS_SEND_NEXT_FRAME == status))
        {
            NXPLOG_SPIHAL_D(" Recovery is done");
            phNxpEseP61_ResetRecovery();
        }
#endif
        break;

    case PH_SCAL_S_FRAME:
    {
        NXPLOG_SPIHAL_E("%s 0x%x", __FUNCTION__, error_code);
        switch (error_code)
        {
            case RESYNCH_REQ:
                NXPLOG_SPIHAL_E("RESYNCH_REQ");
                status = ESESTATUS_RESYNCH_REQ;
            break;

            case RESYNCH_RES:
                NXPLOG_SPIHAL_E("RESYNCH_RES");
                status = ESESTATUS_RESYNCH_RES;
            break;

            case IFS_REQ:
                NXPLOG_SPIHAL_E("IFS_REQ");
                status = ESESTATUS_IFS_REQ;
            break;

            case IFS_RES:
                NXPLOG_SPIHAL_E("IFS_RES");
                status = ESESTATUS_IFS_RES;
            break;

            case ABORT_REQ:
                NXPLOG_SPIHAL_E("ABORT_REQ");
                status = ESESTATUS_ABORT_REQ;
            break;

            case ABORT_RES:
                NXPLOG_SPIHAL_E("ABORT_RES");
                status = ESESTATUS_ABORT_RES;
            break;

            case WTX_REQ:
                NXPLOG_SPIHAL_E("WTX_REQ");
#ifdef SPI_RECOVERY_SUPPORTED
                if (ESE_STATUS_RECOVERY == nxpesehal_ctrl.halStatus)
                {
                    phNxpEseP61_ResetRecovery();
                }
#endif
                status = ESESTATUS_WTX_REQ;
            break;

            case WTX_RES:
                NXPLOG_SPIHAL_E("WTX_RES");
                status = ESESTATUS_WTX_RES;
            break;

            default:
                NXPLOG_SPIHAL_E("ERROR Undefined");
                status = ESESTATUS_UNKNOWN_ERROR;
            break;
        }
    }

    break;

    default:
        NXPLOG_SPIHAL_E("%s Wrong Frame ", __FUNCTION__);
        status = ESESTATUS_INVALID_PARAMETER;
        break;
    }
    return status;
}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function decodes PCB byte in bit-field. \n
 * PCB is used in identifying the frame type.
 *
 * \param[in]       uint8_t
 * \param[in]       struct PCB_BITS*
 *
 * \retval void
 *
*/

STATIC void phNxpEseP61_DecodePcb(uint8_t pcb, struct PCB_BITS *pcbbits)
{
    memcpy(pcbbits, &pcb, sizeof(uint8_t));
}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief It used to find out the frame type based on PCB field
 *
 * \param[in]       uint8_t
 *
 * \retval On Success ESESTATUS_SUCCESS else proper error code
 *
*/

STATIC ESESTATUS phNxpEseP61_CheckPCB(uint8_t pcb)
{
    ESESTATUS status = ESESTATUS_SUCCESS;
    PH_SCAL_T1_FRAME_T frame_type;
    struct PCB_BITS pcb_bits;

    /*An information block (I-block) is used to convey information for use by the application layer.
     *  In addition, it conveys a positive or negative acknowledgment.*/

    /*A receive ready block (R-block) is used to convey a positive or negative acknowledgment.
     *  Its information field shall be absent.*/

    /*A supervisory block (S-block) is used to exchange control information between the
     * interface device and the card. Its information field may be present depending on
     * its controlling function.*/

    /*
     * Frame Type       MSB     B7      B6      B5  B4  B3  B2  B1
     *
     * I :              0       N(S)    M bit   (Future use)
     *
     * R :              1       0
     */

    /*Below function will store MSB in pcb_buff[1] and B7 in pcb_buff[0] */
    memset(&pcb_bits, 0x00, sizeof(struct PCB_BITS));
    NXPLOG_SPIHAL_D("PCB Byte = 0x%X", pcb);
    phNxpEseP61_DecodePcb(pcb, &pcb_bits);

    if (0x00 == pcb_bits.msb)
    {
        NXPLOG_SPIHAL_D("%s I-Frame Received", __FUNCTION__);
        frame_type = PH_SCAL_I_FRAME;
        nxpesehal_ctrl.isRFrame = 0;
    }
    else if ((0x01 == pcb_bits.msb) && (0x00 == pcb_bits.bit7))
    {
        NXPLOG_SPIHAL_D("%s R-Frame Received", __FUNCTION__);
        frame_type = PH_SCAL_R_FRAME;
    }
    else if ((0x01 == pcb_bits.msb) && (0x01 == pcb_bits.bit7))
    {
        NXPLOG_SPIHAL_D("%s S-Frame Received", __FUNCTION__);
        frame_type = PH_SCAL_S_FRAME;
        nxpesehal_ctrl.isRFrame = 0;
    }
    else
    {
        NXPLOG_SPIHAL_E("%s Wrong-Frame Received", __FUNCTION__);
        status = ESESTATUS_INVALID_PARAMETER;
    }
    if (ESESTATUS_SUCCESS == status)
    {
        status = phNxpEseP61_DecodeStatus(frame_type, pcb, &pcb_bits);
    }
    return status;
}
/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function invokes the registered JNI callback with status and data
 *
 * \param[in]       ESESTATUS
 * \param[in]       void*
 *
 * \retval void
 *
*/

STATIC void phNxpEseP61_SendtoUpper(ESESTATUS status, void *data)
{

    NXPLOG_SPIHAL_E("%s status 0x%x", __FUNCTION__, status);
    nxpesehal_ctrl.halStatus = ESE_STATUS_IDLE;
    nxpesehal_ctrl.recovery_counter = 0;
    (nxpesehal_ctrl.p_ese_stack_data_cback)(status, data);

}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function processes the frame received from TML
 *
 * \param[in]       uint32_t
 * \param[in]       uint8_t*
 *
 * \retval void
 *
*/

void phNxpEseP61_ProcessResponse(uint32_t data_len, uint8_t *p_data)
{
    ESESTATUS status;
    static int retry_cnt = 0;
    memset(&nxpesehal_ctrl.p_rsp_data[0], 0x00,
            sizeof(nxpesehal_ctrl.p_rsp_data));

    nxpesehal_ctrl.rsp_len = data_len;
    memcpy(&nxpesehal_ctrl.p_rsp_data[0], p_data, data_len);

    /* Make NAD byte zero*/
    nxpesehal_ctrl.p_rsp_data[0] = 0x00;

    status = phNxpEseP61_ProcessAck(nxpesehal_ctrl.rsp_len, &nxpesehal_ctrl.p_rsp_data[0]);

    if (ESESTATUS_FRAME_RESEND == status || ESESTATUS_RESET_SEQ_COUNTER_FRAME_RESEND == status)
    {
        retry_cnt ++;
        if(retry_cnt > 2)
        {
            NXPLOG_SPIHAL_E("%s Bad SMX State \n", __FUNCTION__);
            retry_cnt = 0;
            phNxpEseP61_Action(ESESTATUS_ABORTED, 0, NULL);
        }
        else
        {
            if (ESESTATUS_RESET_SEQ_COUNTER_FRAME_RESEND == status)
            {
                /* reset the send seq counter */
                nxpesehal_ctrl.p_last_frame[1] &= 0xBF;
                nxpesehal_ctrl.seq_counter = 0x01;
            }
            nxpesehal_ctrl.isRFrame = 0;
            NXPLOG_SPIHAL_E("%s Resending frame count %d\n", __FUNCTION__, retry_cnt);
            phNxpEseP61_SendRawFrame(nxpesehal_ctrl.last_frame_len, nxpesehal_ctrl.p_last_frame);
        }
    }
    else
    {
        retry_cnt = 0;
    }

}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief It takes appropriate action based on the error. \n
 * Like for ESESTATUS_RESPONSE_TIMEOUT ----> Send/Resend R-Frame    \n
 *          ESESTATUS_WRITE_TIMEOUT ---> Chained Frame Ack timeout leads to recovery (abort) \n
 *          ESESTATUS_INVALID_PARAMETER ----> Frame Resend  \n
 *          ESESTATUS_CRC_ERROR/ESESTATUS_INVALID_PARAMETER ---> Frame Resend \n
 *          ESESTATUS_MORE_FRAME ----> set chain flag to TRUE   \n
 * ......
 *
 * \param[in]       ESESTATUS
 * \param[in]       uint32_t
 * \param[in]       uint8_t *
 *
 * \retval On Success ESESTATUS_SUCCESS else proper error code
 *
*/

ESESTATUS phNxpEseP61_Action(ESESTATUS action_evt, uint32_t data_len, uint8_t *p_data)
{
    ESESTATUS status = ESESTATUS_FAILED;
    STATIC bool_t chained_flag = FALSE;
    phNxpEseP61_data pRes;
    bool_t wtx_flag = FALSE;
    uint8_t recv_ack[4] = {0x00, 0x80, 0x00, 0x00};

    NXPLOG_SPIHAL_D("%s - Enter action 0x%X - data_len %d !!!", __FUNCTION__, action_evt, data_len);
    if (nxpesehal_ctrl.recovery_counter > 3)
    {
        NXPLOG_SPIHAL_D("%s Recovery counter reached max (abort)", __FUNCTION__);
        if (PH_SCAL_I_CHAINED_FRAME == nxpesehal_ctrl.current_operation)
        {
            NXPLOG_SPIHAL_D("%s Chained Frame Ack timeout", __FUNCTION__);
            phNxpEseP61_UnlockAck(ESESTATUS_RESPONSE_TIMEOUT);
        }
        status = ESESTATUS_ABORTED;
    }
#if(NFC_NXP_ESE_VER == JCOP_VER_3_2)
        if(nxpesehal_ctrl.wtx_counter_value != 0)
        {
           wtx_flag = TRUE;
        }
#endif
    if(wtx_flag)
    {
        if(action_evt != ESESTATUS_WTX_REQ)
        {
            nxpesehal_ctrl.wtx_counter = 0;
        }
        else
        {
            nxpesehal_ctrl.wtx_counter++;
            NXPLOG_SPIHAL_D("wtx count reached  [%lu]",nxpesehal_ctrl.wtx_counter);
        }
    }
    switch(action_evt)
    {
    case ESESTATUS_RESPONSE_TIMEOUT:
    case ESESTATUS_FRAME_RESEND_R_FRAME:
#ifdef SPI_RECOVERY_SUPPORTED
        status = ESESTATUS_REVOCERY_STARTED;
        recv_ack[1] = 0x82;
        if(ESESTATUS_RESPONSE_TIMEOUT == action_evt)
        {
            recv_ack[1] |= (((nxpesehal_ctrl.seq_counter) << 4));
        }
        else
        {
            recv_ack[1] |= (((nxpesehal_ctrl.recv_seq_counter) << 4));
        }
        if (nxpesehal_ctrl.recovery_counter < 3)
        {
            nxpesehal_ctrl.recovery_counter++;
            recv_ack[3] = phNxpEseP61_ComputeLRC(recv_ack, 0x00, (sizeof(recv_ack) -1));
            nxpesehal_ctrl.last_state = PH_SCAL_R_FRAME;
            nxpesehal_ctrl.seq_counter =(0x01 & (recv_ack[1] >> 4) );
            NXPLOG_SPIHAL_D("Send sequence num for RESPONSE_TIMEOUT or FRAME_RESEND_R_FRAME is [0x%x] \n",nxpesehal_ctrl.seq_counter);
            phNxpEseP61_SendRawFrame(sizeof(recv_ack), recv_ack);
        }
        else
        {
            NXPLOG_SPIHAL_E("Recovery is not possible \n");
            status = ESESTATUS_ABORTED;
        }
#else
        NXPLOG_SPIHAL_E("Recovery is not possible \n");
        status = ESESTATUS_ABORTED;
#endif
        break;

    case ESESTATUS_WRITE_TIMEOUT:
        if (PH_SCAL_I_CHAINED_FRAME == nxpesehal_ctrl.current_operation)
        {
            NXPLOG_SPIHAL_D("%s Chained Frame Ack timeout", __FUNCTION__);
            phNxpEseP61_UnlockAck(ESESTATUS_RESPONSE_TIMEOUT);
        }
        status = ESESTATUS_ABORTED;
        break;

    case ESESTATUS_FRAME_RESEND_RNAK:
    case ESESTATUS_INVALID_PARAMETER: /* if received wrong pcb type*/
    case ESESTATUS_CRC_ERROR:
        /* CRC byte is not equal to received */
        status = ESESTATUS_CRC_ERROR;
        nxpesehal_ctrl.halStatus = ESESTATUS_FRAME_RESEND_RNAK;

        uint8_t recv_RNAK[4] = {0x00, 0x82, 0x00, 0x00};
        if(ESESTATUS_FRAME_RESEND_RNAK == action_evt)
        {
            recv_RNAK[1] |= (((!(nxpesehal_ctrl.recv_seq_counter)) << 4));
        }
        else if(ESESTATUS_INVALID_PARAMETER == status)
        {
            recv_RNAK[1] |= (((nxpesehal_ctrl.seq_counter) << 4));
        }
        else
        {
            recv_RNAK[1] |= (((CRC_SEQ) << 4));
            Rframe_send = 1;
            NXPLOG_SPIHAL_D("CRC_SEQ value is [0x%x] ",CRC_SEQ);
        }
        NXPLOG_SPIHAL_D("%s Send R Frame  0x%x ", __FUNCTION__, recv_RNAK[1]);
        recv_RNAK[3] = phNxpEseP61_ComputeLRC(recv_RNAK, 0x00, (sizeof(recv_RNAK) -1));
        nxpesehal_ctrl.seq_counter =(0x01 & (recv_RNAK[1] >> 4) );
        NXPLOG_SPIHAL_D("sequence counter for send R Frame  0x%x ",nxpesehal_ctrl.seq_counter );
        phNxpEseP61_SendRawFrame(sizeof(recv_RNAK), recv_RNAK);
        break;

    case ESESTATUS_MORE_FRAME:

        chained_flag = TRUE;
        /* acknowledge the frame and save data in linked list
         *
         */
        if (ESESTATUS_SUCCESS != phNxpEseP61_StoreDatainList(data_len, p_data))
        {
            NXPLOG_SPIHAL_E("%s - Error storing chained data in list", __FUNCTION__);
        }
        else
        {
            status = pnNxpEseP61_sendRFrame();
        }

        break;
    case ESESTATUS_LAST_FRAME:
        if (TRUE == chained_flag)
        {
            if (data_len > 0)
            {
                if (ESESTATUS_SUCCESS != phNxpEseP61_StoreDatainList(data_len, p_data))
                {
                    NXPLOG_SPIHAL_E("%s - Error storing chained data in list", __FUNCTION__);
                }

                else
                {

                    status = ESESTATUS_SUCCESS;
                }
            }
        }
        else
        {
            status = ESESTATUS_SUCCESS;
        }
        break;

    case ESESTATUS_FRAME_RESEND:
        status = ESESTATUS_FRAME_RESEND;
        break;
    case ESESTATUS_SEND_NEXT_FRAME:
        if (PH_SCAL_I_CHAINED_FRAME == nxpesehal_ctrl.current_operation)
        {
            NXPLOG_SPIHAL_D("%s Chained Frame Ack", __FUNCTION__);
            phNxpEseP61_UnlockAck(ESESTATUS_SUCCESS);
            status = ESESTATUS_SEND_NEXT_FRAME;
        }
        break;
    case ESESTATUS_PARITY_ERROR:
        if(nxpesehal_ctrl.isRFrame == 1)
        {
            status = pnNxpEseP61_sendRFrame();
        }
        else
        {
            status = ESESTATUS_FRAME_RESEND;
        }

        break;

    case ESESTATUS_FAILED:
        break;

    case ESESTATUS_UNKNOWN_ERROR:
        status =  ESESTATUS_ABORTED;
        break;

    case ESESTATUS_RESYNCH_REQ:
        status = ESESTATUS_FAILED;
        break;

    case ESESTATUS_RESYNCH_RES:
        status = ESESTATUS_FAILED;
        break;

    case ESESTATUS_IFS_REQ:
        status = ESESTATUS_IFS_REQ;
        break;

    case ESESTATUS_IFS_RES:
        break;

    case ESESTATUS_ABORT_REQ:
    case ESESTATUS_ABORT_RES:
        status = ESESTATUS_ABORTED;
        break;

    case ESESTATUS_WTX_REQ:
        status = ESESTATUS_WTX_REQ;
        break;

    case ESESTATUS_WTX_RES:
        status = ESESTATUS_WTX_RES;
        break;

    default:
        status = ESESTATUS_FRAME_RESEND_RNAK;
        NXPLOG_SPIHAL_E("%s Undefined Error code !!!", __FUNCTION__);
        break;
    }

    if ((ESESTATUS_FAILED == status) || (ESESTATUS_ABORTED == status))
    {
        NXPLOG_SPIHAL_D("%s Error Resoponse", __FUNCTION__);
        /*Unblock if any pending reset wait for reset to complete*/
        pnNxpEseP61_resetCmdRspState(status);
        phNxpEseP61_SendtoUpper(status, NULL);
        status = ESESTATUS_FAILED;
    }
    else if (TRUE == chained_flag && ESESTATUS_SUCCESS == status)
    {
        NXPLOG_SPIHAL_D("%s Chained Frame Resoponse", __FUNCTION__);
        status = phNxpEseP61_GetData(&pRes.len, &pRes.p_data);
        if (ESESTATUS_SUCCESS == status)
        {
            NXPLOG_SPIHAL_D("%s DataLen = %d", __FUNCTION__, pRes.len);
            /*Unblock if any pending reset wait for reset to complete*/
            pnNxpEseP61_resetCmdRspState(status);
            phNxpEseP61_SendtoUpper(status, &pRes);
            free(pRes.p_data);
        }
        else
        {
            NXPLOG_SPIHAL_E("%s phNxpEseP61_GetData failed 0x%X", __FUNCTION__, status);
            /*Unblock if any pending reset wait for reset to complete*/
            pnNxpEseP61_resetCmdRspState(status);
            phNxpEseP61_SendtoUpper(ESESTATUS_FAILED, NULL);
        }
        chained_flag = FALSE;

    }
    else if (FALSE == chained_flag && ESESTATUS_SUCCESS == status)
    {
        NXPLOG_SPIHAL_D("%s Single Frame Resoponse", __FUNCTION__);
        pRes.len = data_len;
        pRes.p_data = p_data;
        /*Unblock if any pending reset wait for reset to complete*/
        pnNxpEseP61_resetCmdRspState(status);
        phNxpEseP61_SendtoUpper(status, &pRes);

    }
    else if (ESESTATUS_WTX_REQ == status)
    {
        uint8_t wtx_frame[] = {0x00, 0xE3, 0x01, 0x01, 0xE3};
        NXPLOG_SPIHAL_D("%s WTX Request", __FUNCTION__);
        /* Trigger SPI service to re-set timer */
        if(wtx_flag)
        {
            if(nxpesehal_ctrl.wtx_counter == nxpesehal_ctrl.wtx_counter_value)
            {
                phNxpEseP61_SPM_DisablePwr();
                NXPLOG_SPIHAL_D(" Disable power to P61 wtx count reached!!!");
                status = phNxpEseP61_SendRawFrame(sizeof(wtx_frame), wtx_frame);
                return status;
            }
        }
        status = phNxpEseP61_SendRawFrame(sizeof(wtx_frame), wtx_frame);
        if (ESESTATUS_SUCCESS == status)
        {
            status = ESESTATUS_WTX_REQ;
        }
        phNxpEseP61_SendtoUpper(status, NULL);
    }
#if 0
    else if (ESESTATUS_WTX_REQ == status)
    {
        uint8_t ifs_frame[] = {0x00, 0xE1, 0x01, 0x01, 0xE1};
        NXPLOG_SPIHAL_D("%s IFS Request", __FUNCTION__);
        status = phNxpEseP61_SendRawFrame(sizeof(ifs_frame), ifs_frame);
    }
#endif
    NXPLOG_SPIHAL_D("%s - Exit status 0x%x !!!", __FUNCTION__, status);
    return status;
}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function checks the CRC and PCB byte
 *
 * \param[in]       int32_t
 * \param[in]       uint8_t*
 *
 * \retval On Success ESESTATUS_SUCCESS else proper error code
 *
*/

STATIC ESESTATUS phNxpEseP61_ProcessAck(int32_t data_len, uint8_t *p_data)
{
    ESESTATUS status = ESESTATUS_FAILED;

    status = phNxpEseP61_CheckLRC(data_len, p_data);
    nxpesehal_ctrl.recv_seq_counter = (0x01 & (p_data[1] >>  6));
    NXPLOG_SPIHAL_E("test recieve seq counter I-frame [0x%x]",nxpesehal_ctrl.recv_seq_counter);
    if (ESESTATUS_SUCCESS != status)
    {
        NXPLOG_SPIHAL_E("%s ERROR in CRC", __FUNCTION__);
        CRC_SEQ = (0x01 & (p_data[1] >>  6));
        goto clean_and_return;
    }
    status = phNxpEseP61_CheckPCB(p_data[PH_SCAL_T1_PCB_OFFSET]);

    clean_and_return:
    /* extract the first 3 byte and last CRC byte */
    status = phNxpEseP61_Action(status, (data_len - 4), &p_data[3]);

    return status;
}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function is used to send WTX to P61. \n
 * If the card requires more than BWT (block waiting time) to process the previously received I-block, it transmits S(WTX request)
 * where INF (information field) conveys one byte encoding an integer multiplier of the BWT value. The interface device
 * shall acknowledge by S(WTX response) with the same INF.
 *
 * \param[in]       uint32_t
 * \param[in]       uint8_t*
 *
 * \retval On Success ESESTATUS_SUCCESS else proper error code
 *
*/

STATIC ESESTATUS phNxpEseP61_SendRawFrame(uint32_t data_len, uint8_t *p_data)
{
    ESESTATUS status = ESESTATUS_FAILED;

    status = phNxpEseP61_InternalWriteFrame(data_len, p_data);
    if (ESESTATUS_SUCCESS == status)
    {

    }

    return status;
}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief It initializes the semaphore
 *
 * \param[in]       void
 *
 * \retval On Success ESESTATUS_SUCCESS else proper error code
 *
*/

STATIC ESESTATUS phNxpEseP61_InitWaitAck(void)
{
    ESESTATUS status = ESESTATUS_FAILED;
    /* Create the local semaphore */
    status = phNxpSpiHal_init_cb_data(&nxpesehal_ctrl.ack_cb_data, NULL);
    if (status!= ESESTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_D("%s Create ext_cb_data failed", __FUNCTION__);

    }
    return status;
}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief It waits for the response from P61 till timeout that is BTW (block waiting time).
 *
 * \param[in]       void
 *
 * \retval On Success ESESTATUS_SUCCESS else proper error code
 *
*/

STATIC ESESTATUS phNxpEseP61_WaitForAck(void)
{
    ESESTATUS status = ESESTATUS_FAILED;

    nxpesehal_ctrl.status_code = ESESTATUS_SUCCESS;

    NXPLOG_SPIHAL_D("%s Enter ", __FUNCTION__);

    /* Wait for ack */
    NXPLOG_SPIHAL_D("%s Waiting after chained frame sent ", __FUNCTION__);
    if (SEM_WAIT(nxpesehal_ctrl.ack_cb_data))
    {
        NXPLOG_SPIHAL_E("p_hal_ext->ext_cb_data.sem semaphore error");
        status = ESESTATUS_FAILED;
        goto clean_and_return;
    }
    status = nxpesehal_ctrl.status_code;

clean_and_return:
    phNxpSpiHal_cleanup_cb_data(&nxpesehal_ctrl.ack_cb_data);
    NXPLOG_SPIHAL_D("%s Exit  status 0x%x", __FUNCTION__, status);
    return status;
}
/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function processes the frame received from TML
 *
 * \param[in]       void
 *
 * \retval void
 *
*/

STATIC void phNxpEseP61_UnlockAck(ESESTATUS status)
{
    nxpesehal_ctrl.status_code = status;

    NXPLOG_SPIHAL_D("%s enter status 0x%x", __FUNCTION__, status);

    /*Release the Semaphore to send next packet*/
    SEM_POST(&nxpesehal_ctrl.ack_cb_data);
    NXPLOG_SPIHAL_D("%s exit ", __FUNCTION__);
}
/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function resets the all recovery related flags like counter & halstatus (Open/Busy/Recovery/Idle/Close).
 *
 * \param[in]       void
 *
 * \retval void
 *
*/

STATIC void phNxpEseP61_ResetRecovery(void)
{
    NXPLOG_SPIHAL_D("%s enter ", __FUNCTION__);
    nxpesehal_ctrl.halStatus = ESE_STATUS_BUSY;
    nxpesehal_ctrl.recovery_counter = 0;
    NXPLOG_SPIHAL_D("%s exit ", __FUNCTION__);
}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function sets the IFSC size to 240/254 support JCOP OS Update.
 *
 * \param[in]       void
 *
 * \retval ESESTATUS_SUCCESS on sucess or Error code for failure.
 *
*/
ESESTATUS phNxpEseP61_setIfsc(uint16_t IFSC_Size)
{
    /*SET the IFSC size to 240 bytes*/
    PH_SCAL_T1_MAXLEN = IFSC_Size;
    return ESESTATUS_SUCCESS;
}
/** @} */


/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function sends R-Frame in case of chained response or error
 * recovery cases.
 *
 * \param[in]       void
 *
 * \retval ESESTATUS_SUCCESS on sucess or Error code for failure.
 *
*/
STATIC ESESTATUS pnNxpEseP61_sendRFrame(void)
{
    ESESTATUS status = ESESTATUS_FAILED;
    uint8_t recv_ack[4]= {0x00,0x80,0x00,0x00};

    recv_ack[1] |= ((!recv_chained_frame) << 4);
    nxpesehal_ctrl.seq_counter =(0x01 & (recv_ack[1] >> 4) );
    NXPLOG_SPIHAL_D("%s Send ACK for chained frame 0x%x seq num [0x%x]", __FUNCTION__, recv_ack[1],nxpesehal_ctrl.seq_counter);

    recv_ack[3] = phNxpEseP61_ComputeLRC(recv_ack, 0x00, (sizeof(recv_ack) -1));
    status = phNxpEseP61_SendRawFrame(sizeof(recv_ack), recv_ack);

    if (ESESTATUS_SUCCESS == status)
    {
        status = ESESTATUS_MORE_FRAME;
        nxpesehal_ctrl.isRFrame = 1;
    }
    return status;
}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function initialises semaphore
 *
 * \param[in]       Semaphore pointer
 *
 * \retval ESESTATUS_SUCCESS on sucess or Error code for failure.
 *
*/
ESESTATUS phNxpEseP61_SemInit(phNxpSpiHal_Sem_t *event_ack)
{
    ESESTATUS status = ESESTATUS_FAILED;
    /* Create the local semaphore */
    status = phNxpSpiHal_init_cb_data(event_ack, NULL);
    if (status!= ESESTATUS_SUCCESS)
    {
        NXPLOG_SPIHAL_D("%s Create ext_cb_data failed", __FUNCTION__);

    }
    return status;
}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function waits for the initialised semaphore
 *
 * \param[in]       Semaphore pointer
 *
 * \retval ESESTATUS_SUCCESS on sucess or Error code for failure.
 *
*/
ESESTATUS phNxpEseP61_SemWait(phNxpSpiHal_Sem_t *event_ack)
{
    ESESTATUS status = ESESTATUS_FAILED;
    NXPLOG_SPIHAL_D("%s Enter ", __FUNCTION__);

    nxpesehal_ctrl.status_code = ESESTATUS_SUCCESS;

    NXPLOG_SPIHAL_D("%s start waiting for resp/reset", __FUNCTION__);

    if(SEM_WAIT((*event_ack)))
    {
        NXPLOG_SPIHAL_E("p_hal_ext->ext_cb_data.sem semaphore error");
        status = ESESTATUS_FAILED;
        goto clean_and_return;
    }
    status = nxpesehal_ctrl.status_code;

clean_and_return:
    phNxpSpiHal_cleanup_cb_data(event_ack);
    NXPLOG_SPIHAL_D("%s Exit  status 0x%x", __FUNCTION__, status);
    return status;
}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function posts the waiting semaphore to unblock
 *
 * \param[in]       Semaphore pointer, ESESTATUS
 *
 * \retval ESESTATUS_SUCCESS on sucess or Error code for failure.
 *
*/
void phNxpEseP61_SemUnlock(phNxpSpiHal_Sem_t *event_ack, ESESTATUS status)
{
    nxpesehal_ctrl.status_code = status;

    NXPLOG_SPIHAL_D("%s enter status 0x%x", __FUNCTION__, status);

    /*Release the Semaphore to send next packet*/
    SEM_POST(event_ack);
    NXPLOG_SPIHAL_D("%s exit ", __FUNCTION__);
}

/**
 * \ingroup spi_t1_protocol_implementation
 * \brief This function resets the TRANS STATE when response is received
 * and waits on reset action semaphore.
 *
 * \param[in]       status
 *
 * \retval ESESTATUS_SUCCESS on sucess or Error code for failure.
 *
*/
void pnNxpEseP61_resetCmdRspState(ESESTATUS status)
{
    if(nxpesehal_ctrl.cmd_rsp_state == STATE_RESET_BLOCKED)
    {
        NXPLOG_SPIHAL_D("%s enter state = STATE_RESET_BLOCKED", __FUNCTION__);
        /*unblock pending reset operation*/
        phNxpEseP61_SemUnlock(&nxpesehal_ctrl.cmd_rsp_ack, status);
        /*wait for pending reset event to complete before sending it to upper layer*/
        phNxpEseP61_SemInit(&nxpesehal_ctrl.reset_ack);
        phNxpEseP61_SemWait(&nxpesehal_ctrl.reset_ack);
        NXPLOG_SPIHAL_D("%s release blocked transceive", __FUNCTION__);
    }
    nxpesehal_ctrl.cmd_rsp_state = STATE_IDLE;
    NXPLOG_SPIHAL_D("%s state = STATE_IDLE", __FUNCTION__);
}
