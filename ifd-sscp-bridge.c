#include "ifd-sscp.h"
#include "ifd-sscp_i.h"

extern BOOL SSCP_DEBUG_SERIAL;
extern BOOL SSCP_DEBUG_EXCHANGE;

IFDH_SSCP_DATA_ST *global_vars = NULL;

static void *IFDH_SSCP_Proc(void *arg)
{
    IFDH_SSCP_DATA_ST *vars = (IFDH_SSCP_DATA_ST *)arg;
    LONG rc;

    printf("%s:Thread starting\n", Name);

    while (1)
    {
        (void) WaitEvent(&vars->actionEvent, 150); /* Default polling interval, don't care for result */

        if (!vars->running)
        {
            printf("%s:Not running anymore\n", Name);
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
                        printf("Tracking...\n");
                        rc = SSCP_TransceiveNFC(vars->sscp_ctx, NULL, 0, NULL, 0, NULL);
                        if (rc == SSCP_SUCCESS)
                        {
                            /* Success, card is still there */
                            printf("Tracking:Card still present\n");
                        }
                        else if ((rc == SSCP_ERR_NFC_CARD_MUTE_OR_REMOVED) || (rc == SSCP_ERR_NFC_CARD_COMM_ERROR))
                        {
                            printf("Tracking:Card lost\n");
                            /* Reset card data */
                            memset(&vars->cardState, 0, sizeof(vars->cardState));
                            /* Say we have lost the card */
                            SetEvent(&vars->statusEvent);
                        }
                        else if (rc == 2)
                        {
                            printf("Tracking:Not supported by the reader\n");
                        }
                        else
                        {
                            printf("Tracking:Reader error %d\n", rc);
                            /* We have lost the reader? */
                            vars->readerState.available = FALSE;
                        }
                    }
                    else
                    {
                        /* The card is either absent or not active, we can do the polling */
                        printf("Polling...\n");
                        rc = SSCP_ScanNFC(vars->sscp_ctx, &vars->cardState.protocol, vars->cardState.uid, sizeof(vars->cardState.uid), &vars->cardState.uidLength, vars->cardState.ats, sizeof(vars->cardState.ats), &vars->cardState.atsLength);
                        if (rc == SSCP_SUCCESS)
                        {
                            BOOL oldCardPresent = vars->cardState.present;
                            if (vars->cardState.protocol)
                            {
                                printf("Polling:Card inserted, protocol=%04X\n", vars->cardState.protocol);
                                vars->cardState.present = TRUE;
                            }
                            else
                            {
                                printf("Polling:Card inserted, but protocol=0\n");
                                vars->cardState.present = FALSE;
                            }
                            /* Status has changed! */
                            if (oldCardPresent != vars->cardState.present)
                                SetEvent(&vars->statusEvent);
                        }
                        else
                        {
                            printf("Polling:Reader error %d\n", rc);
                            vars->readerState.available = FALSE;
                        }
                    }
                break;
                case IFDH_SSCP_ACTION_CONTROL :
                    /* TODO */
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
                        printf("Transmit:Reader error %d\n", rc);
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
                        printf("Disconnect:Reader error %d\n", rc);
                        vars->readerState.available = FALSE;
                    }
                break;

                default:
                    printf("%s:Invalid reader action %d\n", Name, vars->readerAction);
                    vars->readerAction = IFDH_SSCP_ACTION_IDLE;
                    break;
            }
        }
        Unlock(vars);
    }

    printf("%s:Thread terminating\n", Name);
}

static BOOL IFDHOpen(IFDH_SSCP_DATA_ST *vars)
{
    LONG rc;

    if (vars == NULL)
        return FALSE;

    memset(&vars->readerState, 0, sizeof(vars->readerState));

    /* Try to open the reader */
    rc = SSCP_Open(vars->sscp_ctx, vars->device, 38400, 0);
    if (rc != SSCP_SUCCESS)
    {
        printf("SSCP_Open(%s) failed (err. %d)\n", vars->device, rc);
        return FALSE;
    }

	rc = SSCP_SetAddress(vars->sscp_ctx, 0x01); /* RS485 */
	if (rc)
	{
		printf("SSCP_SetAddress(%02X) failed (err. %d)\n", vars->address, rc);
		SSCP_Close(vars->sscp_ctx);
        return FALSE;
	}

	rc = SSCP_Authenticate(vars->sscp_ctx, NULL);
	if (rc)
	{
		printf("SSCP_Authenticate failed (err. %d)\n", rc);
		SSCP_Close(vars->sscp_ctx);
        return FALSE;
	}

	rc = SSCP_Outputs(vars->sscp_ctx, 0x02, 0x0A, 0x02);
	if (rc)
	{
		printf("SSCP_Outputs failed (err. %d)\n", rc);
		SSCP_Close(vars->sscp_ctx);
        return FALSE;
	}

    rc = SSCP_GetInfos(vars->sscp_ctx, &vars->readerInfo.version, &vars->readerInfo.baudrate, &vars->readerInfo.address, &vars->readerInfo.voltage);
    if (rc)
    {
        printf("SSCP_GetInfos failed (err. %d)\n", rc);
		SSCP_Close(vars->sscp_ctx);
        return FALSE;
    }
    printf("SSCP_GetInfos OK, version=%02X, baudrate=%02X, address=%02X, voltage=%04X\n", vars->readerInfo.version, vars->readerInfo.baudrate, vars->readerInfo.address, vars->readerInfo.voltage);

    rc = SSCP_GetSerialNumber(vars->sscp_ctx, vars->readerInfo.serialNumber, sizeof(vars->readerInfo.serialNumber));
    if (rc)
    {
        printf("SSCP_GetSerialNumber failed (err. %d)\n", rc);
		SSCP_Close(vars->sscp_ctx);
        return FALSE;
    }
    printf("SSCP_GetSerialNumber OK, serialNumber=%s\n", vars->readerInfo.serialNumber);

    rc = SSCP_GetReaderType(vars->sscp_ctx, vars->readerInfo.readerType, sizeof(vars->readerInfo.readerType));
    if (rc)
    {
        printf("SSCP_GetReaderType failed (err. %d)\n", rc);
		SSCP_Close(vars->sscp_ctx);
        return FALSE;
    }
    printf("SSCP_GetReaderType OK, readerType=%s\n", vars->readerInfo.readerType);

    vars->readerState.open = TRUE;
    vars->readerState.available = TRUE;
    return TRUE;
}

BOOL IFDHCreate(DWORD Lun, LPSTR Device, UCHAR Address)
{
    SSCP_DEBUG_SERIAL = TRUE;
    SSCP_DEBUG_EXCHANGE = TRUE;

    if (global_vars != NULL)
    {
        printf("%s:Driver is not yet thread-safe\n", Name);
        return FALSE;
    }

    /* Allocate the global variables */
    global_vars = calloc(1, sizeof(IFDH_SSCP_DATA_ST));
    if (global_vars == NULL)
    {
        printf("%s:Alloc variables failed\n", Name);
        return FALSE;
    }
    global_vars->sscp_ctx = SSCP_Alloc();
    if (global_vars->sscp_ctx == NULL)
    {
        printf("%s:Alloc context failed\n", Name);
        free(global_vars);
        return FALSE;
    }

    /* Initialize the variables */
    global_vars->address = Address;
    global_vars->device = strdup(Device);
    if (!CreateMutex(&global_vars->mutex))
    {
        printf("%s:Create mutex failed\n", Name);
        goto failed;
    }
    if (!CreateEvent(&global_vars->statusEvent) || !CreateEvent(&global_vars->actionEvent) || !CreateEvent(&global_vars->responseEvent))
    {
        printf("%s:Create event failed\n", Name);
        goto failed;
    }
    global_vars->running = TRUE;

    /* Open the device */
    if (!IFDHOpen(global_vars))
    {
        printf("%s:Open device %s:%d failed\n", Name, Device, Address);
        free(global_vars);
        return FALSE;       
    }

    /* Create the thread */
    if (pthread_create(&global_vars->thread_id, NULL, IFDH_SSCP_Proc, global_vars) != 0)
    {
        printf("%s:Failed to start SSCP thread\n", Name);
        goto failed;
    }

    return TRUE;

failed:
    /* Try to de-allocated correctly */
    DestroyEvent(&global_vars->responseEvent);
    DestroyEvent(&global_vars->actionEvent);
    DestroyEvent(&global_vars->statusEvent);
    DestroyMutex(&global_vars->mutex);
    if (global_vars->device != NULL)
        free(global_vars->device);
    if (global_vars->sscp_ctx != NULL)
        SSCP_Free(global_vars->sscp_ctx);
    free(global_vars);
    return FALSE;    
}

BOOL IFDHDestroy(DWORD Lun)
{
    if (global_vars == NULL)
    {
        printf("%s:Driver is not started\n", Name);
        return FALSE;
    }

    /* Say we want to exit */
    global_vars->running = FALSE;

    /* Wakeup the thread */
    SetEvent(&global_vars->actionEvent);

    /* Join the thread */
    if (pthread_join(global_vars->thread_id, NULL) != 0)
    {
        printf("%s:Thread to stop the driver\n", Name);
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

BOOL IFDHAsyncControl(DWORD Lun, PUCHAR TxBuffer, DWORD TxLength, PUCHAR RxBuffer, DWORD RxLength)
{
    BOOL rc = FALSE;
    if (global_vars == NULL)
        return FALSE;
    if (Lock(global_vars))
    {
        if ((global_vars->readerState.available) && (global_vars->readerAction == IFDH_SSCP_ACTION_IDLE))
        {
            /* Store the buffer */
            global_vars->x.control.txBuffer = TxBuffer;
            global_vars->x.control.txLength = TxLength;
            global_vars->x.control.rxBuffer = RxBuffer;
            global_vars->x.control.rxLengthMax = RxLength;
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

BOOL IFDHWaitControl(DWORD Lun, int Timeout, PDWORD RxLength)
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
            *RxLength = global_vars->x.control.rxLengthAct;
            /* No more pending action */
            global_vars->readerAction = IFDH_SSCP_ACTION_IDLE;
            /* No need to wakeup the SSCP thread, let its timeout expire */
            rc = TRUE;
        }
        Unlock(global_vars);
    }
    return rc;
}
