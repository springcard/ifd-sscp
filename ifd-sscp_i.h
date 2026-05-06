#ifndef __IFD_SSCP_I_H__
#define __IFD_SSCP_I_H__

#include "ifd-sscp.h"

BOOL IFDHParseDeviceParameters(LPSTR Device, char **parsedDevice, BYTE *address, DWORD *bitrate,
                               BYTE authKey[IFDH_SSCP_AUTH_KEY_LENGTH], BOOL *hasAuthKey);

RESPONSECODE IFDH_SSCP_Control(IFDH_SSCP_INSTANCE_ST *instance);

BOOL IFDH_SSCP_SetDefaultLEDs(IFDH_SSCP_INSTANCE_ST *instance);

DWORD IFDH_SSCP_Now(void);

#endif
