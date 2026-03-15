/**
 * @file DHTBus.cpp
 * @brief Implementation of the hardware-accelerated DHT22 PHY layer.
 * @see DHTBus.h for full API documentation.
 */

#include "DHTBus.h"
#include "dht22_protocol.pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"

// ====================================================================
// Construction / Destruction (RAII)
// ====================================================================

DHTBus::DHTBus(PIO pio_instance)
    : _sm(0)
    , _offset(0)
    , _pio(pio_instance)
    , _c{}
    , _isInitialized(false)
{
}

DHTBus::~DHTBus() {
    if (_isInitialized) {
        pio_sm_set_enabled(_pio, _sm, false);
        pio_sm_unclaim(_pio, _sm);
        pio_remove_program(_pio, &dht22_protocol_program, _offset);
        _isInitialized = false;
    }
}

// ====================================================================
// Initialization
// ====================================================================

bool DHTBus::begin(uint pin) {
    // Check if there is room in instruction memory for the PIO program.
    // Dynamic allocation prevents collisions with the CYW43 Wi-Fi driver
    // on the Pico W, which also uses PIO instruction memory.
    if (!pio_can_add_program(_pio, &dht22_protocol_program)) {
        return false;
    }

    // Try to claim an unused state machine (non-blocking)
    int sm = pio_claim_unused_sm(_pio, false);
    if (sm < 0) {
        return false;
    }

    _sm     = static_cast<uint>(sm);
    _offset = pio_add_program(_pio, &dht22_protocol_program);

    // Get the protocol-specific default config (includes wrap points)
    _c = dht22_protocol_program_get_default_config(_offset);

    // Join both FIFOs into a single 8-deep RX FIFO for reliable buffering
    sm_config_set_fifo_join(&_c, PIO_FIFO_JOIN_RX);

    // Shift in MSB-first, auto-push every 8 bits (one byte per FIFO entry)
    sm_config_set_in_shift(&_c, false, true, 8);

    // Clock divider: 1 tick = 1 µs (required by the PIO timing program)
    float clk_div = static_cast<float>(clock_get_hz(clk_sys)) / 1000000.0f;
    sm_config_set_clkdiv(&_c, clk_div);

    // Configure the GPIO and prepare the state machine
    setPin(pin);

    _isInitialized = true;
    return true;
}

// ====================================================================
// Pin Management
// ====================================================================

void DHTBus::setPin(uint pin) {
    pio_sm_set_enabled(_pio, _sm, false);

    // Hand GPIO control to the PIO block and enable internal pull-up
    pio_gpio_init(_pio, pin);
    gpio_pull_up(pin);

    // Map SET and IN to the chosen pin
    sm_config_set_in_pins(&_c, pin);
    sm_config_set_set_pins(&_c, pin, 1);

    // Start with pin as input (idle state — bus pulled HIGH by resistor)
    pio_sm_set_consecutive_pindirs(_pio, _sm, pin, 1, false);
    pio_sm_init(_pio, _sm, _offset, &_c);
}

// ====================================================================
// PIO Read Cycle Control
// ====================================================================

void DHTBus::startPIORead() {
    pio_sm_clear_fifos(_pio, _sm);
    pio_sm_set_enabled(_pio, _sm, true);
}

void DHTBus::stopPIORead() {
    pio_sm_set_enabled(_pio, _sm, false);
}

// ====================================================================
// FIFO Access
// ====================================================================

bool DHTBus::hasData() {
    return !pio_sm_is_rx_fifo_empty(_pio, _sm);
}

uint32_t DHTBus::readFIFO() {
    return pio_sm_get(_pio, _sm);
}
