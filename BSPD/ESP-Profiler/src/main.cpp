#include <Arduino.h>
#include <driver/ledc.h>

#define BSPD_OK_PIN 4
#define BSPD_POWER_PIN 2
#define BSPD_INPUT_SIGNAL_PIN 5
#define DUTY_CYCLE_RESOLUTION 9
#define DUTY_CYCLE_MAX 511


volatile bool BSPD_FAULT = false;

void BSPD_OK_FAULT() {
    BSPD_FAULT = true;
}

void resetBSPD() {
    // Reset the BSPD
    digitalWrite(BSPD_INPUT_SIGNAL_PIN, LOW); // Hold the signal low so there's no fault
    digitalWrite(BSPD_POWER_PIN, LOW);
    delay(500); // Allow time for shutdown
    digitalWrite(BSPD_POWER_PIN, HIGH);
    delay(500); // Allow time for startup
    BSPD_FAULT = digitalRead(BSPD_OK_PIN) == HIGH;
    if (BSPD_FAULT)
        Serial.print("BSPD Startup Fault!\n");
}



void setup() {
    pinMode(BSPD_OK_PIN, INPUT_PULLUP);
    pinMode(BSPD_POWER_PIN, OUTPUT);
    pinMode(BSPD_INPUT_SIGNAL_PIN, OUTPUT);
    
    digitalWrite(BSPD_INPUT_SIGNAL_PIN, LOW); // Hold the signal low so there's no fault
    digitalWrite(BSPD_POWER_PIN, LOW);
    delay(500); // Allow time for shutdown

    attachInterrupt(digitalPinToInterrupt(BSPD_OK_PIN), BSPD_OK_FAULT, RISING);

    Serial.begin(115200);
    Serial.printf("\nBSPD Threshold Threshold Testing\n");
}

bool testDutyCycleLEDc(float freq_Hz, uint32_t duty_cycle) {
    pinMode(BSPD_INPUT_SIGNAL_PIN, OUTPUT);
    digitalWrite(BSPD_INPUT_SIGNAL_PIN, LOW); // Hold the signal low so there's no fault
    digitalWrite(BSPD_POWER_PIN, LOW);
    delay(500); // Allow time for shutdown
    digitalWrite(BSPD_POWER_PIN, HIGH);
    delay(500); // Allow time for startup
    BSPD_FAULT = false;

    if (!ledcAttach(BSPD_INPUT_SIGNAL_PIN, freq_Hz, DUTY_CYCLE_RESOLUTION)) {
        Serial.printf("Failed to attach LEDC to pin %d\n", BSPD_INPUT_SIGNAL_PIN);
        return false;
    }

    if (!ledcWrite(BSPD_INPUT_SIGNAL_PIN, duty_cycle)) {
        Serial.printf("Failed to write LEDC to pin %d\n", BSPD_INPUT_SIGNAL_PIN);
        return false;
    }
    
    for (int i = 0; i < 50; i++) {
        delay(100);
        if (BSPD_FAULT) {
            ledcDetach(BSPD_INPUT_SIGNAL_PIN);
            return false;
        }
    }

    ledcDetach(BSPD_INPUT_SIGNAL_PIN);
    return true;
}

bool testDutyCycle(float freq_Hz, uint32_t duty_cycle) {
    const double runtime_s = 5.0;

    if (freq_Hz >= 10) {
        return testDutyCycleLEDc(freq_Hz, duty_cycle);
    }

    resetBSPD(); // This leaves the input pin in a safe condition. If the BSPD faults, it will be caught by the interrupt

    // Calculate our low & high times
    float period_s = 1 / freq_Hz;
    float high_time_s = period_s * duty_cycle / DUTY_CYCLE_MAX;
    float low_time_s = period_s - high_time_s;
    uint32_t high_time_us = high_time_s * 1e6;
    uint32_t low_time_us = low_time_s * 1e6;

    uint32_t repeat_count = ceil(runtime_s / period_s); // At least 5s of continuous signalling

    for (int i = 0; i < repeat_count; i++) {
        digitalWrite(BSPD_INPUT_SIGNAL_PIN, HIGH);
        delayMicroseconds(high_time_us);
        if (BSPD_FAULT) {
            return false;
        }
        digitalWrite(BSPD_INPUT_SIGNAL_PIN, LOW);
        delayMicroseconds(low_time_us);
        if (BSPD_FAULT) {
            return false;
        }
    }

    return !BSPD_FAULT;
}

uint32_t findDutyCycleThreshold(float frequency) {
    // Find the duty cycle threshold for a given frequency
    // This is the duty cycle at which the BSPD will fault (at a given frequency)

    // Duty cycle is % of time the signal is causing a fault condition
    uint32_t duty_cycle = 0;
    int i = DUTY_CYCLE_RESOLUTION - 1;
    do {
        duty_cycle |= (1 << i);
        bool passed = testDutyCycle(frequency, duty_cycle);
        if (!passed) {
            duty_cycle &= ~(1 << i); // This duty cycle was too aggressive, try a less aggressive duty cycle
        }
        i--;
    } while (i >= 0);

    return duty_cycle;
}


void loop() {  
    // Sweep through frequencies from 0.1 to 9000 Hz
    for (int multiplier = -1; multiplier <= 3; multiplier++) {
        for (int num = 1; num < 10; num++) {
            float freq = num * pow(10, multiplier);
            uint32_t duty_cycle_threshold = findDutyCycleThreshold(freq);

            uint32_t freq_num = (uint32_t) freq;
            uint32_t freq_dec = (uint32_t) (freq * 100) % 100;
            
            uint32_t duty_cycle_num = (uint32_t) (100. * duty_cycle_threshold / DUTY_CYCLE_MAX);
            uint32_t duty_cycle_dec = (uint32_t) (10000. * duty_cycle_threshold / DUTY_CYCLE_MAX) % 100;
            Serial.printf("Frequency: %d.%02d, Duty Cycle: %d.%02d%% (%d)\n", freq_num, freq_dec, duty_cycle_num, duty_cycle_dec, duty_cycle_threshold);
        }
    }
    

    digitalWrite(BSPD_INPUT_SIGNAL_PIN, LOW); // Hold the signal low so there's no fault
    digitalWrite(BSPD_POWER_PIN, LOW);
    delay(500); // Allow time for shutdown

    while (1) {}
}

