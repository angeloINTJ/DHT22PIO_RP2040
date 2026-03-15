/**
 * @file DiagnosticMode.ino
 * @brief Advanced example — observe the full state machine lifecycle and raw data.
 *
 * Prints every state transition and the raw 5-byte payload, useful for
 * debugging wiring issues, noisy buses, or sensor failures.
 *
 * Wiring: GP15 + 4.7kΩ pull-up to 3.3V.
 */

#include <DHTBus.h>
#include <DHT22PIO.h>

static const uint SENSOR_PIN = 15;

DHTBus bus(pio0);
DHT22PIO sensor(bus);

/**
 * @brief Convert a State enum to a human-readable string.
 */
const char* stateToString(DHT22PIO::State s) {
    switch (s) {
        case DHT22PIO::IDLE:           return "IDLE";
        case DHT22PIO::WAITING_PIO:    return "WAITING_PIO";
        case DHT22PIO::DATA_READY:     return "DATA_READY";
        case DHT22PIO::ERROR_TIMEOUT:  return "ERROR_TIMEOUT";
        case DHT22PIO::ERROR_CHECKSUM: return "ERROR_CHECKSUM";
        default:                       return "UNKNOWN";
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    Serial.println("=== DHT22PIO_RP2040 — Diagnostic Mode ===\n");

    if (!bus.begin(SENSOR_PIN)) {
        Serial.println("FATAL: PIO init failed.");
        while (true) { delay(1000); }
    }
}

void loop() {
    Serial.println("--- New Reading Cycle ---");
    Serial.print("State: ");
    Serial.println(stateToString(sensor.getState()));

    // Start reading
    sensor.requestReading(SENSOR_PIN);
    Serial.print("State: ");
    Serial.println(stateToString(sensor.getState()));

    unsigned long t0 = millis();

    // Poll until terminal state
    DHT22PIO::State prevState = sensor.getState();
    while (sensor.getState() == DHT22PIO::WAITING_PIO) {
        sensor.update();
    }

    unsigned long elapsed = millis() - t0;
    DHT22PIO::State finalState = sensor.getState();

    Serial.print("State: ");
    Serial.print(stateToString(finalState));
    Serial.print("  (took ");
    Serial.print(elapsed);
    Serial.println(" ms)");

    // Show results or error
    if (finalState == DHT22PIO::DATA_READY) {
        float temp, hum;
        sensor.getResults(temp, hum);

        Serial.print("Temperature: ");
        Serial.print(temp, 1);
        Serial.println(" °C");

        Serial.print("Humidity:    ");
        Serial.print(hum, 1);
        Serial.println(" %RH");
    } else {
        char err[64];
        sensor.getLastErrorString(err, sizeof(err));
        Serial.println(err);
        sensor.reset();
    }

    Serial.println();
    delay(5000);
}
