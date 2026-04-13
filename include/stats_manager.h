#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <vector>
#include <map>
#include "types.h"
#include "constants.h"

class StatsManager {
public:
    static void loadDailyGrid(DayData* grid, GridMetadata& metadata, 
                              int maxWeeks, int daysPerWeek,
                              int& totalCorrect, int& totalIncorrect);
    
    static void loadWordStats(std::vector<WordStats>& allWords);
    
    static String selectWeightedWord(const std::vector<WordStats>& words);
    
    static std::vector<WordStats> filterWordsByAge(const std::vector<WordStats>& allWords, 
                                                    int maxAgeWeeks);
    
    static std::vector<WordStats> filterWordsByMinAge(const std::vector<WordStats>& allWords, 
                                                       int minAgeWeeks);

private:
    static time_t floorToLocalMidnight(time_t t);
    static time_t getWeekStartMonday(time_t t);
    static bool isValidTime(time_t t);
    static time_t getEndMondayWith3WeeksOfMonth(time_t anchorTime);
    static void generateMonthLabels(time_t gridStartTime, int totalDays, GridMetadata& metadata);
};
