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

RESPONSECODE IFDH_SSCP_Control(IFDH_SSCP_DATA_ST *vars)
{
    const BYTE *txBuffer;
    DWORD txLength;
    LONG rc;

    if (vars == NULL)
        return IFD_COMMUNICATION_ERROR;

    IFDH_LOG_INFO("IFDH_SSCP_Control(ControlCode=%08X, TxLength=%lu)",
                  vars->x.control.controlCode, vars->x.control.txLength);

    txBuffer = vars->x.control.txBuffer;
    txLength = vars->x.control.txLength;
    vars->x.control.rxLengthAct = 0;

    switch (vars->x.control.controlCode)
    {
        case IFDH_SSCP_CONTROL_OUTPUTS:
            if ((txBuffer == NULL) || (txLength != 3))
            {
                IFDH_LOG_CRITICAL("IFDH_SSCP_Control: invalid SSCP_Outputs parameters");
                return IFD_COMMUNICATION_ERROR;
            }
            rc = SSCP_Outputs(vars->sscp_ctx, txBuffer[0], txBuffer[1], txBuffer[2]);
            return ControlResult(rc, "SSCP_Outputs");

        case IFDH_SSCP_CONTROL_OUTPUTS_RGB:
            if ((txBuffer == NULL) || (txLength != 5))
            {
                IFDH_LOG_CRITICAL("IFDH_SSCP_Control: invalid SSCP_OutputsRGB parameters");
                return IFD_COMMUNICATION_ERROR;
            }
            rc = SSCP_OutputsRGB(vars->sscp_ctx, ReadRgb(txBuffer), txBuffer[3], txBuffer[4]);
            return ControlResult(rc, "SSCP_OutputsRGB");

        case IFDH_SSCP_CONTROL_EXTERNAL_LED_RGB:
            if ((txBuffer == NULL) || (txLength != 9))
            {
                IFDH_LOG_CRITICAL("IFDH_SSCP_Control: invalid SSCP_ExternalLEDRGB parameters");
                return IFD_COMMUNICATION_ERROR;
            }
            rc = SSCP_ExternalLEDRGB(vars->sscp_ctx, ReadRgb(txBuffer), ReadRgb(&txBuffer[3]), ReadRgb(&txBuffer[6]));
            return ControlResult(rc, "SSCP_ExternalLEDRGB");

        default:
            IFDH_LOG_CRITICAL("IFDH_SSCP_Control: unsupported ControlCode %08X", vars->x.control.controlCode);
            return IFD_NOT_SUPPORTED;
    }
}
