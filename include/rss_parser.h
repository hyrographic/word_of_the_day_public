#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include "types.h"
#include "constants.h"
#include "text_utils.h"

class RSSParser {
public:
    static String extractXMLTag(const String& xml, const String& tagName);
    static long extractDateFromLink(const String& link);
    static String formatDateFromTimestamp(long timestamp);
    static WordData parseRSSItem(const String& item);
};

// Implementation
String RSSParser::extractXMLTag(const String& xml, const String& tagName) {
    String openTag = "<" + tagName + ">";
    String closeTag = "</" + tagName + ">";
    
    int startPos = xml.indexOf(openTag);
    if (startPos == -1) return "";
    
    startPos += openTag.length();
    int endPos = xml.indexOf(closeTag, startPos);
    if (endPos == -1) return "";
    
    return xml.substring(startPos, endPos);
}

long RSSParser::extractDateFromLink(const String& link) {
    int lastDash = link.lastIndexOf('-');
    int secondLastDash = link.lastIndexOf('-', lastDash - 1);
    int thirdLastDash = link.lastIndexOf('-', secondLastDash - 1);
    
    if (lastDash != -1 && secondLastDash != -1) {
        int day = link.substring(lastDash + 1).toInt();
        int month = link.substring(secondLastDash + 1, lastDash).toInt();
        int year = link.substring(thirdLastDash + 1, secondLastDash).toInt();
        
        if (month >= 1 && month <= 12) {
            struct tm timeinfo = {0}; 
            timeinfo.tm_mday = day;
            timeinfo.tm_mon = month - 1;
            timeinfo.tm_year = year - 1900;
            return mktime(&timeinfo);
        }
    }
    return 0;
}

String RSSParser::formatDateFromTimestamp(long timestamp) {
    struct tm timeinfo;
    localtime_r((time_t*)&timestamp, &timeinfo);
    
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    
    char buffer[10];
    sprintf(buffer, "%02d-%s", timeinfo.tm_mday, months[timeinfo.tm_mon]);
    return String(buffer);
}

String removeCDATA(String text) {
    text.trim();
    if (text.startsWith("<![CDATA[")) {
        text = text.substring(9);
    }
    if (text.endsWith("]]>")) {
        text = text.substring(0, text.length() - 3);
    }
    return text;
}

WordData RSSParser::parseRSSItem(const String& item) {
    WordData data;
    data.valid = false;
    
    // Parse title (word)
    String title = extractXMLTag(item, "title");
    title = removeCDATA(title);
    title.trim();
    data.word = title;
    
    // Parse link (for date)
    String link = extractXMLTag(item, "link");
    link = removeCDATA(link);
    link.trim();
    data.date = extractDateFromLink(link);
    
    // Parse description
    String description = extractXMLTag(item, "description");
    description = removeCDATA(description);
    
    // Extract phonetic (between backslashes)
    int phoneticStart = description.indexOf('\\');
    if (phoneticStart != -1) {
        int phoneticEnd = description.indexOf('\\', phoneticStart + 1);
        if (phoneticEnd != -1) {
            data.phonetic = description.substring(phoneticStart + 1, phoneticEnd);
            data.phonetic.replace("&nbsp;", "");
            data.phonetic.replace("&amp;", "&");
            data.phonetic.trim();
        }
    }
    
    // Extract part of speech (in <em> tags)
    int emStart = description.indexOf("<em>", phoneticStart > 0 ? phoneticStart : 0);
    if (emStart != -1) {
        int emEnd = description.indexOf("</em>", emStart);
        if (emEnd != -1) {
            data.partOfSpeech = description.substring(emStart + 4, emEnd);
            data.partOfSpeech.trim();
            data.partOfSpeech.replace("adjective", "adj");
            data.partOfSpeech.replace("adverb", "adv");
            data.partOfSpeech.replace("preposition", "prep");
        }
    }
    
    // Extract definition (first <p> tag)
    int defStart = description.indexOf("<p>", emStart > 0 ? emStart : 0);
    if (defStart != -1) {
        int defEnd = description.indexOf("</p>", defStart);
        if (defEnd != -1) {
            String def = description.substring(defStart + 3, defEnd);
            def = TextUtils::cleanText(def);
            
            if (def.length() > MAX_DEFINITION_LENGTH) {
                int firstPeriod = def.indexOf('.');
                if (firstPeriod != -1 && firstPeriod <= MAX_DEFINITION_LENGTH) {
                    def = def.substring(0, firstPeriod + 1);
                } else {
                    def = def.substring(0, MAX_DEFINITION_LENGTH) + "...";
                }
            }
            data.definition = def;
            data.definition.trim();
        }
    }
    
    // Extract example (second <p> tag)
    int exampleStart = description.indexOf("<p>", defStart > 0 ? defStart + 1 : 0);
    if (exampleStart != -1) {
        int exampleEnd = description.indexOf("</p>", exampleStart);
        if (exampleEnd != -1) {
            String example = description.substring(exampleStart + 3, exampleEnd);
            example = TextUtils::cleanText(example);
            
            if (example.length() > MAX_EXAMPLE_LENGTH) {
                example = example.substring(0, MAX_EXAMPLE_LENGTH) + "...";
            }
            example.replace("// ", "");
            example.trim();
            example = "\"" + example + "\"";
            data.example = TextUtils::blockWordInExample(example, data.word);
        }
    }
    
    data.valid = (data.word.length() > 0);
    return data;
}
