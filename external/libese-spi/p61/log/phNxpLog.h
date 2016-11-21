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

#if ! defined (NXPLOG__H_INCLUDED)
#define NXPLOG__H_INCLUDED

#include <cutils/log.h>
#include <string.h>

typedef struct spi_log_level
{
    uint8_t global_log_level;
    uint8_t extns_log_level;
    uint8_t hal_log_level;
    uint8_t dnld_log_level;
    uint8_t tml_log_level;
    uint8_t spix_log_level;
    uint8_t spir_log_level;
} spi_log_level_t;

/* global log level Ref */
extern spi_log_level_t gLog_level;

/* define log module included when compile */
#define ENABLE_EXTNS_TRACES   TRUE
#define ENABLE_HAL_TRACES     TRUE
#define ENABLE_TML_TRACES     TRUE
#define ENABLE_FWDNLD_TRACES  TRUE
#define ENABLE_SPIX_TRACES    TRUE
#define ENABLE_SPIR_TRACES    TRUE

#define ENABLE_HCPX_TRACES    FALSE
#define ENABLE_HCPR_TRACES    FALSE

/* ####################### Set the log module name in .conf file ########################## */
#define NAME_NXPLOG_EXTNS_LOGLEVEL          "NXPLOG_EXTNS_LOGLEVEL"
#define NAME_NXPLOG_HAL_LOGLEVEL            "NXPLOG_SPIHAL_LOGLEVEL"
#define NAME_NXPLOG_SPIX_LOGLEVEL           "NXPLOG_SPIX_LOGLEVEL"
#define NAME_NXPLOG_SPIR_LOGLEVEL           "NXPLOG_SPIR_LOGLEVEL"
#define NAME_NXPLOG_FWDNLD_LOGLEVEL         "NXPLOG_FWDNLD_LOGLEVEL"
#define NAME_NXPLOG_TML_LOGLEVEL            "NXPLOG_TML_LOGLEVEL"

/* ####################### Set the log module name by Android property ########################## */
#define PROP_NAME_NXPLOG_GLOBAL_LOGLEVEL       "ese.nxp_log_level_global"
#define PROP_NAME_NXPLOG_EXTNS_LOGLEVEL        "ese.nxp_log_level_extns"
#define PROP_NAME_NXPLOG_HAL_LOGLEVEL          "ese.nxp_log_level_hal"
#define PROP_NAME_NXPLOG_SPI_LOGLEVEL          "ese.nxp_log_level_spi"
#define PROP_NAME_NXPLOG_FWDNLD_LOGLEVEL       "ese.nxp_log_level_dnld"
#define PROP_NAME_NXPLOG_TML_LOGLEVEL          "ese.nxp_log_level_tml"

/* ####################### Set the logging level for EVERY COMPONENT here ######################## :START: */
#define NXPLOG_LOG_SILENT_LOGLEVEL             0x00
#define NXPLOG_LOG_ERROR_LOGLEVEL              0x01
#define NXPLOG_LOG_WARN_LOGLEVEL               0x02
#define NXPLOG_LOG_DEBUG_LOGLEVEL              0x03
/* ####################### Set the default logging level for EVERY COMPONENT here ########################## :END: */


/* The Default log level for all the modules. */
#define NXPLOG_DEFAULT_LOGLEVEL                NXPLOG_LOG_ERROR_LOGLEVEL


/* ################################################################################################################ */
/* ############################################### Component Names ################################################ */
/* ################################################################################################################ */

extern const char * NXPLOG_ITEM_EXTNS;   /* Android logging tag for NxpExtns  */
extern const char * NXPLOG_ITEM_SPIHAL;  /* Android logging tag for NxpSpiHal */
extern const char * NXPLOG_ITEM_SPIX;    /* Android logging tag for NxpSpiX   */
extern const char * NXPLOG_ITEM_SPIR;    /* Android logging tag for NxpSpiR   */
extern const char * NXPLOG_ITEM_FWDNLD;  /* Android logging tag for NxpFwDnld */
extern const char * NXPLOG_ITEM_TML;     /* Android logging tag for NxpTml    */

#ifdef NXP_HCI_REQ
extern const char * NXPLOG_ITEM_HCPX;    /* Android logging tag for NxpHcpX   */
extern const char * NXPLOG_ITEM_HCPR;    /* Android logging tag for NxpHcpR   */
#endif /*NXP_HCI_REQ*/

/* ######################################## Defines used for Logging data ######################################### */
#ifdef NXP_VRBS_REQ
#define NXPLOG_FUNC_ENTRY(COMP) \
    LOG_PRI(ANDROID_LOG_VERBOSE,(COMP),"+:%s",(__FUNCTION__))
#define NXPLOG_FUNC_EXIT(COMP) \
    LOG_PRI(ANDROID_LOG_VERBOSE,(COMP),"-:%s",(__FUNCTION__))
#endif /*NXP_VRBS_REQ*/

/* ################################################################################################################ */
/* ######################################## Logging APIs of actual modules ######################################## */
/* ################################################################################################################ */
/* Logging APIs used by NxpExtns module */
#if (ENABLE_EXTNS_TRACES == TRUE )
#    define NXPLOG_EXTNS_D(...)  {if(gLog_level.extns_log_level >= NXPLOG_LOG_DEBUG_LOGLEVEL) LOG_PRI(ANDROID_LOG_DEBUG,NXPLOG_ITEM_EXTNS,__VA_ARGS__);}
#    define NXPLOG_EXTNS_W(...)  {if(gLog_level.extns_log_level >= NXPLOG_LOG_WARN_LOGLEVEL) LOG_PRI(ANDROID_LOG_WARN,NXPLOG_ITEM_EXTNS,__VA_ARGS__);}
#    define NXPLOG_EXTNS_E(...)  {if(gLog_level.extns_log_level >= NXPLOG_LOG_ERROR_LOGLEVEL) LOG_PRI(ANDROID_LOG_ERROR,NXPLOG_ITEM_EXTNS,__VA_ARGS__);}
#else
#    define NXPLOG_EXTNS_D(...)
#    define NXPLOG_EXTNS_W(...)
#    define NXPLOG_EXTNS_E(...)
#endif /* Logging APIs used by NxpExtns module */

/* Logging APIs used by NxpSpiHal module */
#if (ENABLE_HAL_TRACES == TRUE )
#    define NXPLOG_SPIHAL_D(...)  {if(gLog_level.hal_log_level >= NXPLOG_LOG_DEBUG_LOGLEVEL) LOG_PRI(ANDROID_LOG_DEBUG,NXPLOG_ITEM_SPIHAL,__VA_ARGS__);}
#    define NXPLOG_SPIHAL_W(...)  {if(gLog_level.hal_log_level >= NXPLOG_LOG_WARN_LOGLEVEL) LOG_PRI(ANDROID_LOG_WARN,NXPLOG_ITEM_SPIHAL,__VA_ARGS__);}
#    define NXPLOG_SPIHAL_E(...)  {if(gLog_level.hal_log_level >= NXPLOG_LOG_ERROR_LOGLEVEL) LOG_PRI(ANDROID_LOG_ERROR,NXPLOG_ITEM_SPIHAL,__VA_ARGS__);}
#else
#    define NXPLOG_SPIHAL_D(...)
#    define NXPLOG_SPIHAL_W(...)
#    define NXPLOG_SPIHAL_E(...)
#endif /* Logging APIs used by HAL module */

/* Logging APIs used by NxpSpiX module */
#if (ENABLE_SPIX_TRACES == TRUE )
#    define NXPLOG_SPIX_D(...)  {if(gLog_level.spix_log_level >= NXPLOG_LOG_DEBUG_LOGLEVEL) LOG_PRI(ANDROID_LOG_DEBUG,NXPLOG_ITEM_SPIX,__VA_ARGS__);}
#    define NXPLOG_SPIX_W(...)  {if(gLog_level.spix_log_level >= NXPLOG_LOG_WARN_LOGLEVEL) LOG_PRI(ANDROID_LOG_WARN,NXPLOG_ITEM_SPIX,__VA_ARGS__);}
#    define NXPLOG_SPIX_E(...)  {if(gLog_level.spix_log_level >= NXPLOG_LOG_ERROR_LOGLEVEL) LOG_PRI(ANDROID_LOG_ERROR,NXPLOG_ITEM_SPIX,__VA_ARGS__);}
#else
#    define NXPLOG_SPIX_D(...)
#    define NXPLOG_SPIX_W(...)
#    define NXPLOG_SPIX_E(...)
#endif /* Logging APIs used by SPIx module */

/* Logging APIs used by NxpSpiR module */
#if (ENABLE_SPIR_TRACES == TRUE )
#    define NXPLOG_SPIR_D(...)  {if(gLog_level.spir_log_level >= NXPLOG_LOG_DEBUG_LOGLEVEL) LOG_PRI(ANDROID_LOG_DEBUG,NXPLOG_ITEM_SPIR,__VA_ARGS__);}
#    define NXPLOG_SPIR_W(...)  {if(gLog_level.spir_log_level >= NXPLOG_LOG_WARN_LOGLEVEL) LOG_PRI(ANDROID_LOG_WARN,NXPLOG_ITEM_SPIR,__VA_ARGS__);}
#    define NXPLOG_SPIR_E(...)  {if(gLog_level.spir_log_level >= NXPLOG_LOG_ERROR_LOGLEVEL) LOG_PRI(ANDROID_LOG_ERROR,NXPLOG_ITEM_SPIR,__VA_ARGS__);}
#else
#    define NXPLOG_SPIR_D(...)
#    define NXPLOG_SPIR_W(...)
#    define NXPLOG_SPIR_E(...)
#endif /* Logging APIs used by SPIR module */

/* Logging APIs used by NxpFwDnld module */
#if (ENABLE_FWDNLD_TRACES == TRUE )
#    define NXPLOG_FWDNLD_D(...)  {if(gLog_level.dnld_log_level >= NXPLOG_LOG_DEBUG_LOGLEVEL) LOG_PRI(ANDROID_LOG_DEBUG,NXPLOG_ITEM_FWDNLD,__VA_ARGS__);}
#    define NXPLOG_FWDNLD_W(...)  {if(gLog_level.dnld_log_level >= NXPLOG_LOG_WARN_LOGLEVEL) LOG_PRI(ANDROID_LOG_WARN,NXPLOG_ITEM_FWDNLD,__VA_ARGS__);}
#    define NXPLOG_FWDNLD_E(...)  {if(gLog_level.dnld_log_level >= NXPLOG_LOG_ERROR_LOGLEVEL) LOG_PRI(ANDROID_LOG_ERROR,NXPLOG_ITEM_FWDNLD,__VA_ARGS__);}
#else
#    define NXPLOG_FWDNLD_D(...)
#    define NXPLOG_FWDNLD_W(...)
#    define NXPLOG_FWDNLD_E(...)
#endif /* Logging APIs used by NxpFwDnld module */

/* Logging APIs used by NxpTml module */
#if (ENABLE_TML_TRACES == TRUE )
#    define NXPLOG_TML_D(...)  {if(gLog_level.tml_log_level >= NXPLOG_LOG_DEBUG_LOGLEVEL) LOG_PRI(ANDROID_LOG_DEBUG,NXPLOG_ITEM_TML,__VA_ARGS__);}
#    define NXPLOG_TML_W(...)  {if(gLog_level.tml_log_level >= NXPLOG_LOG_WARN_LOGLEVEL) LOG_PRI(ANDROID_LOG_WARN,NXPLOG_ITEM_TML,__VA_ARGS__);}
#    define NXPLOG_TML_E(...)  {if(gLog_level.tml_log_level >= NXPLOG_LOG_ERROR_LOGLEVEL) LOG_PRI(ANDROID_LOG_ERROR,NXPLOG_ITEM_TML,__VA_ARGS__);}
#else
#    define NXPLOG_TML_D(...)
#    define NXPLOG_TML_W(...)
#    define NXPLOG_TML_E(...)
#endif /* Logging APIs used by NxpTml module */

#ifdef NXP_HCI_REQ
/* Logging APIs used by NxpHcpX module */
#if (ENABLE_HCPX_TRACES == TRUE )
#    define NXPLOG_HCPX_D(...)  {if(gLog_level.dnld_log_level >= NXPLOG_LOG_DEBUG_LOGLEVEL) LOG_PRI(ANDROID_LOG_DEBUG,NXPLOG_ITEM_FWDNLD,__VA_ARGS__);}
#    define NXPLOG_HCPX_W(...)  {if(gLog_level.dnld_log_level >= NXPLOG_LOG_WARN_LOGLEVEL) LOG_PRI(ANDROID_LOG_WARN,NXPLOG_ITEM_FWDNLD,__VA_ARGS__);}
#    define NXPLOG_HCPX_E(...)  {if(gLog_level.dnld_log_level >= NXPLOG_LOG_ERROR_LOGLEVEL) LOG_PRI(ANDROID_LOG_ERROR,NXPLOG_ITEM_FWDNLD,__VA_ARGS__);}
#else
#    define NXPLOG_HCPX_D(...)
#    define NXPLOG_HCPX_W(...)
#    define NXPLOG_HCPX_E(...)
#endif /* Logging APIs used by NxpHcpX module */

/* Logging APIs used by NxpHcpR module */
#if (ENABLE_HCPR_TRACES == TRUE )
#    define NXPLOG_HCPR_D(...)  {if(gLog_level.dnld_log_level >= NXPLOG_LOG_DEBUG_LOGLEVEL) LOG_PRI(ANDROID_LOG_DEBUG,NXPLOG_ITEM_FWDNLD,__VA_ARGS__);}
#    define NXPLOG_HCPR_W(...)  {if(gLog_level.dnld_log_level >= NXPLOG_LOG_WARN_LOGLEVEL) LOG_PRI(ANDROID_LOG_WARN,NXPLOG_ITEM_FWDNLD,__VA_ARGS__);}
#    define NXPLOG_HCPR_E(...)  {if(gLog_level.dnld_log_level >= NXPLOG_LOG_ERROR_LOGLEVEL) LOG_PRI(ANDROID_LOG_ERROR,NXPLOG_ITEM_FWDNLD,__VA_ARGS__);}
#else
#    define NXPLOG_HCPR_D(...)
#    define NXPLOG_HCPR_W(...)
#    define NXPLOG_HCPR_E(...)
#endif /* Logging APIs used by NxpHcpR module */
#endif /* NXP_HCI_REQ */

#ifdef NXP_VRBS_REQ
#if (ENABLE_EXTNS_TRACES == TRUE )
#    define NXPLOG_EXTNS_ENTRY() NXPLOG_FUNC_ENTRY(NXPLOG_ITEM_EXTNS)
#    define NXPLOG_EXTNS_EXIT()  NXPLOG_FUNC_EXIT(NXPLOG_ITEM_EXTNS)
#else
#    define NXPLOG_EXTNS_ENTRY()
#    define NXPLOG_EXTNS_EXIT()
#endif

#if (ENABLE_HAL_TRACES == TRUE )
#    define NXPLOG_SPIHAL_ENTRY() NXPLOG_FUNC_ENTRY(NXPLOG_ITEM_SPIHAL)
#    define NXPLOG_SPIHAL_EXIT()  NXPLOG_FUNC_EXIT(NXPLOG_ITEM_SPIHAL)
#else
#    define NXPLOG_SPIHAL_ENTRY()
#    define NXPLOG_SPIHAL_EXIT()
#endif

#if (ENABLE_SPIX_TRACES == TRUE )
#    define NXPLOG_SPIX_ENTRY() NXPLOG_FUNC_ENTRY(NXPLOG_ITEM_SPIX)
#    define NXPLOG_SPIX_EXIT()  NXPLOG_FUNC_EXIT(NXPLOG_ITEM_SPIX)
#else
#    define NXPLOG_SPIX_ENTRY()
#    define NXPLOG_SPIX_EXIT()
#endif

#if (ENABLE_SPIR_TRACES == TRUE )
#    define NXPLOG_SPIR_ENTRY() NXPLOG_FUNC_ENTRY(NXPLOG_ITEM_SPIR)
#    define NXPLOG_SPIR_EXIT()  NXPLOG_FUNC_EXIT(NXPLOG_ITEM_SPIR)
#else
#    define NXPLOG_SPIR_ENTRY()
#    define NXPLOG_SPIR_EXIT()
#endif

#ifdef NXP_HCI_REQ

#if (ENABLE_HCPX_TRACES == TRUE )
#    define NXPLOG_HCPX_ENTRY() NXPLOG_FUNC_ENTRY(NXPLOG_ITEM_HCPX)
#    define NXPLOG_HCPX_EXIT()  NXPLOG_FUNC_EXIT(NXPLOG_ITEM_HCPX)
#else
#    define NXPLOG_HCPX_ENTRY()
#    define NXPLOG_HCPX_EXIT()
#endif

#if (ENABLE_HCPR_TRACES == TRUE )
#    define NXPLOG_HCPR_ENTRY() NXPLOG_FUNC_ENTRY(NXPLOG_ITEM_HCPR)
#    define NXPLOG_HCPR_EXIT()  NXPLOG_FUNC_EXIT(NXPLOG_ITEM_HCPR)
#else
#    define NXPLOG_HCPR_ENTRY()
#    define NXPLOG_HCPR_EXIT()
#endif
#endif /* NXP_HCI_REQ */

#endif /* NXP_VRBS_REQ */

void phNxpLog_InitializeLogLevel(void);

#endif /* NXPLOG__H_INCLUDED */
