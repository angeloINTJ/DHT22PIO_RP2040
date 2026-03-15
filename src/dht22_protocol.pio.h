/**
 * @file dht22_protocol.pio.h
 * @brief Pre-assembled PIO program for the DHT22 (AM2302) protocol.
 *
 * This file contains the binary representation of a PIO state machine program
 * that implements the complete DHT22 communication cycle in hardware:
 *   1. Wake-up pulse (1.56 ms LOW — master to sensor)
 *   2. Handshake detection (sensor ACK: ~80 µs LOW + ~80 µs HIGH)
 *   3. 40-bit data capture with intelligent sampling
 *
 * The program runs at 1 MHz (1 µs per instruction cycle) and uses a single
 * GPIO configured as open-drain via the SET pindirs mechanism.
 *
 * Bit discrimination strategy:
 *   The DHT22 encodes '0' as ~28 µs HIGH and '1' as ~70 µs HIGH.
 *   After each rising edge, the program waits exactly 40 µs and then
 *   samples the pin: HIGH = '1' (pulse still active), LOW = '0' (pulse
 *   already ended). This eliminates the need for pulse width measurement.
 *
 * FIFO configuration (set by the driver):
 *   - Joined RX FIFO (8-deep) for reliable buffering
 *   - Auto-push every 8 bits (one byte per FIFO entry)
 *   - MSB-first shift to match DHT22 bit order
 *
 * @note This header is auto-generated from dht22_protocol.pio.
 *       Do not edit manually unless you understand RP2040 PIO assembly.
 *
 * @author Ângelo Moisés Alves
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include "hardware/pio.h"

/**
 * @brief Pre-assembled PIO instruction array for the DHT22 protocol.
 *
 * 17 instructions total. Organized in three logical stages:
 *   [0-7]   Wake-up pulse generation (1.56 ms LOW)
 *   [8-10]  Sensor handshake detection
 *   [11-16] Bit capture loop (40 bits, MSB first, with auto-wrap)
 */
static const uint16_t dht22_protocol_instructions[] = {

    // === STAGE 1: WAKE-UP PULSE (Master → Sensor) ===
    // The master must pull the data line LOW for ≥1 ms to wake the sensor.
    // This implementation generates exactly 1.56 ms: (4+1) × 10 iterations
    // × (1+1) × 30 outer iterations = 1560 µs.
    0xe081, //  0: set    pindirs, 1         ; Configure pin as OUTPUT
    0xe000, //  1: set    pins, 0            ; Drive pin LOW
    0xe03d, //  2: set    x, 29              ; Outer loop counter (30 iterations)
    // delay_outer:
    0xe049, //  3: set    y, 9              ; Inner loop counter (10 iterations)
    // delay_inner:
    0xa342, //  4: nop              [3]     ; 4 cycles (1 + 3 delay)
    0x0084, //  5: jmp    y--, 4            ; 1 cycle → (4+1) × 10 = 50 µs inner
    0x0043, //  6: jmp    x--, 3            ; 1 cycle → (50+2) × 30 = 1560 µs total
    0xe080, //  7: set    pindirs, 0         ; Release pin (INPUT — pull-up takes over)

    // === STAGE 2: SENSOR HANDSHAKE ===
    // The DHT22 responds with: ~80 µs LOW (ACK) → ~80 µs HIGH (preparation)
    // → then begins transmitting 40 data bits.
    0x2020, //  8: wait   0 pin, 0           ; Wait for sensor to pull LOW (ACK)
    0x20a0, //  9: wait   1 pin, 0           ; Wait for sensor to pull HIGH (prep)
    0x2020, // 10: wait   0 pin, 0           ; Wait for end of HIGH (data starts)

    // === STAGE 3: BIT CAPTURE LOOP (40 bits, MSB first) ===
    // .wrap_target — PIO automatically loops back here after instruction 16
    0x20a0, // 11: wait   1 pin, 0           ; Wait for rising edge (bit start)
    0xe034, // 12: set    x, 20              ; Delay counter for 40 µs sampling window
    // bit_loop:
    0xa042, // 13: nop                       ; 1 µs
    0x004d, // 14: jmp    x--, 13            ; 1 µs → (1+1) × 20 = 40 µs total

    // INTELLIGENT SAMPLING POINT:
    // At 40 µs after the rising edge:
    //   - A '0' bit (28 µs HIGH) has already ended → pin is LOW
    //   - A '1' bit (70 µs HIGH) is still active  → pin is HIGH
    0x4001, // 15: in     pins, 1            ; Sample pin → shift 1 bit into ISR
    0x2020  // 16: wait   0 pin, 0           ; Wait for falling edge (bit end)
    // .wrap — automatically jumps back to instruction 11
};

/**
 * @brief PIO program descriptor for the RP2040 SDK.
 *
 * Used by pio_add_program() and pio_can_add_program() for dynamic
 * instruction memory management. origin = -1 allows the SDK to place
 * the program at any available offset.
 */
static const struct pio_program dht22_protocol_program = {
    .instructions = dht22_protocol_instructions,
    .length       = 17,
    .origin       = -1,
};

/**
 * @brief Get the default state machine configuration for this program.
 *
 * Sets the wrap points to the bit capture loop (instructions 11-16)
 * so the PIO automatically loops through the 40 data bits after the
 * one-shot wake-up and handshake stages.
 *
 * @param offset Instruction memory offset returned by pio_add_program().
 * @return Pre-configured pio_sm_config with correct wrap boundaries.
 */
static inline pio_sm_config dht22_protocol_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();

    // Wrap only the bit capture loop (instructions 11 through 16)
    sm_config_set_wrap(&c, offset + 11, offset + 16);

    return c;
}
