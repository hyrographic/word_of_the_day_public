#include "stats_manager.h"

time_t StatsManager::floorToLocalMidnight(time_t t) {
    struct tm tmv;
    localtime_r(&t, &tmv);
    tmv.tm_hour = 0;
    tmv.tm_min = 0;
    tmv.tm_sec = 0;
    return mktime(&tmv);
}

time_t StatsManager::getWeekStartMonday(time_t t) {
    struct tm tmv;
    localtime_r(&t, &tmv);
    int currentWeekday = tmv.tm_wday;
    int daysToMonday = (currentWeekday == 0) ? 6 : (currentWeekday - 1);
    return floorToLocalMidnight(t) - (daysToMonday * 86400);
}

bool StatsManager::isValidTime(time_t t) {
    return t > 1577836800;  // After Jan 1, 2020
}

time_t StatsManager::getEndMondayWith3WeeksOfMonth(time_t anchorTime) {
    time_t anchorMidnight = floorToLocalMidnight(anchorTime);
    
    struct tm tmAnchor;
    localtime_r(&anchorMidnight, &tmAnchor);
    int anchorMonth = tmAnchor.tm_mon;
    
    time_t currentMonday = getWeekStartMonday(anchorMidnight);
    
    // Count weeks in current month
    int weeksInCurrentMonth = 0;
    time_t testMonday = currentMonday;
    
    while (weeksInCurrentMonth < 10) {
        struct tm tmTest;
        localtime_r(&testMonday, &tmTest);
        
        if (tmTest.tm_mon != anchorMonth) break;
        
        weeksInCurrentMonth++;
        testMonday -= (7 * 86400);
    }
    
    if (weeksInCurrentMonth >= 3) {
        return getWeekStartMonday(anchorMidnight) + (7 * 86400);
    }
    
    int additionalWeeksNeeded = 3 - weeksInCurrentMonth;
    time_t endMonday = currentMonday + (7 * 86400);
    
    for (int i = 0; i < additionalWeeksNeeded; i++) {
        endMonday += (7 * 86400);
    }
    
    return endMonday;
}

void StatsManager::generateMonthLabels(time_t gridStartTime, int totalDays, GridMetadata& metadata) {
    metadata.monthCount = 0;
    String currentMonth = "";
    int prevWeekCol = -1;
    
    for (int d = 0; d < totalDays; d++) {
        time_t thisDayMidnight = gridStartTime + (d * 86400);
        struct tm tm;
        localtime_r(&thisDayMidnight, &tm);
        
        char monthKey[4];
        strftime(monthKey, sizeof(monthKey), "%b", &tm);
        String thisMonth(monthKey);
        
        int weekColumn = d / 7;
        
        if (thisMonth != currentMonth && weekColumn != prevWeekCol) {
            currentMonth = thisMonth;
            
            // Count weeks in this month
            int weeksInThisMonth = 1;
            int lastWeekOfMonth = weekColumn;
            for (int futureDay = d + 1; futureDay < totalDays; futureDay++) {
                time_t futureMidnight = gridStartTime + (futureDay * 86400);
                struct tm futureTm;
                localtime_r(&futureMidnight, &futureTm);
                char futureMonth[4];
                strftime(futureMonth, sizeof(futureMonth), "%b", &futureTm);

                if (String(futureMonth) != currentMonth) break;

                int futureWeek = futureDay / 7;
                if (futureWeek > lastWeekOfMonth) {
                    lastWeekOfMonth = futureWeek;
                }
            }
            weeksInThisMonth = lastWeekOfMonth - weekColumn;
            
            // Check if last month
            bool isLastMonth = true;
            for (int futureDay = d + 1; futureDay < totalDays; futureDay++) {
                time_t futureMidnight = gridStartTime + (futureDay * 86400);
                struct tm futureTm;
                localtime_r(&futureMidnight, &futureTm);
                char futureMonth[4];
                strftime(futureMonth, sizeof(futureMonth), "%b", &futureTm);
                
                if (String(futureMonth) != currentMonth) {
                    isLastMonth = false;
                    break;
                }
            }
            
            if ((weeksInThisMonth >= 3 && metadata.monthCount < 12) || isLastMonth) {
                String labelUpper = thisMonth;
                labelUpper.toUpperCase();
                metadata.monthLabels[metadata.monthCount] = labelUpper;
                metadata.monthStartColumns[metadata.monthCount] = weekColumn;
                metadata.partialMonth[metadata.monthCount] = (weeksInThisMonth < 4);
                metadata.monthCount++;
            }
            
            prevWeekCol = weekColumn;
        }
    }
}

void StatsManager::loadDailyGrid(DayData* grid, GridMetadata& metadata,
                                 int maxWeeks, int daysPerWeek,
                                 int& totalCorrect, int& totalIncorrect) {
    
    const int TOTAL_DAYS = maxWeeks * daysPerWeek;
    
    memset(grid, 0, sizeof(DayData) * TOTAL_DAYS);
    totalCorrect = 0;
    totalIncorrect = 0;
    
    File file = LittleFS.open(RECALL_HISTORY_FILE, "r");
    if (!file) {
        Serial.println("No recall history file found");
        return;
    }
    
    time_t oldestTime = 0;
    time_t newestTime = 0;
    
    std::vector<uint16_t> correctCount(TOTAL_DAYS, 0);
    std::vector<uint16_t> incorrectCount(TOTAL_DAYS, 0);
    
    // First pass: find date range and count totals
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        
        int tsPos = line.indexOf("\"ts\":");
        if (tsPos == -1) continue;
        
        int tsStart = tsPos + 5;
        int tsEnd = line.indexOf(',', tsStart);
        if (tsEnd == -1) tsEnd = line.indexOf('}', tsStart);
        time_t timestamp = line.substring(tsStart, tsEnd).toInt();
        
        bool isNull = (line.indexOf("\"correct\":null") != -1) ||
                      (line.indexOf("\"correct\": null") != -1);
        if (isNull) continue;

        bool isCorrect = (line.indexOf("\"correct\":true") != -1) ||
                         (line.indexOf("\"correct\": true") != -1);

        if (oldestTime == 0 || timestamp < oldestTime) oldestTime = timestamp;
        if (timestamp > newestTime) newestTime = timestamp;

        if (isCorrect) totalCorrect++;
        else totalIncorrect++;
    }
    
    file.close();
    
    if (newestTime == 0) {
        Serial.println("No recall history data");
        return;
    }
    
    // Determine anchor point
    time_t now = time(nullptr);
    time_t anchorTime = isValidTime(now) ? now : newestTime;
    
    // Calculate grid time range
    time_t nextMondayMidnight = getEndMondayWith3WeeksOfMonth(anchorTime);
    time_t gridStartTime = nextMondayMidnight - (maxWeeks * 7 * 86400);
    
    Serial.printf("Grid span: %d weeks (%d days)\n", maxWeeks, TOTAL_DAYS);
    
    // Generate month labels
    generateMonthLabels(gridStartTime, TOTAL_DAYS, metadata);
    
    // Second pass: aggregate counts
    file = LittleFS.open(RECALL_HISTORY_FILE, "r");
    if (!file) return;
    
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        
        int tsPos = line.indexOf("\"ts\":");
        if (tsPos == -1) continue;
        
        int tsStart = tsPos + 5;
        int tsEnd = line.indexOf(',', tsStart);
        if (tsEnd == -1) tsEnd = line.indexOf('}', tsStart);
        time_t timestamp = line.substring(tsStart, tsEnd).toInt();
        
        bool isNull = (line.indexOf("\"correct\":null") != -1) ||
                      (line.indexOf("\"correct\": null") != -1);
        if (isNull) continue;

        bool isCorrect = (line.indexOf("\"correct\":true") != -1) ||
                         (line.indexOf("\"correct\": true") != -1);

        if (timestamp < gridStartTime) continue;

        time_t tsMidnight = floorToLocalMidnight(timestamp);
        int daysFromStart = (tsMidnight - gridStartTime) / 86400;

        if (daysFromStart >= 0 && daysFromStart < TOTAL_DAYS) {
            if (isCorrect) correctCount[daysFromStart]++;
            else incorrectCount[daysFromStart]++;
        }
    }
    
    file.close();
    
    // Populate grid
    time_t anchorMidnight = floorToLocalMidnight(anchorTime);
    
    for (int d = 0; d < TOTAL_DAYS; d++) {
        time_t thisDayMidnight = gridStartTime + (d * 86400);
        struct tm tmThis;
        localtime_r(&thisDayMidnight, &tmThis);
        
        int week = d / 7;
        int day = (tmThis.tm_wday == 0) ? 6 : (tmThis.tm_wday - 1);
        int idx = week * 7 + day;
        
        uint16_t c = correctCount[d];
        uint16_t ic = incorrectCount[d];
        
        grid[idx].hasData = (c > 0 || ic > 0);
        grid[idx].incorrectRatio = (c + ic > 0) ? ic / (double)(c + ic) : 0.0;
        grid[idx].isFuture = (thisDayMidnight > anchorMidnight);
    }
}

void StatsManager::loadWordStats(std::vector<WordStats>& allWords) {
    allWords.clear();
    
    std::map<String, WordStats> wordMap;
    
    File file = LittleFS.open(RECALL_HISTORY_FILE, "r");
    if (!file) {
        Serial.println("No recall history file");
        return;
    }
    
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        
        // Parse timestamp
        int tsPos = line.indexOf("\"ts\"");
        if (tsPos == -1) continue;
        
        int colonPos = line.indexOf(':', tsPos);
        if (colonPos == -1) continue;
        
        int tsStart = colonPos + 1;
        while (tsStart < line.length() && line[tsStart] == ' ') tsStart++;
        
        int tsEnd = line.indexOf(',', tsStart);
        if (tsEnd == -1) tsEnd = line.indexOf('}', tsStart);
        time_t timestamp = line.substring(tsStart, tsEnd).toInt();
        
        // Parse word
        int wordPos = line.indexOf("\"word\"");
        if (wordPos == -1) continue;
        
        int wordColonPos = line.indexOf(':', wordPos);
        if (wordColonPos == -1) continue;
        
        int wordQuotePos = wordColonPos + 1;
        while (wordQuotePos < line.length() && 
               (line[wordQuotePos] == ' ' || line[wordQuotePos] == '\t')) {
            wordQuotePos++;
        }
        
        if (wordQuotePos >= line.length() || line[wordQuotePos] != '\"') continue;
        
        int wordStart = wordQuotePos + 1;
        int wordEnd = line.indexOf('\"', wordStart);
        if (wordEnd == -1) continue;
        
        String word = line.substring(wordStart, wordEnd);
        word.trim();
        if (word.length() == 0) continue;
        
        // Parse correct flag
        bool isCorrect = false;
        bool isNull = false;
        int correctPos = line.indexOf("\"correct\"");
        if (correctPos != -1) {
            int correctColonPos = line.indexOf(':', correctPos);
            if (correctColonPos != -1) {
                int searchStart = correctColonPos + 1;
                while (searchStart < line.length() && line[searchStart] == ' ') {
                    searchStart++;
                }

                if (line.substring(searchStart).startsWith("null")) {
                    isNull = true;
                } else if (line.substring(searchStart).startsWith("true")) {
                    isCorrect = true;
                }
            }
        }

        // Update stats
        if (wordMap.find(word) == wordMap.end()) {
            WordStats stats;
            stats.word = word;
            stats.totalAttempts = 0;
            stats.correctAttempts = 0;
            stats.lastSeen = timestamp;
            stats.weight = 0.0;
            wordMap[word] = stats;
        }

        WordStats& stats = wordMap[word];
        if (!isNull) {
            stats.totalAttempts++;
            if (isCorrect) stats.correctAttempts++;
        }
        if (timestamp > stats.lastSeen) stats.lastSeen = timestamp;
    }
    
    file.close();
    
    // Calculate weights
    for (auto& pair : wordMap) {
        WordStats& stats = pair.second;
        
        double successRate = (stats.totalAttempts > 0) 
            ? (double)stats.correctAttempts / stats.totalAttempts 
            : 0.5;
        
        double errorRate = 1.0 - successRate;
        double attemptBoost = 1.0 + (stats.totalAttempts * 0.1);
        if (attemptBoost > 2.0) attemptBoost = 2.0;
        
        stats.weight = errorRate * attemptBoost;
        if (stats.weight < 0.1) stats.weight = 0.1;
        
        allWords.push_back(stats);
    }
    
    Serial.printf("Loaded %d unique words from history\n", allWords.size());
}

String StatsManager::selectWeightedWord(const std::vector<WordStats>& words) {
    if (words.size() == 0) {
        Serial.println("No words available");
        return "";
    }
    
    if (words.size() == 1) {
        return words[0].word;
    }
    
    double totalWeight = 0.0;
    for (const auto& w : words) {
        totalWeight += w.weight;
    }
    
    if (totalWeight <= 0.0) {
        int idx = random(0, words.size());
        return words[idx].word;
    }
    
    double randomPoint = random(0, 10000) / 10000.0 * totalWeight;
    
    double cumulative = 0.0;
    for (const auto& w : words) {
        cumulative += w.weight;
        if (randomPoint <= cumulative) {
            return w.word;
        }
    }
    
    return words[words.size() - 1].word;
}

std::vector<WordStats> StatsManager::filterWordsByAge(const std::vector<WordStats>& allWords, 
                                                       int maxAgeWeeks) {
    std::vector<WordStats> filtered;
    time_t now = time(nullptr);
    time_t cutoffTime = now - (maxAgeWeeks * 7 * 86400);
    
    for (const auto& w : allWords) {
        if (w.lastSeen >= cutoffTime) {
            filtered.push_back(w);
        }
    }
    
    return filtered;
}

std::vector<WordStats> StatsManager::filterWordsByMinAge(const std::vector<WordStats>& allWords, 
                                                          int minAgeWeeks) {
    std::vector<WordStats> filtered;
    time_t now = time(nullptr);
    time_t cutoffTime = now - (minAgeWeeks * 7 * 86400);
    
    for (const auto& w : allWords) {
        if (w.lastSeen < cutoffTime) {
            filtered.push_back(w);
        }
    }
    
    return filtered;
}
