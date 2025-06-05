#ifndef __SSCP_ERRORS_H__
#define __SSCP_ERRORS_H__

#define SSCP_SUCCESS 0

#define SSCP_ERR_INVALID_CONTEXT -1 /* Library call error: invalid context */
#define SSCP_ERR_INVALID_PARAMETER -2 /* Library call error: invalid parameter */
#define SSCP_ERR_NOT_YET_IMPLEMENTED -3 /* Library error: function not yet implemented */
#define SSCP_ERR_OUTPUT_BUFFER_OVERFLOW -4 /* Library error: supplied buffer is too small */

#define SSCP_ERR_COMMAND_TOO_LONG -5 /* Library error: command is too long for the communication layer */
#define SSCP_ERR_RESPONSE_TOO_LONG -6 /* Library error: response is too long for the communication layer */

#define SSCP_ERR_INTERNAL_FAILURE -8 /* Library error: an internal operation has failed */
#define SSCP_ERR_OUT_OF_MEMORY -9 /* Library error: dynamic allocation failed */

#define SSCP_ERR_COMM_NOT_AVAILABLE -10 /* Comm error: failed to open the port */
#define SSCP_ERR_COMM_NOT_OPEN -11 /* Comm error: the port is not open */
#define SSCP_ERR_COMM_CONTROL_FAILED -12 /* Comm error: failed to configure the port */
#define SSCP_ERR_COMM_SEND_FAILED -13 /* Comm error: failed to send through the serial port */

#define SSCP_ERR_COMM_RECV_FAILED -17 /* Comm error: unable to receive */
#define SSCP_ERR_COMM_RECV_STOPPED -18 /* Comm error: device has stopped transmitting */
#define SSCP_ERR_COMM_RECV_MUTE -19 /* Comm error: no response from device */

#define SSCP_ERR_WRONG_RESPONSE_LENGTH -20 /* Protocol error: wrong response length */
#define SSCP_ERR_WRONG_RESPONSE_CRC -21 /* Protocol error: wrong CRC in response */
#define SSCP_ERR_WRONG_RESPONSE_SIGNATURE -22 /* Protocol error: wrong HMAC in response */
#define SSCP_ERR_WRONG_RESPONSE_COUNTER -23 /* Protocol error: response counter does not match command */
#define SSCP_ERR_WRONG_RESPONSE_TYPE -24 /* Protocol error: type in response footer does not match command */
#define SSCP_ERR_WRONG_RESPONSE_COMMAND -25 /* Protocol error: command in response header does not match command */
#define SSCP_ERR_WRONG_RESPONSE_FORMAT -26 /* Protocol error: length response header does not match size of response */

#define SSCP_ERR_UNSUPPORTED_RESPONSE_STATUS -30 /* Application error: wrong response status byte */
#define SSCP_ERR_UNSUPPORTED_RESPONSE_VALUE -31  /* Application error: wrong value in response */
#define SSCP_ERR_UNSUPPORTED_RESPONSE_LENGTH -32 /* Application error: response length is incorrect */

#define SSCP_ERR_NFC_CARD_MUTE_OR_REMOVED -40 /* Card error: timeout */
#define SSCP_ERR_NFC_CARD_COMM_ERROR -41 /* Card error: communication error */

#endif
