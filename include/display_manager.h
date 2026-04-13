#include <Arduino.h>
#include "epd2in9b_V4.h"
#include "epdpaint.h"
#include "fonts.h"
#include "varfonts.h"
#include "types.h"
#include "constants.h"
#include "rss_parser.h"
#include "text_utils.h"

// Forward declarations for font functions (from varfonts.h/cpp)
// #ifndef display_manager_h
// #define display_manager_h
// extern uint16_t MeasureVarString(const char* text, const sVARFONT* font, uint8_t spacing);
// extern uint16_t DrawVarString(int x, int y, const char* text, const sVARFONT* font, uint16_t color, uint8_t spacing, bool draw, Paint& paint);

// Font object declarations (from fonts.h and varfonts.h)
extern sFONT Font20;
extern sVARFONT BoldPixels;
extern sVARFONT Born2bSporty;
extern sVARFONT OpenSansPX;

// Bitmap declarations (from config.h)
// extern const unsigned char epd_bitmap_wotd_symbol[];
// extern const unsigned char epd_bitmap_recall[];
// extern const unsigned char epd_bitmap_recap_symbol[];
// extern const unsigned char epd_bitmap_black_25[];
// extern const unsigned char epd_bitmap_black_50[];
// extern const unsigned char epd_bitmap_black_75[];
// extern const unsigned char epd_bitmap_black_100[];
// extern const unsigned char epd_bitmap_red_25[];
// extern const unsigned char epd_bitmap_red_50[];
// extern const unsigned char epd_bitmap_red_75[];
// extern const unsigned char epd_bitmap_red_100[];
// extern const unsigned char epd_bitmap_cross[];
// extern const unsigned char epd_bitmap_pipe_symbol[];
// extern const unsigned char epd_bitmap_hash[];
// extern const unsigned char epd_bitmap_tick[];
// #endif

class DisplayManager {
public:
    DisplayManager(Epd& display);
    
    void displayWordOfDay(const WordData& word, int streak, bool recallDone);
    void displayRecall(const WordData& word, int daysAgo, bool showAnswer);
    void displayStats(int totalCorrect, int totalIncorrect, int currentStreak,
                     const DayData* grid, const GridMetadata& metadata,
                     int maxWeeks, int daysPerWeek);
    void displaySetupMessage(const String& instruction, const String& target, const String& blurb);

    // Debug/development display
    void displayAlignmentGuides();
    
private:
    Epd& epd;
    UBYTE *blackImage, *redImage;
    UBYTE *prevBlackImage;
    
    bool allocateBuffers();
    bool _needs_full_reinit_after_partial = false;
    void freeBuffers();
    void display();
    void displayPartial(UWORD yStart = 20);
    
    // Drawing utilities
    void drawBitmap(Paint& paint, const UBYTE* bitmap, int x, int y, 
                    int width, int height, int colour, bool drawBackground = true);
    int measureStringWidth(const char* text, sFONT* font);
    void drawStringWrapped(Paint& paint, int x, int y, const char* text, 
                          int maxWidth, sVARFONT* font, int lineSpacing, UWORD colour, int maxLines=10);
    
    // Section drawing
    void drawTitle(Paint& blackPaint, Paint& redPaint, const char* title, 
                  const char* subtitle, const UBYTE* icon, int iconW, int iconH, bool invertSubtitle);
    void drawRecallIndicator(Paint& blackPaint, Paint& redPaint, int x, int y, 
                            int streak, bool recallDone);
};

// Implementation
DisplayManager::DisplayManager(Epd& display) : epd(display), blackImage(nullptr), redImage(nullptr), prevBlackImage(nullptr) {}

bool DisplayManager::allocateBuffers() {
    UWORD imageSize = ((EPD_WIDTH % 8 == 0) ? (EPD_WIDTH / 8) : (EPD_WIDTH / 8 + 1)) * EPD_HEIGHT;
    
    blackImage = (UBYTE*)malloc(imageSize);
    redImage = (UBYTE*)malloc(imageSize);
    
    if (!blackImage || !redImage) {
        Serial.println("Memory allocation failed");
        return false;
    }
    return true;
}

void DisplayManager::freeBuffers() {
    free(blackImage);
    free(redImage);
    blackImage = nullptr;
    redImage = nullptr;
}

void DisplayManager::display() {
    unsigned long t0 = millis();

    epd.Reset();

    if (_needs_full_reinit_after_partial) {
        epd.ReinitFast();
        _needs_full_reinit_after_partial = false;
    }
    unsigned long tInit = millis();

    epd.Display_Fast(blackImage, redImage);
    unsigned long tDisplay = millis();

    epd.Sleep();

    Serial.printf("[display] Init:%lums Display:%lums Total:%lums\n",
                  tInit - t0, tDisplay - tInit, tDisplay - t0);
}

void DisplayManager::displayPartial(UWORD yStart) {
    unsigned long t0 = millis();

    epd.Reset();
    unsigned long tReset = millis();

    epd.LoadPartialLUT();
    unsigned long tLUT = millis();

    UWORD imageSize = EPD_WIDTH / 8 * EPD_HEIGHT;

    epd.SendCommand(0x26);
    if (prevBlackImage) {
        epd.SendDataBulk(prevBlackImage, imageSize);
    } else {
        for (UWORD i = 0; i < imageSize; i++) {
            epd.SendData(0xFF);
        }
    }

    epd.SendCommand(0x24);
    epd.SendDataBulk(blackImage, imageSize);
    unsigned long tData = millis();

    epd.TurnOnDisplay_Partial();
    unsigned long tRefresh = millis();

    _needs_full_reinit_after_partial = true;

    epd.Sleep();

    free(prevBlackImage);
    prevBlackImage = nullptr;

    Serial.printf("[partial] Reset:%lums LUT:%lums Data:%lums Refresh:%lums Total:%lums\n",
                  tReset - t0, tLUT - tReset, tData - tLUT, tRefresh - tData, tRefresh - t0);
}

void DisplayManager::drawBitmap(Paint& paint, const UBYTE* bitmap, int x, int y, 
                                int width, int height, int colour, bool drawBackground) {
    int byteWidth = (width + 7) / 8;
    int backgroundColour = (colour == BLACK) ? WHITE : BLACK;
    
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int byteIndex = row * byteWidth + (col / 8);
            int bitIndex = 7 - (col % 8);
            UBYTE byte = pgm_read_byte(&bitmap[byteIndex]);

            if (!(byte & (1 << bitIndex))) {
                paint.DrawPixel(x + col, y + row, colour);
            } else if (drawBackground) {
                paint.DrawPixel(x + col, y + row, backgroundColour);
            }
        }
    }
}

int DisplayManager::measureStringWidth(const char* text, sFONT* font) {
    return strlen(text) * font->Width;
}

void DisplayManager::drawStringWrapped(Paint& paint, int x, int y, const char* text, 
                                       int maxWidth, sVARFONT* font, int lineSpacing, UWORD colour, int maxLines) {
    String inputText = String(text);
    String currentLine = "";
    int currentNumLines = 0;
    int currentY = y;
    int wordStart = 0;
    
    for (int i = 0; i <= inputText.length(); i++) {
        if (currentNumLines >= maxLines) {
            break;
        }
        if (i == inputText.length() || inputText[i] == ' ') {
            String word = inputText.substring(wordStart, i);
            word.trim();
            
            String testLine = currentLine;
            if (testLine.length() > 0) testLine += " ";
            testLine += word;
            
            int lineWidth = MeasureVarString(testLine.c_str(), font, 1);
            
            if (lineWidth <= maxWidth || currentLine.length() == 0) {
                if (currentLine.length() > 0) currentLine += " ";
                currentLine += word;
            } else {
                currentLine.trim();
                if (currentNumLines == maxLines - 1) {
                    currentLine = currentLine.substring(0, currentLine.length()-3) + "...";
                }
                DrawVarString(x, currentY, currentLine.c_str(), font, colour, 1, true, paint);
                currentY += font->Height + lineSpacing;
                currentLine = word;
                currentNumLines += 1;
            }
            
            wordStart = i + 1;
        }
    }
    
    if (currentLine.length() > 0 && currentNumLines < maxLines) {
        currentLine.trim();
        DrawVarString(x, currentY, currentLine.c_str(), font, colour, 1, true, paint);
    }
}

void DisplayManager::drawTitle(Paint& blackPaint, Paint& redPaint, const char* title, 
                               const char* subtitle, const UBYTE* icon, int iconW, int iconH, 
                               bool invertSubtitle) {
    int titleWidth = MeasureVarString(title, &BoldPixels, 1);
    int subtitleWidth = MeasureVarString(subtitle, &BoldPixels, 1);
    
    // Title box
    blackPaint.DrawRectangle(
        DISPLAY_MARGIN, 
        TITLE_Y_POS - 2, 
        DISPLAY_MARGIN + iconW + SYMBOL_PADDING + titleWidth + TITLE_BOX_PADDING * 2, 
        TITLE_Y_POS + BoldPixels.Height - 2,
        BLACK
    );
    
    drawBitmap(blackPaint, icon, DISPLAY_MARGIN + TITLE_BOX_PADDING, TITLE_Y_POS + 2, iconW, iconH, BLACK);
    DrawVarString(DISPLAY_MARGIN + TITLE_BOX_PADDING + iconW + SYMBOL_PADDING, TITLE_Y_POS, 
                  title, &BoldPixels, BLACK, 1, true, blackPaint);
    
    // Subtitle box
    int subtitleX = DISPLAY_MARGIN + iconW + SYMBOL_PADDING + titleWidth + TITLE_BOX_PADDING * 2;
    blackPaint.DrawFilledRectangle(
        subtitleX, 
        TITLE_Y_POS - 2, 
        subtitleX + subtitleWidth + TITLE_BOX_PADDING * 2, 
        TITLE_Y_POS + BoldPixels.Height - 2,
        BLACK
    );
    
    DrawVarString(subtitleX + TITLE_BOX_PADDING, TITLE_Y_POS, subtitle, &BoldPixels, 
                  invertSubtitle ? WHITE : BLACK, 1, true, blackPaint);
}

void DisplayManager::drawRecallIndicator(Paint& blackPaint, Paint& redPaint, int x, int y, 
                                        int streak, bool recallDone) {
    if (!recallDone) {
        drawBitmap(redPaint, epd_bitmap_recall, x, y, 11, 15, BLACK);
        String streakText = String(streak);
        int streakX = x + 11 + 3;
        DrawVarString(streakX, y, streakText.c_str(), &BoldPixels, BLACK, 1, true, redPaint);
    } else {
        drawBitmap(blackPaint, epd_bitmap_recall, x, y, 11, 15, BLACK);
        String streakText = String(streak);
        int streakX = x + 11 + 3;
        DrawVarString(streakX, y, streakText.c_str(), &BoldPixels, BLACK, 1, true, blackPaint);
    }
}

void DisplayManager::displayWordOfDay(const WordData& word, int streak, bool recallDone) {
    if (!allocateBuffers()) return;
    
    Paint paint_black(blackImage, EPD_WIDTH, EPD_HEIGHT);
    paint_black.SetRotate(ROTATE_270);
    paint_black.Clear(WHITE);

    Paint paint_red(redImage, EPD_WIDTH, EPD_HEIGHT);
    paint_red.SetRotate(ROTATE_270);
    paint_red.Clear(WHITE);

    // Title and date
    String formattedDate = RSSParser::formatDateFromTimestamp(word.date);
    int wotdSymbolWidth = 11;
    int titleWidth = MeasureVarString("WORD OF THE DAY", &BoldPixels, 1);
    int dateWidth = MeasureVarString(formattedDate.c_str(), &BoldPixels, 1);
    
    paint_black.DrawRectangle(
        DISPLAY_MARGIN, TITLE_Y_POS - 2, 
        DISPLAY_MARGIN + wotdSymbolWidth + SYMBOL_PADDING + titleWidth + TITLE_BOX_PADDING * 2, 
        TITLE_Y_POS + BoldPixels.Height - 2, BLACK
    );
    drawBitmap(paint_black, epd_bitmap_wotd_symbol, DISPLAY_MARGIN + TITLE_BOX_PADDING, TITLE_Y_POS + 2, 11, 15, BLACK);
    DrawVarString(DISPLAY_MARGIN + TITLE_BOX_PADDING + wotdSymbolWidth + SYMBOL_PADDING, TITLE_Y_POS, 
                  "WORD OF THE DAY", &BoldPixels, BLACK, 1, true, paint_black);
    
    // Date box
    paint_black.DrawFilledRectangle(
        DISPLAY_MARGIN + wotdSymbolWidth + SYMBOL_PADDING + titleWidth + TITLE_BOX_PADDING * 2, 
        TITLE_Y_POS - 2, 
        DISPLAY_MARGIN + wotdSymbolWidth + SYMBOL_PADDING + titleWidth + dateWidth + TITLE_BOX_PADDING * 4, 
        TITLE_Y_POS + BoldPixels.Height - 2, BLACK
    );
    DrawVarString(DISPLAY_MARGIN + wotdSymbolWidth + SYMBOL_PADDING + titleWidth + TITLE_BOX_PADDING * 3, 
                  TITLE_Y_POS, formattedDate.c_str(), &BoldPixels, WHITE, 1, true, paint_black);
    
    // Recall indicator
    int recallSymbolX = DISPLAY_MARGIN + wotdSymbolWidth + SYMBOL_PADDING + titleWidth + TITLE_BOX_PADDING * 4 + dateWidth + 6;
    drawRecallIndicator(paint_black, paint_red, recallSymbolX, TITLE_Y_POS, streak, recallDone);
    
    // Word
    String sWord = word.word;
    sWord.toUpperCase();
    paint_red.DrawStringAt(DISPLAY_MARGIN, WORD_Y_POS, sWord.c_str(), &Font20, BLACK);

    // Part of Speech
    int wordWidth = measureStringWidth(sWord.c_str(), &Font20);
    int posX = DISPLAY_MARGIN + 1 + wordWidth + WORD_SPACING;
    int posY = WORD_Y_POS + Font20.Height - Born2bSporty.Height - WORD_SPACING;
    DrawVarString(posX, posY, word.partOfSpeech.c_str(), &Born2bSporty, BLACK, 1, true, paint_black);
    
    // Phonetic box
    int phoTextWidth = MeasureVarString(word.phonetic.c_str(), &Born2bSporty, 1);
    int phoTextHeight = Born2bSporty.Height - 4;
    int phoX = DISPLAY_MARGIN + 1 + PHONETIC_PADDING;
    
    paint_black.DrawFilledRectangle(
        phoX - PHONETIC_PADDING, PHONETIC_Y_POS - PHONETIC_PADDING + 1, 
        phoX + phoTextWidth + PHONETIC_PADDING * 2 + PHONETIC_SYMBOL_WIDTH, 
        PHONETIC_Y_POS + phoTextHeight + PHONETIC_PADDING, BLACK
    );

    // Phonetic symbol
    const UBYTE phonoSymbolBitMap[] = {
        0xf7, 0xe0, 0xf9, 0xe0, 0xfc, 0x60, 0xef, 0x60, 0xf3, 0x20, 0x39, 0x80, 
        0x99, 0x80, 0x99, 0x80, 0x39, 0x80, 0xf3, 0x20, 0xef, 0x60, 0xfc, 0x60, 
        0xf9, 0xe0, 0xf7, 0xe0
    };
    drawBitmap(paint_black, phonoSymbolBitMap, phoX, PHONETIC_Y_POS, PHONETIC_SYMBOL_WIDTH, PHONETIC_SYMBOL_HEIGHT, WHITE);
    DrawVarString(phoX + PHONETIC_SYMBOL_WIDTH + PHONETIC_PADDING + 1, PHONETIC_Y_POS - 1, 
                  word.phonetic.c_str(), &Born2bSporty, WHITE, 1, true, paint_black);
    
    // Definition
    drawStringWrapped(paint_black, DISPLAY_MARGIN + 1, DEFINITION_Y_POS, word.definition.c_str(), 
                     WRAPPED_TEXT_WIDTH, &OpenSansPX, LINE_SPACING, BLACK);
    
    display();
    freeBuffers();
}

void DisplayManager::displayRecall(const WordData& word, int daysAgo, bool showAnswer) {
    if (!allocateBuffers()) return;
    
    Paint paint_black(blackImage, EPD_WIDTH, EPD_HEIGHT);
    paint_black.SetRotate(ROTATE_270);
    paint_black.Clear(WHITE);

    Paint paint_red(redImage, EPD_WIDTH, EPD_HEIGHT);
    paint_red.SetRotate(ROTATE_270);
    paint_red.Clear(WHITE);
    
    // Title based on mode
    String promptText = (daysAgo == 1) ? "YESTERDAY'S WORD" : "THE WORD " + String(daysAgo) + " DAYS AGO";
    drawTitle(paint_black, paint_red, "RECALL", promptText.c_str(), epd_bitmap_recall, 11, 15, true);

    // Recall word display
    int recallWordXEnd = 0;
    if (!showAnswer) {
        String firstLetter = word.word.substring(0, 1);
        firstLetter.toUpperCase();
        paint_red.DrawStringAt(DISPLAY_MARGIN, RECALL_WORD_Y, firstLetter.c_str(), &Font20, BLACK);

        int showI = random(1, word.word.length());
        int blankWidth = Font20.Width - 4;
        int boxX = DISPLAY_MARGIN + Font20.Width + 2;
        
        for (int i = 1; i < word.word.length(); i++) {
            if ((i == showI) && (word.word.length() > 3) || (word.word.substring(i, i+1) == " ")) {
                String letter = word.word.substring(i, i + 1);
                letter.toUpperCase();
                paint_red.DrawStringAt(boxX-2, RECALL_WORD_Y, letter.c_str(), &Font20, BLACK);
                int letterW = measureStringWidth(letter.c_str(), &Font20);
                boxX += letterW;
            } else {
                String letter = word.word.substring(i, i + 1);
                int letterW = measureStringWidth(letter.c_str(), &Font20);
                paint_black.DrawFilledRectangle(boxX, RECALL_WORD_Y, boxX + blankWidth, 
                                               RECALL_WORD_Y + BLANK_CHAR_HEIGHT, BLACK);
                boxX += letterW;
            }
        }
        recallWordXEnd = boxX;
    } else {
        String wordUpper = word.word;
        wordUpper.toUpperCase();
        paint_black.DrawStringAt(DISPLAY_MARGIN, RECALL_WORD_Y, wordUpper.c_str(), &Font20, BLACK);
        recallWordXEnd = DISPLAY_MARGIN + measureStringWidth(wordUpper.c_str(), &Font20);
    }
        
    // Part of Speech
    // TODO Is it even needed to check overflow for the part of speech on 2.9 inch display?
    int examplePosY = 20;
    int posX = recallWordXEnd + WORD_SPACING;
    if (posX + MeasureVarString(word.partOfSpeech.c_str(), &Born2bSporty, 1) < RECALL_MAX_WIDTH) {
        int posY = RECALL_WORD_Y + Font20.Height - Born2bSporty.Height - WORD_SPACING;
        DrawVarString(posX, posY, word.partOfSpeech.c_str(), &Born2bSporty, BLACK, 1, true, paint_black);
        examplePosY += posY - 4;
    } else {
        DrawVarString(DISPLAY_MARGIN + 1, RECALL_POS_OFFSET, word.partOfSpeech.c_str(), 
                     &Born2bSporty, BLACK, 1, true, paint_black);
        examplePosY += 50;
    }

    // Example or Definition
    int maxLines = showAnswer ? 6 : 4;
    const char* textToShow = showAnswer ? word.definition.c_str() : word.example.c_str();
    drawStringWrapped(paint_black, DISPLAY_MARGIN + 1, examplePosY, textToShow, 
                     WRAPPED_TEXT_WIDTH, &OpenSansPX, LINE_SPACING, BLACK, maxLines);
    
    if (!showAnswer) {
        int RECALL_INSTRUCTIONS_Y = EPD_WIDTH - 24;
        // "GOT IT" / "PRESS" and "FORGOT IT" / "HOLD" titles
        int gotItWidth = MeasureVarString("GOT IT", &BoldPixels, 1);
        int pressWidth = MeasureVarString("PRESS", &BoldPixels, 1);
        
        paint_black.DrawRectangle(DISPLAY_MARGIN, RECALL_INSTRUCTIONS_Y - 2, 
                                 DISPLAY_MARGIN + gotItWidth + TITLE_BOX_PADDING * 2, 
                                 RECALL_INSTRUCTIONS_Y + BoldPixels.Height - 2, BLACK);
        DrawVarString(DISPLAY_MARGIN + TITLE_BOX_PADDING, RECALL_INSTRUCTIONS_Y, "GOT IT", &BoldPixels, BLACK, 1, true, paint_black);
        
        paint_black.DrawFilledRectangle(DISPLAY_MARGIN + gotItWidth + TITLE_BOX_PADDING * 2, RECALL_INSTRUCTIONS_Y - 2, 
                                       DISPLAY_MARGIN + gotItWidth + pressWidth + TITLE_BOX_PADDING * 4, 
                                       RECALL_INSTRUCTIONS_Y + BoldPixels.Height - 2, BLACK);
        DrawVarString(DISPLAY_MARGIN + gotItWidth + TITLE_BOX_PADDING * 3, RECALL_INSTRUCTIONS_Y, "PRESS", 
                     &BoldPixels, WHITE, 1, true, paint_black);
        
        int forgotItWidth = MeasureVarString("FORGOT IT", &BoldPixels, 1);
        int holdWidth = MeasureVarString("HOLD", &BoldPixels, 1);
        int startX = EPD_HEIGHT - DISPLAY_MARGIN - forgotItWidth - holdWidth - (TITLE_BOX_PADDING * 4) - 3;
        
        paint_black.DrawRectangle(startX, RECALL_INSTRUCTIONS_Y - 2, 
                                 startX + forgotItWidth + TITLE_BOX_PADDING * 2, 
                                 RECALL_INSTRUCTIONS_Y + BoldPixels.Height - 2, BLACK);
        DrawVarString(startX + TITLE_BOX_PADDING, RECALL_INSTRUCTIONS_Y, "FORGOT IT", &BoldPixels, BLACK, 1, true, paint_black);
        
        paint_black.DrawFilledRectangle(startX + forgotItWidth + TITLE_BOX_PADDING * 2, RECALL_INSTRUCTIONS_Y - 2, 
                                       startX + forgotItWidth + holdWidth + TITLE_BOX_PADDING * 4, 
                                       RECALL_INSTRUCTIONS_Y + BoldPixels.Height - 2, BLACK);
        DrawVarString(startX + forgotItWidth + TITLE_BOX_PADDING * 3, RECALL_INSTRUCTIONS_Y, "HOLD", 
                     &BoldPixels, WHITE, 1, true, paint_black);

        // Save black frame for partial refresh diff
        UWORD imageSize = ((EPD_WIDTH % 8 == 0) ? (EPD_WIDTH / 8) : (EPD_WIDTH / 8 + 1)) * EPD_HEIGHT;
        free(prevBlackImage);
        prevBlackImage = (UBYTE*)malloc(imageSize);
        if (prevBlackImage) memcpy(prevBlackImage, blackImage, imageSize);
        display();
    } else {
        displayPartial(EPD_WIDTH / 2);
    }

    freeBuffers();
}

void DisplayManager::displayStats(int totalCorrect, int totalIncorrect, int currentStreak,
                                  const DayData* grid, const GridMetadata& metadata, 
                                  int maxWeeks, int daysPerWeek) {
    if (!allocateBuffers()) return;
    
    Paint paint_black(blackImage, EPD_WIDTH, EPD_HEIGHT);
    paint_black.SetRotate(ROTATE_270);
    paint_black.Clear(WHITE);

    Paint paint_red(redImage, EPD_WIDTH, EPD_HEIGHT);
    paint_red.SetRotate(ROTATE_270);
    paint_red.Clear(WHITE);
    
    // Title
    int recapWidth = MeasureVarString("RECAP", &BoldPixels, 1);
    int recapSymbolWidth = 11;
    paint_black.DrawRectangle(DISPLAY_MARGIN, STATS_TITLE_Y - 2, 
                             DISPLAY_MARGIN + recapSymbolWidth + SYMBOL_PADDING + recapWidth + TITLE_BOX_PADDING * 2, 
                             STATS_TITLE_Y + BoldPixels.Height - 2, BLACK);
    drawBitmap(paint_black, epd_bitmap_recap_symbol, DISPLAY_MARGIN + TITLE_BOX_PADDING, STATS_TITLE_Y + 1, 11, 15, BLACK);
    DrawVarString(DISPLAY_MARGIN + TITLE_BOX_PADDING + recapSymbolWidth + SYMBOL_PADDING, STATS_TITLE_Y, 
                  "RECAP", &BoldPixels, BLACK, 1, true, paint_black);
    
    // Grid constants
    const int ICON_SIZE = 9;
    const int ICON_PADDING_X = 1.5;
    const int ICON_PADDING_Y = 1;
    const int ICON_TOTAL_X = ICON_SIZE + (ICON_PADDING_X * 2);
    const int ICON_TOTAL_Y = ICON_SIZE + (ICON_PADDING_Y * 2) + 1;
    const int GRID_START_X = DISPLAY_MARGIN + 14;
    const int GRID_START_Y = 43;
    
    // Month labels
    const int MONTH_LABEL_Y = GRID_START_Y - 14;
    for (int i = 0; i < metadata.monthCount; i++) {
        bool addPipe = true;
        bool addLabel = true;

        int monthLabelWidth = MeasureVarString(metadata.monthLabels[i].c_str(), &Born2bSporty, 1);
        int x = GRID_START_X + (metadata.monthStartColumns[i] * ICON_TOTAL_X);

        if (i == metadata.monthCount - 1) {
            int remainingDistance = GRID_START_X + (maxWeeks * ICON_TOTAL_X) - x;
            if (remainingDistance < monthLabelWidth) addLabel = false;
        }

        if (metadata.partialMonth[i] && i == 0) addPipe = false;

        if (addPipe) drawBitmap(paint_black, epd_bitmap_pipe_symbol, x, MONTH_LABEL_Y, 5, 12, BLACK);
        if (addLabel) DrawVarString(x + 5, MONTH_LABEL_Y, metadata.monthLabels[i].c_str(), 
                                   &Born2bSporty, BLACK, 1, true, paint_black);
    }

    // Draw grid
    for (int week = 0; week < maxWeeks; week++) {
        for (int day = 0; day < daysPerWeek; day++) {
            int x = GRID_START_X + (week * ICON_TOTAL_X) + ICON_PADDING_X;
            int y = GRID_START_Y + (day * ICON_TOTAL_Y) + ICON_PADDING_Y;
            
            int idx = week * daysPerWeek + day;
            const DayData& data = grid[idx];
            
            if (data.isFuture) continue;
            
            if (!data.hasData) {
                drawBitmap(paint_black, epd_bitmap_cross, x, y, ICON_SIZE, ICON_SIZE, BLACK);
            } else {
                if (data.incorrectRatio <= 0.01) {
                    drawBitmap(paint_black, epd_bitmap_black_100, x, y, ICON_SIZE, ICON_SIZE, BLACK, false);
                } else if (data.incorrectRatio <= 0.25) {
                    drawBitmap(paint_red, epd_bitmap_red_25, x, y, ICON_SIZE, ICON_SIZE, BLACK, false);
                    drawBitmap(paint_black, epd_bitmap_black_75, x, y, ICON_SIZE, ICON_SIZE, BLACK, false);
                } else if (data.incorrectRatio <= 0.5) {
                    drawBitmap(paint_red, epd_bitmap_red_50, x, y, ICON_SIZE, ICON_SIZE, BLACK, false);
                    drawBitmap(paint_black, epd_bitmap_black_50, x, y, ICON_SIZE, ICON_SIZE, BLACK, false);
                } else if (data.incorrectRatio <= 0.75) {
                    drawBitmap(paint_red, epd_bitmap_red_75, x, y, ICON_SIZE, ICON_SIZE, BLACK, false);
                    drawBitmap(paint_black, epd_bitmap_black_25, x, y, ICON_SIZE, ICON_SIZE, BLACK, false);
                } else {
                    drawBitmap(paint_red, epd_bitmap_red_100, x, y, ICON_SIZE, ICON_SIZE, BLACK, false);
                }
            }
        }
    }
    
    // Day labels
    const char* dayLabels[] = {"M", "T", "W", "T", "F", "S", "S"};
    for (int d = 0; d < daysPerWeek; d++) {
        int y = GRID_START_Y + (d * ICON_TOTAL_Y) + ICON_PADDING_Y - 4;
        DrawVarString(DISPLAY_MARGIN, y, dayLabels[d], &Born2bSporty, BLACK, 1, true, paint_black);
    }

    // Overall stats
    int total = totalCorrect + totalIncorrect;
    if (total > 0) {
        int overallPercent = (totalCorrect * 100) / total;
        String countText = String(total);
        String percentText = String(overallPercent) + "%";
        const char* sep = "   ";

        int wCount = MeasureVarString(countText.c_str(), &BoldPixels, 1);
        int wSep = MeasureVarString(sep, &BoldPixels, 1);
        int wPct = MeasureVarString(percentText.c_str(), &BoldPixels, 1);

        const int HASH_W = 10, HASH_H = 10, TICK_W = 11, TICK_H = 10, PAD = 2;
        int blockW = (HASH_W + PAD) + wCount + wSep + (TICK_W + PAD) + wPct;

        int x = DISPLAY_MARGIN + recapSymbolWidth + SYMBOL_PADDING + recapWidth + TITLE_BOX_PADDING * 3;
        
        paint_black.DrawFilledRectangle(
            DISPLAY_MARGIN + recapSymbolWidth + SYMBOL_PADDING + recapWidth + TITLE_BOX_PADDING * 2, 
            STATS_TITLE_Y - 2, 
            DISPLAY_MARGIN + recapSymbolWidth + SYMBOL_PADDING + recapWidth + blockW + TITLE_BOX_PADDING * 4, 
            STATS_TITLE_Y + BoldPixels.Height - 2, BLACK
        );
        
        drawBitmap(paint_black, epd_bitmap_hash, x, STATS_TITLE_Y + 4, HASH_W, HASH_H, WHITE);
        x += HASH_W + PAD;
        DrawVarString(x, STATS_TITLE_Y, countText.c_str(), &BoldPixels, WHITE, 1, true, paint_black);
        x += wCount;
        DrawVarString(x, STATS_TITLE_Y, sep, &BoldPixels, WHITE, 1, true, paint_black);
        x += wSep;
        drawBitmap(paint_black, epd_bitmap_tick, x, STATS_TITLE_Y + 2, TICK_W, TICK_H, WHITE);
        x += TICK_W + PAD;
        DrawVarString(x, STATS_TITLE_Y, percentText.c_str(), &BoldPixels, WHITE, 1, true, paint_black);
    }

    // Legend
    int legendTextW = MeasureVarString(" FORGOT IT", &OpenSansPX, 1);
    int legendIconW = 9;
    int leftPadText = 10;
    int keyX = GRID_START_X + (ICON_TOTAL_X * maxWeeks) - legendTextW - legendIconW - leftPadText + 3;
    int keyY = STATS_TITLE_Y - 1;

    drawBitmap(paint_black, epd_bitmap_black_100, keyX, keyY, legendIconW, legendIconW, BLACK);
    DrawVarString(keyX + leftPadText, keyY - 4, " GOT IT", &OpenSansPX, BLACK, 1, true, paint_black);

    drawBitmap(paint_red, epd_bitmap_red_100, keyX, keyY + 11, legendIconW, legendIconW, BLACK);
    DrawVarString(keyX + leftPadText, keyY + 7, " FORGOT IT", &OpenSansPX, BLACK, 1, true, paint_black);

    display();
    freeBuffers();
}

void DisplayManager::displaySetupMessage(const String& instruction, const String& target, const String& blurb) {
    if (!allocateBuffers()) return;

    Paint paint_black(blackImage, EPD_WIDTH, EPD_HEIGHT);
    Paint paint_red(redImage, EPD_WIDTH, EPD_HEIGHT);
    paint_black.SetRotate(ROTATE_270);
    paint_red.SetRotate(ROTATE_270);
    paint_black.Clear(WHITE);
    paint_red.Clear(WHITE);

    // Title: WORD BOX | SETTINGS
    int wotdSymbolWidth = 11;
    int titleWidth = MeasureVarString("WORD BOX", &BoldPixels, 1);
    int settingsWidth = MeasureVarString("SETTINGS", &BoldPixels, 1);

    paint_black.DrawRectangle(
        DISPLAY_MARGIN, TITLE_Y_POS - 2,
        DISPLAY_MARGIN + wotdSymbolWidth + SYMBOL_PADDING + titleWidth + TITLE_BOX_PADDING * 2,
        TITLE_Y_POS + BoldPixels.Height - 2, BLACK
    );
    drawBitmap(paint_black, epd_bitmap_settings_symbol, DISPLAY_MARGIN + TITLE_BOX_PADDING, TITLE_Y_POS + 1, 11, 15, BLACK);
    DrawVarString(DISPLAY_MARGIN + TITLE_BOX_PADDING + wotdSymbolWidth + SYMBOL_PADDING, TITLE_Y_POS,
                  "WORD BOX", &BoldPixels, BLACK, 1, true, paint_black);

    // Settings box (inverted)
    paint_black.DrawFilledRectangle(
        DISPLAY_MARGIN + wotdSymbolWidth + SYMBOL_PADDING + titleWidth + TITLE_BOX_PADDING * 2,
        TITLE_Y_POS - 2,
        DISPLAY_MARGIN + wotdSymbolWidth + SYMBOL_PADDING + titleWidth + settingsWidth + TITLE_BOX_PADDING * 4,
        TITLE_Y_POS + BoldPixels.Height - 2, BLACK
    );
    DrawVarString(DISPLAY_MARGIN + wotdSymbolWidth + SYMBOL_PADDING + titleWidth + TITLE_BOX_PADDING * 3,
                  TITLE_Y_POS, "SETTINGS", &BoldPixels, WHITE, 1, true, paint_black);

    // Instruction line (e.g., "CONNECT TO" or "VISIT")
    String instrUpper = instruction;
    instrUpper.toUpperCase();
    int instrWidth = MeasureVarString(instrUpper.c_str(), &BoldPixels, 1);
    DrawVarString(DISPLAY_MARGIN + 3, WORD_Y_POS+2, instrUpper.c_str(), &BoldPixels, BLACK, 1, true, paint_black);

    // Target (network name or IP) in red on next line
    String targetUpper = target;
    targetUpper.toUpperCase();
    DrawVarString(DISPLAY_MARGIN + instrWidth + 3, WORD_Y_POS + 2, targetUpper.c_str(), &BoldPixels, BLACK, 1, true, paint_red);

    // Blurb (like the definition)
    drawStringWrapped(paint_black, DISPLAY_MARGIN + 1, DEFINITION_Y_POS - 13, blurb.c_str(),WRAPPED_TEXT_WIDTH, &OpenSansPX, LINE_SPACING, BLACK);

    
    DrawVarString(DISPLAY_MARGIN + 1, DEFINITION_Y_POS + 25, "PRESS TO RESTART WORD BOX", &BoldPixels, BLACK, 1, true, paint_black);

    display();
    freeBuffers();
}

void DisplayManager::displayAlignmentGuides() {
    if (!allocateBuffers()) return;

    Paint paint_black(blackImage, EPD_WIDTH, EPD_HEIGHT);
    Paint paint_red(redImage, EPD_WIDTH, EPD_HEIGHT);
    paint_black.SetRotate(ROTATE_270);
    paint_red.SetRotate(ROTATE_270);
    paint_black.Clear(WHITE);
    paint_red.Clear(WHITE);

    // Display dimensions after rotation: width=EPD_HEIGHT(296), height=EPD_WIDTH(128)
    int dispWidth = EPD_HEIGHT;
    int dispHeight = EPD_WIDTH;

    // Edge lines (black) - outer boundary
    paint_black.DrawRectangle(0, 0, dispWidth - 1, dispHeight - 1, BLACK);

    // Margin lines (red) - safe area
    int marginLeft = DISPLAY_MARGIN;
    int marginRight = dispWidth - DISPLAY_MARGIN;
    int marginTop = DISPLAY_MARGIN;
    int marginBottom = dispHeight - DISPLAY_MARGIN;
    paint_red.DrawRectangle(marginLeft, marginTop, marginRight, marginBottom, BLACK);

    // Center lines (black dashed)
    int centerX = dispWidth / 2;
    int centerY = dispHeight / 2;

    // Vertical center line
    for (int y = 0; y < dispHeight; y += 4) {
        paint_black.DrawPixel(centerX, y, BLACK);
        paint_black.DrawPixel(centerX, y + 1, BLACK);
    }

    // Horizontal center line
    for (int x = 0; x < dispWidth; x += 4) {
        paint_black.DrawPixel(x, centerY, BLACK);
        paint_black.DrawPixel(x + 1, centerY, BLACK);
    }

    // Corner marks (red) - 10px L-shaped marks at margin corners
    // int markLen = 10;
    // Top-left
    // paint_red.DrawHorizontalLine(marginLeft, marginTop, markLen, BLACK);
    // paint_red.DrawVerticalLine(marginLeft, marginTop, markLen, BLACK);
    // // Top-right
    // paint_red.DrawHorizontalLine(marginRight - markLen, marginTop, markLen, BLACK);
    // paint_red.DrawVerticalLine(marginRight, marginTop, markLen, BLACK);
    // // Bottom-left
    // paint_red.DrawHorizontalLine(marginLeft, marginBottom, markLen, BLACK);
    // paint_red.DrawVerticalLine(marginLeft, marginBottom - markLen, markLen, BLACK);
    // // Bottom-right
    // paint_red.DrawHorizontalLine(marginRight - markLen, marginBottom, markLen, BLACK);
    // paint_red.DrawVerticalLine(marginRight, marginBottom - markLen, markLen, BLACK);

    // Dimension labels
    String widthLabel = String(dispWidth) + "px";
    String heightLabel = String(dispHeight) + "px";
    String marginLabel = "M:" + String(DISPLAY_MARGIN);

    DrawVarString(centerX - 15, 15, widthLabel.c_str(), &Born2bSporty, BLACK, 1, true, paint_black);
    DrawVarString(15, centerY - 5, heightLabel.c_str(), &Born2bSporty, BLACK, 1, true, paint_black);
    DrawVarString(marginLeft + 2, marginTop + 2, marginLabel.c_str(), &Born2bSporty, BLACK, 1, true, paint_red);

    display();
    freeBuffers();
}