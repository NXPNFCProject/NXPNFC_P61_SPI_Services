/*
 * Copyright (C) 2010 The Android Open Source Project
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
 *
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright (C) 2013-2014 NXP Semiconductors
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include <phNxpSpiHal_utils.h>
#include <errno.h>
#include <phNxpLog.h>

/*********************** Link list functions **********************************/

/*******************************************************************************
**
** Function         listInit
**
** Description      List initialization
**
** Returns          1, if list initialized, 0 otherwise
**
*******************************************************************************/
int listInit(struct listHead* pList)
{
    pList->pFirst = NULL;
    if (pthread_mutex_init(&pList->mutex, NULL) == -1)
    {
        NXPLOG_SPIHAL_E("Mutex creation failed (errno=0x%08x)", errno);
        return 0;
    }

    return 1;
}

/*******************************************************************************
**
** Function         listDestroy
**
** Description      List destruction
**
** Returns          1, if list destroyed, 0 if failed
**
*******************************************************************************/
int listDestroy(struct listHead* pList)
{
    int bListNotEmpty = 1;
    while (bListNotEmpty)
    {
        bListNotEmpty = listGetAndRemoveNext(pList, NULL);
    }

    if (pthread_mutex_destroy(&pList->mutex) == -1)
    {
        NXPLOG_SPIHAL_E("Mutex destruction failed (errno=0x%08x)", errno);
        return 0;
    }

    return 1;
}

/*******************************************************************************
**
** Function         listAdd
**
** Description      Add a node to the list
**
** Returns          1, if added, 0 if otherwise
**
*******************************************************************************/
int listAdd(struct listHead* pList, void* pData)
{
    struct listNode* pNode;
    struct listNode* pLastNode;
    int result;

    /* Create node */
    pNode = (struct listNode*) malloc(sizeof(struct listNode));
    if (pNode == NULL)
    {
        result = 0;
        NXPLOG_SPIHAL_E("Failed to malloc");
        goto clean_and_return;
    }
    pNode->pData = pData;
    pNode->pNext = NULL;

    pthread_mutex_lock(&pList->mutex);

    /* Add the node to the list */
    if (pList->pFirst == NULL)
    {
        /* Set the node as the head */
        pList->pFirst = pNode;
    }
    else
    {
        /* Seek to the end of the list */
        pLastNode = pList->pFirst;
        while (pLastNode->pNext != NULL)
        {
            pLastNode = pLastNode->pNext;
        }

        /* Add the node to the current list */
        pLastNode->pNext = pNode;
    }

    result = 1;

clean_and_return:
    pthread_mutex_unlock(&pList->mutex);
    return result;
}

/*******************************************************************************
**
** Function         listRemove
**
** Description      Remove node from the list
**
** Returns          1, if removed, 0 if otherwise
**
*******************************************************************************/
int listRemove(struct listHead* pList, void* pData)
{
    struct listNode* pNode;
    struct listNode* pRemovedNode;
    int result;

    pthread_mutex_lock(&pList->mutex);

    if (pList->pFirst == NULL)
    {
        /* Empty list */
        NXPLOG_SPIHAL_E("Failed to deallocate (list empty)");
        result = 0;
        goto clean_and_return;
    }

    pNode = pList->pFirst;
    if (pList->pFirst->pData == pData)
    {
        /* Get the removed node */
        pRemovedNode = pNode;

        /* Remove the first node */
        pList->pFirst = pList->pFirst->pNext;
    }
    else
    {
        while (pNode->pNext != NULL)
        {
            if (pNode->pNext->pData == pData)
            {
                /* Node found ! */
                break;
            }
            pNode = pNode->pNext;
        }

        if (pNode->pNext == NULL)
        {
            /* Node not found */
            result = 0;
            NXPLOG_SPIHAL_E("Failed to deallocate (not found %8p)", pData);
            goto clean_and_return;
        }

        /* Get the removed node */
        pRemovedNode = pNode->pNext;

        /* Remove the node from the list */
        pNode->pNext = pNode->pNext->pNext;
    }

    /* Deallocate the node */
    free(pRemovedNode);

    result = 1;

clean_and_return:
    pthread_mutex_unlock(&pList->mutex);
    return result;
}

/*******************************************************************************
**
** Function         listGetAndRemoveNext
**
** Description      Get next node on the list and remove it
**
** Returns          1, if successful, 0 if otherwise
**
*******************************************************************************/
int listGetAndRemoveNext(struct listHead* pList, void** ppData)
{
    struct listNode* pNode;
    int result;

    pthread_mutex_lock(&pList->mutex);

    if (pList->pFirst ==  NULL)
    {
        /* Empty list */
        NXPLOG_SPIHAL_E("Failed to deallocate (list empty)");
        result = 0;
        goto clean_and_return;
    }

    /* Work on the first node */
    pNode = pList->pFirst;

    /* Return the data */
    if (ppData != NULL)
    {
        *ppData = pNode->pData;
    }

    /* Remove and deallocate the node */
    pList->pFirst = pNode->pNext;
    free(pNode);

    result = 1;

clean_and_return:
    listDump(pList);
    pthread_mutex_unlock(&pList->mutex);
    return result;
}

/*******************************************************************************
**
** Function         listDump
**
** Description      Dump list information
**
** Returns          None
**
*******************************************************************************/
void listDump(struct listHead* pList)
{
    struct listNode* pNode = pList->pFirst;

    NXPLOG_SPIHAL_D("Node dump:");
    while (pNode != NULL)
    {
        NXPLOG_SPIHAL_D("- %8p (%8p)", pNode, pNode->pData);
        pNode = pNode->pNext;
    }

    return;
}

/* END Linked list source code */

/****************** Semaphore and mutex helper functions **********************/

static phNxpSpiHal_Monitor_t *nxpspihal_monitor = NULL;

/*******************************************************************************
**
** Function         phNxpEseHal_init_monitor
**
** Description      Initialize the semaphore monitor
**
** Returns          Pointer to monitor, otherwise NULL if failed
**
*******************************************************************************/
phNxpSpiHal_Monitor_t*
phNxpEseHal_init_monitor(void)
{
    NXPLOG_SPIHAL_D("Entering phNxpEseHal_init_monitor");

    if (nxpspihal_monitor == NULL)
    {
        nxpspihal_monitor = (phNxpSpiHal_Monitor_t *) malloc(
                sizeof(phNxpSpiHal_Monitor_t));
    }

    if (nxpspihal_monitor != NULL)
    {
        memset(nxpspihal_monitor, 0x00, sizeof(phNxpSpiHal_Monitor_t));

        if (pthread_mutex_init(&nxpspihal_monitor->reentrance_mutex, NULL)
                == -1)
        {
            NXPLOG_SPIHAL_E("reentrance_mutex creation returned 0x%08x", errno);
            goto clean_and_return;
        }

        if (pthread_mutex_init(&nxpspihal_monitor->concurrency_mutex, NULL)
                == -1)
        {
            NXPLOG_SPIHAL_E("concurrency_mutex creation returned 0x%08x", errno);
            pthread_mutex_destroy(&nxpspihal_monitor->reentrance_mutex);
            goto clean_and_return;
        }

        if (listInit(&nxpspihal_monitor->sem_list) != 1)
        {
            NXPLOG_SPIHAL_E("Semaphore List creation failed");
            pthread_mutex_destroy(&nxpspihal_monitor->concurrency_mutex);
            pthread_mutex_destroy(&nxpspihal_monitor->reentrance_mutex);
            goto clean_and_return;
        }
    }
    else
    {
        NXPLOG_SPIHAL_E("nxphal_monitor creation failed");
        goto clean_and_return;
    }

    NXPLOG_SPIHAL_D("Returning with SUCCESS");

    return nxpspihal_monitor;

clean_and_return:
    NXPLOG_SPIHAL_D("Returning with FAILURE");

    if (nxpspihal_monitor != NULL)
    {
        free(nxpspihal_monitor);
        nxpspihal_monitor = NULL;
    }

    return NULL;
}

/*******************************************************************************
**
** Function         phNxpSpiHal_cleanup_monitor
**
** Description      Clean up semaphore monitor
**
** Returns          None
**
*******************************************************************************/
void phNxpSpiHal_cleanup_monitor(void)
{
    if (nxpspihal_monitor != NULL)
    {
        pthread_mutex_destroy(&nxpspihal_monitor->concurrency_mutex);
        pthread_mutex_destroy(&nxpspihal_monitor->reentrance_mutex);
        phNxpSpiHal_releaseall_cb_data();
        listDestroy(&nxpspihal_monitor->sem_list);
    }

    free(nxpspihal_monitor);
    nxpspihal_monitor = NULL;

    return;
}

/*******************************************************************************
**
** Function         phNxpSpiHal_get_monitor
**
** Description      Get monitor
**
** Returns          Pointer to monitor
**
*******************************************************************************/
phNxpSpiHal_Monitor_t*
phNxpSpiHal_get_monitor(void)
{
    return nxpspihal_monitor;
}

/* Initialize the callback data */
ESESTATUS phNxpSpiHal_init_cb_data(phNxpSpiHal_Sem_t *pCallbackData,
        void *pContext)
{
    /* Create semaphore */
    if (sem_init(&pCallbackData->sem, 0, 0) == -1)
    {
        NXPLOG_SPIHAL_E("Semaphore creation failed (errno=0x%08x)", errno);
        return ESESTATUS_FAILED;
    }

    /* Set default status value */
    pCallbackData->status = ESESTATUS_FAILED;

    /* Copy the context */
    pCallbackData->pContext = pContext;

    /* Add to active semaphore list */
    if (listAdd(&phNxpSpiHal_get_monitor()->sem_list, pCallbackData) != 1)
    {
        NXPLOG_SPIHAL_E("Failed to add the semaphore to the list");
    }

    return ESESTATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         phNxpSpiHal_cleanup_cb_data
**
** Description      Clean up callback data
**
** Returns          None
**
*******************************************************************************/
void phNxpSpiHal_cleanup_cb_data(phNxpSpiHal_Sem_t* pCallbackData)
{
    /* Destroy semaphore */
    if (sem_destroy(&pCallbackData->sem))
    {
        NXPLOG_SPIHAL_E("phNxpSpiHal_cleanup_cb_data: Failed to destroy semaphore (errno=0x%08x)", errno);
    }

    /* Remove from active semaphore list */
    if (listRemove(&phNxpSpiHal_get_monitor()->sem_list, pCallbackData) != 1)
    {
        NXPLOG_SPIHAL_E("phNxpSpiHal_cleanup_cb_data: Failed to remove semaphore from the list");
    }

    return;
}

/*******************************************************************************
**
** Function         phNxpSpiHal_releaseall_cb_data
**
** Description      Release all callback data
**
** Returns          None
**
*******************************************************************************/
void phNxpSpiHal_releaseall_cb_data(void)
{
    phNxpSpiHal_Sem_t* pCallbackData;

    while (listGetAndRemoveNext(&phNxpSpiHal_get_monitor()->sem_list,
            (void**) &pCallbackData))
    {
        pCallbackData->status = ESESTATUS_FAILED;
        sem_post(&pCallbackData->sem);
    }

    return;
}

/* END Semaphore and mutex helper functions */

/**************************** Other functions *********************************/

/*******************************************************************************
**
** Function         phNxpSpiHal_print_packet
**
** Description      Print packet
**
** Returns          None
**
*******************************************************************************/
void phNxpSpiHal_print_packet(const char *pString, const uint8_t *p_data,
        uint16_t len)
{
    uint32_t i;
    char print_buffer[len * 3 + 1];

    memset (print_buffer, 0, sizeof(print_buffer));
    for (i = 0; i < len; i++) {
        snprintf(&print_buffer[i * 2], 3, "%02X", p_data[i]);
    }
    if( 0 == memcmp(pString,"SEND",0x04))
    {
        NXPLOG_SPIX_D("len = %3d > %s", len, print_buffer);
    }
    else if( 0 == memcmp(pString,"RECV",0x04))
    {
        NXPLOG_SPIR_D("len = %3d > %s", len, print_buffer);
    }

    return;
}


/*******************************************************************************
**
** Function         phNxpSpiHal_emergency_recovery
**
** Description      Emergency recovery in case of no other way out
**
** Returns          None
**
*******************************************************************************/

void phNxpSpiHal_emergency_recovery(void)
{
    NXPLOG_SPIHAL_E("%s: abort()", __FUNCTION__);
//    abort();
}
