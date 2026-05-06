#include "ifd-sscp.h"
#include "ifd-sscp_i.h"

extern BOOL SSCP_DEBUG_SERIAL;
extern BOOL SSCP_DEBUG_EXCHANGE;

typedef struct _instance_list_st
{    
    struct _instance_list_st *next;
    DWORD Lun;
    IFDH_SSCP_INSTANCE_ST instance;
} instance_list_st;

static instance_list_st *instance_list = NULL;

static IFDH_SSCP_INSTANCE_ST *getInstance(DWORD Lun)
{
    instance_list_st *current = instance_list;
    while (current != NULL)
    {
        if (current->Lun == Lun)
            return &current->instance;
        current = current->next;
    }
    IFDH_LOG_CRITICAL("Instance with Lun %lu not found", Lun);
    return NULL;
}

static IFDH_SSCP_INSTANCE_ST *allocInstance(DWORD Lun)
{
    instance_list_st *current = instance_list;
    while (current != NULL)
    {
        if (current->Lun == Lun)
        {
            IFDH_LOG_CRITICAL("Instance with Lun %lu already exists", Lun);
            return NULL;
        }
        current = current->next;
    }
    instance_list_st *newInstance = (instance_list_st *)calloc(1, sizeof(instance_list_st));
    if (newInstance == NULL)
    {
        IFDH_LOG_CRITICAL("Failed to allocate memory for new instance");
        return NULL;
    }
    newInstance->Lun = Lun;
    newInstance->next = instance_list;
    instance_list = newInstance;
    return &newInstance->instance;
}

static void freeInstance(DWORD Lun)
{
    instance_list_st *current = instance_list;
    instance_list_st *previous = NULL;
    while (current != NULL)
    {
        if (current->Lun == Lun)
        {
            if (previous == NULL)
                instance_list = current->next;
            else
                previous->next = current->next;
            free(current);
            return;
        }
        previous = current;
        current = current->next;
    }
}

static void *IFDH_SSCP_Proc(void *arg)
{
    IFDH_SSCP_INSTANCE_ST *instance = (IFDH_SSCP_INSTANCE_ST *)arg;
    LONG rc;

    IFDH_LOG_INFO("Thread starting");

    while (1)
    {
        (void) WaitEvent(&instance->actionEvent, 150); /* Default polling interval, don't care for result */

        if (!instance->running)
        {
            IFDH_LOG_INFO("Not running anymore");
            break;
        }

        Lock(instance);
        if ((instance->readerState.open) && (instance->readerState.available))
        {
            switch (instance->readerAction)
            {
                case IFDH_SSCP_ACTION_IDLE :
                    if ((instance->cardState.present) && (instance->cardState.active))
                    {
                        /* The card is active, we shall not do a polling, but a tracking. Let's track with empty APDUs, in the hope the reader supports it */
                        IFDH_LOG_PERIODIC("Tracking...");
                        rc = SSCP_TransceiveNFC(instance->sscp_ctx, NULL, 0, NULL, 0, NULL);
                        if (rc == SSCP_SUCCESS)
                        {
                            /* Success, card is still there */
                            IFDH_LOG_PERIODIC("Tracking: card still present");
                        }
                        else if ((rc == SSCP_ERR_NFC_CARD_MUTE_OR_REMOVED) || (rc == SSCP_ERR_NFC_CARD_COMM_ERROR))
                        {
                            IFDH_LOG_INFO("Tracking: card lost");
                            /* Reset card data */
                            memset(&instance->cardState, 0, sizeof(instance->cardState));
                            /* Say we have lost the card */
                            SetEvent(&instance->statusEvent);
                        }
                        else if (rc == 2)
                        {
                            IFDH_LOG_INFO("Tracking: not supported by the reader");
                        }
                        else
                        {
                            IFDH_LOG_CRITICAL("Tracking: reader error %d", rc);
                            /* We have lost the reader? */
                            instance->readerState.available = FALSE;
                        }
                    }
                    else
                    {
                        /* The card is either absent or not active, we can do the polling */
                        IFDH_LOG_PERIODIC("Polling...");
                        rc = SSCP_ScanNFC(instance->sscp_ctx, &instance->cardState.protocol, instance->cardState.uid, sizeof(instance->cardState.uid), &instance->cardState.uidLength, instance->cardState.ats, sizeof(instance->cardState.ats), &instance->cardState.atsLength);
                        if (rc == SSCP_SUCCESS)
                        {
                            BOOL oldCardPresent = instance->cardState.present;
                            if (instance->cardState.protocol)
                            {
                                IFDH_LOG_INFO("Polling: card inserted, protocol=%04X", instance->cardState.protocol);
                                instance->cardState.present = TRUE;
                            }
                            else
                            {
                                IFDH_LOG_INFO("Polling: card inserted, but protocol=0");
                                instance->cardState.present = FALSE;
                            }
                            /* Status has changed! */
                            if (oldCardPresent != instance->cardState.present)
                                SetEvent(&instance->statusEvent);
                        }
                        else
                        {
                            IFDH_LOG_CRITICAL("Polling: reader error %d", rc);
                            instance->readerState.available = FALSE;
                        }
                    }
                break;
                case IFDH_SSCP_ACTION_CONTROL :
                    instance->x.control.responseCode = IFDH_SSCP_Control(instance);
                    instance->readerAction = IFDH_SSCP_ACTION_CONTROL_RESP;
                    SetEvent(&instance->responseEvent);
                break;
                case IFDH_SSCP_ACTION_CONTROL_RESP :
                    /* Do nothing, let the client retrieve its response */
                break;
                case IFDH_SSCP_ACTION_TRANSMIT :
                    rc = SSCP_TransceiveNFC(instance->sscp_ctx, instance->x.transmit.txBuffer, instance->x.transmit.txLength, instance->x.transmit.rxBuffer, instance->x.transmit.rxLengthMax, &instance->x.transmit.rxLengthAct);
                    if (rc == SSCP_SUCCESS)
                    {
                        /* Success, response is ready */
                        instance->readerAction = IFDH_SSCP_ACTION_TRANSMIT_RESP;                                
                    }
                    else if ((rc == SSCP_ERR_NFC_CARD_MUTE_OR_REMOVED) || (rc == SSCP_ERR_NFC_CARD_COMM_ERROR))
                    {
                        /* Not a reader error, but a card error */
                        instance->readerAction = IFDH_SSCP_ACTION_IDLE;
                        /* Reset card data */
                        memset(&instance->cardState, 0, sizeof(instance->cardState));
                        /* Say we have lost the card */
                        SetEvent(&instance->statusEvent);
                    }
                    else
                    {
                        /* We have lost the reader? */
                        IFDH_LOG_CRITICAL("Transmit: reader error %d", rc);
                        instance->readerState.available = FALSE;
                    }
                    SetEvent(&instance->responseEvent);
                break;
                case IFDH_SSCP_ACTION_TRANSMIT_RESP :
                    /* Do nothing, let the client retrieve its response */
                break;
                case  IFDH_SSCP_ACTION_DISCONNECT :
                    rc = SSCP_ReleaseNFC(instance->sscp_ctx);
                    if (rc == SSCP_SUCCESS)
                    {
                        instance->readerAction = IFDH_SSCP_ACTION_IDLE;
                    }
                    else
                    {
                        IFDH_LOG_CRITICAL("Disconnect: reader error %d", rc);
                        instance->readerState.available = FALSE;
                    }
                break;

                default:
                    IFDH_LOG_CRITICAL("Invalid reader action %d", instance->readerAction);
                    instance->readerAction = IFDH_SSCP_ACTION_IDLE;
                    break;
            }
        }
        Unlock(instance);
    }

    IFDH_LOG_INFO("Thread terminating");
}

static BOOL IFDHOpen(IFDH_SSCP_INSTANCE_ST *instance)
{
    LONG rc;

    if (instance == NULL)
        return FALSE;

    memset(&instance->readerState, 0, sizeof(instance->readerState));

    /* Try to open the reader */
    rc = SSCP_Open(instance->sscp_ctx, instance->device, instance->bitrate, 0);
    if (rc != SSCP_SUCCESS)
    {
        IFDH_LOG_CRITICAL("SSCP_Open(%s, %lu) failed (err. %d)", instance->device, instance->bitrate, rc);
        return FALSE;
    }

    /* Select the target address locally; SSCP_SetAddress writes a new address to the reader. */
	rc = SSCP_SelectAddress(instance->sscp_ctx, instance->address);
	if (rc)
	{
		IFDH_LOG_CRITICAL("SSCP_SelectAddress(%02X) failed (err. %d)", instance->address, rc);
		SSCP_Close(instance->sscp_ctx);
        return FALSE;
	}

	rc = SSCP_Authenticate(instance->sscp_ctx, instance->hasAuthKey ? instance->authKey : NULL);
	if (rc)
	{
		IFDH_LOG_CRITICAL("SSCP_Authenticate failed (err. %d)", rc);
		SSCP_Close(instance->sscp_ctx);
        return FALSE;
	}

	rc = SSCP_Outputs(instance->sscp_ctx, 0x02, 0x0A, 0x02);
	if (rc)
	{
		IFDH_LOG_CRITICAL("SSCP_Outputs failed (err. %d)", rc);
		SSCP_Close(instance->sscp_ctx);
        return FALSE;
	}

    rc = SSCP_GetInfos(instance->sscp_ctx, &instance->readerInfo.version, &instance->readerInfo.baudrate, &instance->readerInfo.address, &instance->readerInfo.voltage);
    if (rc)
    {
        IFDH_LOG_CRITICAL("SSCP_GetInfos failed (err. %d)", rc);
		SSCP_Close(instance->sscp_ctx);
        return FALSE;
    }
    IFDH_LOG_INFO("SSCP_GetInfos OK, version=%02X, baudrate=%02X, address=%02X, voltage=%04X",
                  instance->readerInfo.version, instance->readerInfo.baudrate, instance->readerInfo.address, instance->readerInfo.voltage);

    rc = SSCP_GetSerialNumber(instance->sscp_ctx, instance->readerInfo.serialNumber, sizeof(instance->readerInfo.serialNumber));
    if (rc)
    {
        IFDH_LOG_CRITICAL("SSCP_GetSerialNumber failed (err. %d)", rc);
		SSCP_Close(instance->sscp_ctx);
        return FALSE;
    }
    IFDH_LOG_INFO("SSCP_GetSerialNumber OK, serialNumber=%s", instance->readerInfo.serialNumber);

    rc = SSCP_GetReaderType(instance->sscp_ctx, instance->readerInfo.readerType, sizeof(instance->readerInfo.readerType));
    if (rc)
    {
        IFDH_LOG_CRITICAL("SSCP_GetReaderType failed (err. %d)", rc);
		SSCP_Close(instance->sscp_ctx);
        return FALSE;
    }
    IFDH_LOG_INFO("SSCP_GetReaderType OK, readerType=%s", instance->readerInfo.readerType);

    instance->readerState.open = TRUE;
    instance->readerState.available = TRUE;
    return TRUE;
}

BOOL IFDHCreate(DWORD Lun, LPSTR Device, UCHAR Address, DWORD Bitrate, const BYTE AuthKey[IFDH_SSCP_AUTH_KEY_LENGTH])
{
    IFDH_SSCP_INSTANCE_ST *instance = NULL;
    BOOL mutexCreated = FALSE;
    BOOL statusEventCreated = FALSE;
    BOOL actionEventCreated = FALSE;
    BOOL responseEventCreated = FALSE;

    SSCP_DEBUG_SERIAL = FALSE;
    SSCP_DEBUG_EXCHANGE = FALSE;

    if ((Device == NULL) || (Device[0] == '\0'))
    {
        IFDH_LOG_CRITICAL("Invalid device");
        return FALSE;
    }

    /* Allocate the global variables */
    instance = allocInstance(Lun);
    if (instance == NULL)
    {
        IFDH_LOG_CRITICAL("Alloc variables failed");
        return FALSE;
    }
    instance->sscp_ctx = SSCP_Alloc();
    if (instance->sscp_ctx == NULL)
    {
        IFDH_LOG_CRITICAL("Alloc context failed");
        freeInstance(Lun);
        return FALSE;
    }

    /* Initialize the variables */
    instance->address = Address;
    instance->bitrate = (Bitrate != 0) ? Bitrate : IFDH_SSCP_DEFAULT_BITRATE;
    if (AuthKey != NULL)
    {
        memcpy(instance->authKey, AuthKey, IFDH_SSCP_AUTH_KEY_LENGTH);
        instance->hasAuthKey = TRUE;
    }
    instance->device = strdup(Device);
    if (instance->device == NULL)
    {
        IFDH_LOG_CRITICAL("Alloc device failed");
        goto failed;
    }
    if (!CreateMutex(&instance->mutex))
    {
        IFDH_LOG_CRITICAL("Create mutex failed");
        goto failed;
    }
    mutexCreated = TRUE;
    if (!CreateEvent(&instance->statusEvent))
    {
        IFDH_LOG_CRITICAL("Create event failed");
        goto failed;
    }
    statusEventCreated = TRUE;
    if (!CreateEvent(&instance->actionEvent))
    {
        IFDH_LOG_CRITICAL("Create event failed");
        goto failed;
    }
    actionEventCreated = TRUE;
    if (!CreateEvent(&instance->responseEvent))
    {
        IFDH_LOG_CRITICAL("Create event failed");
        goto failed;
    }
    responseEventCreated = TRUE;
    instance->running = TRUE;

    /* Open the device */
    if (!IFDHOpen(instance))
    {
        IFDH_LOG_CRITICAL("Open device %s:%02X at %lu failed", Device, Address, instance->bitrate);
        goto failed;
    }

    /* Create the thread */
    if (pthread_create(&instance->thread_id, NULL, IFDH_SSCP_Proc, instance) != 0)
    {
        IFDH_LOG_CRITICAL("Failed to start SSCP thread");
        goto failed;
    }

    return TRUE;

failed:
    /* Try to de-allocated correctly */
    if (responseEventCreated)
        DestroyEvent(&instance->responseEvent);
    if (actionEventCreated)
        DestroyEvent(&instance->actionEvent);
    if (statusEventCreated)
        DestroyEvent(&instance->statusEvent);
    if (mutexCreated)
        DestroyMutex(&instance->mutex);
    if (instance->device != NULL)
        free(instance->device);
    if (instance->sscp_ctx != NULL)
        SSCP_Free(instance->sscp_ctx);
    freeInstance(Lun);
    return FALSE;
}

BOOL IFDHDestroy(DWORD Lun)
{
    IFDH_SSCP_INSTANCE_ST *instance = getInstance(Lun);
    if (instance == NULL)
        return FALSE;

    /* Say we want to exit */
    instance->running = FALSE;

    /* Wakeup the thread */
    SetEvent(&instance->actionEvent);

    /* Join the thread */
    if (pthread_join(instance->thread_id, NULL) != 0)
    {
        IFDH_LOG_CRITICAL("Failed to stop the driver thread");
        return FALSE;
    }

    /* Close the reader */
    if (instance->sscp_ctx != NULL)
        SSCP_Close(instance->sscp_ctx);

    /* Try to de-allocated correctly */
    DestroyEvent(&instance->responseEvent);
    DestroyEvent(&instance->actionEvent);
    DestroyEvent(&instance->statusEvent);
    DestroyMutex(&instance->mutex);

    /* Free the global variables */
    if (instance->sscp_ctx != NULL)
        SSCP_Free(instance->sscp_ctx);
    if (instance->device != NULL)
        free(instance->device);
    freeInstance(Lun);
    return TRUE;
}

BOOL IFDHIsReaderOnline(DWORD Lun)
{
    BOOL rc = FALSE;
    IFDH_SSCP_INSTANCE_ST *instance = getInstance(Lun);
    if (instance == NULL)
        return FALSE;
    Lock(instance);
    if (instance->readerState.available)
        rc = TRUE;
    Unlock(instance);
    return rc;
}

BOOL IFDHIsCardPresent(DWORD Lun)
{
    BOOL rc = FALSE;
    IFDH_SSCP_INSTANCE_ST *instance = getInstance(Lun);
    if (instance == NULL)
        return FALSE;
    Lock(instance);
    if (instance->readerState.available)
        if (instance->cardState.present)
            rc = TRUE;
    Unlock(instance);
    return rc;
}

BOOL IFDHWaitStatusChange(DWORD Lun, int Timeout)
{
    IFDH_SSCP_INSTANCE_ST *instance = getInstance(Lun);
    if (instance == NULL)
        return FALSE;
    return WaitEvent(&instance->statusEvent, Timeout);
}

BOOL IFDHKillStatusChange(DWORD Lun)
{
    IFDH_SSCP_INSTANCE_ST *instance = getInstance(Lun);
    if (instance == NULL)
        return FALSE;
    return SetEvent(&instance->statusEvent);
}

BOOL IFDHGetAtr(DWORD Lun, PUCHAR Atr, PDWORD AtrLength)
{
    static const BYTE DEFAULT_ATR[] = { 0x3B, 0x88, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x51, 0x93, 0x00, 0xCB };
    BOOL rc = FALSE;
    IFDH_SSCP_INSTANCE_ST *instance = getInstance(Lun);
    if (instance == NULL)
        return FALSE;
    if ((Atr == NULL) || (AtrLength == NULL))
        return FALSE;
    if (Lock(instance))
    {
        if ((instance->readerState.available) && (instance->cardState.present))
        {
            if (*AtrLength > sizeof(DEFAULT_ATR))
                *AtrLength = sizeof(DEFAULT_ATR);
            memcpy(Atr, DEFAULT_ATR, *AtrLength);
            rc = TRUE;
        }
        Unlock(instance);
    }
    return rc;
}

BOOL IFDHPowerUp(DWORD Lun)
{
    BOOL rc = FALSE;
    IFDH_SSCP_INSTANCE_ST *instance = getInstance(Lun);
    if (instance == NULL)
        return FALSE;
    if (Lock(instance))
    {
        if ((instance->readerState.available) && (instance->cardState.present))
        {
            /* Remember the card is active */
            instance->cardState.active = TRUE;
            /* No need to wakeup the SSCP thread, it has nothing to do */
            rc = TRUE;
        }
        Unlock(instance);
    }
    return rc;
}

BOOL IFDHPowerDown(DWORD Lun)
{
    BOOL rc = FALSE;
    IFDH_SSCP_INSTANCE_ST *instance = getInstance(Lun);
    if (instance == NULL)
        return FALSE;
    if (Lock(instance))
    {
        if ((instance->readerState.available) && (instance->cardState.present) && (instance->readerAction == IFDH_SSCP_ACTION_IDLE))
        {
            /* Release the card */
            instance->cardState.active = FALSE;
            /* Tell the SSCP thread we have something to do */
            instance->readerAction = IFDH_SSCP_ACTION_DISCONNECT;
            SetEvent(&instance->actionEvent); /* Wakeup the SSCP thread */
            rc = TRUE;
        }
        Unlock(instance);
    }
    return rc;
}

BOOL IFDHAsyncTransmit(DWORD Lun, PUCHAR TxBuffer, DWORD TxLength, PUCHAR RxBuffer, DWORD RxLength)
{
    BOOL rc = FALSE;
    IFDH_SSCP_INSTANCE_ST *instance = getInstance(Lun);
    if (instance == NULL)
        return FALSE;
    if (Lock(instance))
    {
        if ((instance->readerState.available) && (instance->cardState.present) && (instance->readerAction == IFDH_SSCP_ACTION_IDLE))
        {
            /* Prevent further polling */
            instance->cardState.active = TRUE;
            /* Store the buffer */
            instance->x.transmit.txBuffer = TxBuffer;
            instance->x.transmit.txLength = TxLength;
            instance->x.transmit.rxBuffer = RxBuffer;
            instance->x.transmit.rxLengthMax = RxLength;
            /* Be ready to receive */
            ClearEvent(&instance->responseEvent);
            /* Tell the SSCP thread we have something to transmit */
            instance->readerAction = IFDH_SSCP_ACTION_TRANSMIT;
            SetEvent(&instance->actionEvent); /* Wakeup the SSCP thread */
            rc = TRUE;
        }
        Unlock(instance);
    }
    return rc;
}

BOOL IFDHWaitTransmit(DWORD Lun, int Timeout, PDWORD RxLength)
{
    BOOL rc = FALSE;
    IFDH_SSCP_INSTANCE_ST *instance = getInstance(Lun);
    if (instance == NULL)
        return FALSE;
    if (!WaitEvent(&instance->responseEvent, Timeout))
        return FALSE;
    if (Lock(instance))
    {
        if (instance->readerAction == IFDH_SSCP_ACTION_TRANSMIT_RESP)
        {
            /* Retrieve the length of the response */
            *RxLength = instance->x.transmit.rxLengthAct;
            /* No more pending action */
            instance->readerAction = IFDH_SSCP_ACTION_IDLE;
            /* No need to wakeup the SSCP thread, let its timeout expire */
            rc = TRUE;
        }
        Unlock(instance);
    }
    return rc;
}

BOOL IFDHAsyncControl(DWORD Lun, DWORD ControlCode, PUCHAR TxBuffer, DWORD TxLength, PUCHAR RxBuffer, DWORD RxLength)
{
    BOOL rc = FALSE;
    IFDH_SSCP_INSTANCE_ST *instance = getInstance(Lun);
    if (instance == NULL)
        return FALSE;
    if (Lock(instance))
    {
        if ((instance->readerState.available) && (instance->readerAction == IFDH_SSCP_ACTION_IDLE))
        {
            /* Store the buffer */
            instance->x.control.controlCode = ControlCode;
            instance->x.control.txBuffer = TxBuffer;
            instance->x.control.txLength = TxLength;
            instance->x.control.rxBuffer = RxBuffer;
            instance->x.control.rxLengthMax = RxLength;
            instance->x.control.rxLengthAct = 0;
            instance->x.control.responseCode = IFD_COMMUNICATION_ERROR;
            /* Be ready to receive */
            ClearEvent(&instance->responseEvent);
            /* Tell the SSCP thread we have something to control */
            instance->readerAction = IFDH_SSCP_ACTION_CONTROL;
            SetEvent(&instance->actionEvent); /* Wakeup the SSCP thread */
            rc = TRUE;
        }
        Unlock(instance);
    }
    return rc;
}

BOOL IFDHWaitControl(DWORD Lun, int Timeout, PDWORD RxLength, RESPONSECODE *ControlResponse)
{
    BOOL rc = FALSE;
    IFDH_SSCP_INSTANCE_ST *instance = getInstance(Lun);
    if (instance == NULL)
        return FALSE;
    if (!WaitEvent(&instance->responseEvent, Timeout))
        return FALSE;
    if (Lock(instance))
    {
        if (instance->readerAction == IFDH_SSCP_ACTION_CONTROL_RESP)
        {
            /* Retrieve the length of the response */
            if (RxLength != NULL)
                *RxLength = instance->x.control.rxLengthAct;
            if (ControlResponse != NULL)
                *ControlResponse = instance->x.control.responseCode;
            /* No more pending action */
            instance->readerAction = IFDH_SSCP_ACTION_IDLE;
            /* No need to wakeup the SSCP thread, let its timeout expire */
            rc = TRUE;
        }
        Unlock(instance);
    }
    return rc;
}
