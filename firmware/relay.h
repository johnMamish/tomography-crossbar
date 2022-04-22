#ifndef _RELAY_H
#define _RELAY_H

#include <stdint.h>

#include "relay_map.h"

/**
 * Each output (the "upstream" side / side with only 8 ports) corresponds to a specific relay in a
 * bank.
 *
 * Each input corresponds to a specific relay in each bank. For instance, relays connected to input
 * K would all be in 'bank K'.
 *
 *            OUTPUTS
 *         1    2    3    4    ...    8
 *         |    |    |    |    ...    |
 * I       |    |    |    |    ...    |
 * N  A----x----x----x----x--  ...  --x   <--- This is one relay bank. Each 'x' represents a relay.
 * P       |    |    |    |    ...    |
 * U  B----x----x----x----x--  ...  --x
 * T       |    |    |    |    ...    |
 * S  C----x----x----x----x--  ...  --x
 *         |    |    |    |    ...    |
 *        ...  ...  ...  ...   ...   ...
 *         |    |    |    |    ...    |
 *    P----x----x----x----x--  ...  --x
 */


#define RELAY_MAP_NUM_INPUTS 16
#define RELAY_MAP_NUM_OUTPUTS 8

/**
 * This struct holds the state of all the relays - whether they're open or closed.
 */
typedef struct relay_state {
    int num_inputs;
    int num_outputs;

    /// Grid of size num_inputs x num_outputs.
    /// grid[output][input] is non-zero if the corresponding relay should be closed, and zero
    /// if it should be open.
    uint8_t* grid;
} relay_state_t;


/**
 * Given a desired relay on/off configuration, this function calculates the bitstring that should
 * be fed to the shift registers to achieve the relay configuration.
 *
 * @param[in]     rs         The relay state struct describing which relays should be on and off.
 * @param[out]    shift_register_bits   bitstring to be fed to a the shift registers to
 */
void relay_map_to_shift_register_bits(const relay_state_t* rs, uint8_t* shift_register_bits);

/**
 * transmits a bitstring to the shift registers to configure the relays according to the given
 * relay_state_t.
 */
void set_relays(const relay_state_t* rs);

/**
 * Sets the output enable signal for the relay drivers (should be on PD1)
 */
void relays_enable();

/**
 * Clears the output enable signal for the relay drivers (should be on PD1)
 */
void relays_disable();


#endif
