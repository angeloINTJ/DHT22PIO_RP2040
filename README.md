# DHT22PIO_RP2040

**Hardware-accelerated DHT22 library for the Raspberry Pi Pico / Pico W.**

The entire DHT22 protocol — wake-up pulse, sensor handshake, and 40-bit data capture — is offloaded to the RP2040's PIO coprocessor. Fully non-blocking, zero bit-banging, zero `delayMicroseconds()`, zero interrupts.

---

## Features

- **PIO-accelerated** — wake-up, handshake, and bit capture are 100% hardware
- **Non-blocking / async** — request a reading, poll `update()`, retrieve when ready; the CPU is never stalled
- **Wi-Fi safe** — dynamic PIO allocation prevents collisions with the CYW43 driver on Pico W
- **RAII resource management** — PIO state machines and instruction memory are automatically released on destruction
- **Runtime pin switching** — multiplex multiple sensors through a single PIO instance via `setPin()`
- **Checksum validation** — every reading is integrity-checked
- **Negative temperature support** — correctly handles the DHT22 sign bit
- **Structured error states** — state machine with IDLE, WAITING_PIO, DATA_READY, ERROR_TIMEOUT, ERROR_CHECKSUM
- **Minimal footprint** — 17 PIO instructions, lightweight C++ classes

## Hardware Requirements

| Component | Description |
|-----------|-------------|
| Raspberry Pi Pico / Pico W | RP2040-based board |
| DHT22 (AM2302) | Temperature & humidity sensor |
| 4.7 kΩ resistor | Pull-up between data line and 3.3V |

### Wiring Diagram

```
Pico GP15 ──┬── DHT22 Data (pin 2)
            │
          4.7kΩ
            │
          3.3V

DHT22 VCC (pin 1) ── 3.3V
DHT22 GND (pin 4) ── GND
DHT22 pin 3       ── not connected
```

## Installation

### Arduino IDE

1. Download this repository as a `.zip` file
2. Go to **Sketch → Include Library → Add .ZIP Library...**
3. Select the downloaded file
4. Select your board: **Tools → Board → Raspberry Pi Pico / Pico W**

### PlatformIO

Add to your `platformio.ini`:

```ini
[env:pico]
platform = raspberrypi
board = pico
framework = arduino
lib_deps =
    https://github.com/angeloINTJ/DHT22PIO_RP2040.git
```

### Arduino Library Manager

*(Coming soon — pending inclusion in the official Arduino Library Manager index.)*

## Quick Start

```cpp
#include <DHTBus.h>
#include <DHT22PIO.h>

DHTBus bus(pio0);
DHT22PIO sensor(bus);

void setup() {
    Serial.begin(115200);
    bus.begin(15);  // GP15
}

void loop() {
    sensor.requestReading(15);

    while (sensor.getState() == DHT22PIO::WAITING_PIO) {
        sensor.update();
    }

    float temp, hum;
    if (sensor.getResults(temp, hum)) {
        Serial.print(temp, 1);
        Serial.print(" °C, ");
        Serial.print(hum, 1);
        Serial.println(" %RH");
    } else {
        sensor.reset();
    }

    delay(3000);
}
```

## Non-Blocking Pattern

The key advantage of this library is that `update()` returns immediately — the CPU is free to handle other tasks while the PIO captures data in hardware:

```cpp
void loop() {
    // Kick off a reading (returns instantly)
    if (!readingInProgress) {
        sensor.requestReading(15);
        readingInProgress = true;
    }

    // Poll (non-blocking — returns immediately)
    sensor.update();

    if (sensor.getState() == DHT22PIO::DATA_READY) {
        float temp, hum;
        sensor.getResults(temp, hum);
        readingInProgress = false;
    }

    // CPU is free for other work here:
    // update display, handle web requests, read other sensors...
}
```

## Examples

| Example | Description |
|---------|-------------|
| [BasicReading](examples/BasicReading/) | Minimal read loop — start here |
| [MultiSensor](examples/MultiSensor/) | Multiple DHT22 on different pins with one PIO instance |
| [NonBlockingRead](examples/NonBlockingRead/) | True async pattern with free CPU cycles |
| [DiagnosticMode](examples/DiagnosticMode/) | State machine lifecycle and timing inspection |
| [HeatIndex](examples/HeatIndex/) | Compute feels-like temperature (NOAA Rothfusz equation) |

## API Reference

### DHTBus (Physical Layer)

```cpp
DHTBus bus(pio0);           // or pio1
bool ok = bus.begin(pin);   // Initialize PIO + claim state machine
bus.setPin(pin);             // Switch GPIO at runtime
bus.startPIORead();          // Start wake-up + data capture
bus.stopPIORead();           // Halt the state machine
bool avail = bus.hasData();  // Check RX FIFO
uint32_t raw = bus.readFIFO(); // Pop one word from FIFO
```

### DHT22PIO (Sensor Driver)

```cpp
DHT22PIO sensor(bus);

// Async read cycle
sensor.requestReading(pin);         // Start (non-blocking)
sensor.update();                    // Poll (call frequently)
DHT22PIO::State s = sensor.getState(); // Check progress
sensor.reset();                     // Clear error state

// Retrieve data (only valid when state == DATA_READY)
float temp, hum;
sensor.getResults(temp, hum);

// Error reporting
char msg[64];
sensor.getLastErrorString(msg, sizeof(msg));
```

### State Machine

```
  requestReading()           update()              update()
IDLE ──────────────► WAITING_PIO ──────► DATA_READY
                         │                    │
                         │ timeout             │ getResults()
                         ▼                    ▼
                    ERROR_TIMEOUT           IDLE
                         │
                         │ bad checksum
                         ▼
                    ERROR_CHECKSUM

         reset() returns any error state → IDLE
```

## How It Works

The RP2040 has two PIO blocks, each with 4 state machines and 32 instruction slots. This library loads a 17-instruction program that implements the full DHT22 protocol in hardware:

1. **Wake-up pulse** — drives the pin LOW for exactly 1.56 ms using nested countdown loops
2. **Handshake** — waits for the sensor's ACK sequence (~80 µs LOW + ~80 µs HIGH)
3. **Bit capture** — for each of the 40 data bits, waits for the rising edge, delays 40 µs, then samples the pin:
   - If HIGH → the 70 µs pulse (bit '1') is still active
   - If LOW → the 28 µs pulse (bit '0') has already ended

The PIO clock runs at 1 MHz (1 µs/tick). All timing is cycle-accurate regardless of CPU load.

## Architecture

```
┌──────────────┐     ┌──────────────┐
│  DHT22PIO    │────▶│   DHTBus     │
│  (Driver)    │     │  (PHY Layer) │
│              │     │              │
│ • State mach │     │ • PIO mgmt   │
│ • Checksum   │     │ • startRead  │
│ • Temp/Hum   │     │ • stopRead   │
│ • Error rpt  │     │ • FIFO access│
└──────────────┘     └──────┬───────┘
                            │
                     ┌──────▼───────┐
                     │  RP2040 PIO  │
                     │  State Mach. │
                     │              │
                     │ 17-instr asm │
                     │ 1 µs/tick    │
                     └──────┬───────┘
                            │
                        GPIO pin
                            │
                       ┌────▼────┐
                       │  DHT22  │
                       └─────────┘
```

## Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting a pull request.

### Quick Guide

1. Fork this repository
2. Create a feature branch: `git checkout -b feature/my-improvement`
3. Commit your changes: `git commit -m "Add: description of change"`
4. Push to the branch: `git push origin feature/my-improvement`
5. Open a Pull Request

## License

This project is licensed under the MIT License — see [LICENSE](LICENSE) for details.

## Acknowledgments

- Aosong Electronics for the DHT22/AM2302 datasheet
- Raspberry Pi Foundation for the RP2040 PIO architecture
- The Arduino-Pico community for the RP2040 Arduino core

## See Also

- [OneWirePIO_RP2040](https://github.com/angeloINTJ/OneWirePIO_RP2040) — PIO-accelerated DS18B20 library by the same author
