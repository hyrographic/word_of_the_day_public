#pragma once
#include <Arduino.h>

// Text utilities
class TextUtils {
public:
    static String escapeJson(const String& str);
    static String cleanText(String text);
    static void removeAccents(String& s);
    static String blockWordInExample(const String& example, const String& word);
    static bool isAlphaNumeric(char c);
    
private:
    static String removeCDATA(String text);
};

// Implementation
String TextUtils::escapeJson(const String& str) {
    String result = str;
    result.replace("\\", "\\\\");
    result.replace("\"", "\\\"");
    result.replace("\n", "\\n");
    return result;
}

String TextUtils::removeCDATA(String text) {
    text.trim();
    if (text.startsWith("<![CDATA[")) {
        text = text.substring(9);
    }
    if (text.endsWith("]]>")) {
        text = text.substring(0, text.length() - 3);
    }
    return text;
}

String TextUtils::cleanText(String text) {
    // Remove HTML tags
    while (text.indexOf('<') != -1) {
        int start = text.indexOf('<');
        int end = text.indexOf('>', start);
        if (end != -1) {
            text = text.substring(0, start) + text.substring(end + 1);
        } else {
            break;
        }
    }
    
    // HTML entity replacements
    const struct { const char* from; const char* to; } rules[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""},
        {"&#39;", "'"}, {"&nbsp;", " "}, {"“", "\""}, {"”", "\""},
        {"’", "'"}, {"‘", "'"}, {"—", ", "}, {" ", " "}
    };
    
    for (const auto& rule : rules) {
        text.replace(rule.from, rule.to);
    }
    
    removeAccents(text);
    text.trim();
    return text;
}

void TextUtils::removeAccents(String& s) {
    String out;
    out.reserve(s.length());

    for (size_t i = 0; i < s.length(); ++i) {
        uint8_t c = s[i];

        if (c == 0xC3 && i + 1 < s.length()) {
            uint8_t n = s[i + 1];
            
            // Map accented characters to base characters
            if (n >= 0x80 && n <= 0x85) out += 'A';
            else if (n >= 0xA0 && n <= 0xA5) out += 'a';
            else if (n >= 0x88 && n <= 0x8B) out += 'E';
            else if (n >= 0xA8 && n <= 0xAB) out += 'e';
            else if (n >= 0x8C && n <= 0x8F) out += 'I';
            else if (n >= 0xAC && n <= 0xAF) out += 'i';
            else if (n >= 0x92 && n <= 0x96) out += 'O';
            else if (n >= 0xB2 && n <= 0xB6) out += 'o';
            else if (n >= 0x99 && n <= 0x9C) out += 'U';
            else if (n >= 0xB9 && n <= 0xBC) out += 'u';
            else if (n == 0x87) out += 'C';
            else if (n == 0xA7) out += 'c';
            else out += '?';
            
            i++;
        } else {
            out += char(c);
        }
    }
    s = out;
}

bool TextUtils::isAlphaNumeric(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

String TextUtils::blockWordInExample(const String& example, const String& word) {
    String result = "";
    String exampleLower = example;
    exampleLower.toLowerCase();
    String wordLower = word;
    wordLower.toLowerCase();
    
    int minStemMatch = (int)(word.length() * 0.7);
    minStemMatch = max(1, minStemMatch);
    minStemMatch = min(minStemMatch, (int)word.length());
    
    int i = 0;
    while (i < example.length()) {
        bool atWordBoundary = (i == 0 || !isAlphaNumeric(example.charAt(i - 1)));
        
        if (atWordBoundary) {
            int matchLen = 0;
            for (size_t j = 0; j < wordLower.length() && (i + j) < exampleLower.length(); j++) {
                if (exampleLower.charAt(i + j) == wordLower.charAt(j)) {
                    matchLen++;
                } else {
                    break;
                }
            }
            
            if (matchLen >= minStemMatch) {
                for (int j = 0; j < matchLen; j++) {
                    result += '_';
                }
                i += matchLen;
                continue;
            }
        }
        
        result += example.charAt(i);
        i++;
    }
    
    return result;
}
