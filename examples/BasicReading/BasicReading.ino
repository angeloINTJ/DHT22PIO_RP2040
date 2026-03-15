/**
 * @file BasicReading.ino
 * @brief Minimal example — read DHT22 temperature and humidity using PIO.
 *
 * Demonstrates the non-blocking read pattern: request → poll → retrieve.
 * A simple blocking wrapper is used here for clarity.
 *
 * Wiring:
 *   GP15 ──┬── DHT22 Data (pin 2)
 *           │
 *          4.7kΩ
 *           │
 *          3.3V
 *
 * Open Serial Monitor at 115200 baud to see the output.
 */

#include <DHTBus.h>
#include <DHT22PIO.h>

/// GPIO pin connected to the DHT22 data line
static const uint SENSOR_PIN = 15;

/// PIO-backed DHT bus (using PIO block 0)
DHTBus bus(pio0);

/// DHT22 high-level driver
DHT22PIO sensor(bus);

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    Serial.println("=== DHT22PIO_RP2040 — Basic Reading ===\n");

    if (!bus.begin(SENSOR_PIN)) {
        Serial.println("FATAL: Could not initialize PIO (no free SM or memory).");
        while (true) { delay(1000); }
    }

    Serial.println("PIO initialized. Reading every 3 seconds...\n");
}

void loop() {
    // Step 1: Start an async reading
    sensor.requestReading(SENSOR_PIN);

    // Step 2: Poll until complete (simple blocking approach)
    while (sensor.getState() == DHT22PIO::WAITING_PIO) {
        sensor.update();
    }

    // Step 3: Check the result
    float temp, hum;
    if (sensor.getResults(temp, hum)) {
        Serial.print("Temperature: ");
        Serial.print(temp, 1);
        Serial.print(" °C  |  Humidity: ");
        Serial.print(hum, 1);
        Serial.println(" %RH");
    } else {
        char err[64];
        sensor.getLastErrorString(err, sizeof(err));
        Serial.println(err);
        sensor.reset();
    }

    // DHT22 requires at least 2 seconds between readings
    delay(3000);
}
