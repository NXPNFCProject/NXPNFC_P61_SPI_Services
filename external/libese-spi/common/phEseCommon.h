 /*
  * Copyright (C) 2015 NXP Semiconductors
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
 *  OSAL header files related to memory, debug, random, semaphore and mutex functions.
 */

#ifndef PHESECOMMON_H
#define PHESECOMMON_H

/*
************************* Include Files ****************************************
*/

#include <phEseStatus.h>
#include <semaphore.h>
#include <phOsalEse_Timer.h>
#include <pthread.h>
#include <phDal4Ese_messageQueueLib.h>


/* HAL Version number (Updated as per release) */
#define NXP_MW_VERSION_MAJ  (1U)
#define NXP_MW_VERSION_MIN  (0x0U)

/*
 *  information to configure OSAL
 */
typedef struct phOsalEse_Config
{
    uint8_t *pLogFile; /* Log File Name*/
    uintptr_t dwCallbackThreadId; /* Client ID to which message is posted */
}phOsalEse_Config_t, *pphOsalEse_Config_t /* Pointer to #phOsalEse_Config_t */;


/*
 * Deferred call declaration.
 * This type of API is called from ClientApplication (main thread) to notify
 * specific callback.
 */
typedef  void (*pphOsalEse_DeferFuncPointer_t) (void*);


/*
 * Deferred message specific info declaration.
 */
typedef struct phOsalEse_DeferedCallInfo
{
        pphOsalEse_DeferFuncPointer_t   pDeferedCall;/* pointer to Deferred callback */
        void                            *pParam;    /* contains timer message specific details*/
}phOsalEse_DeferedCallInfo_t;


/*
 * States in which a OSAL timer exist.
 */
typedef enum
{
    eTimerIdle = 0,         /* Indicates Initial state of timer */
    eTimerRunning = 1,      /* Indicate timer state when started */
    eTimerStopped = 2       /* Indicates timer state when stopped */
}phOsalEse_TimerStates_t;   /* Variable representing State of timer */

/*
 **Timer Handle structure containing details of a timer.
 */
typedef struct phOsalEse_TimerHandle
{
    uint32_t TimerId;                                   /* ID of the timer */
    timer_t hTimerHandle;                               /* Handle of the timer */
    pphOsalEse_TimerCallbck_t   Application_callback;   /* Timer callback function to be invoked */
    void *pContext;                                     /* Parameter to be passed to the callback function */
    phOsalEse_TimerStates_t eState;                     /* Timer states */
    phLibEse_Message_t tOsalMessage;                    /* Osal Timer message posted on User Thread */
    phOsalEse_DeferedCallInfo_t tDeferedCallInfo;       /* Deferred Call structure to Invoke Callback function */
}phOsalEse_TimerHandle_t,*pphOsalEse_TimerHandle_t;     /* Variables for Structure Instance and Structure Ptr */

#endif /*  PHOSALESE_H  */
