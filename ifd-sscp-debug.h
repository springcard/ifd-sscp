#ifndef __IFD_SSCP_DEBUG_H__
#define __IFD_SSCP_DEBUG_H__

#if defined(__has_include)
#  if __has_include(<debuglog.h>)
#    include <debuglog.h>
#    define IFDH_HAVE_PCSC_DEBUGLOG 1
#  endif
#endif

#ifndef IFDH_HAVE_PCSC_DEBUGLOG
#define PCSC_LOG_DEBUG 0
#define PCSC_LOG_INFO 1
#define PCSC_LOG_ERROR 2
#define PCSC_LOG_CRITICAL 3
#define Log2(priority, fmt, data) do { (void)(priority); (void)(fmt); (void)(data); } while (0)
#endif

#define IFDH_LOG_LEVEL_CRITICAL 1
#define IFDH_LOG_LEVEL_INFO     2
#define IFDH_LOG_LEVEL_COMM     4
#define IFDH_LOG_LEVEL_PERIODIC 8

extern int IFDHLogLevel;

int IFDHLogIsEnabled(int level);
void IFDHLog(int level, int priority, const char *fmt, ...);

#define IFDH_LOG_CRITICAL(...) IFDHLog(IFDH_LOG_LEVEL_CRITICAL, PCSC_LOG_CRITICAL, __VA_ARGS__)
#define IFDH_LOG_INFO(...)     IFDHLog(IFDH_LOG_LEVEL_INFO, PCSC_LOG_INFO, __VA_ARGS__)
#define IFDH_LOG_COMM(...)     IFDHLog(IFDH_LOG_LEVEL_COMM, PCSC_LOG_DEBUG, __VA_ARGS__)
#define IFDH_LOG_PERIODIC(...) IFDHLog(IFDH_LOG_LEVEL_PERIODIC, PCSC_LOG_DEBUG, __VA_ARGS__)

#endif
