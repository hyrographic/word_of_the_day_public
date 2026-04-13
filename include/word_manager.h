#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include "types.h"
#include "constants.h"
#include "rss_parser.h"
#include "stats_manager.h"
#include "sleep_manager.h"

class WordManager {
public:
    void init();
    bool fetchAndCacheWords();
    WordData fetchWordOfDay(int daysAgo = 0);
    WordData fetchWordFromHistory(const String& word);
    WordData* findWordInCache(const String& word);
    String selectRecallWord(const std::vector<WordStats>& historyWords);
    bool saveRecallResult(const WordData& word, int daysAgo, bool correct);
    bool saveWordSeen(const WordData& word);
    
    const RSSWordCache& getCache() const { return cache; }
    
private:
    RSSWordCache cache;
    
    String extractJSONString(const String& line, const String& key);
    long extractJSONNumber(const String& line, const String& key);
};

void WordManager::init() {
    cache.words.clear();
    cache.wordList.clear();
    cache.valid = false;
    cache.lastFetch = 0;
}

bool WordManager::fetchAndCacheWords() {
    Serial.println("Fetching and caching RSS words...");
    cache.words.clear();
    cache.wordList.clear();
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        cache.valid = false;
        return false;
    }
    
    HTTPClient http;
    http.begin(RSS_URL);
    http.setTimeout(15000);
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.printf("RSS feed fetched: %d bytes\n", payload.length());
        
        int currentItem = payload.indexOf("<item>");
        int itemIndex = 0;
        
        while (currentItem != -1 && itemIndex < 8) {
            int itemEnd = payload.indexOf("</item>", currentItem);
            if (itemEnd == -1) break;
            
            String item = payload.substring(currentItem, itemEnd + 7);
            WordData data = RSSParser::parseRSSItem(item);
            
            if (data.valid) {
                cache.words.push_back(data);
                cache.wordList.push_back(data.word);
                Serial.printf("Cached RSS word %d: %s\n", itemIndex, data.word.c_str());
            }
            
            currentItem = payload.indexOf("<item>", itemEnd);
            itemIndex++;
        }
        
        cache.valid = (cache.words.size() > 0);
        cache.lastFetch = millis();
        Serial.printf("Cached %d words from RSS\n", cache.words.size());
    } else {
        Serial.printf("HTTP Error: %d\n", httpCode);
        cache.valid = false;
    }
    
    http.end();
    return cache.valid;
}

WordData* WordManager::findWordInCache(const String& word) {
    for (size_t i = 0; i < cache.words.size(); i++) {
        if (cache.words[i].word.equalsIgnoreCase(word)) {
            return &cache.words[i];
        }
    }
    return nullptr;
}

String WordManager::selectRecallWord(const std::vector<WordStats>& historyWords) {
    int selection = random(0, 2);
    
    // 50% chance: Recent RSS word
    if (selection == 0 && cache.wordList.size() > 0) {
        Serial.println("Selected RSS word");
        int idx = random(1, cache.wordList.size());
        Serial.printf("Selected word: %s\n", cache.wordList[idx].c_str());
        return cache.wordList[idx];
    }
    
    // 50% chance: Historic word >2 weeks old
    if (selection == 1) {
        Serial.println("Selected historic word");
        std::vector<WordStats> historicWords = StatsManager::filterWordsByMinAge(historyWords, 2);
        Serial.printf("Available: %d\n", historicWords.size());
        if (historicWords.size() > 5) {
            int idx = random(0, historicWords.size());
            String word = historicWords[idx].word;
            Serial.printf("Selected word: %s\n", word.c_str());
            return word;
        } else {
            Serial.printf("Can't select historic word (over 2 weeks old) with less than 5 options\n");
        }
    }
    
    // Fallback to RSS
    if (cache.wordList.size() > 0) {
        int idx = random(1, cache.wordList.size());
        Serial.printf("Fallback to RSS word: %s\n", cache.wordList[idx].c_str());
        return cache.wordList[idx];
    }
    
    // Last resort: any history word
    if (historyWords.size() > 0) {
        int idx = random(0, historyWords.size());
        String word = historyWords[idx].word;
        Serial.printf("Fallback to any history word: %s\n", word.c_str());
        return word;
    }
    
    Serial.println("No words available!");
    return "";
}

WordData WordManager::fetchWordOfDay(int daysAgo) {
    WordData data;
    data.valid = false;
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        return data;
    }
    
    HTTPClient http;
    Serial.printf("Fetching word from %d day(s) ago\n", daysAgo);
    
    http.begin(RSS_URL);
    http.setTimeout(15000);
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.printf("RSS feed fetched: %d bytes\n", payload.length());
        
        // Find the correct item index
        int currentItem = payload.indexOf("<item>");
        for (int i = 0; i <= daysAgo && currentItem != -1; i++) {
            if (i == daysAgo) {
                int itemEnd = payload.indexOf("</item>", currentItem);
                if (itemEnd != -1) {
                    String item = payload.substring(currentItem, itemEnd + 7);
                    data = RSSParser::parseRSSItem(item);
                    
                    if (data.valid) {
                        Serial.println("Parsed word data:");
                        Serial.println("  Word: " + data.word);
                        Serial.println("  POS: " + data.partOfSpeech);
                        Serial.println("  Phonetic: " + data.phonetic);
                        Serial.println("  Definition: " + data.definition);
                        Serial.println("  Example: " + data.example);
                    }
                }
                break;
            }
            int nextItemEnd = payload.indexOf("</item>", currentItem);
            currentItem = payload.indexOf("<item>", nextItemEnd);
        }
    } else {
        Serial.printf("HTTP Error: %d\n", httpCode);
    }
    
    http.end();
    return data;
}

String WordManager::extractJSONString(const String& line, const String& key) {
    String searchKey = "\"" + key + "\"";
    int keyPos = line.indexOf(searchKey);
    if (keyPos == -1) return "";

    int colonPos = line.indexOf(':', keyPos);
    if (colonPos == -1) return "";

    int quotePos = colonPos + 1;
    while (quotePos < line.length() && (line[quotePos] == ' ' || line[quotePos] == '\t')) {
        quotePos++;
    }

    if (quotePos >= line.length() || line[quotePos] != '\"') return "";

    int valueStart = quotePos + 1;

    // Find closing quote, skipping escaped quotes
    int valueEnd = valueStart;
    while (valueEnd < line.length()) {
        if (line[valueEnd] == '\"' && (valueEnd == 0 || line[valueEnd - 1] != '\\')) {
            break;
        }
        valueEnd++;
    }
    if (valueEnd >= line.length()) return "";

    return line.substring(valueStart, valueEnd);
}

long WordManager::extractJSONNumber(const String& line, const String& key) {
    String searchKey = "\"" + key + "\"";
    int keyPos = line.indexOf(searchKey);
    if (keyPos == -1) return 0;
    
    int colonPos = line.indexOf(':', keyPos);
    if (colonPos == -1) return 0;
    
    int valueStart = colonPos + 1;
    while (valueStart < line.length() && line[valueStart] == ' ') {
        valueStart++;
    }
    
    int valueEnd = line.indexOf(',', valueStart);
    if (valueEnd == -1) valueEnd = line.indexOf('}', valueStart);
    
    String val = line.substring(valueStart, valueEnd);
    val.trim();
    if (val.startsWith("\"")) val = val.substring(1);
    if (val.endsWith("\"")) val = val.substring(0, val.length() - 1);
    return val.toInt();
}

WordData WordManager::fetchWordFromHistory(const String& word) {
    WordData data;
    data.valid = false;
    data.word = word;
    
    File file = LittleFS.open(RECALL_HISTORY_FILE, "r");
    if (!file) {
        Serial.println("No history file");
        return data;
    }
    
    Serial.printf("Searching for '%s' in history...\n", word.c_str());
    
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        
        String lineWord = extractJSONString(line, "word");
        
        if (lineWord.equalsIgnoreCase(word)) {
            data.phonetic = extractJSONString(line, "phon");
            data.partOfSpeech = extractJSONString(line, "pos");
            data.definition = extractJSONString(line, "def");
            data.example = extractJSONString(line, "ex");
            data.date = extractJSONNumber(line, "date");

            // Unescape JSON
            data.definition.replace("\\n", "\n");
            data.definition.replace("\\\"", "\"");
            data.definition.replace("\\\\", "\\");
            data.example.replace("\\n", "\n");
            data.example.replace("\\\"", "\"");
            data.example.replace("\\\\", "\\");

            data.valid = true;
            break;  // Found the word, stop searching
        }
    }
    
    file.close();
    
    if (data.valid) {
        Serial.println("Successfully loaded word from history");
    } else {
        Serial.println("Word not found in history");
    }
    
    return data;
}

bool WordManager::saveRecallResult(const WordData& word, int daysAgo, bool correct) {
    File file = LittleFS.open(RECALL_HISTORY_FILE, "a");
    if (!file) {
        Serial.println("Failed to open history file");
        return false;
    }
    
    time_t now = time(nullptr);
    
    // Build JSON
    String jsonLine = "{";
    jsonLine += "\"ts\":" + String(now) + ",";
    jsonLine += "\"word\":\"" + TextUtils::escapeJson(word.word) + "\",";
    jsonLine += "\"phon\":\"" + TextUtils::escapeJson(word.phonetic) + "\",";
    jsonLine += "\"pos\":\"" + TextUtils::escapeJson(word.partOfSpeech) + "\",";
    jsonLine += "\"def\":\"" + TextUtils::escapeJson(word.definition) + "\",";
    jsonLine += "\"date\":" + String(word.date) + ",";
    jsonLine += "\"ex\":\"" + TextUtils::escapeJson(word.example) + "\",";
    jsonLine += "\"correct\":" + String(correct ? "true" : "false");
    jsonLine += "}";
    
    file.println(jsonLine);
    file.close();
    
    Serial.printf("Recall result saved: %s (%d days ago) - %s\n",
                  word.word.c_str(), daysAgo, correct ? "CORRECT" : "INCORRECT");
    return true;
}

bool WordManager::saveWordSeen(const WordData& word) {
    if (rtc_wotdSeenLoggedToday) {
        Serial.println("WOTD already logged as seen today");
        return true;
    }

    File file = LittleFS.open(RECALL_HISTORY_FILE, "a");
    if (!file) {
        Serial.println("Failed to open history file for seen entry");
        return false;
    }

    time_t now = time(nullptr);
    String jsonLine = "{";
    jsonLine += "\"ts\":" + String(now) + ",";
    jsonLine += "\"word\":\"" + TextUtils::escapeJson(word.word) + "\",";
    jsonLine += "\"phon\":\"" + TextUtils::escapeJson(word.phonetic) + "\",";
    jsonLine += "\"pos\":\"" + TextUtils::escapeJson(word.partOfSpeech) + "\",";
    jsonLine += "\"def\":\"" + TextUtils::escapeJson(word.definition) + "\",";
    jsonLine += "\"date\":" + String(word.date) + ",";
    jsonLine += "\"ex\":\"" + TextUtils::escapeJson(word.example) + "\",";
    jsonLine += "\"correct\":null";
    jsonLine += "}";

    file.println(jsonLine);
    file.close();

    rtc_wotdSeenLoggedToday = true;
    Serial.printf("WOTD logged as seen: %s\n", word.word.c_str());
    return true;
}
