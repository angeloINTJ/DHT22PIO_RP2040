/**
 * @file NonBlockingRead.ino
 * @brief Demonstrates the true non-blocking async pattern.
 *
 * Unlike BasicReading (which uses a while-loop to wait), this example
 * shows how to integrate DHT22PIO into a cooperative multitasking loop
 * where the CPU is free to do other work while the PIO captures data.
 *
 * Wiring: GP15 + 4.7kΩ pull-up to 3.3V.
 */

#include <DHTBus.h>
#include <DHT22PIO.h>

static const uint SENSOR_PIN    = 15;
static const unsigned long READ_INTERVAL_MS = 5000;

DHTBus bus(pio0);
DHT22PIO sensor(bus);

unsigned long lastReadTime  = 0;
bool readingInProgress      = false;
uint32_t loopCounter        = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    Serial.println("=== DHT22PIO_RP2040 — Non-Blocking Read ===");
    Serial.println("The CPU stays free between readings.\n");

    if (!bus.begin(SENSOR_PIN)) {
        Serial.println("FATAL: PIO init failed.");
        while (true) { delay(1000); }
    }
}

void loop() {
    unsigned long now = millis();

    // --- Task 1: Kick off a new reading every READ_INTERVAL_MS ---
    if (!readingInProgress && (now - lastReadTime >= READ_INTERVAL_MS)) {
        sensor.requestReading(SENSOR_PIN);
        readingInProgress = true;
        lastReadTime = now;
        Serial.println("[DHT] Reading requested...");
    }

    // --- Task 2: Poll the sensor (non-blocking, returns immediately) ---
    if (readingInProgress) {
        sensor.update();

        DHT22PIO::State state = sensor.getState();

        if (state == DHT22PIO::DATA_READY) {
            float temp, hum;
            sensor.getResults(temp, hum);
            Serial.print("[DHT] Temperature: ");
            Serial.print(temp, 1);
            Serial.print(" °C  |  Humidity: ");
            Serial.print(hum, 1);
            Serial.print(" %RH  |  Loops while waiting: ");
            Serial.println(loopCounter);
            readingInProgress = false;
            loopCounter = 0;
        }
        else if (state == DHT22PIO::ERROR_TIMEOUT ||
                 state == DHT22PIO::ERROR_CHECKSUM) {
            char err[64];
            sensor.getLastErrorString(err, sizeof(err));
            Serial.print("[DHT] ");
            Serial.println(err);
            sensor.reset();
            readingInProgress = false;
            loopCounter = 0;
        }
    }

    // --- Task 3: Other work the CPU can do while PIO captures data ---
    loopCounter++;
    // Your own application logic goes here: update displays, handle
    // web requests, read other sensors, blink LEDs, etc.
}
