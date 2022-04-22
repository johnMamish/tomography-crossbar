#include "commands.h"
#include "relay.h"
#include "uart.h"

#include <string.h>

#define SYSTICK_CVR *((volatile uint32_t*)0xe000e014);

static int is_numeric(char ch)
{
    return ((ch >= '0') && (ch <= '9'));
}

/**
 * Returns -1 on error.
 */
static int my_atoi(const char* s, uint32_t* result)
{
    // consume whitespace at start
    int i;
    for (i = 0; s[i] == ' '; i++);

    *result = 0;
    for (; s[i]; i++) {
        if (!is_numeric(s[i]))
            return -1;

        *result *= 10;
        *result += s[i] - '0';
    }

    return 0;
}

/**
 * Commands
 *
 * Commands are sent in non-human readable binary over UART.
 *
 * All commands consist of a variable-length command string followed by a command-specific
 * formatted data field. Data fields are typically of a fixed format for each command.
 *
 * The device echos each character sent (and also echos '\n' with '\r\n').
 *
 * The device responds to commands sent.
 *
 * If a command is received and successfully executed, the device responds first with the string
 * "ACK.\r\n" and then with a command-specific response.
 * If a command is received and it is malformed or an error occurs while the command is being
 * executed, the device responds with the string "NAK.\r\n" and then with a command-specific
 * response.
 *
 *
 *     '?' - query device                       (!! UNIMPLEMENTED !!)
 *         Checks to see if the device is present. The device responds with its number of inputs
 *         and outputs, as well as the responses from its left and right children.
 *         Example response for a single 16x8 crossbar with no children:
 *             tx: "<?>"
 *             rx: "<16x8>"
 *         This
 *
 *     ' ' - Connect
 *         Connects an input and an output
 *
 *     ' ' - Disconnect
 *         Breaks
 *
 *     'o' - Map output                      (!! UNIMPLEMENTED !!)
 *         Sets a
 *         Given in lowercase ascii hex digits.
 *             <0x3c><0x6f><output><input><0x3e>
 *         for instance, the following command will route input 15 to output 0.
 *             <0x3c><0x6f><0x30><0x66><0x3e>
 *
 *     'C' - enable config sanity checking   (!! UNIMPLEMENTED !!)
 *         If this command is sent, future "map output" commands will be ineffective if they short 2
 *         or more inputs to the same output. By default, sanity checking is off.
 *             <0x3c><0x43><0x3e>
 *
 *     'c' - disable config sanity checking  (!! UNIMPLEMENTED !!)
 *         If this command is sent, future "map output" commands will not be subjected to sanity
 *         checking; outputs are allowed to be shorted together.
 *             <0x3c><0x63><0x3e>
 *
 *     'E' - enable relay output             (!! UNIMPLEMENTED !!)
 *         Turns all relays on
 *             <0x3c><0x45><0x3e>
 *
 *     'e' - disable relay output            (!! UNIMPLEMENTED !!)
 *         Turns all relays off without forgetting the assigned mapping
 *
 *     'L' - transmit to child left          (!! UNIMPLEMENTED !!)
 *         Sends all following characters up until the
 *
 *     'R' - transmit to child right         (!! UNIMPLEMENTED !!)
 *
 *     'k' - Clear all relays
 *           Shuts off all relays
 */

static int poll_until_newline(uint8_t* response, int (*getch)())
{
    int i = 0;
    int32_t tstart = SYSTICK_CVR;
    int32_t telap = 0;
    int timed_out = 0;
    do {
        int ch = getch();
        if (ch != -1) {
            tstart = SYSTICK_CVR;
            response[i++] = ch;

            // check for endline
            if ((i >= 1) &&
                (response[i - 1] == '\n') &&
                (response[i - 2] == '\r')) {
                i -= 2;
                break;
            }
            else if (response[i - 1] == '\n') {
                i--;
                break;
            }
        }

        int32_t tnow = SYSTICK_CVR;
        if (tstart > tnow)
            telap = tstart - tnow;
        else
            telap = (tstart - tnow) + 0x00ffffff;

        if (telap >= 800000)
            timed_out = 1;
    } while (!timed_out);

    if (timed_out)
        return -1;
    else
        return i;
}

static int forward_to_child(int child_number,
                            const uint8_t* command_data,
                            int command_data_len,
                            uint8_t* response,
                            int* response_len)
{
    *response_len = 0;

    void (*child_putch)(char) = NULL;
    int (*child_getch)(void) = NULL;
    if (child_number == 1) {
        child_putch = uart5_putch;
        child_getch = uart5_getch;
    } else if (child_number == 2) {
        child_putch = uart7_putch;
        child_getch = uart7_getch;
    } else {
        return -1;
    }

    ////////////////////////////////////////////////////////////////
    // find and check parentheses in input
    if (command_data[0] != '(') {
        return -1;
    }

    int retval = 0;
    int input_len, p;
    for (input_len = 1, p = 1; (input_len < command_data_len) && (p > 0); input_len++) {
        if (command_data[input_len] == '(')
            p++;
        if (command_data[input_len] == ')')
            p--;
    }

    if (input_len != command_data_len) {
        const char* r = "trailing data after closing parenthesis.";
        *response_len = strlen(r);
        memcpy(response, r, *response_len);
        return -1;
    }

    ////////////////////////////////////////////////////////////////
    // send command to child
    for (int i = 1; i < (command_data_len - 1); i++) {
        child_putch(command_data[i]);
    }

    ////////////////////////////////////////////////////////////////
    // busywait for ACK from child
    int ack_len = poll_until_newline(response, child_getch);
    if ((ack_len != 4) || (memcmp(response, "ACK.", 4))) {
        // ACK wasn't received
        retval = -1;
    }

    ////////////////////////////////////////////////////////////////
    // busywait for response from child
    *response_len = poll_until_newline(response + 1, child_getch);
    if (*response_len != -1) {
        // ACK was received and command didn't time out.
        // pad the response with parentheses.
        response[0] = '(';
        response[*response_len + 2] = ')';
        *response_len += 2;
    } else {
        retval = -1;
    }

    return retval;
}

static int forward_to_child_1(const uint8_t* command_data,
                              int command_data_len,
                              uint8_t* response,
                              int* response_len)
{
    return forward_to_child(1, command_data, command_data_len, response, response_len);
}

static int forward_to_child_2(const uint8_t* command_data,
                              int command_data_len,
                              uint8_t* response,
                              int* response_len)
{
    return forward_to_child(2, command_data, command_data_len, response, response_len);
}

static int query(const uint8_t* command_data,
                 int command_data_len,
                 uint8_t* response,
                 int* response_len)
{
    if (command_data_len == 0) {
        *response_len = 6;
        memcpy((char*)response, "(16,8)", *response_len);
        return 0;
    } else {
        return -1;
    }
}

/**
 * Connects an input to an output.
 *
 * Command format:
 *     connect <output>,<input>
 *
 * The input and output fields are both 3-character, decimal numbers. The leading characters can
 * be either spaces or zeros.
 *
 * example:
 * connect input 14 to output 2
 *     connect 002,014
 * or
 *     connect   2, 14
 */
static int connect(const uint8_t* command_data,
                   int command_data_len,
                   uint8_t* response,
                   int* response_len)
{
    // Sanity check input.
    if ((command_data[3] != ',') || (command_data_len != 7)) {
        const char* s = "malformed command";
        *response_len = strlen(s);
        memcpy(response, s, *response_len);
        return -1;
    }

    // Parse numbers
    char num_strs[2][4] = { 0 };
    memcpy(num_strs[0], &command_data[0], 3);
    memcpy(num_strs[1], &command_data[4], 3);

    uint32_t output, input;
    if ((my_atoi(num_strs[0], &output) == -1) ||
        (my_atoi(num_strs[1], &input) == -1)) {
        const char* s = "invalid input/output numbers";
        *response_len = strlen(s);
        memcpy(response, s, *response_len);
        return -1;
    }




    *response_len = 0;
}

static int disconnect(const uint8_t* command_data,
                      int command_data_len,
                      uint8_t* response,
                      int* response_len)
{

}

const command_descriptor_t commands[] = {
    {
        "forward_to_child_1",
        forward_to_child_1,
    },

    {
        "forward_to_child_2",
        forward_to_child_2
    },

    {
        "query",
        query
    },

    {
        "connnect",
        connect
    },

    {
        "disconnect",
        disconnect
    },

    {
        "enable_output",
        enable_output
    },

    {
        "disable_output",
        disable_output
    },
};
