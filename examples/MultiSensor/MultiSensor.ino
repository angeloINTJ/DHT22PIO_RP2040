/**
 * @file MultiSensor.ino
 * @brief Read multiple DHT22 sensors on different GPIO pins with a single PIO instance.
 *
 * Demonstrates the setPin() feature: one PIO state machine is shared across
 * multiple sensors by switching the active GPIO pin between readings.
 *
 * Wiring (each sensor needs its own 4.7kΩ pull-up to 3.3V):
 *   GP15 ──┬── DHT22 #1 Data     GP16 ──┬── DHT22 #2 Data
 *          4.7kΩ                         4.7kΩ
 *          3.3V                          3.3V
 */

#include <DHTBus.h>
#include <DHT22PIO.h>

/// GPIO pins for each sensor
static const uint SENSOR_PINS[] = { 15, 16 };
static const uint NUM_SENSORS   = sizeof(SENSOR_PINS) / sizeof(SENSOR_PINS[0]);

DHTBus bus(pio0);
DHT22PIO sensor(bus);

/**
 * @brief Blocking helper: request, poll, and return results.
 */
bool readSensor(uint pin, float &temp, float &hum) {
    sensor.requestReading(pin);

    while (sensor.getState() == DHT22PIO::WAITING_PIO) {
        sensor.update();
    }

    if (sensor.getResults(temp, hum)) {
        return true;
    }

    sensor.reset();
    return false;
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    Serial.println("=== DHT22PIO_RP2040 — Multi-Sensor ===\n");

    if (!bus.begin(SENSOR_PINS[0])) {
        Serial.println("FATAL: PIO init failed.");
        while (true) { delay(1000); }
    }
}

void loop() {
    for (uint i = 0; i < NUM_SENSORS; i++) {
        float temp, hum;

        if (readSensor(SENSOR_PINS[i], temp, hum)) {
            Serial.print("Sensor #");
            Serial.print(i);
            Serial.print(" (GP");
            Serial.print(SENSOR_PINS[i]);
            Serial.print("): ");
            Serial.print(temp, 1);
            Serial.print(" °C, ");
            Serial.print(hum, 1);
            Serial.println(" %RH");
        } else {
            char err[64];
            sensor.getLastErrorString(err, sizeof(err));
            Serial.println(err);
        }

        // DHT22 needs at least 2 s between readings on the same sensor
        delay(2500);
    }

    Serial.println("---");
}
