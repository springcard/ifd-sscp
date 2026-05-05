#include "ifd-sscp-debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

extern const char *Name;

int IFDHLogLevel = IFDH_LOG_LEVEL_CRITICAL | IFDH_LOG_LEVEL_INFO;

static void IFDHLogInit(void)
{
    static int initialized = 0;
    char *levelText;
    char *end = NULL;
    unsigned long level;

    if (initialized)
        return;

    initialized = 1;
    levelText = getenv("IFD_SSCP_LOG_LEVEL");
    if ((levelText == NULL) || (levelText[0] == '\0'))
        return;

    level = strtoul(levelText, &end, 0);
    if ((end != NULL) && (*end == '\0'))
        IFDHLogLevel = (int) level;
}

int IFDHLogIsEnabled(int level)
{
    IFDHLogInit();
    return (IFDHLogLevel & level) != 0;
}

void IFDHLog(int level, int priority, const char *fmt, ...)
{
    char buffer[512];
    char message[640];
    va_list args;

    IFDHLogInit();

    if ((IFDHLogLevel & level) == 0)
        return;

    va_start(args, fmt);
    (void) vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    (void) snprintf(message, sizeof(message), "%s: %s", Name, buffer);
    (void) priority;
    Log2(priority, "%s", message);
}
