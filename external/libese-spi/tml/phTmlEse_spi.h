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
 * TML I2C port implementation for linux
 */

/* Basic type definitions */
#include <phEseTypes.h>
#include <phTmlEse.h>

/* Function declarations */
void phTmlEse_spi_close(void *pDevHandle);
ESESTATUS phTmlEse_spi_open_and_configure(pphTmlEse_Config_t pConfig, void ** pLinkHandle);
int phTmlEse_spi_read(void *pDevHandle, uint8_t * pBuffer, int nNbBytesToRead);
int phTmlEse_spi_write(void *pDevHandle,uint8_t * pBuffer, int nNbBytesToWrite);
int phTmlEse_spi_ioctl(phTmlEse_ControlCode_t eControlCode, void *pDevHandle, long level);
