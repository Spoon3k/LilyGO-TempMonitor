#pragma once

// Wifi
#define WIFI_SSID "MatrixNet"
#define WIFI_PASSWORD "Nelinka1"

// FTP server
char FTP_HOST[] = "192.168.1.100";
char FTP_USER[]   = "MatrixFTPUser";
char FTP_PASSWORD[]   = "zPwllJ358dY5";

//Proměnná pro restart
volatile bool restartFlag = false; // Globální příznak pro restart

// SD card
#define USE_SD
#define FILE_SYSTEM SD

// WiFi
#define MAX_WIFI_RETRIES 5
#define WIFI_RETRY_DELAY_MS 500
#define FILE_BUFFER_SIZE 512

// Error log
#define ERROR_LOG_FILE "/error_log.txt"
#define DBG_OUTPUT_PORT Serial

// Display
#define SCREEN_WIDTH   EPD_WIDTH
#define SCREEN_HEIGHT  EPD_HEIGHT

// Intervals
#define JSON_WRITE_INTERVAL_SEC 60          // Interval pro zápis do JSON souborů ve vteřinách (1 minuta)
#define TIME_UPDATE_INTERVAL_HOURS 24       // Interval pro aktualizaci času v hodinách (1 den)
#define FTP_TRANSFER_INTERVAL_MIN 10        // Interval pro přenos souborů na FTP server v minutách (10 minut)

#define DATA_DIR "/data"
#define CHART_DATA_DIR "/chartData"

// Debug
#define DEBUG 1                             // Změňte na 0 pro vypnutí ladicích výpisů
#if DEBUG
  #define DEBUG_PRINT(msg) Serial.println(String(msg))
#else
  #define DEBUG_PRINT(msg)  // Nic nedělá
#endif

/*
// openweathermap.org

// Use your own API key by signing up for a free developer account at https://openweathermap.org/
String apikey       = "1e77c64868da52a1c6d7e53feee4d71b";                      // See: https://openweathermap.org/
const char server[] = "api.openweathermap.org";
//http://api.openweathermap.org/data/2.5/forecast?q=Melksham,UK&APPID=your_OWM_API_key&mode=json&units=metric&cnt=40
//http://api.openweathermap.org/data/2.5/weather?q=Melksham,UK&APPID=your_OWM_API_key&mode=json&units=metric&cnt=1

//Set your location according to OWM locations
String City             = "Stechovice";                    // Your home city See: http://bulk.openweathermap.org/sample/
String Country          = "CZ";                            // Your _ISO-3166-1_two-letter_country_code country code, on OWM find your nearest city and the country code is displayed
                                                           // https://en.wikipedia.org/wiki/List_of_ISO_3166_country_codes
String Language         = "EN";                            // NOTE: Only the weather description is translated by OWM
                                                           // Examples: Arabic (AR) Czech (CZ) English (EN) Greek (EL) Persian(Farsi) (FA) Galician (GL) Hungarian (HU) Japanese (JA)
                                                           // Korean (KR) Latvian (LA) Lithuanian (LT) Macedonian (MK) Slovak (SK) Slovenian (SL) Vietnamese (VI)
String Hemisphere       = "north";                         // or "south"  
String Units            = "M";                             // Use 'M' for Metric or I for Imperial 
*/

// NTP
const char* Timezone    = "CET-1CEST,M3.5.0,M10.5.0/3";  // Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv 
                                                           // See below for examples
const char* ntpServer   = "0.cz.pool.ntp.org";             // Or, choose a time server close to you, but in most cases it's best to use pool.ntp.org to find an NTP server
                                                           // then the NTP system decides e.g. 0.pool.ntp.org, 1.pool.ntp.org as the NTP syem tries to find  the closest available servers
                                                           // EU "0.europe.pool.ntp.org"
                                                           // US "0.north-america.pool.ntp.org"
                                                           // See: https://www.ntppool.org/en/                                                           
int   gmtOffset_sec     = 3600;    // UK normal time is GMT, so GMT Offset is 0, for US (-5Hrs) is typically -18000, AU is typically (+8hrs) 28800
int  daylightOffset_sec = 3600; // In the UK DST is +1hr or 3600-secs, other countries may use 2hrs 7200 or 30-mins 1800 or 5.5hrs 19800 Ahead of GMT use + offset behind - offset
/* Example time zones
const char* Timezone = "MET-1METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
const char* Timezone = "CET-1CEST,M3.5.0,M10.5.0/3";       // Central Europe
const char* Timezone = "EST-2METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
const char* Timezone = "EST5EDT,M3.2.0,M11.1.0";           // EST USA  
const char* Timezone = "CST6CDT,M3.2.0,M11.1.0";           // CST USA
const char* Timezone = "MST7MDT,M4.1.0,M10.5.0";           // MST USA
const char* Timezone = "NZST-12NZDT,M9.5.0,M4.1.0/3";      // Auckland
const char* Timezone = "EET-2EEST,M3.5.5/0,M10.5.5/0";     // Asia
const char* Timezone = "ACST-9:30ACDT,M10.1.0,M4.1.0/3":   // Australia
*/


// EPD47/ESP32 PIN settings
#if defined(CONFIG_IDF_TARGET_ESP32)

#define BUTTON_1  (34)
#define BUTTON_2  (35)
#define BUTTON_3  (39)

#define BATT_PIN  (36)

#define SD_MISO   (12)
#define SD_MOSI   (13)
#define SD_SCLK   (14)
#define SD_CS     (15)

#define BOARD_SCL (14)
#define BOARD_SDA (15)
#define TOUCH_INT (13)

#define GPIO_MISO (12)
#define GPIO_MOSI (13)
#define GPIO_SCLK (14)
#define GPIO_CS   (15)

#elif defined(CONFIG_IDF_TARGET_ESP32S3)

#define BUTTON_1   (21)

#define BATT_PIN   (14)

#define SD_MISO    (16)
#define SD_MOSI    (15)
#define SD_SCLK    (11)
#define SD_CS      (42)

#define BOARD_SCL  (17)
#define BOARD_SDA  (18)
#define TOUCH_INT  (47)

#define GPIO_MISO  (45)
#define GPIO_MOSI  (10)
#define GPIO_SCLK  (48)
#define GPIO_CS    (39)

#endif