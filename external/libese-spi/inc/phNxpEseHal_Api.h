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
 * \addtogroup spi_libese
 *
 * @{ */

#ifndef _PHNXPSPIHAL_API_H_
#define _PHNXPSPIHAL_API_H_

#include <phNxpEseHal_Apdu.h>
#include <phEseStatus.h>

/* ESE Data */
typedef struct phNxpEseP61_data
{
    uint32_t len;
    uint8_t  *p_data;
} phNxpEseP61_data;

typedef void ese_stack_data_callback_t (ESESTATUS , phNxpEseP61_data *);

/**
 * \ingroup spi_libese
 * \brief This function is called by Jni during the
 *        initialization of the ESE. It opens the physical connection
 *        with ESE (P61) and creates required client thread for
 *        operation.  If null call-back provided than, application will use
 *        phNxpEseP61_7816_Transceive api and local call-back would be used.
 *
 * \param[in]       ese_stack_data_callback_t
 *
 * \retval ESESTATUS_SUCCESS On Success ESESTATUS_SUCCESS else proper error code
 *
*/

ESESTATUS phNxpEseP61_open(ese_stack_data_callback_t *p_data_cback);

/**
 * \ingroup spi_libese
 * \brief This function is called by Jni during the
 *        initialization of the ESE. It opens the physical connection
 *        with ESE (P61) and creates required client thread for
 *        operation.  This will get priority access to ESE for timeout period.
 *
 * \param[in]       ese_stack_data_callback_t
 *
 * \retval ESESTATUS_SUCCESS On Success ESESTATUS_SUCCESS else proper error code
 *
*/
ESESTATUS phNxpEseP61_openPrioSession(ese_stack_data_callback_t *p_data_cback, uint32_t timeOut);

/**
 * \ingroup spi_libese
 * \brief This function update the len and provided buffer
 *
 * \param[in]       uint16_t
 * \param[in]       const uint8_t *
 *
 * \retval ESESTATUS_SUCCESS On Success ESESTATUS_SUCCESS else proper error code
 *\msc
 * SPIClient,  SPILibEse,  SPIWriterThread,  SPIReaderThread;
 * SPIClient=>SPILibEse   [label="phNxpEseP61_open(callback)",URL="\ref    phNxpEseP61_open"];
 * SPILibEse=>SPIWriterThread;
 * SPIWriterThread=>SPIWriterThread;
 * SPILibEse=>SPIReaderThread;
 * SPIReaderThread=>SPIReaderThread;
 * SPIClient<=SPILibEse   [label="ESESTATUS_SUCCESS"];
 * SPIClient=>SPILibEse   [label="phNxpEseP61_Transceive()",URL="\ref    phNxpEseP61_Transceive"];
 * SPILibEse->SPIWriterThread;
 * SPILibEse<-SPIWriterThread;
 * SPILibEse->SPIReaderThread;
 * SPILibEse<-SPIReaderThread;
 * SPIClient<=SPILibEse [label="callback(status, data)"];
 *\endmsc
 *
*/

ESESTATUS phNxpEseP61_Transceive(uint16_t data_len, const uint8_t *p_data);

/**
 * \ingroup spi_libese
 * \brief This function close the ESE interface and free all resources.
 *
 * \param[in]       void
 *
 * \retval ESESTATUS_SUCCESS Always return ESESTATUS_SUCCESS (0).
 *
*/

ESESTATUS phNxpEseP61_close(void);

/**
 * \ingroup spi_libese
 * \brief This function reset the ESE interface and free all
 *
 * \param[in]       void
 *
 * \retval ESESTATUS_SUCCESS Always return ESESTATUS_SUCCESS (0).
 *
*/

ESESTATUS phNxpEseP61_reset(void);

/**
 * \ingroup spi_libese
 * \brief This function is used to set IFSC size
 *
 * \param[in]       void
 *
 * \retval ESESTATUS_SUCCESS Always return ESESTATUS_SUCCESS (0).
 *
*/

ESESTATUS phNxpEseP61_setIfsc(uint16_t IFSC_Size);

/** @} */
#endif /* _PHNXPSPIHAL_API_H_ */
