#pragma once
#include <Arduino.h>
#include <esp_sleep.h>
#include <Preferences.h>
#include <time.h>
#include "types.h"

// WordData struct for RTC memory (uses char arrays instead of Strings)
struct WordDataRTC {
    char word[50];
    char phonetic[100];
    char partOfSpeech[20];
    char definition[500];
    char example[500];
    time_t date;
    bool valid;
};

// RTC memory structure - survives deep sleep
RTC_DATA_ATTR unsigned long rtc_lastRecallDay = 0;
RTC_DATA_ATTR bool rtc_recallDoneToday = false;
RTC_DATA_ATTR int rtc_currentStreak = 0;
RTC_DATA_ATTR unsigned long rtc_lastWordFetchDay = 0;
RTC_DATA_ATTR int rtc_bootCount = 0;
RTC_DATA_ATTR WordDataRTC rtc_currentWord = {0};
RTC_DATA_ATTR uint64_t rtc_sleepDuration = 0;
RTC_DATA_ATTR bool rtc_wotdSeenLoggedToday = false;

class SleepManager {
public:
    enum WakeReason {
        WAKE_FIRST_BOOT,
        WAKE_TIMER,
        WAKE_BUTTON,
        WAKE_LED_REMINDER,
        WAKE_UNKNOWN
    };
    
    static void init(int buttonPin);
    static void enterDeepSleepUntil(int hour, int minute);
    static void enterDeepSleepFor(uint64_t seconds);
    static WakeReason getWakeReason();
    static bool shouldUpdateWord();
    static uint64_t calculateSecondsUntil(int targetHour, int targetMinute);
    
    static void saveStateToRTC(unsigned long lastRecallDay, bool recallDone, int streak, unsigned long lastFetchDay);
    static void loadStateFromRTC(unsigned long& lastRecallDay, bool& recallDone, int& streak, unsigned long& lastFetchDay);

    static void saveWordToRTC(const WordData& word);
    static WordData loadWordFromRTC();
    static unsigned long getCurrentDay();

private:
};

void SleepManager::init(int buttonPin) {
    rtc_bootCount++;

    // On first boot, restore state from Preferences (survives reflash)
    if (rtc_bootCount == 1) {
        Preferences prefs;
        prefs.begin("recall", true);  // read-only
        rtc_lastRecallDay = prefs.getULong("lastRecallDay", 0);
        rtc_recallDoneToday = prefs.getBool("recallDone", false);
        rtc_currentStreak = prefs.getInt("streak", 0);
        rtc_lastWordFetchDay = prefs.getULong("lastFetchDay", 0);
        prefs.end();
        Serial.printf("Restored from Preferences: streak=%d, lastRecallDay=%lu\n",
                      rtc_currentStreak, rtc_lastRecallDay);
    }

    // Configure button as wake source (wake on LOW - button pressed)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)buttonPin, LOW);

    Serial.printf("Sleep manager initialized (boot #%d)\n", rtc_bootCount);
}

void SleepManager::enterDeepSleepUntil(int hour, int minute) {
    // rtc_lastKnownTime = time(nullptr);  // Save current time
    
    uint64_t sleepSeconds = calculateSecondsUntil(hour, minute);
    rtc_sleepDuration = sleepSeconds;
    
    Serial.printf("Sleeping until %02d:%02d (in %llu seconds / %.1f hours)\n", 
                  hour, minute, sleepSeconds, sleepSeconds / 3600.0);
    
    esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);
    
    Serial.flush();
    delay(100);
    esp_deep_sleep_start();
}

void SleepManager::enterDeepSleepFor(uint64_t seconds) {
    // rtc_lastKnownTime = time(nullptr);  // Save current time
    rtc_sleepDuration = seconds;
    
    Serial.printf("Sleeping for %llu seconds (%.1f minutes)\n", 
                  seconds, seconds / 60.0);
    
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
    
    Serial.flush();
    delay(100);
    esp_deep_sleep_start();
}

SleepManager::WakeReason SleepManager::getWakeReason() {
    // First boot
    if (rtc_bootCount == 1) {
        Serial.println("Wake reason: FIRST_BOOT");
        return WAKE_FIRST_BOOT;
    }

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("Wake reason: BUTTON");
            return WAKE_BUTTON;

        case ESP_SLEEP_WAKEUP_TIMER:
            // Use stored sleep duration to distinguish wake type.
            // LED reminder intervals are short (< 1 hour), daily updates are longer.
            if (rtc_sleepDuration >= 1500) {
                Serial.printf("Wake reason: TIMER (daily update, slept %llu sec)\n", rtc_sleepDuration);
                return WAKE_TIMER;
            }
            Serial.printf("Wake reason: LED_REMINDER (slept %llu sec)\n", rtc_sleepDuration);
            return WAKE_LED_REMINDER;

        default:
            Serial.printf("Wake reason: UNKNOWN (%d)\n", wakeup_reason);
            return WAKE_UNKNOWN;
    }
}

bool SleepManager::shouldUpdateWord() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Can't get time, assuming update needed");
        return true;
    }
    
    unsigned long currentDay = getCurrentDay();
    
    // If we haven't fetched today and it's past 5:15 AM
    if (rtc_lastWordFetchDay < currentDay) {
        if (timeinfo.tm_hour > 5 || (timeinfo.tm_hour == 5 && timeinfo.tm_min >= 15)) {
            Serial.println("Missed update detected - catch up needed");
            return true;
        }
    }
    
    return false;
}

uint64_t SleepManager::calculateSecondsUntil(int targetHour, int targetMinute) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to get time, defaulting to 24 hour sleep");
        return 86400;
    }
    
    // Current time in seconds since midnight
    int currentSeconds = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
    
    // Target time in seconds since midnight  
    int targetSeconds = targetHour * 3600 + targetMinute * 60;
    
    // Calculate seconds until target
    int secondsUntilTarget;
    
    if (targetSeconds > currentSeconds) {
        // Target is later today
        secondsUntilTarget = targetSeconds - currentSeconds;
    } else {
        // Target is tomorrow
        secondsUntilTarget = (86400 - currentSeconds) + targetSeconds;
    }
    
    return secondsUntilTarget;
}

unsigned long SleepManager::getCurrentDay() {
    time_t now;
    time(&now);
    if (now < 0) {
        return 0;
    }
    return now / 86400;  // Days since Unix epoch (Jan 1, 1970)
}

void SleepManager::saveStateToRTC(unsigned long lastRecallDay, bool recallDone, int streak, unsigned long lastFetchDay) {
    rtc_lastRecallDay = lastRecallDay;
    rtc_recallDoneToday = recallDone;
    rtc_currentStreak = streak;
    rtc_lastWordFetchDay = lastFetchDay;
}

void SleepManager::loadStateFromRTC(unsigned long& lastRecallDay, bool& recallDone, int& streak, unsigned long& lastFetchDay) {
    lastRecallDay = rtc_lastRecallDay;
    recallDone = rtc_recallDoneToday;
    streak = rtc_currentStreak;
    lastFetchDay = rtc_lastWordFetchDay;
}

void SleepManager::saveWordToRTC(const WordData& word) {
    strncpy(rtc_currentWord.word, word.word.c_str(), sizeof(rtc_currentWord.word) - 1);
    strncpy(rtc_currentWord.phonetic, word.phonetic.c_str(), sizeof(rtc_currentWord.phonetic) - 1);
    strncpy(rtc_currentWord.partOfSpeech, word.partOfSpeech.c_str(), sizeof(rtc_currentWord.partOfSpeech) - 1);
    strncpy(rtc_currentWord.definition, word.definition.c_str(), sizeof(rtc_currentWord.definition) - 1);
    strncpy(rtc_currentWord.example, word.example.c_str(), sizeof(rtc_currentWord.example) - 1);
    rtc_currentWord.date = word.date;
    rtc_currentWord.valid = word.valid;

    rtc_currentWord.word[sizeof(rtc_currentWord.word) - 1] = '\0';
    rtc_currentWord.phonetic[sizeof(rtc_currentWord.phonetic) - 1] = '\0';
    rtc_currentWord.partOfSpeech[sizeof(rtc_currentWord.partOfSpeech) - 1] = '\0';
    rtc_currentWord.definition[sizeof(rtc_currentWord.definition) - 1] = '\0';
    rtc_currentWord.example[sizeof(rtc_currentWord.example) - 1] = '\0';
}

WordData SleepManager::loadWordFromRTC() {
    WordData word;
    word.word = String(rtc_currentWord.word);
    word.phonetic = String(rtc_currentWord.phonetic);
    word.partOfSpeech = String(rtc_currentWord.partOfSpeech);
    word.definition = String(rtc_currentWord.definition);
    word.example = String(rtc_currentWord.example);
    word.date = rtc_currentWord.date;
    word.valid = rtc_currentWord.valid;
    return word;
}