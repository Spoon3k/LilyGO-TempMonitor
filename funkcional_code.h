#include "settings.h"           // Basic settings, SSIDs, passwords, server addresses, https://openweathermap.org/ API key and credentials

#include <WiFi.h>               // Wifi library

#include <Arduino.h>            // In-built

#include <esp_task_wdt.h>       // Task paralellization
#include "freertos/FreeRTOS.h"  // Task paralellization
#include "freertos/task.h"      // Task paralellization
#include "epd_driver.h"         // https://github.com/Xinyuan-LilyGO/LilyGo-EPD47
#include "esp_adc_cal.h"        // Voltage

#include <FS.h>                 // Filesystem
#include <SPI.h>                // In-built
#include <SD.h>                 // SD Card library
#include <FFat.h>               // Filesystem

#include <ArduinoJson.h>        // https://github.com/bblanchon/ArduinoJson
#include <HTTPClient.h>         // HTTP GET, POST

#include <time.h>               // Time library

#include <ESP32_FTPClient.h>    // FTP Client library

#include <DallasTemperature.h>  // Temperature sensor library
#include <OneWire.h>            // OneWire communication library

//################  VERSION  ##################################################
String version = "2.5 / 4.7in";  // Programme version, see change log at end
//################ VARIABLES ##################################################

enum alignment {LEFT, RIGHT, CENTER};
#define White         0xFF
#define LightGrey     0xBB
#define Grey          0x88
#define DarkGrey      0x44
#define Black         0x00

#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false

boolean LargeIcon   = true;
boolean SmallIcon   = false;
#define Large  20           // For icon drawing
#define Small  8            // For icon drawing

String  timeStamp;
String  Time_str = "--:--:--";
String  Date_str = "-- --- ----";
int     wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0, CurrentDay = 0, CurrentMonth = 0, CurrentYear = 0, EventCnt = 0, vref = 1100;
//################ PROGRAM VARIABLES and OBJECTS ##########################################

String getFormattedTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        DEBUG_PRINT("[CHYBA] Nepodařilo se získat aktuální čas.");
        return "N/A"; // Náhradní hodnota, pokud čas není dostupný
    }

    char timeStampBuffer[30];
    strftime(timeStampBuffer, sizeof(timeStampBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timeStampBuffer);
}

// Function to initialize SD card
void initializeSD() {
    DEBUG_PRINT("Inicializuji SD kartu...");
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);     // Initialize SPI bus with specific pins
    if (!FILE_SYSTEM.begin(SD_CS)) {                 // Initializing SD card with specific SPI pins
        DEBUG_PRINT("[CHYBA] Nepodařilo se inisializovat SD kartu");
        return;
    }
    DEBUG_PRINT("SD karta úspěšně inicializována");
}



void logError(const String &message) {
    String timeStamp = getFormattedTime();
    String logEntry = "[" + timeStamp + "]\t[CHYBA]\t" + message;

    DEBUG_PRINT("Logging error: " + logEntry);
    fs::File file = SD.open(ERROR_LOG_FILE, FILE_APPEND);
    if (!file) {
        DEBUG_PRINT("[CHYBA] Nepodařilo se zapsat chybu do souboru.");
        return;
    }
    file.println(logEntry);
    file.close();
    DEBUG_PRINT("Chyba se zalogovala do souboru.");
}

// Function to connect to wifi, see settings.h for wifi SSID and password
void connectToWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Připojuji se k WiFi");
    unsigned long startAttemptTime = millis();
    int maxAttempts = 3;
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        Serial.print(".");
        if (millis() - startAttemptTime >= 20000) {
            attempt++;
            if (attempt >= maxAttempts) {
                DEBUG_PRINT("\n[CHYBA] Nepodařilo se připojit k WiFi po opakovaných pokusech");
                logError("Nepodařilo se připojit k WiFi po opakovaných pokusech");
                return;
            } else {
                DEBUG_PRINT("\nOpakuji pokus o připojení k WiFi...");
                WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
                startAttemptTime = millis();
            }
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
        DEBUG_PRINT("\nPřipojení k WiFi úspěšné");
    } else {
        DEBUG_PRINT("\n[CHYBA] Nepodařilo se připojit k WiFi");
        logError("Nepodařilo se připojit k WiFi");
    }
}

// Function to setup time with NTP server synchronization
boolean SetupTime() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov"); //(gmtOffset_sec, daylightOffset_sec, ntpServer)
    setenv("TZ", Timezone, 1);  //setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
    tzset(); // Set the TZ environment variable
    vTaskDelay(100 / portTICK_PERIOD_MS);
    return UpdateLocalTime();
}

bool getTimeWithRetry(struct tm &timeinfo, int maxRetries = 3) {
    int retryCount = 0;
    while (!getLocalTime(&timeinfo) && retryCount < maxRetries) {
        DEBUG_PRINT("[CHYBA] Nepodařilo se získat aktuální čas, pokouším se znovu...");
        logError("Nepodařilo se získat aktuální čas, pokouším se znovu...");
        if (!SetupTime()) {
            retryCount++;
        } else {
            retryCount = 0;  // Reset při úspěšném nastavení času
        }
    }
    if (retryCount >= maxRetries) {
        DEBUG_PRINT("[CHYBA] Nepodařilo se získat čas po maximálním počtu pokusů.");
        logError("Nepodařilo se získat čas po maximálním počtu pokusů.");
        return false;
    }
    return true;
}

boolean UpdateLocalTime() {
    struct tm timeinfo;
    if (!getTimeWithRetry(timeinfo)) {
        return false;
    }
    char   time_output[30], day_output[30], update_time[30];

    // Pole s českými názvy dnů
    const char* weekday_CZ[] = {"Neděle", "Pondělí", "Úterý", "Středa", "Čtvrtek", "Pátek", "Sobota"};
    const char* month_CZ[] = {"ledna", "února", "března", "dubna", "května", "června", "července", "srpna", "září", "října", "listopadu", "prosince"};
    
    // Čekání na synchronizaci času
    while (!getLocalTime(&timeinfo, 5000)) { // Čekej 5 sekund na synchronizaci času
      DEBUG_PRINT("Nepodařilo se aktualizovat čas");
      return false;
    }
    
    // Uložení hodin, minut a sekund do proměnných
    CurrentHour = timeinfo.tm_hour;
    CurrentMin  = timeinfo.tm_min;
    CurrentSec  = timeinfo.tm_sec;

    CurrentDay = timeinfo.tm_mday;
    CurrentMonth = timeinfo.tm_mon + 1;
    CurrentYear = timeinfo.tm_year +1900;
     
    // Přiřazení hodnot k řetězcům
    Date_str = day_output;
    Time_str = time_output;
    
    return true;
}

// Function to create a directory if it doesn't exist
void createDirectory(const String &path, const String &errorMessage) {
    if (!FILE_SYSTEM.exists(path)) {
        DEBUG_PRINT("Creating directory: " + path);
        if (!FILE_SYSTEM.mkdir(path)) {
            logError(errorMessage + path);
            DEBUG_PRINT("[CHYBA] Nepodařilo se vytvořit adresář: " + path);
        }
    }
}

void createAndSaveData(const String &basePath, bool includeSeconds = true) {
    // Get current time
    struct tm timeinfo;
    if (!getTimeWithRetry(timeinfo)) {
        return;
    }

    // File path for data
    String year = String(timeinfo.tm_year + 1900);
    String month = (timeinfo.tm_mon < 9 ? "0" : "") + String(timeinfo.tm_mon + 1);
    String day = (timeinfo.tm_mday < 10 ? "0" : "") + String(timeinfo.tm_mday);
    String filePath = basePath + "/" + year + "/" + month + "/" + day + ".json";

    // Ensure directories exist
    createDirectory(basePath, "Nepodařilo se vytvořit adresář: ");
    createDirectory(basePath + "/" + year, "Nepodařilo se vytvořit adresář pro rok: ");
    createDirectory(basePath + "/" + year + "/" + month, "Nepodařilo se vytvořit adresář pro měsíc: ");

    // Create JSON document with time and temperature data
    const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(5) + 200;
    StaticJsonDocument<capacity> doc;
    doc["datum"] = year + "-" + month + "-" + day;

    // Adding current time
    String hour = (timeinfo.tm_hour < 10 ? "0" : "") + String(timeinfo.tm_hour);
    String minute = (timeinfo.tm_min < 10 ? "0" : "") + String(timeinfo.tm_min);
    if (includeSeconds) {
        String second = (timeinfo.tm_sec < 10 ? "0" : "") + String(timeinfo.tm_sec);
        doc["cas"] = hour + ":" + minute + ":" + second;
    } else {
        doc["cas"] = hour + ":" + minute;
    }

    // Add temperature data
    JsonArray mereni = doc.createNestedArray("mereni");
    JsonObject teplota = mereni.createNestedObject();
    teplota["teplota_senzor_1"] = random(-50, 101) + 0.1 * random(0, 10);
    teplota["teplota_senzor_2"] = random(-50, 101) + 0.1 * random(0, 10);
    teplota["teplota_senzor_3"] = random(-50, 101) + 0.1 * random(0, 10);
    teplota["teplota_senzor_4"] = random(-50, 101) + 0.1 * random(0, 10);

    // Open file in append mode
    File root = FILE_SYSTEM.open(filePath, FILE_APPEND);
    if (!root) {
        DEBUG_PRINT("[CHYBA] Nepodařilo se otevřít soubor pro přidání záznamu: " + filePath);
        logError("Nepodařilo se otevřít soubor pro přidání záznamu: " + filePath);
        return;
    }

    // Serialize JSON as a single line and append it to the file
    if (serializeJson(doc, root) == 0) {
        DEBUG_PRINT("[CHYBA] Nepodařil se zápis JSON do souboru");
        logError("Nepodařil se zápis JSON do souboru");
    } else {
        root.println(); // Add a newline after each JSON object
        DEBUG_PRINT("[ÚSPĚCH] JSON data přidána do souboru " + filePath);
    }

    root.close();
    DEBUG_PRINT("Soubor uzavřen");
}

void taskSaveData(void *pvParameters) {
    while (true) {
        DEBUG_PRINT("Spouštím úlohu \"SaveData\"...");
        createAndSaveData("/data", true);
        DEBUG_PRINT("Úloha \"SaveData\" dokončena. Volné prostředky: " + String(uxTaskGetStackHighWaterMark(NULL)));
        vTaskDelay(10000 / portTICK_PERIOD_MS); // 10 vteřin
    }
}

void taskSaveChartData (void *pvParameters) {
    while (true) {
        DEBUG_PRINT("Spouštím úlohu \"SaveChartData\"...");
        createAndSaveData("/chartData", false);
        DEBUG_PRINT("Úloha \"SaveChartData\" dokončena. Volné prostředky: " + String(uxTaskGetStackHighWaterMark(NULL)));
        vTaskDelay(600000 / portTICK_PERIOD_MS); // 10 minut
    }
}

void initialize() {
  	DEBUG_PRINT("Spouštím inicializaci SD karty");
    initializeSD();
    DEBUG_PRINT("Spouštím připojení k Wifi");
    connectToWiFi();
	  //waitingAnimation(2000);
    DEBUG_PRINT("Nastavuji čas");
    if (!SetupTime()) {
		DEBUG_PRINT("[CHYBA] Nepodařilo se nastavit čas");
        logError("Nepodařilo se nastavit čas");
    } else {
        DEBUG_PRINT("Nastavení času dokončeno");
    }
}

void setup() {
    Serial.begin(115200);
    DEBUG_PRINT("Spouštím úvodní nastavení");
    initialize();
    DEBUG_PRINT("Nastavení dokončeno");
    DEBUG_PRINT("Spouštím úlohy monitoringu teplot...");
    xTaskCreatePinnedToCore(taskSaveData, "SaveData", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskSaveChartData, "SaveChartData", 4096, NULL, 1, NULL, 1);

    DEBUG_PRINT("Úlohy vytvořeny");
}

void loop() {
    // Prázdné, protože veškerá práce běží v úlohách
}
