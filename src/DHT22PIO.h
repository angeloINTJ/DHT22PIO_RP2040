/**
 * @file DHT22PIO.h
 * @brief Non-blocking DHT22 sensor driver over PIO-accelerated bus.
 *
 * Built on top of DHTBus, this class provides a complete DHT22 interface
 * with an asynchronous state machine design: request a reading, poll
 * update() in your loop, and retrieve results when ready — the CPU is
 * never blocked waiting for the sensor.
 *
 * Features:
 *   - Fully non-blocking / async-friendly (no delay() inside the driver)
 *   - Checksum validation on every read
 *   - Structured state machine for clean error handling
 *   - Supports negative temperatures (bit 15 sign flag)
 *   - Runtime pin switching for multi-sensor setups
 *   - Safe error reporting via snprintf()
 *
 * Wiring:
 * @code
 *   Pico GP pin ──┬── DHT22 Data (pin 2)
 *                 │
 *                4.7kΩ
 *                 │
 *                3.3V
 *
 *   DHT22 VCC (pin 1) ── 3.3V
 *   DHT22 GND (pin 4) ── GND
 *   DHT22 pin 3 ── not connected
 * @endcode
 *
 * @author Ângelo Moisés Alves
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include <Arduino.h>
#include "DHTBus.h"

/**
 * @class DHT22PIO
 * @brief Non-blocking DHT22 driver with state machine and checksum validation.
 *
 * This class does NOT manage the DHTBus lifetime — it receives a reference
 * via dependency injection. The caller is responsible for calling
 * DHTBus::begin() before using any DHT22PIO methods.
 *
 * Typical usage pattern:
 * @code
 *   DHTBus bus(pio0);
 *   bus.begin(15);
 *
 *   DHT22PIO sensor(bus);
 *   sensor.requestReading(15);
 *
 *   // In your main loop:
 *   sensor.update();
 *   if (sensor.getState() == DHT22PIO::DATA_READY) {
 *       float temp, hum;
 *       sensor.getResults(temp, hum);
 *   }
 * @endcode
 */
class DHT22PIO {
public:
    // ----------------------------------------------------------------
    // Enumerations
    // ----------------------------------------------------------------

    /**
     * @brief Async state machine states.
     *
     * The driver progresses through these states during a read cycle:
     *   IDLE → WAITING_PIO → DATA_READY (success)
     *   IDLE → WAITING_PIO → ERROR_TIMEOUT (sensor not responding)
     *   IDLE → WAITING_PIO → ERROR_CHECKSUM (data corruption)
     */
    enum State {
        IDLE,             ///< No operation in progress
        WAITING_PIO,      ///< PIO is capturing data from the sensor
        DATA_READY,       ///< Valid data available via getResults()
        ERROR_TIMEOUT,    ///< Sensor did not respond within TIMEOUT_MS
        ERROR_CHECKSUM    ///< Checksum mismatch — data corrupted
    };

    // ----------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------

    static const uint8_t  PAYLOAD_SIZE = 5;    ///< DHT22 sends 5 bytes (2 hum + 2 temp + 1 checksum)
    static const uint32_t TIMEOUT_MS   = 300;  ///< Maximum wait time for a complete reading (ms)

    // ----------------------------------------------------------------
    // Public API
    // ----------------------------------------------------------------

    /**
     * @brief Construct with a reference to an initialized DHTBus.
     * @param bus Reference to the PIO-backed DHT bus (must outlive this object).
     */
    DHT22PIO(DHTBus &bus);

    /**
     * @brief Start an asynchronous sensor reading.
     *
     * Configures the pin, starts the PIO state machine, and transitions
     * to WAITING_PIO. If a reading is already in progress, the call is
     * ignored (no double-start).
     *
     * @param pin GPIO pin the DHT22 is connected to.
     */
    void requestReading(uint pin);

    /**
     * @brief Poll for new data from the PIO FIFO.
     *
     * Call this frequently in your main loop (e.g., every iteration).
     * It checks if the PIO has deposited bytes in the FIFO and
     * transitions the state machine accordingly:
     *   - All 5 bytes received + checksum OK → DATA_READY
     *   - All 5 bytes received + checksum bad → ERROR_CHECKSUM
     *   - Timeout elapsed → ERROR_TIMEOUT
     *
     * Does nothing if the state is not WAITING_PIO.
     */
    void update();

    /**
     * @brief Reset the state machine back to IDLE.
     *
     * Use this to clear an error state and allow a new requestReading().
     */
    void reset();

    /**
     * @brief Get the current state of the driver.
     * @return Current State enum value.
     */
    State getState();

    /**
     * @brief Retrieve temperature and humidity from the last successful read.
     *
     * Only valid when getState() == DATA_READY. After calling this method,
     * the state automatically resets to IDLE, allowing a new reading cycle.
     *
     * @param temp [out] Temperature in °C (supports negative values).
     * @param hum  [out] Relative humidity in %RH.
     * @return true  Values stored in temp and hum.
     * @return false State is not DATA_READY — no valid data available.
     */
    bool getResults(float &temp, float &hum);

    /**
     * @brief Format a human-readable status/error string.
     *
     * Uses snprintf() internally — safe against buffer overflows.
     *
     * @param buffer    Destination character buffer.
     * @param maxLength Size of the buffer in bytes.
     */
    void getLastErrorString(char *buffer, size_t maxLength);

private:
    DHTBus   &_bus;          ///< Reference to the underlying PIO bus
    uint      _currentPin;   ///< GPIO pin of the current reading
    State     _state;        ///< Current state machine state
    uint32_t  _timerStart;   ///< millis() timestamp when reading started
    uint8_t   _data[5];      ///< Raw payload buffer (2 hum + 2 temp + 1 csum)
    uint8_t   _bytesRead;    ///< Number of bytes captured so far

    /**
     * @brief Validate the 5-byte payload against the DHT22 checksum.
     * @return true  Checksum matches (data[0]+data[1]+data[2]+data[3] == data[4]).
     * @return false Checksum mismatch — data is corrupt.
     */
    bool validateChecksum();
};
