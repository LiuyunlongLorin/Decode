#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "rtsp_client_log.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

static RTSP_CLIENT_LOG_E g_senClientLogLevel = RTSP_CLIENT_LOG_ERR;
static RTSP_CLIENT_OUTPUT_FUNC g_spfnClientOutput = HI_NULL;

HI_S32 RTSP_CLIENT_LOG_SetEnabledLevel(RTSP_CLIENT_LOG_E enLevel)
{
    g_senClientLogLevel = enLevel;
    return HI_SUCCESS;
}

HI_S32 RTSP_CLIENT_LOG_SetOutputFunc(RTSP_CLIENT_OUTPUT_FUNC pFunc)
{
    g_spfnClientOutput = pFunc;
    return HI_SUCCESS;
}

HI_S32 RTSP_CLIENT_LOG_Printf(const HI_CHAR* pszModName, RTSP_CLIENT_LOG_E enLevel, const HI_CHAR* pszFmt, ...)
{
    if ( enLevel < g_senClientLogLevel )
    {
        return HI_SUCCESS;
    }

    if (  !pszModName || !pszFmt)
    {
        return HI_FAILURE;
    }

    va_list args;
    if ( !g_spfnClientOutput )
    {
        printf("[%s] ",  pszModName);
        va_start( args, pszFmt );
        vprintf(pszFmt, args);
        va_end( args );
    }
    else
    {
        g_spfnClientOutput("[%s] ",  pszModName);
        va_start( args, pszFmt );
        g_spfnClientOutput(pszFmt, args);
        va_end( args );
    }

    return HI_SUCCESS;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
