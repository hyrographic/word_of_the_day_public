#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <time.h>
#include <DNSServer.h>

#include "config.h"
#include "epd2in9b_V4.h"
#include "epdpaint.h"
#include "epdif.h"

#include "types.h"
#include "constants.h"
#include "led_controller.h"
#include "text_utils.h"
#include "rss_parser.h"
#include "word_manager.h"
#include "stats_manager.h"
#include "display_manager.h"
#include "sleep_manager.h"
#include "wifi_manager.h"
#include "settings_page.h"
#include "settings_assets.h"

// Global instances
Epd epd;
LEDController led;
WordManager wordManager;
DisplayManager displayManager(epd);
WebServer server(80);
Preferences preferences;
DNSServer dnsServer;

// Application state
WordData currentWord;
WordData currentRecallWord;
int currentRecallDaysAgo;
std::vector<WordStats> allHistoryWords;

String wifi_ssid = "";
String wifi_password = "";
bool enterSettings = false;

// Helper function to wait for button click or hold with timeout
ButtonAction waitForClickOrHold(unsigned long timeoutMs) {
    unsigned long startTime = millis();
    bool lastButton = digitalRead(RECALL_SWITCH_PIN);
    unsigned long lastDebounce = millis();
    bool stableState = lastButton;

    while (millis() - startTime < timeoutMs) {
        bool reading = digitalRead(RECALL_SWITCH_PIN);

        if (reading != lastButton) {
            lastDebounce = millis();
            lastButton = reading;
        }

        if (millis() - lastDebounce > DEBOUNCE_DELAY_MS) {
            if (reading == LOW && stableState == HIGH) {
                stableState = LOW;

                unsigned long pressStart = millis();
                bool holdDetected = false;

                while (digitalRead(RECALL_SWITCH_PIN) == LOW) {
                    if (millis() - pressStart >= HOLD_THRESHOLD_MS && !holdDetected) {
                        holdDetected = true;
                        while (digitalRead(RECALL_SWITCH_PIN) == LOW) {
                            led.start(LED_DISPLAY_UPDATE);
                            delay(10);
                        }
                        return HELD;
                    }
                    delay(10);
                }

                if (!holdDetected) {
                    led.start(LED_DISPLAY_UPDATE);
                    return CLICKED;
                }
            }
            else if (reading == HIGH && stableState == LOW) {
                stableState = HIGH;
            }
        }

        delay(10);
    }

    return NO_ACTION;
}

void updateStreak() {
    unsigned long currentDay = SleepManager::getCurrentDay();
    if (currentDay == 0) {
        Serial.println("Time invalid - skipping streak update");
        if (rtc_currentStreak < 1) rtc_currentStreak = 1;
        return;
    }

    unsigned long yesterday = currentDay - 1;

    if (rtc_lastRecallDay == yesterday) {
        rtc_currentStreak++;
    } else if (rtc_lastRecallDay != currentDay) {
        rtc_currentStreak = 1;
    }

    if (rtc_currentStreak < 1) {
        rtc_currentStreak = 1;
    }

    rtc_lastRecallDay = currentDay;
    Serial.printf("Streak: %d days\n", rtc_currentStreak);
}

void markRecallDone() {
    updateStreak();
    rtc_recallDoneToday = true;

    preferences.begin("recall", false);
    preferences.putULong("lastRecallDay", rtc_lastRecallDay);
    preferences.putBool("recallDone", true);
    preferences.putInt("streak", rtc_currentStreak);
    preferences.end();
}

void fetchWord() {
    currentWord = wordManager.fetchWordOfDay();

    if (currentWord.valid) {
        SleepManager::saveWordToRTC(currentWord);
        displayManager.displayWordOfDay(currentWord, rtc_currentStreak, rtc_recallDoneToday);
        rtc_lastWordFetchDay = SleepManager::getCurrentDay();
        Serial.println("Word displayed and saved to RTC");
    } else {
        Serial.println("Fetch failed");
    }
}

void showRecall() {
    if (!wordManager.getCache().valid) {
        wordManager.fetchAndCacheWords();
    }

    if (allHistoryWords.size() == 0) {
        StatsManager::loadWordStats(allHistoryWords);
    }

    String selectedWord = wordManager.selectRecallWord(allHistoryWords);
    if (selectedWord.length() == 0) {
        Serial.println("No recall word");
        return;
    }

    WordData* cached = wordManager.findWordInCache(selectedWord);
    currentRecallWord = cached ? *cached : wordManager.fetchWordFromHistory(selectedWord);

    if (!currentRecallWord.valid) {
        Serial.println("Recall word fetch failed");
        return;
    }

    time_t now = time(nullptr);
    currentRecallDaysAgo = (now - currentRecallWord.date) / 86400;

    displayManager.displayRecall(currentRecallWord, currentRecallDaysAgo, false);
    // led.stop();
    led.start(LED_RECALL);
}

bool waitForRecallAnswer() {
    Serial.println("Waiting for answer...");

    ButtonAction action = waitForClickOrHold(LONG_TIMEOUT);

    if (action == HELD) {
        Serial.println("FORGOT IT");
        led.start(LED_DISPLAY_UPDATE);
        wordManager.saveRecallResult(currentRecallWord, currentRecallDaysAgo, false);
        markRecallDone();
        displayManager.displayRecall(currentRecallWord, currentRecallDaysAgo, true);
        led.stop();
        return true;
    } else if (action == CLICKED) {
        Serial.println("GOT IT");
        led.start(LED_DISPLAY_UPDATE);
        wordManager.saveRecallResult(currentRecallWord, currentRecallDaysAgo, true);
        markRecallDone();
        displayManager.displayRecall(currentRecallWord, currentRecallDaysAgo, true);
        led.stop();
        return true;
    }

    return false;
}

void showStats() {
    const int MAX_WEEKS = 24;
    const int DAYS_PER_WEEK = 7;
    DayData grid[MAX_WEEKS * DAYS_PER_WEEK];
    GridMetadata metadata;
    int totalCorrect = 0;
    int totalIncorrect = 0;

    StatsManager::loadDailyGrid(grid, metadata, MAX_WEEKS, DAYS_PER_WEEK, totalCorrect, totalIncorrect);
    displayManager.displayStats(totalCorrect, totalIncorrect, rtc_currentStreak, grid, metadata, MAX_WEEKS, DAYS_PER_WEEK);
}

void handleFirstBoot() {
    led.start(LED_DISPLAY_UPDATE);
    Serial.println("\n=== FIRST BOOT ===\n");

    WiFiManager::loadCredentials(wifi_ssid, wifi_password);
    int wifi_tries = WiFiManager::getFailCount();

    // First boot - no WiFi credentials or too many failures, create AP
    if (wifi_ssid.length() == 0 || wifi_tries > 3) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID);

        IPAddress IP = WiFi.softAPIP();
        String ipStr = IP.toString();
        Serial.printf("First boot - AP IP: %s\n", ipStr.c_str());

        epd.Init_Fast();
        displayManager.displaySetupMessage(
            "Connect to ",
            String(AP_SSID),
            "Then visit " + ipStr + " in your browser to configure WiFi settings."
        );

        SettingsServer::setup(server);
        Serial.printf("WiFi setup server started - connect to '%s' and visit http://%s\n", AP_SSID, ipStr.c_str());
        SettingsServer::loop(server, RECALL_SWITCH_PIN, led);
    }

    // Settings mode - has WiFi credentials, use local network
    if (enterSettings) {
        if (!WiFiManager::connect(wifi_ssid, wifi_password)) {
            Serial.println("Settings mode: WiFi connection failed");
            ESP.restart();
            return;
        }

        String ipStr = WiFi.localIP().toString();
        Serial.printf("Settings mode: %s\n", ipStr.c_str());

        epd.Init_Fast();
        displayManager.displaySetupMessage(
            "Visit ",
            ipStr,
            "Whilst connected to " + wifi_ssid + " to update WiFi network or manage recall history."
        );

        SettingsServer::setup(server);
        Serial.printf("Settings server started at http://%s\n", ipStr.c_str());
        SettingsServer::loop(server, RECALL_SWITCH_PIN, led);
    }

    // Normal boot - connect to WiFi and continue
    if (WiFiManager::connect(wifi_ssid, wifi_password)) {
        bool timeValid = WiFiManager::syncTime();
        StatsManager::loadWordStats(allHistoryWords);

        if (timeValid) {
            unsigned long currentDay = SleepManager::getCurrentDay();
            if (currentDay != rtc_lastRecallDay) {
                rtc_recallDoneToday = false;
                rtc_wotdSeenLoggedToday = false;
            }
            if ((currentDay - rtc_lastRecallDay) > 1) {
                rtc_currentStreak = 0;
            }
        }

        epd.Init_Fast();
        fetchWord();

    }
    led.stop();

    if (rtc_recallDoneToday) {
        SleepManager::enterDeepSleepUntil(DAILY_UPDATE_HOUR, DAILY_UPDATE_MINUTE);
    } else {
        SleepManager::enterDeepSleepFor(LED_REMINDER_INTERVAL);
    }
}

void handleDailyUpdate() {
    led.start(LED_DISPLAY_UPDATE);
    Serial.println("\n=== DAILY UPDATE ===\n");

    if (WiFiManager::connect(wifi_ssid, wifi_password)) {
        bool timeValid = WiFiManager::syncTime();
        wordManager.init();
        StatsManager::loadWordStats(allHistoryWords);

        if (timeValid) {
            unsigned long currentDay = SleepManager::getCurrentDay();
            if (currentDay != rtc_lastRecallDay) {
                rtc_recallDoneToday = false;
                rtc_wotdSeenLoggedToday = false;
            }
            if ((currentDay - rtc_lastRecallDay) > 1) {
                rtc_currentStreak = 0;
            }
        }

        epd.Init_Fast();
        fetchWord();

        preferences.begin("recall", false);
        preferences.putULong("lastRecallDay", rtc_lastRecallDay);
        preferences.putBool("recallDone", false);
        preferences.putInt("streak", rtc_currentStreak);
        preferences.putULong("lastFetchDay", rtc_lastWordFetchDay);
        preferences.end();
    }
    led.stop();
    SleepManager::enterDeepSleepFor(LED_REMINDER_INTERVAL);
}

void handleLEDReminder() {
    Serial.println("\n=== LED REMINDER ===\n");

    if (!rtc_recallDoneToday) {
        led.start(LED_DAILY_REMINDER);
        delay(15000);
        led.stop();
    }

    SleepManager::enterDeepSleepFor(LED_REMINDER_INTERVAL);
}

void handleButtonPress(ButtonAction wakeButtonPress) {
    led.start(LED_DISPLAY_UPDATE);
    Serial.println("\n=== BUTTON WAKE ===\n");

    bool needsUpdate = false;
    if (WiFi.status() != WL_CONNECTED) {
        if (WiFiManager::connect(wifi_ssid, wifi_password)) {
            bool timeValid = WiFiManager::syncTime();
            if (timeValid) {
                needsUpdate = SleepManager::shouldUpdateWord();
            }
        }
    }

    // Load current word - fetch new one if update needed
    if (needsUpdate) {
        Serial.println("Fetching new word of the day");
        wordManager.init();
        currentWord = wordManager.fetchWordOfDay();
        if (currentWord.valid) {
            SleepManager::saveWordToRTC(currentWord);
            rtc_lastWordFetchDay = SleepManager::getCurrentDay();
            rtc_wotdSeenLoggedToday = false;
        }
    } else if (rtc_currentWord.valid) {
        currentWord = SleepManager::loadWordFromRTC();
        Serial.println("Loaded word from RTC memory");
    } else {
        Serial.println("No valid word in RTC - fetching");
        wordManager.init();
        currentWord = wordManager.fetchWordOfDay();
        SleepManager::saveWordToRTC(currentWord);
    }

    if (allHistoryWords.size() == 0) {
        StatsManager::loadWordStats(allHistoryWords);
    }

    if (currentWord.valid) {
        wordManager.saveWordSeen(currentWord);
    }

    epd.Init_Fast();

    if (wakeButtonPress == HELD) {
        Serial.println("Showing stats (button held on wake)");
        showStats();
        led.stop();

        if (waitForClickOrHold(SHORT_TIMEOUT)) {
            Serial.println("Button pressed - returning to main");
        }

        led.start(LED_DISPLAY_UPDATE);
        displayManager.displayWordOfDay(currentWord, rtc_currentStreak, rtc_recallDoneToday);
        led.stop();
    } else {
        showRecall();
        bool answered = waitForRecallAnswer();
                
        if (answered) {
            ButtonAction finishedRecall = waitForClickOrHold(SHORT_TIMEOUT);

            // led.start(LED_DISPLAY_UPDATE);
            showStats();
            led.stop();

            Serial.println("Showing stats (press button to skip)");
            if (waitForClickOrHold(SHORT_TIMEOUT)) {
                Serial.println("Button pressed - skipping stats");
            }
        }

        // led.start(LED_DISPLAY_UPDATE); // TODO need to check if this is needed or not
        displayManager.displayWordOfDay(currentWord, rtc_currentStreak, rtc_recallDoneToday);
        led.stop();
    }

    if (rtc_recallDoneToday) {
        SleepManager::enterDeepSleepUntil(DAILY_UPDATE_HOUR, DAILY_UPDATE_MINUTE);
    } else {
        SleepManager::enterDeepSleepFor(LED_REMINDER_INTERVAL);
    }
}

void setup() {
    led.init(LED_PIN);
    led.start(LED_DISPLAY_UPDATE);

    // Start WiFi early so it connects during button detection and serial init
    WiFiManager::loadCredentials(wifi_ssid, wifi_password);
    if (wifi_ssid.length() > 0) {
        WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    }

    // Check if button is STILL pressed (they're holding it during boot)
    pinMode(RECALL_SWITCH_PIN, INPUT_PULLUP);
    ButtonAction wakeButtonPress = NO_ACTION;
    int longHold = 0;
    delay(WAKE_BUTTON_HOLD_THRESHOLD);
    if (digitalRead(RECALL_SWITCH_PIN) == LOW) {
        wakeButtonPress = HELD;
        while (digitalRead(RECALL_SWITCH_PIN) == LOW) {
            delay(10);
            longHold += 10;
        }
    } else {
        wakeButtonPress = CLICKED;
    }

    Serial.begin(115200);
    delay(100);
    randomSeed((unsigned long)esp_random());

    Serial.println("\n=== Running Setup ===\n");
    Serial.printf("Wake Button Press: %s\n", (wakeButtonPress==HELD)? "HELD" : "CLICKED");

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS failed");
    }

    if (longHold >= 5000) {
    // if (true) { // ! TESTING
        enterSettings = true;
        Serial.print("Long Press for settings detected\n");
        handleFirstBoot();
    }
    
    // ! TESTING DISPLAY ALIGNMENT
    // epd.Init_Fast();
    // displayManager.displayAlignmentGuides();
    // ButtonAction exit = waitForClickOrHold(300000);

    // ! Test code to reset preferences
    // preferences.begin("recall", false);
    // preferences.clear();
    // preferences.end();
    // Serial.println("Preferences cleared!");
    // ! Test code to set prefs
    // preferences.begin("recall", false);
    // preferences.putULong("lastRecallDay", 20498);
    // preferences.putBool("recallDone", false);
    // preferences.putInt("streak", 3);
    // preferences.end();
    // Serial.println("Preferences set!");

    SleepManager::init(RECALL_SWITCH_PIN);

    SleepManager::WakeReason reason = SleepManager::getWakeReason();

    switch (reason) {
        case SleepManager::WAKE_FIRST_BOOT:
            handleFirstBoot();
            break;

        case SleepManager::WAKE_TIMER:
            handleDailyUpdate();
            break;

        case SleepManager::WAKE_LED_REMINDER:
            handleLEDReminder();
            break;

        case SleepManager::WAKE_BUTTON:
            handleButtonPress(wakeButtonPress);
            break;

        default:
            Serial.println("Unknown wake - sleeping");
            SleepManager::enterDeepSleepFor(LED_REMINDER_INTERVAL);
            break;
    }
    Serial.println("ERROR: Reached end of setup");
    SleepManager::enterDeepSleepFor(LED_REMINDER_INTERVAL);
}

void loop() {
    Serial.println("ERROR: In loop - sleeping");
    delay(1000);
    SleepManager::enterDeepSleepFor(LED_REMINDER_INTERVAL);
}
