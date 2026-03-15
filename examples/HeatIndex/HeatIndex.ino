/**
 * @file HeatIndex.ino
 * @brief Compute the Heat Index (feels-like temperature) from DHT22 data.
 *
 * Demonstrates a practical application: combining temperature and humidity
 * to calculate the perceived temperature using the Rothfusz regression
 * equation (same formula used by NOAA / US National Weather Service).
 *
 * Wiring: GP15 + 4.7kΩ pull-up to 3.3V.
 */

#include <DHTBus.h>
#include <DHT22PIO.h>

static const uint SENSOR_PIN = 15;

DHTBus bus(pio0);
DHT22PIO sensor(bus);

/**
 * @brief Compute the Heat Index using the Rothfusz regression.
 *
 * @param tempC Temperature in Celsius.
 * @param rh    Relative humidity in %RH.
 * @return Heat Index in Celsius.
 */
float computeHeatIndex(float tempC, float rh) {
    // Convert to Fahrenheit for the NOAA formula
    float T = (tempC * 1.8f) + 32.0f;

    // Simple formula for low heat index values
    float hi = 0.5f * (T + 61.0f + ((T - 68.0f) * 1.2f) + (rh * 0.094f));

    if (hi < 80.0f) {
        // Convert back to Celsius
        return (hi - 32.0f) / 1.8f;
    }

    // Full Rothfusz regression
    hi = -42.379f
       + 2.04901523f  * T
       + 10.14333127f  * rh
       - 0.22475541f   * T * rh
       - 0.00683783f   * T * T
       - 0.05481717f   * rh * rh
       + 0.00122874f   * T * T * rh
       + 0.00085282f   * T * rh * rh
       - 0.00000199f   * T * T * rh * rh;

    // Adjustments for extreme conditions
    if (rh < 13.0f && T >= 80.0f && T <= 112.0f) {
        hi -= ((13.0f - rh) / 4.0f) * sqrtf((17.0f - fabsf(T - 95.0f)) / 17.0f);
    }
    else if (rh > 85.0f && T >= 80.0f && T <= 87.0f) {
        hi += ((rh - 85.0f) / 10.0f) * ((87.0f - T) / 5.0f);
    }

    // Convert back to Celsius
    return (hi - 32.0f) / 1.8f;
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    Serial.println("=== DHT22PIO_RP2040 — Heat Index ===\n");

    if (!bus.begin(SENSOR_PIN)) {
        Serial.println("FATAL: PIO init failed.");
        while (true) { delay(1000); }
    }
}

void loop() {
    sensor.requestReading(SENSOR_PIN);

    while (sensor.getState() == DHT22PIO::WAITING_PIO) {
        sensor.update();
    }

    float temp, hum;
    if (sensor.getResults(temp, hum)) {
        float heatIdx = computeHeatIndex(temp, hum);

        Serial.print("Temperature: ");
        Serial.print(temp, 1);
        Serial.print(" °C  |  Humidity: ");
        Serial.print(hum, 1);
        Serial.print(" %RH  |  Feels like: ");
        Serial.print(heatIdx, 1);
        Serial.println(" °C");
    } else {
        char err[64];
        sensor.getLastErrorString(err, sizeof(err));
        Serial.println(err);
        sensor.reset();
    }

    delay(5000);
}
