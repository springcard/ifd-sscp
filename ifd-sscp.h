#ifndef __IFD_SSCP_H__
#define __IFD_SSCP_H__

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#define WIN32_TYPES_DEFINED
#include <PCSC/wintypes.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <ifdhandler.h>
#include <errno.h>
#include <time.h>

#include <sscp-host.h>
#include "ifd-sscp-debug.h"

#define IFDH_SSCP_DEFAULT_ADDRESS 0x01
#define IFDH_SSCP_DEFAULT_BITRATE 38400
#define IFDH_SSCP_AUTH_KEY_LENGTH 16

BOOL IFDHCreate(DWORD Lun, LPSTR Device, UCHAR Address, DWORD Bitrate, const BYTE AuthKey[IFDH_SSCP_AUTH_KEY_LENGTH]);
BOOL IFDHDestroy(DWORD Lun);

BOOL IFDHIsReaderOnline(DWORD Lun);
BOOL IFDHIsCardPresent(DWORD Lun);
BOOL IFDHWaitStatusChange(DWORD Lun, int Timeout);
BOOL IFDHKillStatusChange(DWORD Lun);
BOOL IFDHPowerUp(DWORD Lun);
BOOL IFDHPowerDown(DWORD Lun);
BOOL IFDHGetAtr(DWORD Lun, PUCHAR Atr, PDWORD AtrLength);
BOOL IFDHAsyncTransmit(DWORD Lun, PUCHAR TxBuffer, DWORD TxLength, PUCHAR RxBuffer, DWORD RxLength);
BOOL IFDHWaitTransmit(DWORD Lun, int Timeout, PDWORD RxLength);
BOOL IFDHAsyncControl(DWORD Lun, DWORD ControlCode, PUCHAR TxBuffer, DWORD TxLength, PUCHAR RxBuffer, DWORD RxLength);
BOOL IFDHWaitControl(DWORD Lun, int Timeout, PDWORD RxLength, RESPONSECODE *ControlResponse);

#define IFDH_SSCP_CONTROL_OUTPUTS SSCP_CMD_OUTPUTS
#define IFDH_SSCP_CONTROL_OUTPUTS_RGB SSCP_CMD_OUTPUT_RGB
#define IFDH_SSCP_CONTROL_EXTERNAL_LED_RGB SSCP_CMD_EXTERNAL_LED_COLORS

#define IFDH_SSCP_ACTION_IDLE 0
#define IFDH_SSCP_ACTION_CONTROL 1
#define IFDH_SSCP_ACTION_CONTROL_RESP 2
#define IFDH_SSCP_ACTION_TRANSMIT 3
#define IFDH_SSCP_ACTION_TRANSMIT_RESP 4
#define IFDH_SSCP_ACTION_DISCONNECT 5

typedef struct 
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    BOOL signaled;
} pthread_event_t;

typedef struct 
{
    pthread_t thread_id;
    pthread_mutex_t mutex;
    pthread_event_t statusEvent;
    pthread_event_t actionEvent;
    pthread_event_t responseEvent;
    char *device;
    BYTE address;
    DWORD bitrate;
    BOOL hasAuthKey;
    BYTE authKey[IFDH_SSCP_AUTH_KEY_LENGTH];
    SSCP_CTX_ST *sscp_ctx;
    BOOL running;
    BOOL inited;    

    struct
    {
        BOOL open;
        BOOL ready;
        DWORD timerOutput;
    } readerState;    

    BYTE readerAction;    

    struct
    {
        BYTE version;
        BYTE baudrate;
        BYTE address;
        WORD voltage;
        char serialNumber[16+1];
        char readerType[16+1];
    } readerInfo;    

    struct
    {
        BOOL present;
        BOOL active;
        WORD protocol;
        BYTE uid[16];
        BYTE uidLength;
        BYTE ats[16];
        BYTE atsLength;
        BOOL apduPassed;
    } cardState;

    union
    {
        struct
        {
            BYTE *txBuffer;
            DWORD txLength;
            BYTE *rxBuffer;
            DWORD rxLengthMax;
            DWORD rxLengthAct;
        } transmit;
        struct
        {
            DWORD controlCode;
            BYTE *txBuffer;
            DWORD txLength;
            BYTE *rxBuffer;
            DWORD rxLengthMax;
            DWORD rxLengthAct;
            RESPONSECODE responseCode;
        } control;
    } x;
    
} IFDH_SSCP_INSTANCE_ST;

BOOL CreateMutex(pthread_mutex_t *mutex);
void DestroyMutex(pthread_mutex_t *mutex);
BOOL CreateEvent(pthread_event_t *event);
void DestroyEvent(pthread_event_t *event);
BOOL SetEvent(pthread_event_t *event);
BOOL ClearEvent(pthread_event_t *event);
BOOL WaitEvent(pthread_event_t *event, int timeout);
BOOL Lock(IFDH_SSCP_INSTANCE_ST *instance);
void Unlock(IFDH_SSCP_INSTANCE_ST *instance);

extern const char *Name;

#endif
