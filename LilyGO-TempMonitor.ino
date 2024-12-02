#include "settings.h" // Basic settings, SSIDs, passwords, server addresses, https://openweathermap.org/ API key and credentials

#include <WiFi.h>               // Wifi library
#include <Arduino.h>            // In-built
#include <esp_task_wdt.h>       // Task parallelization
#include "freertos/FreeRTOS.h"  // Task parallelization
#include "freertos/task.h"      // Task parallelization
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

//################ VERSION #########################
String version = "2.5 / 4.7in"; // Programme version, see change log at end

//################ VARIABLES #######################
String lastUploadedFileData;
String lastUploadedFileChart;
String Time_str = "--:--:--";
String Date_str = "-- --- ----";

int CurrentHour = 0, CurrentMin = 0, CurrentSec = 0, CurrentDay = 0, CurrentMonth = 0, CurrentYear = 0;

//################ FUNCTIONS #######################

String getFormattedTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        DEBUG_PRINT("[CHYBA] Nepodařilo se získat aktuální čas.");
        return "N/A";
    }
    char timeStampBuffer[30];
    strftime(timeStampBuffer, sizeof(timeStampBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timeStampBuffer);
}

void initializeSD() {
    DEBUG_PRINT("[" + getFormattedTime() + "] Inicializuji SD kartu...");
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    if (!FILE_SYSTEM.begin(SD_CS)) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nepodařilo se inicializovat SD kartu");
        return;
    }
    DEBUG_PRINT("[" + getFormattedTime() + "] SD karta úspěšně inicializována");
}

// Funkce pro získání cesty k souboru ve formátu /data/YYYYMMDD.json
String getDataFilePath(const String &date) {
    return "/data/" + date + ".json";
}

// Funkce pro získání cesty k souboru ve formátu /chartData/YYYYMMDD.json
String getChartDataFilePath(const String &date) {
    return "/chartData/" + date + ".json";
}

void clearUploadedFilesLog() {
    if (FILE_SYSTEM.exists("/uploaded_files.log")) {
        if (FILE_SYSTEM.remove("/uploaded_files.log")) {
            DEBUG_PRINT("[" + getFormattedTime() + "] Soubor uploaded_files.log byl úspěšně odstraněn.");
        } else {
            DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nepodařilo se odstranit soubor uploaded_files.log.");
        }
    } else {
        DEBUG_PRINT("[" + getFormattedTime() + "] Soubor uploaded_files.log neexistuje, není co mazat.");
    }
}

bool appendToFile(const String &filePath, const String &data) {
    File file = FILE_SYSTEM.open(filePath, FILE_APPEND);
    if (!file) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nepodařilo se otevřít soubor pro zápis: " + filePath);
        return false;
    }
    file.println(data);
    file.close();
    return true;
}

void logError(const String &message) {   // <-
    String timeStamp = getFormattedTime();
    String logEntry = "[" + timeStamp + "]\t[CHYBA]\t" + message;

    DEBUG_PRINT("[" + getFormattedTime() + "] Logging error: " + logEntry);
    if (!appendToFile(ERROR_LOG_FILE, logEntry)) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nepodařilo se zapsat chybu do souboru.");
    }
}

void connectToWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    DEBUG_PRINT("[" + getFormattedTime() + "] Připojuji k WiFi...");
    
    for (int attempt = 0; attempt < MAX_WIFI_RETRIES; ++attempt) {
        if (WiFi.status() == WL_CONNECTED) {
            DEBUG_PRINT("[" + getFormattedTime() + "] Úspěšně připojeno k WiFi.");
            return;
        }
        vTaskDelay(WIFI_RETRY_DELAY_MS / portTICK_PERIOD_MS);
        DEBUG_PRINT("[" + getFormattedTime() + "] Zkouším se znovu připojit k WiFi (" + String(attempt + 1) + "/" + String(MAX_WIFI_RETRIES) + ")...");
    }
    DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Opakovaně se nepodařilo připojit k WiFi.");
    logError("Opakovaně se nepodařilo připojit k WiFi.");
}

boolean SetupTime() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov");
    setenv("TZ", Timezone, 1);
    tzset();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    return true;
}

void createAndSaveData(const String &basePath, bool includeSeconds = true) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nepodařilo se získat aktuální čas.");
        return;
    }

    // Sestavení názvu souboru ve formátu YYYYMMDD.json
    String year = String(timeinfo.tm_year + 1900);
    String month = (timeinfo.tm_mon < 9 ? "0" : "") + String(timeinfo.tm_mon + 1);
    String day = (timeinfo.tm_mday < 10 ? "0" : "") + String(timeinfo.tm_mday);
    String fileName = year + month + day + ".json";
    String filePath = basePath + "/" + fileName;

    const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(5) + 200;
    StaticJsonDocument<capacity> doc;
    doc["datum"] = year + "-" + month + "-" + day;

    // Přidání času do JSON dokumentu
    String hour = (timeinfo.tm_hour < 10 ? "0" : "") + String(timeinfo.tm_hour);
    String minute = (timeinfo.tm_min < 10 ? "0" : "") + String(timeinfo.tm_min);
    if (includeSeconds) {
        String second = (timeinfo.tm_sec < 10 ? "0" : "") + String(timeinfo.tm_sec);
        doc["cas"] = hour + ":" + minute + ":" + second;
    } else {
        doc["cas"] = hour + ":" + minute;
    }

    // Generování dat pro měření
    JsonArray mereni = doc.createNestedArray("mereni");
    JsonObject teplota = mereni.createNestedObject();
    teplota["teplota_senzor_1"] = random(-50, 101) + 0.1 * random(0, 10);
    teplota["teplota_senzor_2"] = random(-50, 101) + 0.1 * random(0, 10);
    teplota["teplota_senzor_3"] = random(-50, 101) + 0.1 * random(0, 10);
    teplota["teplota_senzor_4"] = random(-50, 101) + 0.1 * random(0, 10);

    // Otevření souboru pro zápis nebo vytvoření nového
    File file = FILE_SYSTEM.open(filePath, FILE_APPEND);
    if (!file) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nepodařilo se otevřít soubor: " + filePath);
        return;
    }

    // Zápis dat do JSON souboru
    if (serializeJson(doc, file) == 0) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nepodařil se zápis JSON do souboru.");
    } else {
        file.println();
        DEBUG_PRINT("[" + getFormattedTime() + "] [ÚSPĚCH] JSON data uložena do souboru: " + filePath);
    }

    file.close();
}

bool ensureFTPDirectoryExists(ESP32_FTPClient &ftp, const String &path) {
    ftp.ChangeWorkDir("/"); // Začněte v root adresáři
    int start = 0;
    while (true) {
        int slash = path.indexOf('/', start);
        String subDir = (slash == -1) ? path.substring(start) : path.substring(start, slash);
        if (subDir.isEmpty()) break;

        ftp.MakeDir(subDir.c_str());  // Vytvoření adresáře
        ftp.ChangeWorkDir(subDir.c_str());  // Přechod do adresáře
        DEBUG_PRINT("[" + getFormattedTime() + "] Kontrola adresáře na FTP: " + subDir);

        if (slash == -1) break;
        start = slash + 1;
    }
    return true;
}

bool uploadFileToFTP(const String &localPath, const String &ftpBasePath) {
    ESP32_FTPClient ftp(FTP_HOST, FTP_USER, FTP_PASSWORD);
    ftp.OpenConnection();

    if (!ftp.isConnected()) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nelze se připojit k FTP serveru.");
        return false;
    }

    // Zajištění adresáře
    String fileName = localPath.substring(localPath.lastIndexOf('/') + 1);
    String ftpDirectoryPath = ftpBasePath; // Základní cesta na FTP
    if (!ensureFTPDirectoryExists(ftp, ftpDirectoryPath)) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nelze zajistit adresář na FTP: " + ftpDirectoryPath);
        ftp.CloseConnection();
        return false;
    }

    // Přenos souboru
    File file = FILE_SYSTEM.open(localPath, FILE_READ);
    if (!file) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nelze otevřít soubor: " + localPath);
        ftp.CloseConnection();
        return false;
    }

    ftp.InitFile("Type A");
    ftp.NewFile(fileName.c_str());
    while (file.available()) {
        String line = file.readStringUntil('\n');
        ftp.Write(line.c_str());
        ftp.Write("\r\n");
    }
    file.close();

    ftp.CloseFile();
    ftp.CloseConnection();
    DEBUG_PRINT("[" + getFormattedTime() + "] Soubor úspěšně nahrán: " + ftpBasePath + "/" + fileName);
    return true;
}


void taskFTPTransfer(void *pvParameters) {
    const int delayInterval = 15 * 60 * 1000 / portTICK_PERIOD_MS; // 15 minut
    while (true) {
        DEBUG_PRINT("[" + getFormattedTime() + "] Spouštím přenos souborů na FTP.");

        // Přenos souborů z DATA_DIR
        File dataDir = FILE_SYSTEM.open(DATA_DIR);
        if (dataDir && dataDir.isDirectory()) {
            File file = dataDir.openNextFile();
            while (file) {
                String localPath = String(DATA_DIR) + "/" + file.name();
                if (!uploadFileToFTP(localPath, "/data")) {
                    DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nepodařilo se nahrát soubor: " + localPath);
                }
                file = dataDir.openNextFile();
            }
            dataDir.close();
        } else {
            DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nelze otevřít adresář: " + String(DATA_DIR));
        }

        // Přenos souborů z CHART_DATA_DIR
        File chartDataDir = FILE_SYSTEM.open(CHART_DATA_DIR);
        if (chartDataDir && chartDataDir.isDirectory()) {
            File file = chartDataDir.openNextFile();
            while (file) {
                String localPath = String(CHART_DATA_DIR) + "/" + file.name();
                if (!uploadFileToFTP(localPath, "/chartData")) {
                    DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nepodařilo se nahrát soubor: " + localPath);
                }
                file = chartDataDir.openNextFile();
            }
            chartDataDir.close();
        } else {
            DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nelze otevřít adresář: " + String(CHART_DATA_DIR));
        }

        DEBUG_PRINT("[" + getFormattedTime() + "] Přenos souborů dokončen. Čekám 15 minut.");
        vTaskDelay(delayInterval);
    }
}


void taskMonthlyRestart(void *pvParameters) {
    static int lastMonth = -1; // Uchovává hodnotu posledního měsíce
    while (true) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            int currentMonth = timeinfo.tm_mon + 1; // Měsíce od 0 do 11, proto +1
            if (lastMonth != -1 && currentMonth != lastMonth) {
                DEBUG_PRINT("[" + getFormattedTime() + "] Měsíční restart detekován. Ukončuji úlohy...");
                restartFlag = true; // Nastavíme příznak pro ukončení ostatních úloh
                vTaskDelay(5000 / portTICK_PERIOD_MS); // 5 sekund na ukončení
                DEBUG_PRINT("[" + getFormattedTime() + "] Všechny úlohy ukončeny. Restartuji ESP...");
                vTaskDelay(2000 / portTICK_PERIOD_MS); // Krátké zpoždění pro zajištění výstupu do Serial Monitoru
                esp_restart(); // Provede restart ESP32
            }

            lastMonth = currentMonth; // Aktualizace hodnoty posledního měsíce
        } else {
            DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nepodařilo se načíst čas pro měsíční restart.");
        }

        vTaskDelay(60000 / portTICK_PERIOD_MS); // Kontrola jednou za minutu
    }
}

void taskSaveData(void *pvParameters) {
    while (true) {
        if (restartFlag) { // Kontrola příznaku
            DEBUG_PRINT("[" + getFormattedTime() + "] Úloha \"SaveData\" se ukončuje kvůli restartu...");
            vTaskDelete(NULL); // Bezpečné ukončení úlohy
        }
        DEBUG_PRINT("[" + getFormattedTime() + "] Spouštím úlohu \"SaveData\"...");
        createAndSaveData(DATA_DIR, true);
        DEBUG_PRINT("[" + getFormattedTime() + "] Úloha \"SaveData\" dokončena. Volné prostředky: " + String(uxTaskGetStackHighWaterMark(NULL)));
        vTaskDelay(60000 / portTICK_PERIOD_MS); // 10 vteřin
    }
}

void taskSaveChartData (void *pvParameters) {
    while (true) {
        if (restartFlag) { // Kontrola příznaku
            DEBUG_PRINT("[" + getFormattedTime() + "] Úloha \"SaveChartData\" se ukončuje kvůli restartu...");
            vTaskDelete(NULL); // Bezpečné ukončení úlohy
        }
        DEBUG_PRINT("[" + getFormattedTime() + "] Spouštím úlohu \"SaveChartData\"...");
        createAndSaveData(CHART_DATA_DIR, false);
        DEBUG_PRINT("[" + getFormattedTime() + "] Úloha \"SaveChartData\" dokončena. Volné prostředky: " + String(uxTaskGetStackHighWaterMark(NULL)));
        vTaskDelay(600000 / portTICK_PERIOD_MS); // 10 minut
    }
}

void setup() {
    Serial.begin(115200);
    initializeSD();
    connectToWiFi();
    SetupTime();
    
    // Vymazání logovacího souboru pro ladění (odstraňte pro produkční provoz)
    clearUploadedFilesLog();

    xTaskCreatePinnedToCore(taskMonthlyRestart, "MonthlyRestart", 2048, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskSaveData, "SaveData", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskSaveChartData, "SaveChartData", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskFTPTransfer, "taskFTPTransfer", 8192, NULL, 1, NULL, 1);
}

void loop() {
    // Prázdné, protože veškerá práce běží v úlohách FreeRTOS
}
