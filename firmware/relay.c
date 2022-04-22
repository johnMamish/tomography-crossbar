#include "relay.h"
#include "tm4c123gh6pm.h"

/**
 * out '7' - bit 0
 * out '8' - bit 7
 * out '5' - bit 1
 * out '6' - bit 6
 * out '3' - bit 2
 * out '4' - bit 5
 * out '1' - bit 3
 * out '2' - bit 4
 */

static void ssi1_blocking_write(uint8_t data)
{
    while (!(SSI1_SR_R & SSI_SR_TNF));
    SSI1_DR_R = data;
}


/**
 * This array maps input BNC connector numbers to banks of relays.
 *
 * The connector labelled with number 'n' on the board will correspond to the input_map[n]-th
 * bank of relays.
 */
const static int input_bnc_connector_to_relay_bank_mapping[RELAY_MAP_NUM_INPUTS] = {
    0, 15, 1, 14, 2, 13, 3, 12, 4, 11, 5, 10, 6, 9, 7, 8
};

/**
 * This array maps output BNC connector numbers to relay numbers in each bank.
 */
const static int output_bnc_connector_to_relay_number_mapping[RELAY_MAP_NUM_OUTPUTS] = {
    // 3, 4, 2, 5, 1, 6, 0, 7
    3, 4, 2, 5, 1, 6, 0, 7
};

/**
 * Given an output number, gets the associated relay number in a bank.
 *
 * e.g. to connect a relay bank to output 0, relay number 7 in that bank might need to be actuated.
 *      This function will return the relay number associated with an output
 *
 * This function return -1 if the given number is out of bounds.
 */
int get_relay_number_for_output(int output_number)
{
    if ((output_number < 0) ||
        (output_number >= RELAY_MAP_NUM_OUTPUTS))
        return -1;
    else
        return output_bnc_connector_to_relay_number_mapping[output_number];
}

int get_relay_bank_for_input(int input_number)
{
    if ((input_number < 0) ||
        (input_number >= RELAY_MAP_NUM_INPUTS))
        return -1;
    else
        return input_bnc_connector_to_relay_bank_mapping[input_number];
}

void relay_map_to_shift_register_bits(const relay_state_t* rs, uint8_t* shift_register_bits)
{
    for (int output_idx = 0; output_idx < rs->num_outputs; output_idx++) {
        for (int input_idx = 0; input_idx < rs->num_inputs; input_idx++) {
            if (rs->grid[(output_idx * rs->num_inputs) + input_idx]) {
                int bank_number = get_relay_bank_for_input(input_idx);
                int relay_number = get_relay_number_for_output(output_idx);
                shift_register_bits[bank_number] |= (1 << relay_number);
            }
        }
    }

}

void set_relays(const relay_state_t* rs)
{
    uint8_t shift_register_bits[16] = { 0 };
    relay_map_to_shift_register_bits(rs, shift_register_bits);

    // bring strobe low
    GPIO_PORTD_DATA_BITS_R[(1 << 2)] = 0;
    for (int i = 0; i < 16; i++) {
        ssi1_blocking_write(shift_register_bits[i]);
    }

    // busywait before bringing strobe high
    while (SSI1_SR_R & SSI_SR_BSY);
    GPIO_PORTD_DATA_BITS_R[(1 << 2)] = (1 << 2);
}

void relays_enable()
{
    GPIO_PORTD_DATA_BITS_R[(1 << 1)] = 0;
}

void relays_disable()
{
    GPIO_PORTD_DATA_BITS_R[(1 << 1)] = (1 << 1);
}
