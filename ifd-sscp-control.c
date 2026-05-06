#include "ifd-sscp_i.h"

static DWORD ReadRgb(const BYTE *buffer)
{
    return ((DWORD)buffer[0] << 16) | ((DWORD)buffer[1] << 8) | (DWORD)buffer[2];
}

static RESPONSECODE ControlResult(LONG rc, const char *functionName)
{
    if (rc == SSCP_SUCCESS)
        return IFD_SUCCESS;

    IFDH_LOG_CRITICAL("%s failed (err. %d)", functionName, rc);
    return IFD_COMMUNICATION_ERROR;
}

RESPONSECODE IFDH_SSCP_Control(IFDH_SSCP_INSTANCE_ST *instance)
{
    const BYTE *txBuffer;
    DWORD txLength;
    LONG rc;

    if (instance == NULL)
        return IFD_COMMUNICATION_ERROR;

    IFDH_LOG_INFO("IFDH_SSCP_Control(ControlCode=%08X, TxLength=%lu)",
                  instance->x.control.controlCode, instance->x.control.txLength);

    txBuffer = instance->x.control.txBuffer;
    txLength = instance->x.control.txLength;
    instance->x.control.rxLengthAct = 0;

    switch (instance->x.control.controlCode)
    {
        case IFDH_SSCP_CONTROL_OUTPUTS:
            /* LedColor, LedDuration, BuzzerDuration */
            if ((txBuffer == NULL) || (txLength != 3))
            {
                IFDH_LOG_CRITICAL("IFDH_SSCP_Control: invalid SSCP_Outputs parameters");
                return IFD_COMMUNICATION_ERROR;
            }
            rc = SSCP_Outputs(instance->sscp_ctx, txBuffer[0], txBuffer[1], txBuffer[2]);
            if (rc != SSCP_SUCCESS)
                instance->readerState.ready = FALSE; /* We have lost the reader? */
            else
                instance->readerState.timerOutput = IFDH_SSCP_Now() + (txBuffer[1] * 100) + 250; /* Set the timer to restore the default LED value */
            return ControlResult(rc, "SSCP_Outputs");

        case IFDH_SSCP_CONTROL_OUTPUTS_RGB:
            /* Mode, LedColorR, LedColorG, LedColorB, LedDuration, BuzzerDuration */
            if ((txBuffer == NULL) || (txLength != 5))
            {
                IFDH_LOG_CRITICAL("IFDH_SSCP_Control: invalid SSCP_OutputsRGB parameters");
                return IFD_COMMUNICATION_ERROR;
            }
            rc = SSCP_OutputsRGB(instance->sscp_ctx, ReadRgb(txBuffer), txBuffer[3], txBuffer[4]);
            if (rc != SSCP_SUCCESS)
                instance->readerState.ready = FALSE; /* We have lost the reader? */
            else
                instance->readerState.timerOutput = IFDH_SSCP_Now() + (txBuffer[3] * 100) + 250; /* Set the timer to restore the default LED value */
            return ControlResult(rc, "SSCP_OutputsRGB");

        case IFDH_SSCP_CONTROL_EXTERNAL_LED_RGB:
            if ((txBuffer == NULL) || (txLength != 9))
            {
                IFDH_LOG_CRITICAL("IFDH_SSCP_Control: invalid SSCP_ExternalLEDRGB parameters");
                return IFD_COMMUNICATION_ERROR;
            }
            rc = SSCP_ExternalLEDRGB(instance->sscp_ctx, ReadRgb(txBuffer), ReadRgb(&txBuffer[3]), ReadRgb(&txBuffer[6]));
            if (rc != SSCP_SUCCESS)
                instance->readerState.ready = FALSE; /* We have lost the reader? */
            return ControlResult(rc, "SSCP_ExternalLEDRGB");

        default:
            IFDH_LOG_CRITICAL("IFDH_SSCP_Control: unsupported ControlCode %08X", instance->x.control.controlCode);
            return IFD_NOT_SUPPORTED;
    }
}

BOOL IFDH_SSCP_SetDefaultLEDs(IFDH_SSCP_INSTANCE_ST *instance)
{
    LONG rc;

    if (instance == NULL)
        return FALSE;

    rc = SSCP_OutputsRGB(instance->sscp_ctx, 0x0000FF, 0xFF, 0x00);
    if (rc != SSCP_SUCCESS)
    {
        IFDH_LOG_CRITICAL("SSCP_Outputs failed (err. %d)", rc);
        return FALSE;
    }

    return TRUE;
}