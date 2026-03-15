/**
 * @file DHTBus.h
 * @brief Hardware-accelerated DHT22 Physical Layer (PHY) using RP2040 PIO.
 *
 * This class manages the PIO coprocessor to handle the entire DHT22 wire
 * protocol in hardware: wake-up pulse generation, sensor handshake, and
 * bit-level data capture — all without CPU intervention.
 *
 * Features:
 *   - Dynamic PIO program loading (no hardcoded offsets)
 *   - RAII resource management (automatic cleanup on destruction)
 *   - Runtime pin reassignment via setPin()
 *   - Wi-Fi safe: does not collide with CYW43 PIO usage on Pico W
 *   - Joined RX FIFO (8-deep) for reliable data buffering
 *
 * Hardware Requirements:
 *   - Raspberry Pi Pico / Pico W (RP2040)
 *   - DHT22 (AM2302) sensor with 4.7kΩ pull-up resistor
 *
 * @author Ângelo Moisés Alves
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include <Arduino.h>
#include "hardware/pio.h"

/**
 * @class DHTBus
 * @brief DHT22 Physical Layer implemented via the RP2040 PIO state machine.
 *
 * Manages PIO resource allocation (state machine + instruction memory) and
 * provides low-level control over the DHT22 communication cycle. This class
 * does NOT interpret the data — it only captures raw bytes from the FIFO.
 *
 * @note One instance controls one PIO state machine. The RP2040 has 8 state
 *       machines total (4 per PIO block), so up to 8 independent buses can
 *       coexist.
 *
 * Usage:
 * @code
 *   DHTBus bus(pio0);
 *   if (!bus.begin(GP_PIN)) {
 *       // Handle: no free state machine or instruction memory
 *   }
 *   bus.startPIORead();          // PIO sends wake-up + captures 40 bits
 *   while (!bus.hasData()) {}    // Poll FIFO
 *   uint32_t raw = bus.readFIFO();
 *   bus.stopPIORead();
 * @endcode
 */
class DHTBus {
public:
    /**
     * @brief Construct a new DHTBus instance.
     * @param pio_instance PIO block to use: pio0 (default) or pio1.
     *
     * No hardware is touched until begin() is called. This allows safe
     * construction of global objects before the runtime is fully initialized.
     */
    DHTBus(PIO pio_instance = pio0);

    /**
     * @brief Destructor — releases all PIO resources (RAII).
     *
     * Disables the state machine, unclaims it, and removes the PIO program
     * from instruction memory. Safe to call even if begin() was never called
     * or if it failed.
     */
    ~DHTBus();

    /**
     * @brief Initialize the PIO hardware for DHT22 communication.
     *
     * Dynamically loads the PIO program, claims an unused state machine,
     * configures clock divider to 1 µs/tick, joins the RX FIFO, and
     * sets up the data pin with an internal pull-up.
     *
     * @param pin GPIO pin number connected to the DHT22 data line.
     * @return true  Initialization successful.
     * @return false No free state machine or instruction memory available.
     */
    bool begin(uint pin);

    /**
     * @brief Change the active GPIO pin at runtime.
     *
     * Temporarily disables the state machine, reconfigures pin mapping
     * with internal pull-up, and re-initializes the SM. Useful for
     * multiplexing multiple sensors on different pins.
     *
     * @param pin New GPIO pin number.
     */
    void setPin(uint pin);

    /**
     * @brief Start the PIO read cycle.
     *
     * Clears any residual FIFO data and enables the state machine.
     * The PIO immediately begins the protocol: sends the 1.56 ms wake-up
     * pulse, waits for the sensor handshake, and captures 40 data bits.
     */
    void startPIORead();

    /**
     * @brief Stop the PIO state machine.
     *
     * Disables the state machine, halting all PIO activity on the pin.
     * Call this after reading all 5 bytes or on timeout.
     */
    void stopPIORead();

    /**
     * @brief Check if the PIO has deposited data in the RX FIFO.
     * @return true  At least one byte is available in the FIFO.
     * @return false FIFO is empty — data not yet captured.
     */
    bool hasData();

    /**
     * @brief Read one raw 32-bit word from the PIO RX FIFO.
     *
     * The useful data byte is in the lowest 8 bits. The upper bits
     * should be masked by the caller.
     *
     * @return Raw 32-bit value from the FIFO.
     */
    uint32_t readFIFO();

private:
    uint        _sm;              ///< Claimed PIO state machine index
    uint        _offset;          ///< PIO instruction memory offset
    PIO         _pio;             ///< PIO block instance (pio0 or pio1)
    pio_sm_config _c;             ///< Cached state machine configuration
    bool        _isInitialized;   ///< Safety flag for RAII destructor
};
