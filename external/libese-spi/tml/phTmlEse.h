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
 * Transport Mapping Layer header files containing APIs related to initializing, reading
 * and writing data into files provided by the driver interface.
 *
 * API listed here encompasses Transport Mapping Layer interfaces required to be mapped
 * to different Interfaces and Platforms.
 *
 */

#ifndef PHTMLESE_H
#define PHTMLESE_H

#include <phEseCommon.h>

/*
 * Message posted by Reader thread upon
 * completion of requested operation
 */
#define PH_TMLESE_READ_MESSAGE              (0xAA)

/*
 * Message posted by Writer thread upon
 * completion of requested operation
 */
#define PH_TMLESE_WRITE_MESSAGE             (0x55)

/*
 * Value indicates to reset device
 */
#define PH_TMLESE_RESETDEVICE               (0x00008001)

#define MAX_RETRY_COUNT   3
#define MAX_DATA_LEN      260
/*
***************************Globals,Structure and Enumeration ******************
*/

/*
 * Transaction (Tx/Rx) completion information structure of TML
 *
 * This structure holds the completion callback information of the
 * transaction passed from the TML layer to the Upper layer
 * along with the completion callback.
 *
 * The value of field wStatus can be interpreted as:
 *
 *     - ESESTATUS_SUCCESS                    Transaction performed successfully.
 *     - ESESTATUS_FAILED                     Failed to wait on Read/Write operation.
 *     - ESESTATUS_INSUFFICIENT_STORAGE       Not enough memory to store data in case of read.
 *     - ESESTATUS_BOARD_COMMUNICATION_ERROR  Failure to Read/Write from the file or timeout.
 */

typedef struct phTmlEse_TransactInfo
{
    ESESTATUS           wStatus;    /* Status of the Transaction Completion*/
    uint8_t             *pBuff;     /* Response Data of the Transaction*/
    uint16_t            wLength;    /* Data size of the Transaction*/
}phTmlEse_TransactInfo_t;           /* Instance of Transaction structure */

/*
 * TML transreceive completion callback to Upper Layer
 *
 * pContext - Context provided by upper layer
 * pInfo    - Transaction info. See phTmlEse_TransactInfo
 */
typedef void (*pphTmlEse_TransactCompletionCb_t) (void *pContext, phTmlEse_TransactInfo_t *pInfo);

/*
 * TML Deferred callback interface structure invoked by upper layer
 *
 * This could be used for read/write operations
 *
 * dwMsgPostedThread Message source identifier
 * pParams Parameters for the deferred call processing
 */
typedef  void (*pphTmlEse_DeferFuncPointer_t) (uint32_t dwMsgPostedThread,void *pParams);

/*
 * Enum definition contains  supported ioctl control codes.
 *
 * phTmlEse_IoCtl
 */
typedef enum
{
    phTmlEse_e_Invalid = 0,
    phTmlEse_e_ResetDevice = PH_TMLESE_RESETDEVICE, /* Reset the device */
    phTmlEse_e_EnableLog, /* Enable the spi driver logs */
    phTmlEse_e_EnablePollMode /* Enable the polling for SPI */
} phTmlEse_ControlCode_t ;  /* Control code for IOCTL call */

/*
 * Enable / Disable Re-Transmission of Packets
 *
 * phTmlEse_ConfigSpiPktReTx
 */
typedef enum
{
    phTmlEse_e_EnableRetrans = 0x00, /*Enable retransmission of Spi packet */
    phTmlEse_e_DisableRetrans = 0x01 /*Disable retransmission of Spi packet */
} phTmlEse_ConfigRetrans_t ;  /* Configuration for Retransmission */

/*
 * Structure containing details related to read and write operations
 *
 */
typedef struct phTmlEse_ReadWriteInfo
{
    volatile uint8_t bEnable; /*This flag shall decide whether to perform Write/Read operation */
    uint8_t bThreadBusy; /*Flag to indicate thread is busy on respective operation */
    /* Transaction completion Callback function */
    pphTmlEse_TransactCompletionCb_t pThread_Callback;
    void *pContext; /*Context passed while invocation of operation */
    uint8_t *pBuffer; /*Buffer passed while invocation of operation */
    uint16_t wLength; /*Length of data read/written */
    ESESTATUS wWorkStatus; /*Status of the transaction performed */
} phTmlEse_ReadWriteInfo_t;

/*
 *Base Context Structure containing members required for entire session
 */
typedef struct phTmlEse_Context
{
    pthread_t readerThread; /*Handle to the thread which handles write and read operations */
    pthread_t writerThread;
    volatile uint8_t bThreadDone; /*Flag to decide whether to run or abort the thread */
    phTmlEse_ConfigRetrans_t eConfig; /*Retransmission of Spi Packet during timeout */
    uint8_t bRetryCount; /*Number of times retransmission shall happen */
    uint8_t bWriteCbInvoked; /* Indicates whether write callback is invoked during retransmission */
    uint32_t dwTimerId; /* Timer used to retransmit spi packet */
    phTmlEse_ReadWriteInfo_t tReadInfo; /*Pointer to Reader Thread Structure */
    phTmlEse_ReadWriteInfo_t tWriteInfo; /*Pointer to Writer Thread Structure */
    void *pDevHandle; /* Pointer to Device Handle */
    uintptr_t dwCallbackThreadId; /* Thread ID to which message to be posted */
    uint8_t bEnableCrc; /*Flag to validate/not CRC for input buffer */
    sem_t   rxSemaphore;
    sem_t   txSemaphore; /* Lock/Aquire txRx Semaphore */
    sem_t   postMsgSemaphore; /* Semaphore to post message atomically by Reader & writer thread */
} phTmlEse_Context_t;

/*
 * TML Configuration exposed to upper layer.
 */
typedef struct phTmlEse_Config
{
    /* Port name connected to PN547
     *
     * Platform specific canonical device name to which PN547 is connected.
     *
     * e.g. On Linux based systems this would be /dev/pn547
     */
    int8_t *pDevName;
    /* Callback Thread ID
     *
     * This is the thread ID on which the Reader & Writer thread posts message. */
    uintptr_t dwGetMsgThreadId;
    /* Communication speed between DH and PN547
     *
     * This is the baudrate of the bus for communication between DH and PN547 */
    uint32_t dwBaudRate;
} phTmlEse_Config_t,*pphTmlEse_Config_t;    /* pointer to phTmlEse_Config_t */

/*
 * TML Deferred Callback structure used to invoke Upper layer Callback function.
 */
typedef struct {
    pphTmlEse_DeferFuncPointer_t pDef_call; /*Deferred callback function to be invoked */
    /* Source identifier
     *
     * Identifier of the source which posted the message
     */
    uint32_t dwMsgPostedThread;
    /** Actual Message
     *
     * This is passed as a parameter passed to the deferred callback function pDef_call. */
    void* pParams;
} phTmlEse_DeferMsg_t;                      /* DeferMsg structure passed to User Thread */


/* Function declarations */
ESESTATUS phTmlEse_Init(pphTmlEse_Config_t pConfig);
ESESTATUS phTmlEse_Shutdown(void);
ESESTATUS phTmlEse_Write(uint8_t *pBuffer, uint16_t wLength, pphTmlEse_TransactCompletionCb_t pTmlWriteComplete,  void *pContext);
ESESTATUS phTmlEse_Read(uint8_t *pBuffer, uint16_t wLength, pphTmlEse_TransactCompletionCb_t pTmlReadComplete,  void *pContext);
ESESTATUS phTmlEse_WriteAbort(void);
ESESTATUS phTmlEse_ReadAbort(void);
ESESTATUS phTmlEse_IoCtl(phTmlEse_ControlCode_t eControlCode, long level);
void phTmlEse_DeferredCall(uintptr_t dwThreadId, phLibEse_Message_t *ptWorkerMsg);
void phTmlEse_ConfigSpiPktReTx( phTmlEse_ConfigRetrans_t eConfig, uint8_t bRetryCount);

#endif /*  PHTMLESE_H  */
