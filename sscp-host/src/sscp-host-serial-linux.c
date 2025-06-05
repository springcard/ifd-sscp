#include "sscp-host-serial_i.h"

#ifndef _WIN32

#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

BOOL SSCP_DEBUG_SERIAL = FALSE;

LONG SSCP_SerialOpen(SSCP_CTX_ST* ctx, const char* commName)
{
	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;
	if (commName == NULL)
		return SSCP_ERR_INVALID_PARAMETER;

	/* Don't forget to close in case of it were previously open */
	SSCP_SerialClose(ctx);

	/* Start-up here */
	if (SSCP_DEBUG_SERIAL)
		SSCP_Trace("Opening device %s...\n", commName);

    ctx->commFd = open(commName, O_RDWR | O_NOCTTY);

	if (ctx->commFd < 0)
	{
		if (SSCP_DEBUG_SERIAL)
			SSCP_Trace("open (%d)\n", errno);
        return SSCP_ERR_COMM_NOT_AVAILABLE;
	}

	/* Clear UART */
	tcflush(ctx->commFd, TCIFLUSH);
    
    return SSCP_SUCCESS;
}

LONG SSCP_SerialClose(SSCP_CTX_ST* ctx)
{
	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;
    if (ctx->commFd < 0)
		return SSCP_ERR_COMM_NOT_OPEN;

	if (SSCP_DEBUG_SERIAL)
		SSCP_Trace("Closing device\n");

	close(ctx->commFd);

	ctx->commFd = -1;

	return SSCP_SUCCESS;
}

LONG SSCP_SerialConfigure(SSCP_CTX_ST* ctx, DWORD baudrate)
{
    struct termios newtio;

	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;
	if (ctx->commFd < 0)
		return SSCP_ERR_COMM_NOT_OPEN;

	bzero(&newtio, sizeof(newtio));
	// CS8  = 8n1 (8bit,no parity,1 stopbit
	// CLOCAL= local connection, no modem control
	// CREAD  = enable receiving characters
	newtio.c_cflag = CS8 | CLOCAL | CREAD;
	switch (baudrate)
	{
		case 115200:
			newtio.c_cflag |= B115200;
		break;
		case 38400:
			newtio.c_cflag |= B38400;
		break;
		case 19200:
			newtio.c_cflag |= B19200;
		break;
		case 9600:
			newtio.c_cflag |= B9600;
		break;
		case 4800:
			newtio.c_cflag |= B4800;
		break;
		case 2400:
			newtio.c_cflag |= B2400;
		break;
		case 1200:
			newtio.c_cflag |= B1200;
		break;
		default:
			return FALSE;
	}
	newtio.c_iflag = IGNPAR | IGNBRK;
	newtio.c_oflag = 0;

	// set input mode (non-canonical, no echo,...) 
	newtio.c_lflag = 0;

	newtio.c_cc[VTIME] = 0;	// inter-character timer unused
	newtio.c_cc[VMIN] = 1;	// blocking read until 1 chars received

	tcflush(ctx->commFd, TCIFLUSH);

	if (tcsetattr(ctx->commFd, TCSANOW, &newtio))
	{
		if (SSCP_DEBUG_SERIAL)
			SSCP_Trace("tcsetattr failed (%d)\n", errno);
		return SSCP_ERR_COMM_CONTROL_FAILED;
	}

    return SSCP_SUCCESS;
}

LONG SSCP_SerialSetTimeouts(SSCP_CTX_ST* ctx, DWORD first_byte, DWORD inter_byte)
{
	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;
	if (ctx->commFd < 0)
		return SSCP_ERR_COMM_NOT_OPEN;

	ctx->firstByteTimeout = first_byte;
	ctx->interByteTimeout = inter_byte;

    return SSCP_SUCCESS;
}

LONG SSCP_SerialSend(SSCP_CTX_ST* ctx, const BYTE buffer[], DWORD length)
{
	DWORD remainingLen = length;
	DWORD offset = 0;

	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;
	if (ctx->commFd < 0)
		return SSCP_ERR_COMM_NOT_OPEN;
	if (buffer == NULL)
		return SSCP_ERR_INVALID_PARAMETER;

	while (remainingLen)
	{
        DWORD writeLen = remainingLen;
        int i;        
		int written = write(ctx->commFd, &buffer[offset], writeLen);

		if (written <= 0)
		{
			if (SSCP_DEBUG_SERIAL)
				SSCP_Trace("write(%d) error (%d)\n", writeLen, errno);
			return SSCP_ERR_COMM_SEND_FAILED;
		}

		if (SSCP_DEBUG_SERIAL)
		{
			SSCP_Trace("<");
			for (i = 0; i < written; i++)
				SSCP_Trace("%02X", buffer[offset + i]);
			SSCP_Trace("\n");
		}

		if (written < writeLen)
		{
			if (SSCP_DEBUG_SERIAL)
				SSCP_Trace("write(%d/%d) failed (%d)\n", written, writeLen, errno);
			return SSCP_ERR_COMM_SEND_FAILED;
		}

		remainingLen -= written;
		offset += written;
	}

	return SSCP_SUCCESS;        
}

LONG SSCP_SerialRecv(SSCP_CTX_ST* ctx, BYTE buffer[], DWORD length)
{
	size_t received = 0;

	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;
	if (ctx->commFd < 0)
		return SSCP_ERR_COMM_NOT_OPEN;
	if (buffer == NULL)
		return SSCP_ERR_INVALID_PARAMETER;

    while (received < length)
	{
		struct timeval timeout;
		fd_set read_fds;
		int done;

        FD_ZERO(&read_fds);
        FD_SET(ctx->commFd, &read_fds);

        if (received == 0)
		{
            timeout.tv_sec = 1;
            timeout.tv_usec = 500000; // 1500ms
        }
		else
		{
            timeout.tv_sec = 0;
            timeout.tv_usec = 50000; // 50ms
        }

        int sel = select(ctx->commFd + 1, &read_fds, NULL, NULL, &timeout);
        if (sel < 0)
		{
			if (SSCP_DEBUG_SERIAL)
				SSCP_Trace("select on read(%d/%d) failed (%d) [%d]\n", received, length, errno, sel);
            return SSCP_ERR_COMM_RECV_FAILED;
        }
		else if (sel == 0)
		{
			if (SSCP_DEBUG_SERIAL)
				SSCP_Trace("select on read(%d/%d) failed (%d)\n", received, length, errno);
            return (received == 0) ? SSCP_ERR_COMM_RECV_MUTE : SSCP_ERR_COMM_RECV_STOPPED;
        }

        done = read(ctx->commFd, &buffer[received], length - received);
        if (done < 0)
		{
			if (SSCP_DEBUG_SERIAL)
				SSCP_Trace("read(%d/%d) failed (%d) [%d]\n", received, length, errno, done);
            return SSCP_ERR_COMM_RECV_FAILED;
        } 
		else if (done == 0)
		{
			if (SSCP_DEBUG_SERIAL)
				SSCP_Trace("read(%d/%d) failed (%d)\n", received, length, errno);
            return SSCP_ERR_COMM_RECV_FAILED; // Shouldn’t happen, select said ready
        }

		if (SSCP_DEBUG_SERIAL)
		{
			int i;
			SSCP_Trace(">");
			for (i = 0; i < done; i++)
				SSCP_Trace("%02X", buffer[received + i]);
			SSCP_Trace("\n");
		}

        received += done;
    }    

	return SSCP_SUCCESS;
}

#endif
