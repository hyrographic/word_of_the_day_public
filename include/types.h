#pragma once
#include <Arduino.h>
#include <vector>
#include <map>

enum ButtonAction { NO_ACTION, CLICKED, HELD };

// Display state machine
enum DisplayState {
    DISPLAY_UNINITIALIZED,
    SHOWING_WORD,
    SHOWING_RECALL,
    SHOWING_ANSWER,
    SHOWING_PROFILE
};

// LED modes
enum LEDMode {
    LED_OFF,
    LED_DISPLAY_UPDATE,
    LED_DAILY_REMINDER,
    LED_RECALL
};

// Word data structure
struct WordData {
    String word;
    String phonetic;
    String partOfSpeech;
    String definition;
    long date;
    String example;
    bool valid = false;
};

// RSS cache structure
struct RSSWordCache {
    std::vector<WordData> words;
    std::vector<String> wordList;
    unsigned long lastFetch;
    bool valid = false;
};

// LED state
struct LEDState {
    TaskHandle_t taskHandle;
    volatile LEDMode mode;
    volatile bool stopping;
    int brightness;
    int direction;
};

// Day data for stats grid
struct DayData {
    bool hasData;
    double incorrectRatio;
    bool isFuture;
};

// Grid metadata for stats display
struct GridMetadata {
    String monthLabels[12];
    int monthStartColumns[12];
    bool partialMonth[12];
    int monthCount;
};

// Word statistics for weighted selection
struct WordStats {
    String word;
    int totalAttempts;
    int correctAttempts;
    time_t lastSeen;
    double weight;
};
