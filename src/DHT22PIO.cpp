/**
 * @file DHT22PIO.cpp
 * @brief Implementation of the non-blocking DHT22 sensor driver.
 * @see DHT22PIO.h for full API documentation.
 */

#include "DHT22PIO.h"

// ====================================================================
// Construction
// ====================================================================

DHT22PIO::DHT22PIO(DHTBus &bus)
    : _bus(bus)
    , _currentPin(0)
    , _state(IDLE)
    , _timerStart(0)
    , _data{}
    , _bytesRead(0)
{
}

// ====================================================================
// Async Read Cycle
// ====================================================================

void DHT22PIO::requestReading(uint pin) {
    // Prevent double-start: ignore if already reading
    if (_state == WAITING_PIO) {
        return;
    }

    _currentPin = pin;
    _bytesRead  = 0;

    // Transfer full control to the PIO hardware.
    // The state machine will: send the 1.56 ms wake-up pulse,
    // wait for the sensor handshake, and capture 40 data bits —
    // all without any CPU involvement.
    _bus.setPin(_currentPin);
    _bus.startPIORead();

    _timerStart = millis();
    _state = WAITING_PIO;
}

void DHT22PIO::update() {
    switch (_state) {
        // Nothing to do in terminal states
        case IDLE:
        case DATA_READY:
        case ERROR_TIMEOUT:
        case ERROR_CHECKSUM:
            return;

        case WAITING_PIO:
            // Drain available bytes from the PIO FIFO
            while (_bus.hasData() && _bytesRead < PAYLOAD_SIZE) {
                _data[_bytesRead++] = static_cast<uint8_t>(_bus.readFIFO() & 0xFF);
            }

            // All 5 bytes captured — validate and finalize
            if (_bytesRead >= PAYLOAD_SIZE) {
                _bus.stopPIORead();
                _state = validateChecksum() ? DATA_READY : ERROR_CHECKSUM;
            }
            // Timeout protection
            else if (millis() - _timerStart > TIMEOUT_MS) {
                _bus.stopPIORead();
                _state = ERROR_TIMEOUT;
            }
            break;
    }
}

void DHT22PIO::reset() {
    _state = IDLE;
}

// ====================================================================
// Data Retrieval
// ====================================================================

bool DHT22PIO::getResults(float &temp, float &hum) {
    if (_state != DATA_READY) {
        return false;
    }

    // Humidity: bytes 0-1, unsigned, resolution 0.1 %RH
    uint16_t humRaw = (static_cast<uint16_t>(_data[0]) << 8) | _data[1];
    hum = static_cast<float>(humRaw) * 0.1f;

    // Temperature: bytes 2-3, bit 15 = sign, resolution 0.1 °C
    uint16_t tempRaw = (static_cast<uint16_t>(_data[2]) << 8) | _data[3];
    if (tempRaw & 0x8000) {
        tempRaw &= 0x7FFF;
        temp = static_cast<float>(tempRaw) * -0.1f;
    } else {
        temp = static_cast<float>(tempRaw) * 0.1f;
    }

    // Auto-reset to IDLE for the next reading cycle
    _state = IDLE;
    return true;
}

// ====================================================================
// State & Error Reporting
// ====================================================================

DHT22PIO::State DHT22PIO::getState() {
    return _state;
}

void DHT22PIO::getLastErrorString(char *buffer, size_t maxLength) {
    if (buffer == nullptr || maxLength == 0) {
        return;
    }

    switch (_state) {
        case IDLE:
            snprintf(buffer, maxLength, "OK: Idle");
            break;
        case WAITING_PIO:
            snprintf(buffer, maxLength,
                     "Busy: Reading sensor on pin %u", _currentPin);
            break;
        case DATA_READY:
            snprintf(buffer, maxLength,
                     "OK: Data ready from pin %u", _currentPin);
            break;
        case ERROR_TIMEOUT:
            snprintf(buffer, maxLength,
                     "Error: Timeout on pin %u — sensor not responding", _currentPin);
            break;
        case ERROR_CHECKSUM:
            snprintf(buffer, maxLength,
                     "Error: Checksum mismatch on pin %u — data corrupted", _currentPin);
            break;
        default:
            snprintf(buffer, maxLength, "Error: Unknown state");
            break;
    }
}

// ====================================================================
// Private Helpers
// ====================================================================

bool DHT22PIO::validateChecksum() {
    uint8_t sum = _data[0] + _data[1] + _data[2] + _data[3];
    return (sum == _data[4]);
}
