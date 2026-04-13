#pragma once
#include "types.h"
#include "constants.h"

class LEDController {
public:
    LEDController();
    void init(int pin);
    void start(LEDMode mode);
    void stop();
    LEDMode currentMode() const { return state.mode; }
    
private:
    LEDState state;
    int ledPin;
    
    static void ledTask(void* parameter);
    void stopTask();
    
    // Mode-specific parameters
    struct ModeParams {
        int fadeStep;
        int delayMs;
        int maxBrightness;
        int minBrightness;
        int pauseMs;
    };
    
    ModeParams getParams(LEDMode mode);
};

// Implementation
LEDController::LEDController() {
    state.taskHandle = NULL;
    state.mode = LED_OFF;
    state.stopping = false;
    state.brightness = 0;
    state.direction = 1;
}

void LEDController::init(int pin) {
    ledPin = pin;
    ledcSetup(LED_CHANNEL, LED_FREQ, LED_RESOLUTION);
    ledcAttachPin(ledPin, LED_CHANNEL);
}

LEDController::ModeParams LEDController::getParams(LEDMode mode) {
    if (mode == LED_DISPLAY_UPDATE) {
        return {5, 20, 120, 1, 0};
    } else if (mode == LED_DAILY_REMINDER) {
        return {2, 40, 100, 0, 2500};
    } else {  // LED_RECALL — heartbeat
        return {5, 15, 120, 0, 900};
    }
}

void LEDController::ledTask(void* parameter) {
    LEDController* controller = static_cast<LEDController*>(parameter);
    LEDMode currentMode = controller->state.mode;
    ModeParams params = controller->getParams(currentMode);
        
    while (controller->state.mode == currentMode && !controller->state.stopping) {
        // Fade up from 0 to max
        for (int brightness = 0; brightness <= params.maxBrightness; brightness += params.fadeStep) {
            if (brightness > params.maxBrightness) brightness = params.maxBrightness;
            controller->state.brightness = brightness;
            ledcWrite(LED_CHANNEL, controller->state.brightness);
            delay(params.delayMs);
            if (controller->state.stopping) break;
        }
        
        if (controller->state.stopping) break;

        // Hold at peak
        if (currentMode == LED_RECALL) {
            delay(300);
        } else if (params.pauseMs > 0) {
            delay(params.pauseMs);
        }
        if (controller->state.stopping) break;

        // Fade down from max to 0
        for (int brightness = params.maxBrightness; brightness >= 0; brightness -= params.fadeStep) {
            if (brightness < 0) brightness = 0;
            controller->state.brightness = brightness;
            ledcWrite(LED_CHANNEL, controller->state.brightness);
            delay(params.delayMs);
            if (controller->state.stopping) break;
        }

        if (controller->state.stopping) break;

        // Ensure LED is fully off
        controller->state.brightness = 0;
        ledcWrite(LED_CHANNEL, 0);

        // Heartbeat: second shorter beat after gap
        if (currentMode == LED_RECALL) {
            delay(180);
            if (controller->state.stopping) break;
            int secondMax = params.maxBrightness / 2;
            int fastStep = round(params.fadeStep * 1.5);
            for (int b = 0; b <= secondMax; b += fastStep) {
                ledcWrite(LED_CHANNEL, b);
                delay(params.delayMs);
                if (controller->state.stopping) break;
            }
            if (controller->state.stopping) break;
            for (int b = secondMax; b >= 0; b -= fastStep) {
                ledcWrite(LED_CHANNEL, b);
                delay(params.delayMs);
                if (controller->state.stopping) break;
            }
            if (controller->state.stopping) break;
            ledcWrite(LED_CHANNEL, 0);
        }

        if (params.pauseMs > 0) {
            delay(params.pauseMs);
            if (controller->state.stopping) break;
        }
    }
    
    // Fade out smoothly before stopping
    while (controller->state.brightness > 0) {
        controller->state.brightness -= params.fadeStep;
        if (controller->state.brightness < 0) controller->state.brightness = 0;
        ledcWrite(LED_CHANNEL, controller->state.brightness);
        delay(params.delayMs);
    }
    
    // Clean shutdown
    ledcWrite(LED_CHANNEL, 0);
    controller->state.taskHandle = NULL;
    controller->state.stopping = false;
    vTaskDelete(NULL);
}

void LEDController::start(LEDMode mode) {
    if (state.mode == mode && state.taskHandle != NULL) {
        return;
    }
    
    stopTask();
    
    state.mode = mode;
    state.stopping = false;
    
    xTaskCreate(
        ledTask,
        mode == LED_DISPLAY_UPDATE ? "LED_Screen" : mode == LED_DAILY_REMINDER? "LED_Reminder" : "LED_Recall",
        2048,
        this,
        1,
        &state.taskHandle
    );
}

void LEDController::stopTask() {
    if (state.taskHandle == NULL) {
        return;
    }
    
    state.stopping = true;
    
    int timeout = 5000;
    while (state.taskHandle != NULL && timeout > 0) {
        delay(1);
        timeout--;
    }
}

void LEDController::stop() {
    stopTask();
    ledcWrite(LED_CHANNEL, 0);
    state.mode = LED_OFF;
}
