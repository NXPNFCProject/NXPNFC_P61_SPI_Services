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
 * TML Implementation.
 */

#include <phTmlEse.h>
#include <phOsalEse_Timer.h>
#include <phNxpLog.h>
#include <phDal4Ese_messageQueueLib.h>
#include <phTmlEse_spi.h>
#include <phNxpSpiHal_utils.h>
#include <phNxpEseHal.h>
#include <phNxpEseProtocol.h>
/*
 * Duration of Timer to wait after sending an Spi packet
 */
#define PHTMLESE_MAXTIME_RETRANSMIT (200U)
#define MAX_WRITE_RETRY_COUNT 0x03
/* Retry Count = Standby Recovery time of ESEC / Retransmission time + 1 */
static uint8_t bCurrentRetryCount = (2000 / PHTMLESE_MAXTIME_RETRANSMIT) + 1;


/* Value to reset variables of TML  */
#define PH_TMLESE_RESET_VALUE               (0x00)

/* Indicates a Initial or offset value */
#define PH_TMLESE_VALUE_ONE                 (0x01)

/* Initialize Context structure pointer used to access context structure */
phTmlEse_Context_t *gpphTmlEse_Context = NULL;

/* Local Function prototypes */
static ESESTATUS phTmlEse_StartThread(void);
static void phTmlEse_CleanUp(void);
static void phTmlEse_ReadDeferredCb(void *pParams);
static void phTmlEse_WriteDeferredCb(void *pParams);
static void phTmlEse_TmlThread(void *pParam);
static void phTmlEse_TmlWriterThread(void *pParam);
static void phTmlEse_ReTxTimerCb(uint32_t dwTimerId, void *pContext);
static ESESTATUS phTmlEse_InitiateTimer(void);

extern phNxpEseP61_Control_t nxpesehal_ctrl;
/* Function definitions */

/*******************************************************************************
**
** Function         phTmlEse_Init
**
** Description      Provides initialization of TML layer and hardware interface
**                  Configures given hardware interface and sends handle to the caller
**
** Parameters       pConfig     - TML configuration details as provided by the upper layer
**
** Returns          ESE status:
**                  ESESTATUS_SUCCESS            - initialization successful
**                  ESESTATUS_INVALID_PARAMETER  - at least one parameter is invalid
**                  ESESTATUS_FAILED             - initialization failed
**                                                 (for example, unable to open hardware interface)
**                  ESESTATUS_INVALID_DEVICE     - device has not been opened or has been disconnected
**
*******************************************************************************/
ESESTATUS phTmlEse_Init(pphTmlEse_Config_t pConfig)
{
    ESESTATUS wInitStatus = ESESTATUS_SUCCESS;

    /* Check if TML layer is already Initialized */
    if (NULL != gpphTmlEse_Context)
    {
        /* TML initialization is already completed */
        wInitStatus =  ESESTATUS_ALREADY_INITIALISED;
    }
    /* Validate Input parameters */
    else if ((NULL == pConfig)  ||
            (PH_TMLESE_RESET_VALUE == pConfig->dwGetMsgThreadId))
    {
        /*Parameters passed to TML init are wrong */
        wInitStatus = ESESTATUS_INVALID_PARAMETER;
    }
    else
    {
        /* Allocate memory for TML context */
        gpphTmlEse_Context = malloc(sizeof(phTmlEse_Context_t));

        if (NULL == gpphTmlEse_Context)
        {
            wInitStatus = ESESTATUS_FAILED;
        }
        else
        {
            /* Initialise all the internal TML variables */
            memset(gpphTmlEse_Context, PH_TMLESE_RESET_VALUE, sizeof(phTmlEse_Context_t));
            /* Make sure that the thread runs once it is created */
            gpphTmlEse_Context->bThreadDone = 1;

            /* Open the device file to which data is read/written */
            wInitStatus = phTmlEse_spi_open_and_configure(pConfig, &(gpphTmlEse_Context->pDevHandle));

            if (ESESTATUS_SUCCESS != wInitStatus)
            {
                wInitStatus = ESESTATUS_INVALID_DEVICE;
                gpphTmlEse_Context->pDevHandle = (void *) ESESTATUS_INVALID_DEVICE;
            }
            else
            {
                gpphTmlEse_Context->tReadInfo.bEnable = 0;
                gpphTmlEse_Context->tWriteInfo.bEnable = 0;
                gpphTmlEse_Context->tReadInfo.bThreadBusy = FALSE;
                gpphTmlEse_Context->tWriteInfo.bThreadBusy = FALSE;

                if(0 != sem_init(&gpphTmlEse_Context->rxSemaphore, 0, 0))
                {
                    wInitStatus = ESESTATUS_FAILED;
                }
                else if(0 != sem_init(&gpphTmlEse_Context->txSemaphore, 0, 0))
                {
                    wInitStatus = ESESTATUS_FAILED;
                }
                else if(0 != sem_init(&gpphTmlEse_Context->postMsgSemaphore, 0, 0))
                {
                    wInitStatus = ESESTATUS_FAILED;
                }
                else
                {
                    sem_post(&gpphTmlEse_Context->postMsgSemaphore);
                    /* Start TML thread (to handle write and read operations) */
                    if (ESESTATUS_SUCCESS != phTmlEse_StartThread())
                    {
                        wInitStatus = ESESTATUS_FAILED;
                    }
                    else
                    {
                        /* Create Timer used for Retransmission of SPI packets */
                        gpphTmlEse_Context->dwTimerId = phOsalEse_Timer_Create();
                        if (PH_OSALESE_TIMER_ID_INVALID != gpphTmlEse_Context->dwTimerId)
                        {
                            /* Store the Thread Identifier to which Message is to be posted */
                            gpphTmlEse_Context->dwCallbackThreadId = pConfig->dwGetMsgThreadId;
                            /* Enable retransmission of Spi packet & set retry count to default */
                            gpphTmlEse_Context->eConfig = phTmlEse_e_DisableRetrans;
                            /** Retry Count = Standby Recovery time of ESEC / Retransmission time + 1 */
                            gpphTmlEse_Context->bRetryCount = (2000 / PHTMLESE_MAXTIME_RETRANSMIT) + 1;
                            gpphTmlEse_Context->bWriteCbInvoked = FALSE;
                        }
                        else
                        {
                            wInitStatus = ESESTATUS_FAILED;
                        }
                    }
                }
            }
        }
    }
    /* Clean up all the TML resources if any error */
    if (ESESTATUS_SUCCESS != wInitStatus)
    {
        /* Clear all handles and memory locations initialized during init */
        phTmlEse_CleanUp();
    }

    return wInitStatus;
}

/*******************************************************************************
**
** Function         phTmlEse_ConfigSpiPktReTx
**
** Description      Provides Enable/Disable Retransmission of SPI packets
**                  Needed in case of Timeout between Transmission and Reception of SPI packets
**                  Retransmission can be enabled only if standby mode is enabled
**
** Parameters       eConfig     - values from phTmlEse_ConfigRetrans_t
**                  bRetryCount - Number of times Spi packets shall be retransmitted (default = 3)
**
** Returns          None
**
*******************************************************************************/
void phTmlEse_ConfigSpiPktReTx(phTmlEse_ConfigRetrans_t eConfiguration, uint8_t bRetryCounter)
{
    /* Enable/Disable Retransmission */

    gpphTmlEse_Context->eConfig = eConfiguration;
    if (phTmlEse_e_EnableRetrans == eConfiguration)
    {
        /* Check whether Retry counter passed is valid */
        if (0 != bRetryCounter)
        {
            gpphTmlEse_Context->bRetryCount = bRetryCounter;
        }
        /* Set retry counter to its default value */
        else
        {
            /** Retry Count = Standby Recovery time of ESEC / Retransmission time + 1 */
            gpphTmlEse_Context->bRetryCount = (2000 / PHTMLESE_MAXTIME_RETRANSMIT) + 1;
        }
    }

    return;
}

/*******************************************************************************
**
** Function         phTmlEse_StartThread
**
** Description      Initializes comport, reader and writer threads
**
** Parameters       None
**
** Returns          ESE status:
**                  ESESTATUS_SUCCESS    - threads initialized successfully
**                  ESESTATUS_FAILED     - initialization failed due to system error
**
*******************************************************************************/
static ESESTATUS phTmlEse_StartThread(void)
{
    ESESTATUS wStartStatus = ESESTATUS_SUCCESS;
    void *h_threadsEvent = 0x00;
    int pthread_create_status = 0;

    /* Create Reader and Writer threads */
    pthread_create_status = pthread_create(&gpphTmlEse_Context->readerThread,NULL,(void *)&phTmlEse_TmlThread,
                                  (void *)h_threadsEvent);
    if(0 != pthread_create_status)
    {
        wStartStatus = ESESTATUS_FAILED;
    }
    else
    {
        /*Start Writer Thread*/
        pthread_create_status = pthread_create(&gpphTmlEse_Context->writerThread,NULL,(void *)&phTmlEse_TmlWriterThread,
                                   (void *)h_threadsEvent);
        if(0 != pthread_create_status)
        {
            wStartStatus = ESESTATUS_FAILED;
        }
    }

    return wStartStatus;
}

/*******************************************************************************
**
** Function         phTmlEse_ReTxTimerCb
**
** Description      This is the timer callback function after timer expiration.
**
** Parameters       dwThreadId  - id of the thread posting message
**                  pContext    - context provided by upper layer
**
** Returns          None
**
*******************************************************************************/
static void phTmlEse_ReTxTimerCb(uint32_t dwTimerId, void *pContext)
{
    if ((gpphTmlEse_Context->dwTimerId == dwTimerId) &&
            (NULL == pContext))
    {
        /* If Retry Count has reached its limit,Retransmit Spi
           packet */
        if (0 == bCurrentRetryCount)
        {
            /* Since the count has reached its limit,return from timer callback
               Upper layer Timeout would have happened */
        }
        else
        {
            bCurrentRetryCount--;
            gpphTmlEse_Context->tWriteInfo.bThreadBusy = TRUE;
            gpphTmlEse_Context->tWriteInfo.bEnable = 1;
        }
        sem_post(&gpphTmlEse_Context->txSemaphore);
    }

    return;
}

/*******************************************************************************
**
** Function         phTmlEse_InitiateTimer
**
** Description      Start a timer for Tx and Rx thread.
**
** Parameters       void
**
** Returns          ESE status
**
*******************************************************************************/
static ESESTATUS phTmlEse_InitiateTimer(void)
{
    ESESTATUS wStatus = ESESTATUS_SUCCESS;

    /* Start Timer once Spi packet is sent */
    wStatus = phOsalEse_Timer_Start(gpphTmlEse_Context->dwTimerId,
            (uint32_t) PHTMLESE_MAXTIME_RETRANSMIT,
            phTmlEse_ReTxTimerCb, NULL);

    return wStatus;
}

/*******************************************************************************
**
** Function         phTmlEse_TmlThread
**
** Description      Read the data from the lower layer driver
**
** Parameters       pParam  - parameters for Writer thread function
**
** Returns          None
**
*******************************************************************************/
static void phTmlEse_TmlThread(void *pParam)
{
    UNUSED(pParam);
    ESESTATUS wStatus = ESESTATUS_SUCCESS;
    int32_t dwNoBytesWrRd = PH_TMLESE_RESET_VALUE;
    uint8_t temp[MAX_DATA_LEN];
    /* Transaction info buffer to be passed to Callback Thread */
    static phTmlEse_TransactInfo_t tTransactionInfo;
    /* Structure containing Tml callback function and parameters to be invoked
       by the callback thread */
    static phLibEse_DeferredCall_t tDeferredInfo;
    /* Initialize Message structure to post message onto Callback Thread */
    static phLibEse_Message_t tMsg;

    NXPLOG_TML_D("P61 - Tml Reader Thread Started................\n");

    /* Writer thread loop shall be running till shutdown is invoked */
    while (gpphTmlEse_Context->bThreadDone)
    {
        /* If Tml write is requested */
        /* Set the variable to success initially */
        wStatus = ESESTATUS_SUCCESS;
        sem_wait(&gpphTmlEse_Context->rxSemaphore);

        /* If Tml read is requested */
        if (1 == gpphTmlEse_Context->tReadInfo.bEnable)
        {
            NXPLOG_TML_D("P61 - Read requested.....\n");
            /* Set the variable to success initially */
            wStatus = ESESTATUS_SUCCESS;

            /* Variable to fetch the actual number of bytes read */
            dwNoBytesWrRd = PH_TMLESE_RESET_VALUE;

            /* Read the data from the file onto the buffer */
            if (ESESTATUS_INVALID_DEVICE != (uintptr_t)gpphTmlEse_Context->pDevHandle)
            {
                NXPLOG_TML_D("P61 - Invoking SPI Read.....\n");
                dwNoBytesWrRd = phTmlEse_spi_read(gpphTmlEse_Context->pDevHandle, temp, MAX_DATA_LEN);

                if (-1 == dwNoBytesWrRd)
                {
                    NXPLOG_TML_E("P61 - Error in SPI Read.....\n");
                    sem_post(&gpphTmlEse_Context->rxSemaphore);
                }
                else
                {
                    memcpy(gpphTmlEse_Context->tReadInfo.pBuffer, temp, dwNoBytesWrRd);

                    NXPLOG_TML_D("P61 - SPI Read successful.....\n");
                    /* This has to be reset only after a successful read */
                    gpphTmlEse_Context->tReadInfo.bEnable = 0;
                    if ((phTmlEse_e_EnableRetrans == gpphTmlEse_Context->eConfig) &&
                            (0x00 != (gpphTmlEse_Context->tReadInfo.pBuffer[0] & 0xE0)))
                    {

                        NXPLOG_TML_D("P61 - Retransmission timer stopped.....\n");
                        /* Stop Timer to prevent Retransmission */
                        uint32_t timerStatus = phOsalEse_Timer_Stop(gpphTmlEse_Context->dwTimerId);
                        if (ESESTATUS_SUCCESS != timerStatus)
                        {
                            NXPLOG_TML_E("P61 - timer stopped returned failure.....\n");
                        }
                        else
                        {
                            gpphTmlEse_Context->bWriteCbInvoked = FALSE;
                        }
                    }
                    /* Update the actual number of bytes read including header */
                    gpphTmlEse_Context->tReadInfo.wLength = (uint16_t) (dwNoBytesWrRd);
                    phNxpSpiHal_print_packet("RECV", gpphTmlEse_Context->tReadInfo.pBuffer,
                            gpphTmlEse_Context->tReadInfo.wLength);

                    dwNoBytesWrRd = PH_TMLESE_RESET_VALUE;

                    /* Fill the Transaction info structure to be passed to Callback Function */
                    tTransactionInfo.wStatus = wStatus;
                    tTransactionInfo.pBuff = gpphTmlEse_Context->tReadInfo.pBuffer;
                    /* Actual number of bytes read is filled in the structure */
                    tTransactionInfo.wLength = gpphTmlEse_Context->tReadInfo.wLength;

                    /* Read operation completed successfully. Post a Message onto Callback Thread*/
                    /* Prepare the message to be posted on User thread */
                    tDeferredInfo.pCallback = &phTmlEse_ReadDeferredCb;
                    tDeferredInfo.pParameter = &tTransactionInfo;
                    tMsg.eMsgType = PH_LIBESE_DEFERREDCALL_MSG;
                    tMsg.pMsgData = &tDeferredInfo;
                    tMsg.Size = sizeof(tDeferredInfo);
                    NXPLOG_TML_D("P61 - Posting read message.....\n");
                    phTmlEse_DeferredCall(gpphTmlEse_Context->dwCallbackThreadId, &tMsg);

                }
            }
            else
            {
                NXPLOG_TML_D("P61 - ESESTATUS_INVALID_DEVICE == gpphTmlEse_Context->pDevHandle");
            }
        }
        else
        {
            NXPLOG_TML_D("P61 - read request NOT enabled");
            usleep(10*1000);
        }
    }/* End of While loop */

    return;
}

/*******************************************************************************
**
** Function         phTmlEse_TmlWriterThread
**
** Description      Writes the requested data onto the lower layer driver
**
** Parameters       pParam  - context provided by upper layer
**
** Returns          None
**
*******************************************************************************/
static void phTmlEse_TmlWriterThread(void *pParam)
{
    UNUSED(pParam);
    ESESTATUS wStatus = ESESTATUS_SUCCESS;
    int32_t dwNoBytesWrRd = PH_TMLESE_RESET_VALUE;
    /* Transaction info buffer to be passed to Callback Thread */
    static phTmlEse_TransactInfo_t tTransactionInfo;
    /* Structure containing Tml callback function and parameters to be invoked
       by the callback thread */
    static phLibEse_DeferredCall_t tDeferredInfo;
    /* Initialize Message structure to post message onto Callback Thread */
    static phLibEse_Message_t tMsg;
    /* In case of SPI Write Retry */
    static uint16_t retry_cnt;

    NXPLOG_TML_D("P61 - Tml Writer Thread Started................\n");

    /* Writer thread loop shall be running till shutdown is invoked */
    while (gpphTmlEse_Context->bThreadDone)
    {
        NXPLOG_TML_D("P61 - Tml Writer Thread Running................\n");
        sem_wait(&gpphTmlEse_Context->txSemaphore);
        /* If Tml write is requested */
        if (1 == gpphTmlEse_Context->tWriteInfo.bEnable)
        {
            NXPLOG_TML_D("P61 - Write requested.....\n");
            /* Set the variable to success initially */
            wStatus = ESESTATUS_SUCCESS;
            if (ESESTATUS_INVALID_DEVICE != (uintptr_t)gpphTmlEse_Context->pDevHandle)
            {
                gpphTmlEse_Context->tWriteInfo.bEnable = 0;
                /* Variable to fetch the actual number of bytes written */
                dwNoBytesWrRd = PH_TMLESE_RESET_VALUE;
                /* Write the data in the buffer onto the file */
                NXPLOG_TML_D("P61 - Invoking SPI Write.....\n");
                dwNoBytesWrRd = phTmlEse_spi_write(gpphTmlEse_Context->pDevHandle,
                        gpphTmlEse_Context->tWriteInfo.pBuffer,
                        gpphTmlEse_Context->tWriteInfo.wLength
                        );

                /* Try SPI Write Five Times, if it fails : Raju */
                if (-1 == dwNoBytesWrRd)
                {
                    NXPLOG_TML_E("P61 - Error in SPI Write.....\n");
                    wStatus = ESESTATUS_FAILED;
                }
                else
                {
                    phNxpSpiHal_print_packet("SEND", gpphTmlEse_Context->tWriteInfo.pBuffer,
                            gpphTmlEse_Context->tWriteInfo.wLength);
                }
                retry_cnt = 0;
                if (ESESTATUS_SUCCESS == wStatus)
                {
                    NXPLOG_TML_D("P61 - SPI Write successful.....\n");
                    dwNoBytesWrRd = PH_TMLESE_VALUE_ONE;
                }
                /* Fill the Transaction info structure to be passed to Callback Function */
                tTransactionInfo.wStatus = wStatus;
                tTransactionInfo.pBuff = gpphTmlEse_Context->tWriteInfo.pBuffer;
                /* Actual number of bytes written is filled in the structure */
                tTransactionInfo.wLength = (uint16_t) dwNoBytesWrRd;

                /* Prepare the message to be posted on the User thread */
                tDeferredInfo.pCallback = &phTmlEse_WriteDeferredCb;
                tDeferredInfo.pParameter = &tTransactionInfo;
                /* Write operation completed successfully. Post a Message onto Callback Thread*/
                tMsg.eMsgType = PH_LIBESE_DEFERREDCALL_MSG;
                tMsg.pMsgData = &tDeferredInfo;
                tMsg.Size = sizeof(tDeferredInfo);

                /* Check whether Retransmission needs to be started,
                 * If yes, Post message only if
                 * case 1. Message is not posted &&
                 * case 11. Write status is success ||
                 * case 12. Last retry of write is also failure
                 */
                if ((phTmlEse_e_EnableRetrans == gpphTmlEse_Context->eConfig) &&
                        (0x00 != (gpphTmlEse_Context->tWriteInfo.pBuffer[0] & 0xE0)))
                {
                    if (FALSE == gpphTmlEse_Context->bWriteCbInvoked)
                    {
                        if ((ESESTATUS_SUCCESS == wStatus) ||
                                (bCurrentRetryCount == 0))
                        {
                                NXPLOG_TML_D("P61 - Posting Write message.....\n");
                                phTmlEse_DeferredCall(gpphTmlEse_Context->dwCallbackThreadId,
                                        &tMsg);
                                gpphTmlEse_Context->bWriteCbInvoked = TRUE;
                        }
                    }
                }
                else
                {
                    NXPLOG_TML_D("P61 - Posting Fresh Write message.....\n");
                    phTmlEse_DeferredCall(gpphTmlEse_Context->dwCallbackThreadId, &tMsg);
                }
            }
            else
            {
                NXPLOG_TML_D("P61 - ESESTATUS_INVALID_DEVICE != gpphTmlEse_Context->pDevHandle");
            }

            /* If Data packet is sent, then NO retransmission */
            if ((phTmlEse_e_EnableRetrans == gpphTmlEse_Context->eConfig) &&
                    (0x00 != (gpphTmlEse_Context->tWriteInfo.pBuffer[0] & 0xE0)))
            {
                NXPLOG_TML_D("P61 - Starting timer for Retransmission case");
                wStatus = phTmlEse_InitiateTimer();
                if (ESESTATUS_SUCCESS != wStatus)
                {
                    /* Reset Variables used for Retransmission */
                    NXPLOG_TML_D("P61 - Retransmission timer initiate failed");
                    gpphTmlEse_Context->tWriteInfo.bEnable = 0;
                    bCurrentRetryCount = 0;
                }
            }
        }
        else
        {
            NXPLOG_TML_D("P61 - Write request NOT enabled");
            usleep(10000);
        }

    }/* End of While loop */

    return;
}

/*******************************************************************************
**
** Function         phTmlEse_CleanUp
**
** Description      Clears all handles opened during TML initialization
**
** Parameters       None
**
** Returns          None
**
*******************************************************************************/
static void phTmlEse_CleanUp(void)
{
    if (NULL != gpphTmlEse_Context->pDevHandle)
    {
        phTmlEse_IoCtl(phTmlEse_e_ResetDevice, 2);
        gpphTmlEse_Context->bThreadDone = 0;
    }
    sem_destroy(&gpphTmlEse_Context->rxSemaphore);
    sem_destroy(&gpphTmlEse_Context->txSemaphore);
    sem_destroy(&gpphTmlEse_Context->postMsgSemaphore);
    phTmlEse_spi_close(gpphTmlEse_Context->pDevHandle);
    gpphTmlEse_Context->pDevHandle = NULL;
    /* Clear memory allocated for storing Context variables */
    free((void *) gpphTmlEse_Context);
    /* Set the pointer to NULL to indicate De-Initialization */
    gpphTmlEse_Context = NULL;

    return;
}

/*******************************************************************************
**
** Function         phTmlEse_Shutdown
**
** Description      Uninitializes TML layer and hardware interface
**
** Parameters       None
**
** Returns          ESE status:
**                  ESESTATUS_SUCCESS            - TML configuration released successfully
**                  ESESTATUS_INVALID_PARAMETER  - at least one parameter is invalid
**                  ESESTATUS_FAILED             - un-initialization failed (example: unable to close interface)
**
*******************************************************************************/
ESESTATUS phTmlEse_Shutdown(void)
{
    ESESTATUS wShutdownStatus = ESESTATUS_SUCCESS;

    /* Check whether TML is Initialized */
    if (NULL != gpphTmlEse_Context)
    {
        /* Reset thread variable to terminate the thread */
        gpphTmlEse_Context->bThreadDone = 0;
        usleep(1000);
        /* Clear All the resources allocated during initialization */
        sem_post(&gpphTmlEse_Context->rxSemaphore);
        usleep(1000);
        sem_post(&gpphTmlEse_Context->txSemaphore);
        usleep(1000);
        sem_post(&gpphTmlEse_Context->postMsgSemaphore);
        usleep(1000);
        sem_post(&gpphTmlEse_Context->postMsgSemaphore);
        usleep(1000);
        if (0 != pthread_join(gpphTmlEse_Context->readerThread, (void**)NULL))
        {
            NXPLOG_TML_E ("Fail to kill reader thread!");
        }
        if (0 != pthread_join(gpphTmlEse_Context->writerThread, (void**)NULL))
        {
            NXPLOG_TML_E ("Fail to kill writer thread!");
        }
        NXPLOG_TML_D ("bThreadDone == 0");

        phTmlEse_CleanUp();
    }
    else
    {
        wShutdownStatus = ESESTATUS_NOT_INITIALISED;
    }

    return wShutdownStatus;
}

/*******************************************************************************
**
** Function         phTmlEse_Write
**
** Description      Asynchronously writes given data block to hardware interface/driver
**                  Enables writer thread if there are no write requests pending
**                  Returns successfully once writer thread completes write operation
**                  Notifies upper layer using callback mechanism
**                  NOTE:
**                  * it is important to post a message with id PH_TMLESE_WRITE_MESSAGE
**                    to IntegrationThread after data has been written to P61
**                  * if CRC needs to be computed, then input buffer should be capable to store
**                    two more bytes apart from length of packet
**
** Parameters       pBuffer              - data to be sent
**                  wLength              - length of data buffer
**                  pTmlWriteComplete    - pointer to the function to be invoked upon completion
**                  pContext             - context provided by upper layer
**
** Returns          ESE status:
**                  ESESTATUS_PENDING             - command is yet to be processed
**                  ESESTATUS_INVALID_PARAMETER   - at least one parameter is invalid
**                  ESESTATUS_BUSY                - write request is already in progress
**
*******************************************************************************/
ESESTATUS phTmlEse_Write(uint8_t *pBuffer, uint16_t wLength, pphTmlEse_TransactCompletionCb_t pTmlWriteComplete, void *pContext)
{
    ESESTATUS wWriteStatus;

    /* Check whether TML is Initialized */

    if (NULL != gpphTmlEse_Context)
    {
        if ((NULL != gpphTmlEse_Context->pDevHandle) && (NULL != pBuffer) &&
                (PH_TMLESE_RESET_VALUE != wLength) && (NULL != pTmlWriteComplete))
        {
            if (!gpphTmlEse_Context->tWriteInfo.bThreadBusy)
            {
                /* Setting the flag marks beginning of a Write Operation */
                gpphTmlEse_Context->tWriteInfo.bThreadBusy = TRUE;
                /* Copy the buffer, length and Callback function,
                   This shall be utilized while invoking the Callback function in thread */
                gpphTmlEse_Context->tWriteInfo.pBuffer = pBuffer;
                gpphTmlEse_Context->tWriteInfo.wLength = wLength;
                gpphTmlEse_Context->tWriteInfo.pThread_Callback = pTmlWriteComplete;
                gpphTmlEse_Context->tWriteInfo.pContext = pContext;

                wWriteStatus = ESESTATUS_PENDING;
                //FIXME: If retry is going on. Stop the retry thread/timer
                if (phTmlEse_e_EnableRetrans == gpphTmlEse_Context->eConfig)
                {
                    /* Set retry count to default value */
                    //FIXME: If the timer expired there, and meanwhile we have created
                    // a new request. The expired timer will think that retry is still
                    // ongoing.
                    bCurrentRetryCount = gpphTmlEse_Context->bRetryCount;
                    gpphTmlEse_Context->bWriteCbInvoked = FALSE;
                }
                /* Set event to invoke Writer Thread */
                gpphTmlEse_Context->tWriteInfo.bEnable = 1;
                sem_post(&gpphTmlEse_Context->txSemaphore);
            }
            else
            {
                wWriteStatus = ESESTATUS_BUSY;
            }
        }
        else
        {
            wWriteStatus = ESESTATUS_INVALID_PARAMETER;
        }
    }
    else
    {
        wWriteStatus = ESESTATUS_NOT_INITIALISED;
    }

    return wWriteStatus;
}

/*******************************************************************************
**
** Function         phTmlEse_Read
**
** Description      Asynchronously reads data from the driver
**                  Number of bytes to be read and buffer are passed by upper layer
**                  Enables reader thread if there are no read requests pending
**                  Returns successfully once read operation is completed
**                  Notifies upper layer using callback mechanism
**
** Parameters       pBuffer              - location to send read data to the upper layer via callback
**                  wLength              - length of read data buffer passed by upper layer
**                  pTmlReadComplete     - pointer to the function to be invoked upon completion of read operation
**                  pContext             - context provided by upper layer
**
** Returns          ESE status:
**                  ESESTATUS_PENDING             - command is yet to be processed
**                  ESESTATUS_INVALID_PARAMETER   - at least one parameter is invalid
**                  ESESTATUS_BUSY                - read request is already in progress
**
*******************************************************************************/
ESESTATUS phTmlEse_Read(uint8_t *pBuffer, uint16_t wLength, pphTmlEse_TransactCompletionCb_t pTmlReadComplete, void *pContext)
{
    ESESTATUS wReadStatus;

    /* Check whether TML is Initialized */
    if (NULL != gpphTmlEse_Context)
    {
        if ((gpphTmlEse_Context->pDevHandle != NULL) && (NULL != pBuffer) &&
                (PH_TMLESE_RESET_VALUE != wLength) && (NULL != pTmlReadComplete))
        {
            if (!gpphTmlEse_Context->tReadInfo.bThreadBusy)
            {
                /* Setting the flag marks beginning of a Read Operation */
                gpphTmlEse_Context->tReadInfo.bThreadBusy = TRUE;
                /* Copy the buffer, length and Callback function,
                   This shall be utilized while invoking the Callback function in thread */
                gpphTmlEse_Context->tReadInfo.pBuffer = pBuffer;
                gpphTmlEse_Context->tReadInfo.wLength = wLength;
                gpphTmlEse_Context->tReadInfo.pThread_Callback = pTmlReadComplete;
                gpphTmlEse_Context->tReadInfo.pContext = pContext;
                wReadStatus = ESESTATUS_PENDING;

                /* Set event to invoke Reader Thread */
                gpphTmlEse_Context->tReadInfo.bEnable = 1;
                sem_post(&gpphTmlEse_Context->rxSemaphore);
            }
            else
            {
                wReadStatus = ESESTATUS_BUSY;
            }
        }
        else
        {
            wReadStatus = ESESTATUS_INVALID_PARAMETER;
        }
    }
    else
    {
        wReadStatus = ESESTATUS_NOT_INITIALISED;
    }

    return wReadStatus;
}

/*******************************************************************************
**
** Function         phTmlEse_ReadAbort
**
** Description      Aborts pending read request (if any)
**
** Parameters       None
**
** Returns          ESE status:
**                  ESESTATUS_SUCCESS                    - ongoing read operation aborted
**                  ESESTATUS_INVALID_PARAMETER          - at least one parameter is invalid
**                  ESESTATUS_NOT_INITIALIZED            - TML layer is not initialized
**                  ESESTATUS_BOARD_COMMUNICATION_ERROR  - unable to cancel read operation
**
*******************************************************************************/
ESESTATUS phTmlEse_ReadAbort(void)
{
    ESESTATUS wStatus = ESESTATUS_INVALID_PARAMETER;
    gpphTmlEse_Context->tReadInfo.bEnable = 0;

    /*Reset the flag to accept another Read Request */
    gpphTmlEse_Context->tReadInfo.bThreadBusy=FALSE;
    wStatus = ESESTATUS_SUCCESS;

    return wStatus;
}

/*******************************************************************************
**
** Function         phTmlEse_WriteAbort
**
** Description      Aborts pending write request (if any)
**
** Parameters       None
**
** Returns          ESE status:
**                  ESESTATUS_SUCCESS                    - ongoing write operation aborted
**                  ESESTATUS_INVALID_PARAMETER          - at least one parameter is invalid
**                  ESESTATUS_NOT_INITIALIZED            - TML layer is not initialized
**                  ESESTATUS_BOARD_COMMUNICATION_ERROR  - unable to cancel write operation
**
*******************************************************************************/
ESESTATUS phTmlEse_WriteAbort(void)
{
    ESESTATUS wStatus = ESESTATUS_INVALID_PARAMETER;

    gpphTmlEse_Context->tWriteInfo.bEnable = 0;
    /* Stop if any retransmission is in progress */
    bCurrentRetryCount = 0;

    /* Reset the flag to accept another Write Request */
    gpphTmlEse_Context->tWriteInfo.bThreadBusy=FALSE;
    wStatus = ESESTATUS_SUCCESS;

    return wStatus;
}

/*******************************************************************************
**
** Function         phTmlEse_IoCtl
**
** Description      Resets device when insisted by upper layer
**                  Number of bytes to be read and buffer are passed by upper layer
**                  Enables reader thread if there are no read requests pending
**                  Returns successfully once read operation is completed
**                  Notifies upper layer using callback mechanism
**
** Parameters       eControlCode       - control code for a specific operation
**
** Returns          ESE status:
**                  ESESTATUS_SUCCESS  - ioctl command completed successfully
**                  ESESTATUS_FAILED   - ioctl command request failed
**
*******************************************************************************/
ESESTATUS phTmlEse_IoCtl(phTmlEse_ControlCode_t eControlCode, long level)
{
    ESESTATUS wStatus = ESESTATUS_SUCCESS;

    if (NULL == gpphTmlEse_Context)
    {
        wStatus = ESESTATUS_FAILED;
    }
    else
    {
        switch (eControlCode)
        {
            case phTmlEse_e_ResetDevice:
                {
                    /*Reset P61*/
                    phTmlEse_spi_ioctl(phTmlEse_e_ResetDevice, gpphTmlEse_Context->pDevHandle, level);
                    usleep(100 * 1000);
                    break;
                }
            case phTmlEse_e_EnableLog:
                {
                    /*Enable logging*/
                    phTmlEse_spi_ioctl(phTmlEse_e_EnableLog, gpphTmlEse_Context->pDevHandle, level);
                    usleep(100 * 1000);
                    break;
                }
            case phTmlEse_e_EnablePollMode:
                {
                    (void)phTmlEse_spi_ioctl(phTmlEse_e_EnablePollMode, gpphTmlEse_Context->pDevHandle, level);
                    usleep(100 * 1000);
                    break;
                }
            default:
                {
                    wStatus = ESESTATUS_INVALID_PARAMETER;
                    break;
                }
        }
    }

    return wStatus;
}

/*******************************************************************************
**
** Function         phTmlEse_DeferredCall
**
** Description      Posts message on upper layer thread
**                  upon successful read or write operation
**
** Parameters       dwThreadId  - id of the thread posting message
**                  ptWorkerMsg - message to be posted
**
** Returns          None
**
*******************************************************************************/
void phTmlEse_DeferredCall(uintptr_t dwThreadId, phLibEse_Message_t *ptWorkerMsg)
{
    int32_t bPostStatus;
    UNUSED(dwThreadId);
    /* Post message on the user thread to invoke the callback function */
    sem_wait(&gpphTmlEse_Context->postMsgSemaphore);
    bPostStatus = phDal4Ese_msgsnd(gpphTmlEse_Context->dwCallbackThreadId,
            ptWorkerMsg,
            0
            );
    sem_post(&gpphTmlEse_Context->postMsgSemaphore);
}

/*******************************************************************************
**
** Function         phTmlEse_ReadDeferredCb
**
** Description      Read thread call back function
**
** Parameters       pParams - context provided by upper layer
**
** Returns          None
**
*******************************************************************************/
static void phTmlEse_ReadDeferredCb(void *pParams)
{
    /* Transaction info buffer to be passed to Callback Function */
    phTmlEse_TransactInfo_t *pTransactionInfo = (phTmlEse_TransactInfo_t *) pParams;

    /* Reset the flag to accept another Read Request */
    gpphTmlEse_Context->tReadInfo.bThreadBusy = FALSE;
    gpphTmlEse_Context->tReadInfo.pThread_Callback(gpphTmlEse_Context->tReadInfo.pContext,
            pTransactionInfo);

    return;
}

/*******************************************************************************
**
** Function         phTmlEse_WriteDeferredCb
**
** Description      Write thread call back function
**
** Parameters       pParams - context provided by upper layer
**
** Returns          None
**
*******************************************************************************/
static void phTmlEse_WriteDeferredCb(void *pParams)
{
    /* Transaction info buffer to be passed to Callback Function */
    phTmlEse_TransactInfo_t *pTransactionInfo = (phTmlEse_TransactInfo_t *) pParams;

    /* Reset the flag to accept another Write Request */
    gpphTmlEse_Context->tWriteInfo.bThreadBusy = FALSE;
    gpphTmlEse_Context->tWriteInfo.pThread_Callback(gpphTmlEse_Context->tWriteInfo.pContext,
            pTransactionInfo);

    return;
}
