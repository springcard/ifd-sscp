#include "ifd-sscp.h"
#include "ifd-sscp_i.h"

extern BOOL SSCP_DEBUG_SERIAL;
extern BOOL SSCP_DEBUG_EXCHANGE;

IFDH_SSCP_DATA_ST *global_vars = NULL;

static void *IFDH_SSCP_Proc(void *arg)
{
    IFDH_SSCP_DATA_ST *vars = (IFDH_SSCP_DATA_ST *)arg;
    LONG rc;

    IFDH_LOG_INFO("Thread starting");

    while (1)
    {
        (void) WaitEvent(&vars->actionEvent, 150); /* Default polling interval, don't care for result */

        if (!vars->running)
        {
            IFDH_LOG_INFO("Not running anymore");
            break;
        }

        Lock(vars);
        if ((vars->readerState.open) && (vars->readerState.available))
        {
            switch (vars->readerAction)
            {
                case IFDH_SSCP_ACTION_IDLE :
                    if ((vars->cardState.present) && (vars->cardState.active))
                    {
                        /* The card is active, we shall not do a polling, but a tracking. Let's track with empty APDUs, in the hope the reader supports it */
                        IFDH_LOG_PERIODIC("Tracking...");
                        rc = SSCP_TransceiveNFC(vars->sscp_ctx, NULL, 0, NULL, 0, NULL);
                        if (rc == SSCP_SUCCESS)
                        {
                            /* Success, card is still there */
                            IFDH_LOG_PERIODIC("Tracking: card still present");
                        }
                        else if ((rc == SSCP_ERR_NFC_CARD_MUTE_OR_REMOVED) || (rc == SSCP_ERR_NFC_CARD_COMM_ERROR))
                        {
                            IFDH_LOG_INFO("Tracking: card lost");
                            /* Reset card data */
                            memset(&vars->cardState, 0, sizeof(vars->cardState));
                            /* Say we have lost the card */
                            SetEvent(&vars->statusEvent);
                        }
                        else if (rc == 2)
                        {
                            IFDH_LOG_INFO("Tracking: not supported by the reader");
                        }
                        else
                        {
                            IFDH_LOG_CRITICAL("Tracking: reader error %d", rc);
                            /* We have lost the reader? */
                            vars->readerState.available = FALSE;
                        }
                    }
                    else
                    {
                        /* The card is either absent or not active, we can do the polling */
                        IFDH_LOG_PERIODIC("Polling...");
                        rc = SSCP_ScanNFC(vars->sscp_ctx, &vars->cardState.protocol, vars->cardState.uid, sizeof(vars->cardState.uid), &vars->cardState.uidLength, vars->cardState.ats, sizeof(vars->cardState.ats), &vars->cardState.atsLength);
                        if (rc == SSCP_SUCCESS)
                        {
                            BOOL oldCardPresent = vars->cardState.present;
                            if (vars->cardState.protocol)
                            {
                                IFDH_LOG_INFO("Polling: card inserted, protocol=%04X", vars->cardState.protocol);
                                vars->cardState.present = TRUE;
                            }
                            else
                            {
                                IFDH_LOG_INFO("Polling: card inserted, but protocol=0");
                                vars->cardState.present = FALSE;
                            }
                            /* Status has changed! */
                            if (oldCardPresent != vars->cardState.present)
                                SetEvent(&vars->statusEvent);
                        }
                        else
                        {
                            IFDH_LOG_CRITICAL("Polling: reader error %d", rc);
                            vars->readerState.available = FALSE;
                        }
                    }
                break;
                case IFDH_SSCP_ACTION_CONTROL :
                    vars->x.control.responseCode = IFDH_SSCP_Control(vars);
                    vars->readerAction = IFDH_SSCP_ACTION_CONTROL_RESP;
                    SetEvent(&vars->responseEvent);
                break;
                case IFDH_SSCP_ACTION_CONTROL_RESP :
                    /* Do nothing, let the client retrieve its response */
                break;
                case IFDH_SSCP_ACTION_TRANSMIT :
                    rc = SSCP_TransceiveNFC(vars->sscp_ctx, vars->x.transmit.txBuffer, vars->x.transmit.txLength, vars->x.transmit.rxBuffer, vars->x.transmit.rxLengthMax, &vars->x.transmit.rxLengthAct);
                    if (rc == SSCP_SUCCESS)
                    {
                        /* Success, response is ready */
                        vars->readerAction = IFDH_SSCP_ACTION_TRANSMIT_RESP;                                
                    }
                    else if ((rc == SSCP_ERR_NFC_CARD_MUTE_OR_REMOVED) || (rc == SSCP_ERR_NFC_CARD_COMM_ERROR))
                    {
                        /* Not a reader error, but a card error */
                        vars->readerAction = IFDH_SSCP_ACTION_IDLE;
                        /* Reset card data */
                        memset(&vars->cardState, 0, sizeof(vars->cardState));
                        /* Say we have lost the card */
                        SetEvent(&vars->statusEvent);
                    }
                    else
                    {
                        /* We have lost the reader? */
                        IFDH_LOG_CRITICAL("Transmit: reader error %d", rc);
                        vars->readerState.available = FALSE;
                    }
                    SetEvent(&vars->responseEvent);
                break;
                case IFDH_SSCP_ACTION_TRANSMIT_RESP :
                    /* Do nothing, let the client retrieve its response */
                break;
                case  IFDH_SSCP_ACTION_DISCONNECT :
                    rc = SSCP_ReleaseNFC(vars->sscp_ctx);
                    if (rc == SSCP_SUCCESS)
                    {
                        vars->readerAction = IFDH_SSCP_ACTION_IDLE;
                    }
                    else
                    {
                        IFDH_LOG_CRITICAL("Disconnect: reader error %d", rc);
                        vars->readerState.available = FALSE;
                    }
                break;

                default:
                    IFDH_LOG_CRITICAL("Invalid reader action %d", vars->readerAction);
                    vars->readerAction = IFDH_SSCP_ACTION_IDLE;
                    break;
            }
        }
        Unlock(vars);
    }

    IFDH_LOG_INFO("Thread terminating");
}

static BOOL IFDHOpen(IFDH_SSCP_DATA_ST *vars)
{
    LONG rc;

    if (vars == NULL)
        return FALSE;

    memset(&vars->readerState, 0, sizeof(vars->readerState));

    /* Try to open the reader */
    rc = SSCP_Open(vars->sscp_ctx, vars->device, vars->bitrate, 0);
    if (rc != SSCP_SUCCESS)
    {
        IFDH_LOG_CRITICAL("SSCP_Open(%s, %lu) failed (err. %d)", vars->device, vars->bitrate, rc);
        return FALSE;
    }

	rc = SSCP_SetAddress(vars->sscp_ctx, vars->address);
	if (rc)
	{
		IFDH_LOG_CRITICAL("SSCP_SetAddress(%02X) failed (err. %d)", vars->address, rc);
		SSCP_Close(vars->sscp_ctx);
        return FALSE;
	}

	rc = SSCP_Authenticate(vars->sscp_ctx, vars->hasAuthKey ? vars->authKey : NULL);
	if (rc)
	{
		IFDH_LOG_CRITICAL("SSCP_Authenticate failed (err. %d)", rc);
		SSCP_Close(vars->sscp_ctx);
        return FALSE;
	}

	rc = SSCP_Outputs(vars->sscp_ctx, 0x02, 0x0A, 0x02);
	if (rc)
	{
		IFDH_LOG_CRITICAL("SSCP_Outputs failed (err. %d)", rc);
		SSCP_Close(vars->sscp_ctx);
        return FALSE;
	}

    rc = SSCP_GetInfos(vars->sscp_ctx, &vars->readerInfo.version, &vars->readerInfo.baudrate, &vars->readerInfo.address, &vars->readerInfo.voltage);
    if (rc)
    {
        IFDH_LOG_CRITICAL("SSCP_GetInfos failed (err. %d)", rc);
		SSCP_Close(vars->sscp_ctx);
        return FALSE;
    }
    IFDH_LOG_INFO("SSCP_GetInfos OK, version=%02X, baudrate=%02X, address=%02X, voltage=%04X",
                  vars->readerInfo.version, vars->readerInfo.baudrate, vars->readerInfo.address, vars->readerInfo.voltage);

    rc = SSCP_GetSerialNumber(vars->sscp_ctx, vars->readerInfo.serialNumber, sizeof(vars->readerInfo.serialNumber));
    if (rc)
    {
        IFDH_LOG_CRITICAL("SSCP_GetSerialNumber failed (err. %d)", rc);
		SSCP_Close(vars->sscp_ctx);
        return FALSE;
    }
    IFDH_LOG_INFO("SSCP_GetSerialNumber OK, serialNumber=%s", vars->readerInfo.serialNumber);

    rc = SSCP_GetReaderType(vars->sscp_ctx, vars->readerInfo.readerType, sizeof(vars->readerInfo.readerType));
    if (rc)
    {
        IFDH_LOG_CRITICAL("SSCP_GetReaderType failed (err. %d)", rc);
		SSCP_Close(vars->sscp_ctx);
        return FALSE;
    }
    IFDH_LOG_INFO("SSCP_GetReaderType OK, readerType=%s", vars->readerInfo.readerType);

    vars->readerState.open = TRUE;
    vars->readerState.available = TRUE;
    return TRUE;
}

BOOL IFDHCreate(DWORD Lun, LPSTR Device, UCHAR Address, DWORD Bitrate, const BYTE AuthKey[IFDH_SSCP_AUTH_KEY_LENGTH])
{
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

    if (global_vars != NULL)
    {
        IFDH_LOG_CRITICAL("Driver is not yet thread-safe");
        return FALSE;
    }

    /* Allocate the global variables */
    global_vars = calloc(1, sizeof(IFDH_SSCP_DATA_ST));
    if (global_vars == NULL)
    {
        IFDH_LOG_CRITICAL("Alloc variables failed");
        return FALSE;
    }
    global_vars->sscp_ctx = SSCP_Alloc();
    if (global_vars->sscp_ctx == NULL)
    {
        IFDH_LOG_CRITICAL("Alloc context failed");
        free(global_vars);
        global_vars = NULL;
        return FALSE;
    }

    /* Initialize the variables */
    global_vars->address = Address;
    global_vars->bitrate = (Bitrate != 0) ? Bitrate : IFDH_SSCP_DEFAULT_BITRATE;
    if (AuthKey != NULL)
    {
        memcpy(global_vars->authKey, AuthKey, IFDH_SSCP_AUTH_KEY_LENGTH);
        global_vars->hasAuthKey = TRUE;
    }
    global_vars->device = strdup(Device);
    if (global_vars->device == NULL)
    {
        IFDH_LOG_CRITICAL("Alloc device failed");
        goto failed;
    }
    if (!CreateMutex(&global_vars->mutex))
    {
        IFDH_LOG_CRITICAL("Create mutex failed");
        goto failed;
    }
    mutexCreated = TRUE;
    if (!CreateEvent(&global_vars->statusEvent))
    {
        IFDH_LOG_CRITICAL("Create event failed");
        goto failed;
    }
    statusEventCreated = TRUE;
    if (!CreateEvent(&global_vars->actionEvent))
    {
        IFDH_LOG_CRITICAL("Create event failed");
        goto failed;
    }
    actionEventCreated = TRUE;
    if (!CreateEvent(&global_vars->responseEvent))
    {
        IFDH_LOG_CRITICAL("Create event failed");
        goto failed;
    }
    responseEventCreated = TRUE;
    global_vars->running = TRUE;

    /* Open the device */
    if (!IFDHOpen(global_vars))
    {
        IFDH_LOG_CRITICAL("Open device %s:%02X at %lu failed", Device, Address, global_vars->bitrate);
        goto failed;
    }

    /* Create the thread */
    if (pthread_create(&global_vars->thread_id, NULL, IFDH_SSCP_Proc, global_vars) != 0)
    {
        IFDH_LOG_CRITICAL("Failed to start SSCP thread");
        goto failed;
    }

    return TRUE;

failed:
    /* Try to de-allocated correctly */
    if (responseEventCreated)
        DestroyEvent(&global_vars->responseEvent);
    if (actionEventCreated)
        DestroyEvent(&global_vars->actionEvent);
    if (statusEventCreated)
        DestroyEvent(&global_vars->statusEvent);
    if (mutexCreated)
        DestroyMutex(&global_vars->mutex);
    if (global_vars->device != NULL)
        free(global_vars->device);
    if (global_vars->sscp_ctx != NULL)
        SSCP_Free(global_vars->sscp_ctx);
    free(global_vars);
    global_vars = NULL;
    return FALSE;
}

BOOL IFDHDestroy(DWORD Lun)
{
    if (global_vars == NULL)
    {
        IFDH_LOG_CRITICAL("Driver is not started");
        return FALSE;
    }

    /* Say we want to exit */
    global_vars->running = FALSE;

    /* Wakeup the thread */
    SetEvent(&global_vars->actionEvent);

    /* Join the thread */
    if (pthread_join(global_vars->thread_id, NULL) != 0)
    {
        IFDH_LOG_CRITICAL("Failed to stop the driver thread");
        return FALSE;
    }

    /* Close the reader */
    if (global_vars->sscp_ctx != NULL)
        SSCP_Close(global_vars->sscp_ctx);

    /* Try to de-allocated correctly */
    DestroyEvent(&global_vars->responseEvent);
    DestroyEvent(&global_vars->actionEvent);
    DestroyEvent(&global_vars->statusEvent);
    DestroyMutex(&global_vars->mutex);

    /* Free the global variables */
    if (global_vars->sscp_ctx != NULL)
        SSCP_Free(global_vars->sscp_ctx);
    if (global_vars->device != NULL)
        free(global_vars->device);
    free(global_vars);
    global_vars = NULL;

    return TRUE;
}

BOOL IFDHIsReaderOnline(DWORD Lun)
{
    BOOL rc = FALSE;
    if (global_vars == NULL)
        return FALSE;
    Lock(global_vars);
    if (global_vars->readerState.available)
        rc = TRUE;
    Unlock(global_vars);
    return rc;
}

BOOL IFDHIsCardPresent(DWORD Lun)
{
    BOOL rc = FALSE;
    if (global_vars == NULL)
        return FALSE;
    Lock(global_vars);
    if (global_vars->readerState.available)
        if (global_vars->cardState.present)
            rc = TRUE;
    Unlock(global_vars);
    return rc;
}

BOOL IFDHWaitStatusChange(DWORD Lun, int Timeout)
{
    if (global_vars == NULL)
        return FALSE;
    return WaitEvent(&global_vars->statusEvent, Timeout);
}

BOOL IFDHKillStatusChange(DWORD Lun)
{
    if (global_vars == NULL)
        return FALSE;
    return SetEvent(&global_vars->statusEvent);
}

BOOL IFDHGetAtr(DWORD Lun, PUCHAR Atr, PDWORD AtrLength)
{
    static const BYTE DEFAULT_ATR[] = { 0x3B, 0x88, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x51, 0x93, 0x00, 0xCB };
    BOOL rc = FALSE;
    if (global_vars == NULL)
        return FALSE;
    if ((Atr == NULL) || (AtrLength == NULL))
        return FALSE;
    if (Lock(global_vars))
    {
        if ((global_vars->readerState.available) && (global_vars->cardState.present))
        {
            if (*AtrLength > sizeof(DEFAULT_ATR))
                *AtrLength = sizeof(DEFAULT_ATR);
            memcpy(Atr, DEFAULT_ATR, *AtrLength);
            rc = TRUE;
        }
        Unlock(global_vars);
    }
    return rc;
}

BOOL IFDHPowerUp(DWORD Lun)
{
    BOOL rc = FALSE;
    if (global_vars == NULL)
        return FALSE;
    if (Lock(global_vars))
    {
        if ((global_vars->readerState.available) && (global_vars->cardState.present))
        {
            /* Remember the card is active */
            global_vars->cardState.active = TRUE;
            /* No need to wakeup the SSCP thread, it has nothing to do */
            rc = TRUE;
        }
        Unlock(global_vars);
    }
    return rc;
}

BOOL IFDHPowerDown(DWORD Lun)
{
    BOOL rc = FALSE;
    if (global_vars == NULL)
        return FALSE;
    if (Lock(global_vars))
    {
        if ((global_vars->readerState.available) && (global_vars->cardState.present) && (global_vars->readerAction == IFDH_SSCP_ACTION_IDLE))
        {
            /* Release the card */
            global_vars->cardState.active = FALSE;
            /* Tell the SSCP thread we have something to do */
            global_vars->readerAction = IFDH_SSCP_ACTION_DISCONNECT;
            SetEvent(&global_vars->actionEvent); /* Wakeup the SSCP thread */
            rc = TRUE;
        }
        Unlock(global_vars);
    }
    return rc;
}

BOOL IFDHAsyncTransmit(DWORD Lun, PUCHAR TxBuffer, DWORD TxLength, PUCHAR RxBuffer, DWORD RxLength)
{
    BOOL rc = FALSE;
    if (global_vars == NULL)
        return FALSE;
    if (Lock(global_vars))
    {
        if ((global_vars->readerState.available) && (global_vars->cardState.present) && (global_vars->readerAction == IFDH_SSCP_ACTION_IDLE))
        {
            /* Prevent further polling */
            global_vars->cardState.active = TRUE;
            /* Store the buffer */
            global_vars->x.transmit.txBuffer = TxBuffer;
            global_vars->x.transmit.txLength = TxLength;
            global_vars->x.transmit.rxBuffer = RxBuffer;
            global_vars->x.transmit.rxLengthMax = RxLength;
            /* Be ready to receive */
            ClearEvent(&global_vars->responseEvent);
            /* Tell the SSCP thread we have something to transmit */
            global_vars->readerAction = IFDH_SSCP_ACTION_TRANSMIT;
            SetEvent(&global_vars->actionEvent); /* Wakeup the SSCP thread */
            rc = TRUE;
        }
        Unlock(global_vars);
    }
    return rc;
}

BOOL IFDHWaitTransmit(DWORD Lun, int Timeout, PDWORD RxLength)
{
    BOOL rc = FALSE;
    if (global_vars == NULL)
        return FALSE;
    if (!WaitEvent(&global_vars->responseEvent, Timeout))
        return FALSE;
    if (Lock(global_vars))
    {
        if (global_vars->readerAction == IFDH_SSCP_ACTION_TRANSMIT_RESP)
        {
            /* Retrieve the length of the response */
            *RxLength = global_vars->x.transmit.rxLengthAct;
            /* No more pending action */
            global_vars->readerAction = IFDH_SSCP_ACTION_IDLE;
            /* No need to wakeup the SSCP thread, let its timeout expire */
            rc = TRUE;
        }
        Unlock(global_vars);
    }
    return rc;
    return TRUE;
}

BOOL IFDHAsyncControl(DWORD Lun, DWORD ControlCode, PUCHAR TxBuffer, DWORD TxLength, PUCHAR RxBuffer, DWORD RxLength)
{
    BOOL rc = FALSE;
    if (global_vars == NULL)
        return FALSE;
    if (Lock(global_vars))
    {
        if ((global_vars->readerState.available) && (global_vars->readerAction == IFDH_SSCP_ACTION_IDLE))
        {
            /* Store the buffer */
            global_vars->x.control.controlCode = ControlCode;
            global_vars->x.control.txBuffer = TxBuffer;
            global_vars->x.control.txLength = TxLength;
            global_vars->x.control.rxBuffer = RxBuffer;
            global_vars->x.control.rxLengthMax = RxLength;
            global_vars->x.control.rxLengthAct = 0;
            global_vars->x.control.responseCode = IFD_COMMUNICATION_ERROR;
            /* Be ready to receive */
            ClearEvent(&global_vars->responseEvent);
            /* Tell the SSCP thread we have something to control */
            global_vars->readerAction = IFDH_SSCP_ACTION_CONTROL;
            ClearEvent(&global_vars->responseEvent);
            SetEvent(&global_vars->actionEvent); /* Wakeup the SSCP thread */
            rc = TRUE;
        }
        Unlock(global_vars);
    }
    return rc;
}

BOOL IFDHWaitControl(DWORD Lun, int Timeout, PDWORD RxLength, RESPONSECODE *ControlResponse)
{
    BOOL rc = FALSE;
    if (global_vars == NULL)
        return FALSE;
    if (!WaitEvent(&global_vars->responseEvent, Timeout))
        return FALSE;
    if (Lock(global_vars))
    {
        if (global_vars->readerAction == IFDH_SSCP_ACTION_CONTROL_RESP)
        {
            /* Retrieve the length of the response */
            if (RxLength != NULL)
                *RxLength = global_vars->x.control.rxLengthAct;
            if (ControlResponse != NULL)
                *ControlResponse = global_vars->x.control.responseCode;
            /* No more pending action */
            global_vars->readerAction = IFDH_SSCP_ACTION_IDLE;
            /* No need to wakeup the SSCP thread, let its timeout expire */
            rc = TRUE;
        }
        Unlock(global_vars);
    }
    return rc;
}
