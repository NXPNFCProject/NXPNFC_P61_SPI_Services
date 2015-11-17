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

#ifndef PHESETYPES_H
#define PHESETYPES_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef TRUE
#define TRUE            (0x01)            /* Logical True Value */
#endif
#ifndef FALSE
#define FALSE           (0x00)            /* Logical False Value */
#endif
typedef uint8_t             utf8_t;       /* UTF8 Character String */
typedef uint8_t             bool_t;       /* boolean data type */
typedef uint16_t        ESESTATUS;        /* Return values */
#define STATIC static

#define PHESE_MAX_UID_LENGTH            0x0AU  /* Maximum UID length expected */
#define PHESE_MAX_ATR_LENGTH            0x30U  /* Maximum ATR_RES (General Bytes) length expected */
#define PHESE_ESEID_LENGTH              0x0AU  /* Maximum length of ESEID 1.3*/
#define PHESE_ATQA_LENGTH               0x02U  /* ATQA length */

/*
 * ESE Data structure
 */
typedef struct phEse_sData
{
    uint8_t             *buffer; /* Buffer to store data */
    uint32_t            length;  /* Buffer length */
} phEse_sData_t;

/*
 * Possible Hardware Configuration exposed to upper layer.
 * Typically this should be port name (Ex:"COM1","COM2") to which PN547 is connected.
 */
typedef enum
{
   ENUM_LINK_TYPE_COM1,
   ENUM_LINK_TYPE_COM2,
   ENUM_LINK_TYPE_COM3,
   ENUM_LINK_TYPE_COM4,
   ENUM_LINK_TYPE_COM5,
   ENUM_LINK_TYPE_COM6,
   ENUM_LINK_TYPE_COM7,
   ENUM_LINK_TYPE_COM8,
   ENUM_LINK_TYPE_I2C,
   ENUM_LINK_TYPE_SPI,
   ENUM_LINK_TYPE_USB,
   ENUM_LINK_TYPE_TCP,
   ENUM_LINK_TYPE_NB
} phLibEse_eConfigLinkType;

/*
 * Deferred message. This message type will be posted to the client application thread
 * to notify that a deferred call must be invoked.
 */
#define PH_LIBESE_DEFERREDCALL_MSG        (0x311)

/*
 * Deferred call declaration.
 * This type of API is called from ClientApplication ( main thread) to notify
 * specific callback.
 */
typedef  void (*pphLibEse_DeferredCallback_t) (void*);

/*
 * Deferred parameter declaration.
 * This type of data is passed as parameter from ClientApplication (main thread) to the
 * callback.
 */
typedef  void *pphLibEse_DeferredParameter_t;

/*
 * Possible Hardware Configuration exposed to upper layer.
 * Typically this should be at least the communication link (Ex:"COM1","COM2")
 * the controller is connected to.
 */
typedef struct phLibEse_sConfig
{
   uint8_t                 *pLogFile; /* Log File Name*/
   /* Hardware communication link to the controller */
   phLibEse_eConfigLinkType  nLinkType;
   /* The client ID (thread ID or message queue ID) */
   uintptr_t                nClientId;
} phLibEse_sConfig_t, *pphLibEse_sConfig_t;

/*
 * ESE Message structure contains message specific details like
 * message type, message specific data block details, etc.
 */
typedef struct phLibEse_Message
{
    uint32_t eMsgType;   /* Type of the message to be posted*/
    void   * pMsgData;   /* Pointer to message specific data block in case any*/
    uint32_t Size;       /* Size of the datablock*/
} phLibEse_Message_t,*pphLibEse_Message_t;

/*
 * Deferred message specific info declaration.
 * This type of information is packed as message data when PH_LIBESE_DEFERREDCALL_MSG
 * type message is posted to message handler thread.
 */
typedef struct phLibEse_DeferredCall
{
    pphLibEse_DeferredCallback_t pCallback;/* pointer to Deferred callback */
    pphLibEse_DeferredParameter_t pParameter;/* pointer to Deferred parameter */
} phLibEse_DeferredCall_t;

/*
 * Definitions for supported protocol
 */
typedef struct phEse_sSupProtocol
{
    unsigned int MifareUL    : 1;  /* Protocol Mifare Ultra Light or any ESE Forum Type-2 tags */
    unsigned int MifareStd   : 1;  /* Protocol Mifare Standard. */
    unsigned int ISO14443_4A : 1;  /* Protocol ISO14443-4 Type A.  */
    unsigned int ISO14443_4B : 1;  /* Protocol ISO14443-4 Type B.  */
    unsigned int ISO15693    : 1;  /* Protocol ISO15693 HiTag.  */
    unsigned int Felica      : 1;  /* Protocol Felica. */
    unsigned int ESE         : 1;  /* Protocol ESE. */
    unsigned int Jewel       : 1;  /* Protocol Innovision Jewel Tag. or Any T1T*/
    unsigned int Desfire     : 1;  /*TRUE indicates specified feature (mapping
                                   or formatting)for DESFire tag supported else not supported.*/
    unsigned int Kovio       : 1;   /* Protocol Kovio Tag*/
    unsigned int HID         : 1;   /* Protocol HID(Picopass) Tag*/
    unsigned int Bprime      : 1;   /* Protocol BPrime Tag*/
    unsigned int EPCGEN2     : 1;   /* Protocol EPCGEN2 Tag*/
}phEse_sSupProtocol_t;

/*
 *  Enumerated MIFARE Commands
 */

typedef enum phEse_eMifareCmdList
{
    phEse_eMifareRaw        = 0x00U,     /* This command performs raw transcations */
    phEse_eMifareAuthentA   = 0x60U,     /* This command performs an authentication with KEY A for a sector. */
    phEse_eMifareAuthentB   = 0x61U,     /* This command performs an authentication with KEY B for a sector. */
    phEse_eMifareRead16     = 0x30U,     /* Read 16 Bytes from a Mifare Standard block */
    phEse_eMifareRead       = 0x30U,     /* Read Mifare Standard */
    phEse_eMifareWrite16    = 0xA0U,     /* Write 16 Bytes to a Mifare Standard block */
    phEse_eMifareWrite4     = 0xA2U,     /* Write 4 bytes. */
    phEse_eMifareInc        = 0xC1U,     /* Increment */
    phEse_eMifareDec        = 0xC0U,     /* Decrement */
    phEse_eMifareTransfer   = 0xB0U,     /* Transfer */
    phEse_eMifareRestore    = 0xC2U,     /* Restore.   */
    phEse_eMifareReadSector = 0x38U,     /* Read Sector.   */
    phEse_eMifareWriteSector= 0xA8U,     /* Write Sector.   */
    /* Above commands could be used for preparing raw command but below one can not be */
    phEse_eMifareReadN      = 0x01,      /* Proprietary Command */
    phEse_eMifareWriteN     = 0x02,      /* Proprietary Command */
    phEse_eMifareSectorSel  = 0x03,      /* Proprietary Command */
    phEse_eMifareAuth       = 0x04,      /* Proprietary Command */
    phEse_eMifareProxCheck  = 0x05,      /* Proprietary Command */
    phEse_eMifareInvalidCmd = 0xFFU      /* Invalid Command */
} phEse_eMifareCmdList_t;

/*
 * Information about ISO14443A
 */
typedef struct phEse_sIso14443AInfo
{
    uint8_t         Uid[PHESE_MAX_UID_LENGTH];      /* UID information of the TYPE A
                                                     * Tag Discovered */
    uint8_t         UidLength;                      /* UID information length */
    uint8_t         AppData[PHESE_MAX_ATR_LENGTH];  /* Application data information of the
                              1                      * tag discovered (= Historical bytes for
                                                     * type A) */
    uint8_t         AppDataLength;                  /* Application data length */
    uint8_t         Sak;                            /* SAK information of the TYPE A
                                                     * Tag Discovered */
    uint8_t         AtqA[PHESE_ATQA_LENGTH];        /* ATQA informationof the TYPE A
                                                     * Tag Discovered */
    uint8_t         MaxDataRate;                    /* Maximum data rate supported
                                                     * by the tag Discovered */
    uint8_t         Fwi_Sfgt;                       /* Frame waiting time and start up
                                                     * frame guard */
}phEse_sIso14443AInfo_t;

/* Remote device information structure */
typedef union phEse_uRemoteDevInfo
{
    phEse_sIso14443AInfo_t          Iso14443A_Info;/* ISO1443A Remote device info */
}phEse_uRemoteDevInfo_t;

/*
*
*  The RF Device Type List is used to identify the type of
*  remote device that is discovered and connected.
*
*/

typedef enum phEse_eRFDevType
{
    phEse_eUnknown_DevType        = 0x00U,

    phEse_eISO14443_A_PCD,
    phEse_eISO14443_B_PCD,
    phEse_eISO14443_BPrime_PCD,
    phEse_eFelica_PCD,
    phEse_eJewel_PCD,
    phEse_eISO15693_PCD,
    phEse_eEpcGen2_PCD,
    phEse_ePCD_DevType,

    phEse_ePICC_DevType,
    phEse_eISO14443_A_PICC,
    phEse_eISO14443_4A_PICC,
    phEse_eISO14443_3A_PICC,
    phEse_eMifare_PICC,
    phEse_eISO14443_B_PICC,
    phEse_eISO14443_4B_PICC,
    phEse_eISO14443_BPrime_PICC,
    phEse_eFelica_PICC,
    phEse_eJewel_PICC,
    phEse_eISO15693_PICC,
    phEse_eEpcGen2_PICC,

    phEse_eEseIP1_Target,
    phEse_eEseIP1_Initiator,

    phEse_eInvalid_DevType

}phEse_eRFDevType_t;

/*
 * The Remote Device Type List is used to identify the type of
 * remote device that is discovered/connected
 */
typedef phEse_eRFDevType_t phEse_eRemDevType_t;
typedef phEse_eRemDevType_t phHal_eRemDevType_t;

/*
 *   Union for each available type of Commands.
 */

typedef union phEse_uCommand
{
  phEse_eMifareCmdList_t         MfCmd;  /* Mifare command structure.  */
}phEse_uCmdList_t;

/*
 *  The Remote Device Information Structure holds information about one single Remote
 *  Device detected.
 */
typedef struct phEse_sRemoteDevInformation
{
    uint8_t                    SessionOpened;       /* Flag indicating the validity of
                                                     * the handle of the remote device.
                                                     * 1 = Device is not activer (Only discovered), 2 = Device is active and ready for use*/
    phEse_eRemDevType_t        RemDevType;          /* Remote device type */
    phEse_uRemoteDevInfo_t     RemoteDevInfo;       /* Union of available Remote Device */
}phEse_sRemoteDevInformation_t;


/*
 * Transceive Information Data Structure for sending commands/response to the remote device
 */

typedef struct phEse_sTransceiveInfo
{
    phEse_uCmdList_t                cmd;        /* Command for transceive */
    uint8_t                         addr;       /* Start Block Number */
    uint8_t                         NumBlock;   /* Number of Blocks to perform operation */
    /* For Felica only*/
    uint16_t *ServiceCodeList;                  /* 2 Byte service Code List */
    uint16_t *Blocklist;                        /* 2 Byte Block list */
    phEse_sData_t                   sSendData; /* Send data */
    phEse_sData_t                   sRecvData; /* Recv data */
    /* For EPC-GEN */
    uint32_t                        dwWordPtr;   /* Word address for the memory write */
    uint8_t                         bWordPtrLen; /* Specifies the length of word pointer
                                                 00: 8  bits
                                                 01: 16 bits
                                                 10: 24 bits
                                                 11: 32 bits
                                                 */
    uint8_t                        bWordCount;   /* Number of words to be read or written */
}phEse_sTransceiveInfo_t;

typedef enum p61_access_state{
    P61_STATE_INVALID = 0x0000,
    P61_STATE_IDLE = 0x0100, /* p61 is free to use */
    P61_STATE_WIRED = 0x0200,  /* p61 is being accessed by DWP (NFCC)*/
    P61_STATE_SPI = 0x0400, /* P61 is being accessed by SPI */
    P61_STATE_DWNLD = 0x0800, /* NFCC fw download is in progress */
    P61_STATE_SPI_PRIO = 0x1000, /*Start of p61 access by SPI on priority*/
    P61_STATE_SPI_PRIO_END = 0x2000, /*End of p61 access by SPI on priority*/
}p61_access_state_t;
#define UNUSED(X) (void)X;

#endif /* PHESETYPES_H */
