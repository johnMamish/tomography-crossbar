#ifndef _COMMANDS_H
#define _COMMANDS_H

#include <stdint.h>

typedef struct command_descriptor {
    /// Name of the command
    const char* cmd_name;

    /// Pointer to a function that should be executed when the command associated with the
    /// descriptor is to be run.
    ///     @param[in]     command_data     Data payload of the associated command
    ///     @param[in]     command_data_len Length of the data payload
    ///     @param[out]    response         to be printed over the serial terminal
    ///     @param[out]    response_len     length of the response to print
    ///     @returns  0 if the command executed successfully, nonzero otherwise.
    ///
    /// This function may have side effects - it may take read or write data from the 'child'
    /// serial ports.
    int (*exec)(const uint8_t* command_data,
                int command_data_len,
                uint8_t* response,
                int* response_len);
} command_descriptor_t;

const extern command_descriptor_t commands[];

#endif
