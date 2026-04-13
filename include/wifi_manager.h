#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include "constants.h"

class WiFiManager {
public:
    static bool connect(const String& ssid, const String& password);
    static void loadCredentials(String& ssid, String& password);
    static void saveCredentials(const String& ssid, const String& password);
    static bool syncTime();
    static int getFailCount();
    static void resetFailCount();
};

bool WiFiManager::connect(const String& ssid, const String& password) {
    WiFi.begin(ssid.c_str(), password.c_str());

    Serial.print("Connecting to WiFi");
    int attempts = 0;

    while (WiFi.status() != WL_CONNECTED && attempts < WIFI_CONNECT_ATTEMPTS) {
        delay(200);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected!");
        Preferences prefs;
        prefs.begin("wifi", false);
        prefs.putInt("fails", 0);
        prefs.end();
        return true;
    }

    Preferences prefs;
    prefs.begin("wifi", false);
    int fails = prefs.getInt("fails", 0) + 1;
    prefs.putInt("fails", fails);
    prefs.end();

    Serial.printf("WiFiManager::connect(): Failed to connect (fail %d)\n", fails);
    return false;
}

void WiFiManager::loadCredentials(String& ssid, String& password) {
    Preferences prefs;
    prefs.begin("wifi", true);
    ssid = prefs.getString("ssid", "");
    password = prefs.getString("password", "");
    prefs.end();
    Serial.printf("Loaded WiFi: %s\n", ssid.c_str());
}

void WiFiManager::saveCredentials(const String& ssid, const String& password) {
    Preferences prefs;
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("password", password);
    prefs.putInt("fails", 0);
    prefs.end();
    Serial.printf("Saved WiFi: %s\n", ssid.c_str());
}

bool WiFiManager::syncTime() {
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "europe.pool.ntp.org");
    setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0", 1);
    tzset();

    Serial.print("Syncing time");
    time_t now = time(nullptr);
    int timeout = 30;

    while (now < 1577836800 && timeout > 0) {
        delay(200);
        Serial.print(".");
        now = time(nullptr);
        timeout--;
    }
    Serial.println();

    if (now > 1577836800) {
        Serial.println("Time synced!");
        return true;
    } else {
        Serial.println("Time sync failed!");
        return false;
    }
}

int WiFiManager::getFailCount() {
    Preferences prefs;
    prefs.begin("wifi", true);
    int fails = prefs.getInt("fails", 0);
    prefs.end();
    return fails;
}

void WiFiManager::resetFailCount() {
    Preferences prefs;
    prefs.begin("wifi", false);
    prefs.putInt("fails", 0);
    prefs.end();
}
