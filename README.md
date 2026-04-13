# Word Box - Word of the Day

An ESP-32 / e-Paper display project for a word-of-the-day desktop device with daily words, recall testing and tracking.

<div style="display: flex; gap: 16px;">
<figure style="margin: 0;">
<img src="https://github.com/hyrographic/word_of_the_day_public/blob/main/images/WOTD%201.JPEG?raw=true">
<figcaption>Word-of-the-day screen</figcaption>
</figure>

<figure style="margin: 0;">
<img src="https://github.com/hyrographic/word_of_the_day_public/blob/main/images/WOTD%202.JPEG?raw=true">
<figcaption>Previous word recall</figcaption>
</figure>

<figure style="margin: 0;">
<img src="https://github.com/hyrographic/word_of_the_day_public/blob/main/images/WOTD%204.JPEG?raw=true">
<figcaption>Recall stats</figcaption>
</figure>
</div>

---

## Features

- **Word of the Day** — fetches daily words from the Merriam-Webster RSS feed
- **Recall mode** — Tests your memory of past words
- **Stats view** — GitHub contribution style grid showing historic recall performance and streaks
- **Recall Prompting** — LED pulses as a reminder when recall hasn't been done that day
---

## Hardware

| Component | Details |
|---|---|
| Microcontroller | ESP-32 (I used a <a href="https://www.espboards.dev/esp32/esp32-lite-v1/">ESP32 Lite V1.0.0</a>)|
| Display | <a href="https://www.waveshare.com/wiki/2.9inch_e-Paper_Module">Waveshare 2.9 inch B/W/R e-paper (epd2in9b V4)</a> |
| Button | Tactile push button |
| LED | Standard 5mm LED |
| Enclosure | 3D-printed (see `3d_printing/`) |

---

## Project Structure

```
├── include/          # Header files
├── src/              # Source files
├── lib/EPD/          # Waveshare e-Paper driver
├── lib/fonts/        # Bitmap font data
├── html/             # WiFi setup web UI and SVG assets
├── data/             # LittleFS filesystem root (recall_history.jsonl)
├── 3d_printing/      # Print files and settings for case
└── platformio.ini    # PlatformIO build config
```

---

## Getting Started

### Requirements

- [PlatformIO](https://platformio.org/) (VS Code)
- ESP-32
- Waveshare 2.9" B/W/R e-paper display

### Build & Flash

```bash
# Build
pio run

# Upload firmware
pio run --target upload

# Upload filesystem (recall history file)
pio run --target uploadfs

# Monitor serial output
pio device monitor
```

### First-Time Setup

1. Power on the device — it will broadcast a WiFi network called **`Word Box Setup`**
2. Connect to that network from your phone or laptop
3. Navigate to the setup page and enter your home WiFi credentials
4. The device will connect, sync time via NTP, fetch the word of the day, and go to sleep until the next morning

---

## Using the Device

| Mode | How to trigger |
|---|---|
| Word-of-the-Day | Default on wake; refreshes each day at 05:15 |
| Recall | Short button press from WOTD/home screen |
| Answer reveal | Short or long button press to log if you GOT IT or FORGOT IT|
| Stats grid | Long button press from WOTD/home screen |
| Settings | 5 second button press from WOTD/home screen |

---

## 3D Printing

Print files (.3MF) are in [`3d_printing/`](3d_printing/). I also used a .json log to track my slicing settings as I perfected the print, which I've included in the same directory.

I printed with Elegoo PLA Pro+ (Grey), Bambu PLA Red, & Bambu PLA Black.

---

## License

See [LICENSE](LICENSE).
