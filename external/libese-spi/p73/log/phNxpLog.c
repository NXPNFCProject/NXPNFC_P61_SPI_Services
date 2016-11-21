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

/* ############################################### Header Includes ################################################ */
#if ! defined (NXPLOG__H_INCLUDED)
#    include "phNxpLog.h"
#    include "phNxpConfig.h"
#endif
#include <cutils/properties.h>

const char * NXPLOG_ITEM_ESELIB  = "NxpEseLib";
const char * NXPLOG_ITEM_SPIX    = "NxpEseDataX";
const char * NXPLOG_ITEM_SPIR    = "NxpEseDataR";
const char * NXPLOG_ITEM_PAL     = "NxpEsePal";

/* global log level structure */
spi_log_level_t gLog_level;

#ifdef ESE_DEBUG_UTILS_INCLUDED
/*******************************************************************************
 *
 * Function         phNxpLog_SetGlobalLogLevel
 *
 * Description      Sets the global log level for all modules.
 *                  This value is set by Android property ese.nxp_log_level_global.
 *                  If value can be overridden by module log level.
 *
 * Returns          The value of global log level
 *
 ******************************************************************************/
static uint8_t phNxpLog_SetGlobalLogLevel (void)
{
    uint8_t level = NXPLOG_DEFAULT_LOGLEVEL;
    unsigned long num = 0;
    char valueStr [PROPERTY_VALUE_MAX] = {0};

    int len = property_get (PROP_NAME_NXPLOG_GLOBAL_LOGLEVEL, valueStr, "");
    if (len > 0)
    {
        /* let Android property override .conf variable */
        sscanf (valueStr, "%lu", &num);
        level = (unsigned char) num;
    }
    memset(&gLog_level, level, sizeof(spi_log_level_t));
    return level;
}

/*******************************************************************************
 *
 * Function         phNxpLog_SetEseLibLogLevel
 *
 * Description      Sets the HAL layer log level.
 *
 * Returns          void
 *
 ******************************************************************************/
static void phNxpLog_SetEseLibLogLevel (uint8_t level)
{
    unsigned long num = 0;
    int len;
    char valueStr [PROPERTY_VALUE_MAX] = {0};

    if (GetNxpNumValue (NAME_NXPLOG_ESELIB_LOGLEVEL, &num, sizeof(num)))
    {
        gLog_level.eselib_log_level = (level > (unsigned char) num) ? level : (unsigned char) num;;
    }

    len = property_get (PROP_NAME_NXPLOG_ESELIB_LOGLEVEL, valueStr, "");
    if (len > 0)
    {
        /* let Android property override .conf variable */
        sscanf (valueStr, "%lu", &num);
        gLog_level.eselib_log_level = (unsigned char) num;
    }
}

/*******************************************************************************
 *
 * Function         phNxpLog_SetExtnsLogLevel
 *
 * Description      Sets the Extensions layer log level.
 *
 * Returns          void
 *
 ******************************************************************************/
static void phNxpLog_SetExtnsLogLevel (uint8_t level)
{
    unsigned long num = 0;
    int len;
    char valueStr [PROPERTY_VALUE_MAX] = {0};
    if (GetNxpNumValue (NAME_NXPLOG_EXTNS_LOGLEVEL, &num, sizeof(num)))
    {
        gLog_level.extns_log_level = (level > (unsigned char) num) ? level : (unsigned char) num;;
    }

    len = property_get (PROP_NAME_NXPLOG_EXTNS_LOGLEVEL, valueStr, "");
    if (len > 0)
    {
        /* let Android property override .conf variable */
        sscanf (valueStr, "%lu", &num);
        gLog_level.extns_log_level = (unsigned char) num;
    }
}

/*******************************************************************************
 *
 * Function         phNxpLog_SetPalLogLevel
 *
 * Description      Sets the Pal layer log level.
 *
 * Returns          void
 *
 ******************************************************************************/
static void phNxpLog_SetPalLogLevel (uint8_t level)
{
    unsigned long num = 0;
    int len;
    char valueStr [PROPERTY_VALUE_MAX] = {0};
    if (GetNxpNumValue (NAME_NXPLOG_PAL_LOGLEVEL, &num, sizeof(num)))
    {
        gLog_level.pal_log_level = (level > (unsigned char) num) ? level : (unsigned char) num;;
    }

    len = property_get (PROP_NAME_NXPLOG_PAL_LOGLEVEL, valueStr, "");
    if (len > 0)
    {
        /* let Android property override .conf variable */
        sscanf (valueStr, "%lu", &num);
        gLog_level.pal_log_level = (unsigned char) num;
    }
}

/*******************************************************************************
 *
 * Function         phNxpLog_SetDnldLogLevel
 *
 * Description      Sets the FW download layer log level.
 *
 * Returns          void
 *
 ******************************************************************************/
static void phNxpLog_SetDnldLogLevel (uint8_t level)
{
    unsigned long num = 0;
    int len;
    char valueStr [PROPERTY_VALUE_MAX] = {0};
    if (GetNxpNumValue (NAME_NXPLOG_FWDNLD_LOGLEVEL, &num, sizeof(num)))
    {
        gLog_level.dnld_log_level = (level > (unsigned char) num) ? level : (unsigned char) num;;
    }

    len = property_get (PROP_NAME_NXPLOG_FWDNLD_LOGLEVEL, valueStr, "");
    if (len > 0)
    {
        /* let Android property override .conf variable */
        sscanf (valueStr, "%lu", &num);
        gLog_level.dnld_log_level = (unsigned char) num;
    }
}

/*******************************************************************************
 *
 * Function         phNxpLog_SetSpiTxLogLevel
 *
 * Description      Sets the SPI transaction layer log level.
 *
 * Returns          void
 *
 ******************************************************************************/
static void phNxpLog_SetSpiTxLogLevel (uint8_t level)
{
    unsigned long num = 0;
    int len;
    char valueStr [PROPERTY_VALUE_MAX] = {0};
    if (GetNxpNumValue (NAME_NXPLOG_SPIX_LOGLEVEL, &num, sizeof(num)))
    {
        gLog_level.spix_log_level = (level > (unsigned char) num) ? level : (unsigned char) num;
    }
    if (GetNxpNumValue (NAME_NXPLOG_SPIR_LOGLEVEL, &num, sizeof(num)))
    {
        gLog_level.spir_log_level = (level > (unsigned char) num) ? level : (unsigned char) num;;
    }

    len = property_get (PROP_NAME_NXPLOG_SPI_LOGLEVEL, valueStr, "");
    if (len > 0)
    {
        /* let Android property override .conf variable */
        sscanf (valueStr, "%lu", &num);
        gLog_level.spix_log_level = (unsigned char) num;
        gLog_level.spir_log_level = (unsigned char) num;
    }
}

/******************************************************************************
 * Function         phNxpLog_InitializeLogLevel
 *
 * Description      Initialize and get log level of module from libese-nxp.conf or
 *                  Android runtime properties.
 *                  The Android property ese.nxp_global_log_level is to
 *                  define log level for all modules. Modules log level will overwide global level.
 *                  The Android property will overwide the level
 *                  in libese-nxp.conf
 *
 *                  Android property names:
 *                      ese.nxp_log_level_global    * defines log level for all modules
 *                      ese.nxp_log_level_extns     * extensions module log
 *                      ese.nxp_log_level_eselib    * ESE Lib module log
 *                      ese.nxp_log_level_dnld      * firmware download module log
 *                      ese.nxp_log_level_pal       * PAL module log
 *                      ese.nxp_log_level_spi       * SPI transaction log
 *
 *                  Log Level values:
 *                      NXPLOG_LOG_SILENT_LOGLEVEL  0        * No trace to show
 *                      NXPLOG_LOG_ERROR_LOGLEVEL   1        * Show Error trace only
 *                      NXPLOG_LOG_WARN_LOGLEVEL    2        * Show Warning trace and Error trace
 *                      NXPLOG_LOG_DEBUG_LOGLEVEL   3        * Show all traces
 *
 * Returns          void
 *
 ******************************************************************************/
#endif
void phNxpLog_InitializeLogLevel(void)
{
#ifdef ESE_DEBUG_UTILS_INCLUDED
    uint8_t level = phNxpLog_SetGlobalLogLevel();
    phNxpLog_SetEseLibLogLevel(level);
    phNxpLog_SetExtnsLogLevel(level);
    phNxpLog_SetPalLogLevel(level);
    phNxpLog_SetDnldLogLevel(level);
    phNxpLog_SetSpiTxLogLevel(level);
#else
   gLog_level.global_log_level = MAX_LOG_LEVEL;
   gLog_level.dnld_log_level = MAX_LOG_LEVEL;
   gLog_level.extns_log_level = MAX_LOG_LEVEL;
   gLog_level.eselib_log_level  = MAX_LOG_LEVEL;
   gLog_level.pal_log_level = MAX_LOG_LEVEL;
   gLog_level.spir_log_level = MAX_LOG_LEVEL;
   gLog_level.spix_log_level = MAX_LOG_LEVEL;
#endif
    ALOGD ("%s: global =%u, Fwdnld =%u, extns =%u, \
                lib =%u, pal =%u, spir =%u, \
                spix =%u", \
                __FUNCTION__, gLog_level.global_log_level, gLog_level.dnld_log_level,
                    gLog_level.extns_log_level, gLog_level.eselib_log_level, gLog_level.pal_log_level,
                    gLog_level.spir_log_level, gLog_level.spix_log_level);
}
