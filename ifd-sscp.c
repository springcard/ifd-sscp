#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <unistd.h>
#include <pthread.h>
#include <ifdhandler.h>

#include "ifd-sscp.h"
#include "ifd-sscp_i.h"

const char *Name = "libifd-sscp.so";

#define TRANSMIT_TIMEOUT 3000

static RESPONSECODE CreateChannelByNameOrChannel(DWORD Lun,	LPSTR Device, DWORD Channel)
{
    char *parsedDevice = NULL;
    BYTE address = IFDH_SSCP_DEFAULT_ADDRESS;
    DWORD bitrate = IFDH_SSCP_DEFAULT_BITRATE;
    BYTE authKey[IFDH_SSCP_AUTH_KEY_LENGTH];
    BOOL hasAuthKey = FALSE;

    (void) Channel;

    if (!IFDHParseDeviceParameters(Device, &parsedDevice, &address, &bitrate, authKey, &hasAuthKey))
        return IFD_NO_SUCH_DEVICE;

    if (!IFDHCreate(Lun, parsedDevice, address, bitrate, hasAuthKey ? authKey : NULL))
    {
        free(parsedDevice);
        return IFD_NO_SUCH_DEVICE;
    }

    free(parsedDevice);
    return IFD_SUCCESS;
}

RESPONSECODE IFDHCreateChannel(DWORD Lun, DWORD Channel)
{
    printf("%s:IFDHCreateChannel(Lun=%08X, Channel=%08X)\n", Name, Lun, Channel);
    return CreateChannelByNameOrChannel(Lun, NULL, Channel);
}

RESPONSECODE IFDHCreateChannelByName(DWORD Lun, LPSTR Device)
{
    printf("%s:IFDHCreateChannelByName(Lun=%08X, Device=%s)\n", Name, Lun, Device);
    return CreateChannelByNameOrChannel(Lun, Device, (DWORD) -1);
}

RESPONSECODE IFDHCloseChannel(DWORD Lun)
{
    printf("%s:IFDHCloseChannel(Lun=%08X)\n", Name, Lun);
    return IFD_SUCCESS;
}

RESPONSECODE IFDHControl(DWORD Lun, DWORD ControlCode, PUCHAR TxBuffer, DWORD TxLength,
                         PUCHAR RxBuffer, DWORD RxLength,
                         PDWORD RxReturnLength)
{
    printf("%s:IFDHControl(Lun=%08X)\n", Name, Lun);
    return IFD_SUCCESS;
}

static RESPONSECODE IFDHWaitCardProc(DWORD Lun, int Timeout)
{
    printf("%s:IFDHWaitCardProc(Lun=%08X, Timeout=%d)\n", Name, Lun, Timeout);
    if (!IFDHWaitStatusChange(Lun, Timeout))
        return IFD_COMMUNICATION_ERROR;
    if (!IFDHIsReaderOnline(Lun))
        return IFD_COMMUNICATION_ERROR;
    if (IFDHIsCardPresent(Lun))
        return IFD_ICC_PRESENT;
    return IFD_ICC_NOT_PRESENT;
}

static RESPONSECODE IFDHWaitCardKill(DWORD Lun, int Timeout)
{
    if (!IFDHKillStatusChange(Lun))
        return IFD_COMMUNICATION_ERROR;
    return IFD_SUCCESS;
}

RESPONSECODE IFDHGetCapabilities(DWORD Lun, DWORD Tag, PDWORD Length, PUCHAR Value)
{
    printf("%s:IFDHGetCapabilities(Lun=%08X, Tag=%08X)\n", Name, Lun, Tag);

    switch (Tag)
    {
        case TAG_IFD_ATR:
            if (!IFDHIsCardPresent(Lun))
                return IFD_ICC_NOT_PRESENT;
            if (!IFDHGetAtr(Lun, Value, Length))
                return IFD_COMMUNICATION_ERROR;
            break;
        case TAG_IFD_SIMULTANEOUS_ACCESS:
            *Value = 1;
            *Length = 1;
            break;
        case TAG_IFD_THREAD_SAFE:
            *Value = 0;
            *Length = 1;
            break;
        case TAG_IFD_SLOTS_NUMBER:
            *Value = 1;
            *Length = 1;
            break;
        case TAG_IFD_SLOT_THREAD_SAFE:
            *Value = 0;
            *Length = 1;
            break;
        case TAG_IFD_POLLING_THREAD_WITH_TIMEOUT:
            {
                *(void **)Value = IFDHWaitCardProc;
                *Length = sizeof(void *);                
            }
            break;
        case TAG_IFD_POLLING_THREAD_KILLABLE:
            *Value = 0;
            *Length = 1;
            break;
        case TAG_IFD_STOP_POLLING_THREAD:
            {
                *(void **)Value = IFDHWaitCardKill;
                *Length = sizeof(void *);
            }
            break;
        default:
            return IFD_ERROR_TAG;
    }

    return IFD_SUCCESS;
}

RESPONSECODE IFDHSetCapabilities(DWORD Lun, DWORD Tag, DWORD Length, PUCHAR Value)
{
    printf("%s:IFDHGetCapabilities(Lun=%08X)\n", Name, Lun);
    return IFD_ERROR_TAG;
}

RESPONSECODE IFDHPowerICC(DWORD Lun, DWORD Action, PUCHAR Atr, PDWORD AtrLength)
{
    printf("%s:IFDHPowerICC(Lun=%08X, Action=%lu)\n", Name, Lun, Action);
    if (!IFDHIsReaderOnline(Lun))
        return IFD_COMMUNICATION_ERROR;
    if (!IFDHIsCardPresent(Lun))
        return IFD_ICC_NOT_PRESENT;

    switch (Action)
    {
        case IFD_POWER_DOWN:
            if (AtrLength != NULL)
                *AtrLength = 0;
            if (!IFDHPowerDown(Lun))
                return IFD_COMMUNICATION_ERROR;
            break;

        case IFD_POWER_UP:
        case IFD_RESET:
            if (!IFDHPowerUp(Lun))
                return IFD_COMMUNICATION_ERROR;
            if (!IFDHGetAtr(Lun, Atr, AtrLength))
                return IFD_COMMUNICATION_ERROR;
            break;

        default:
            return IFD_NOT_SUPPORTED;
    }

    return IFD_SUCCESS;
}

RESPONSECODE IFDHTransmitToICC(DWORD Lun, SCARD_IO_HEADER SendPci,
                                PUCHAR TxBuffer, DWORD TxLength,
                                PUCHAR RxBuffer, PDWORD RxLength,
                                PSCARD_IO_HEADER RecvPci)
{
    printf("%s:IFDHTransmitToICC[In](Lun=%08X, TxLength=%lu)\n", Name, Lun, TxLength);
    if (!IFDHIsReaderOnline(Lun))
        return IFD_COMMUNICATION_ERROR;
    if (!IFDHIsCardPresent(Lun))
        return IFD_ICC_NOT_PRESENT;
    if (!IFDHAsyncTransmit(Lun, TxBuffer, TxLength, RxBuffer, *RxLength))
        return IFD_COMMUNICATION_ERROR;
    if (!IFDHWaitTransmit(Lun, TRANSMIT_TIMEOUT, RxLength))
        return IFD_COMMUNICATION_ERROR;
    printf("%s:IFDHTransmitToICC[Out](Lun=%08X, RxLength=%lu)\n", Name, Lun, *RxLength);
    return IFD_SUCCESS;
}

RESPONSECODE IFDHICCPresence(DWORD Lun)
{
    printf("%s:IFDHICCPresence(Lun=%08X)\n", Name, Lun);
    if (!IFDHIsReaderOnline(Lun))
        return IFD_COMMUNICATION_ERROR;
    if (!IFDHIsCardPresent(Lun))
        return IFD_ICC_NOT_PRESENT;
    return IFD_ICC_PRESENT;
}

__attribute__((constructor))
void on_load(void)
{
    printf("%s:Library loaded\n", Name);
}
