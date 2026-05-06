#include "ifd-sscp_i.h"

static int HexDigitValue(char c)
{
    if ((c >= '0') && (c <= '9'))
        return c - '0';
    if ((c >= 'a') && (c <= 'f'))
        return c - 'a' + 10;
    if ((c >= 'A') && (c <= 'F'))
        return c - 'A' + 10;
    return -1;
}

static BOOL ParseHexByteChars(char hiChar, char loChar, BYTE *byte)
{
    int hi;
    int lo;

    if (byte == NULL)
        return FALSE;

    hi = HexDigitValue(hiChar);
    lo = HexDigitValue(loChar);
    if ((hi < 0) || (lo < 0))
        return FALSE;

    *byte = (BYTE)((hi << 4) | lo);
    return TRUE;
}

static BOOL ParseHexByte(const char *value, BYTE *byte)
{
    if ((value == NULL) || (byte == NULL) || (strlen(value) != 2))
        return FALSE;

    return ParseHexByteChars(value[0], value[1], byte);
}

static BOOL ParseHexKey(const char *value, BYTE key[IFDH_SSCP_AUTH_KEY_LENGTH])
{
    DWORD i;

    if ((value == NULL) || (key == NULL) || (strlen(value) != (IFDH_SSCP_AUTH_KEY_LENGTH * 2)))
        return FALSE;

    for (i = 0; i < IFDH_SSCP_AUTH_KEY_LENGTH; i++)
    {
        BYTE byte;
        if (!ParseHexByteChars(value[i * 2], value[(i * 2) + 1], &byte))
            return FALSE;
        key[i] = byte;
    }

    return TRUE;
}

static BOOL ParseBitrate(const char *value, DWORD *bitrate)
{
    const char *p;
    char *end = NULL;
    unsigned long parsed;

    if ((value == NULL) || (value[0] == '\0') || (bitrate == NULL))
        return FALSE;

    for (p = value; *p != '\0'; p++)
    {
        if ((*p < '0') || (*p > '9'))
            return FALSE;
    }

    errno = 0;
    parsed = strtoul(value, &end, 10);
    if ((errno != 0) || (end == NULL) || (*end != '\0') || (parsed == 0))
        return FALSE;
    if ((unsigned long)((DWORD) parsed) != parsed)
        return FALSE;

    *bitrate = (DWORD) parsed;
    return TRUE;
}

BOOL IFDHParseDeviceParameters(LPSTR Device, char **parsedDevice, BYTE *address, DWORD *bitrate,
                               BYTE authKey[IFDH_SSCP_AUTH_KEY_LENGTH], BOOL *hasAuthKey)
{
    char *copy;
    char *parameters;
    char *token;

    if ((Device == NULL) || (parsedDevice == NULL) || (address == NULL) || (bitrate == NULL) ||
        (authKey == NULL) || (hasAuthKey == NULL))
    {
        return FALSE;
    }

    copy = strdup(Device);
    if (copy == NULL)
        return FALSE;

    *address = IFDH_SSCP_DEFAULT_ADDRESS;
    *bitrate = IFDH_SSCP_DEFAULT_BITRATE;
    *hasAuthKey = FALSE;
    memset(authKey, 0, IFDH_SSCP_AUTH_KEY_LENGTH);

    parameters = strchr(copy, ':');
    if (parameters != NULL)
    {
        *parameters = '\0';
        parameters++;
    }

    if (copy[0] == '\0')
        goto failed;

    token = parameters;
    while (token != NULL)
    {
        char *next = strchr(token, ':');
        char *separator;
        const char *name;
        const char *value;

        if (next != NULL)
        {
            *next = '\0';
            next++;
        }

        if (token[0] == '\0')
            goto failed;

        separator = strchr(token, '=');
        if ((separator == NULL) || (separator == token) || (separator[1] == '\0'))
            goto failed;

        *separator = '\0';
        name = token;
        value = separator + 1;

        if (strcmp(name, "address") == 0)
        {
            if (!ParseHexByte(value, address))
                goto failed;
        }
        else if (strcmp(name, "bitrate") == 0)
        {
            if (!ParseBitrate(value, bitrate))
                goto failed;
        }
        else if (strcmp(name, "key") == 0)
        {
            if (!ParseHexKey(value, authKey))
                goto failed;
            *hasAuthKey = TRUE;
        }
        else
        {
            goto failed;
        }

        token = next;
    }

    *parsedDevice = copy;
    return TRUE;

failed:
    free(copy);
    return FALSE;
}

DWORD IFDH_SSCP_Now(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint32_t)(
        (ts.tv_sec * 1000u) +
        (ts.tv_nsec / 1000000u)
    );
}

