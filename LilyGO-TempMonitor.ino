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

bool ensureDirectoryExists(const String &path) {
    if (!FILE_SYSTEM.exists(path)) {
        DEBUG_PRINT("[" + getFormattedTime() + "] Vytvářím adresář: " + path);
        if (!FILE_SYSTEM.mkdir(path)) {
            DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nepodařilo se vytvořit adresář: " + path);
            return false;
        }
    }
    return true;
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

    String year = String(timeinfo.tm_year + 1900);
    String month = (timeinfo.tm_mon < 9 ? "0" : "") + String(timeinfo.tm_mon + 1);
    String day = (timeinfo.tm_mday < 10 ? "0" : "") + String(timeinfo.tm_mday);
    String filePath = basePath + "/" + year + "/" + month + "/" + day + ".json";

    if (!ensureDirectoryExists(basePath) || 
        !ensureDirectoryExists(basePath + "/" + year) ||
        !ensureDirectoryExists(basePath + "/" + year + "/" + month)) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nepodařilo se zajistit adresářovou strukturu.");
        return;
    }

    const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(5) + 200;
    StaticJsonDocument<capacity> doc;
    doc["datum"] = year + "-" + month + "-" + day;

    String hour = (timeinfo.tm_hour < 10 ? "0" : "") + String(timeinfo.tm_hour);
    String minute = (timeinfo.tm_min < 10 ? "0" : "") + String(timeinfo.tm_min);
    if (includeSeconds) {
        String second = (timeinfo.tm_sec < 10 ? "0" : "") + String(timeinfo.tm_sec);
        doc["cas"] = hour + ":" + minute + ":" + second;
    } else {
        doc["cas"] = hour + ":" + minute;
    }

    JsonArray mereni = doc.createNestedArray("mereni");
    JsonObject teplota = mereni.createNestedObject();
    teplota["teplota_senzor_1"] = random(-50, 101) + 0.1 * random(0, 10);
    teplota["teplota_senzor_2"] = random(-50, 101) + 0.1 * random(0, 10);
    teplota["teplota_senzor_3"] = random(-50, 101) + 0.1 * random(0, 10);
    teplota["teplota_senzor_4"] = random(-50, 101) + 0.1 * random(0, 10);

    File file = FILE_SYSTEM.open(filePath, FILE_APPEND);
    if (!file) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nepodařilo se otevřít soubor: " + filePath);
        return;
    }

    if (serializeJson(doc, file) == 0) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nepodařil se zápis JSON do souboru.");
    } else {
        file.println();
        DEBUG_PRINT("[" + getFormattedTime() + "] [ÚSPĚCH] JSON data uložena do souboru: " + filePath);
    }
    file.close();
}

void markFileUpload(const String &filePath, time_t lastWriteTime) {
    File logFile = FILE_SYSTEM.open("/uploaded_files.log", FILE_APPEND);
    if (!logFile) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nelze zapsat do logovacího souboru.");
        return;
    }
    logFile.println(filePath + "\t" + String(lastWriteTime));
    logFile.close();
    DEBUG_PRINT("[" + getFormattedTime() + "] Zaznamenán přenos souboru: " + filePath);
}


bool isFileOutdated(const String &filePath, time_t lastWriteTime) {
    File logFile = FILE_SYSTEM.open("/uploaded_files.log", FILE_READ);
    if (!logFile) return true;

    while (logFile.available()) {
        String line = logFile.readStringUntil('\n');
        int separator = line.indexOf('\t');
        if (separator == -1) continue;

        String loggedPath = line.substring(0, separator);
        time_t loggedTime = atol(line.substring(separator + 1).c_str());

        if (loggedPath == filePath) {
            logFile.close();
            return lastWriteTime > loggedTime;
        }
    }
    logFile.close();
    return true;
}

void uploadFileWithTimestampCheck(const String &filePath) {
    File file = FILE_SYSTEM.open(filePath, FILE_READ);
    if (!file) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nelze otevřít soubor: " + filePath);
        return;
    }

    time_t lastWriteTime = file.getLastWrite();
    file.close();

    if (isFileOutdated(filePath, lastWriteTime)) {
        DEBUG_PRINT("[" + getFormattedTime() + "] Přenáším aktualizovaný soubor: " + filePath);
        if (uploadFileToFTP(filePath)) {
            markFileUpload(filePath, lastWriteTime);
            DEBUG_PRINT("[" + getFormattedTime() + "] Soubor úspěšně nahrán: " + filePath);
        } else {
            DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nepodařilo se nahrát soubor: " + filePath);
        }
    } else {
        DEBUG_PRINT("[" + getFormattedTime() + "] Soubor nebyl aktualizován, přeskočeno: " + filePath);
    }
}


bool uploadFileToFTP(const String &localPath) {
    if (localPath.isEmpty()) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nelze nahrát prázdný soubor.");
        return false;
    }

    ESP32_FTPClient ftp(FTP_HOST, FTP_USER, FTP_PASSWORD);
    ftp.OpenConnection();

    if (!ftp.isConnected()) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nelze se připojit k FTP serveru.");
        return false;
    }

    String remotePath = localPath.startsWith("/") ? localPath.substring(1) : localPath;
    int lastSlash = remotePath.lastIndexOf('/');
    String directoryPath = remotePath.substring(0, lastSlash);
    String fileName = remotePath.substring(lastSlash + 1);

    // Zajištění adresářů
    ftp.ChangeWorkDir("/");
    int start = 0;
    while (true) {
        int slash = directoryPath.indexOf('/', start);
        String subDir = slash == -1 ? directoryPath.substring(start) : directoryPath.substring(start, slash);
        if (subDir.isEmpty()) break;

        ftp.InitFile("Type A"); // Přepnout na textový režim
        ftp.MakeDir(subDir.c_str()); // Pokus o vytvoření adresáře
        ftp.ChangeWorkDir(subDir.c_str()); // Přesun do adresáře

        if (slash == -1) break;
        start = slash + 1;
    }

    File file = FILE_SYSTEM.open(localPath, FILE_READ);
    if (!file) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nelze otevřít soubor: " + localPath);
        return false;
    }

    size_t fileSize = file.size();
    ftp.InitFile(fileSize == 0 ? "Type A" : "Type I"); // Typ souboru (text/binární)
    ftp.NewFile(fileName.c_str());
    if (fileSize > 0) {
        unsigned char buffer[512];
        size_t bytesRead;
        while ((bytesRead = file.read(buffer, sizeof(buffer))) > 0) {
            ftp.WriteData(buffer, bytesRead);
        }
    }
    ftp.CloseFile();
    file.close();
    DEBUG_PRINT("[" + getFormattedTime() + "] Soubor úspěšně nahrán na FTP: " + localPath);
    return true;
}

void uploadAllFilesFromDirectoryWithLog(const String &baseDir) {
    DEBUG_PRINT("[" + getFormattedTime() + "] Prohledávám adresář: " + baseDir);

    File dir = FILE_SYSTEM.open(baseDir);
    if (!dir || !dir.isDirectory()) {
        DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Adresář neexistuje nebo není adresář: " + baseDir);
        return;
    }

    File file = dir.openNextFile();
    while (file) {
        String fullPath = String(baseDir) + "/" + String(file.name());
        if (file.isDirectory()) {
            DEBUG_PRINT("[" + getFormattedTime() + "] Nalezen podadresář: " + fullPath);
            uploadAllFilesFromDirectoryWithLog(fullPath);
        } else {
            time_t lastWriteTime = file.getLastWrite();
            if (isFileOutdated(fullPath, lastWriteTime)) {
                DEBUG_PRINT("[" + getFormattedTime() + "] Přenáším soubor: " + fullPath);
                if (uploadFileToFTP(fullPath)) {
                    markFileUpload(fullPath, lastWriteTime);
                    DEBUG_PRINT("[" + getFormattedTime() + "] Soubor úspěšně nahrán: " + fullPath);
                } else {
                    DEBUG_PRINT("[" + getFormattedTime() + "] [CHYBA] Nepodařilo se nahrát soubor: " + fullPath);
                }
            } else {
                DEBUG_PRINT("[" + getFormattedTime() + "] Soubor nebyl aktualizován, přeskočeno: " + fullPath);
            }
        }
        file = dir.openNextFile();
    }
    dir.close();
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

void taskFTPTransfer(void *pvParameters) {
    while (true) {
        DEBUG_PRINT("[" + getFormattedTime() + "] Přenáším soubory z adresáře: " + DATA_DIR);
        uploadAllFilesFromDirectoryWithLog(DATA_DIR);
        DEBUG_PRINT("[" + getFormattedTime() + "] Přenáším soubory z adresáře: " + CHART_DATA_DIR);
        uploadAllFilesFromDirectoryWithLog(CHART_DATA_DIR);
        vTaskDelay(600000 / portTICK_PERIOD_MS); // 10 minut
    }
}


void setup() {
    Serial.begin(115200);
    initializeSD();
    connectToWiFi();
    SetupTime();
    xTaskCreatePinnedToCore(taskMonthlyRestart, "MonthlyRestart", 2048, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskSaveData, "SaveData", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskSaveChartData, "SaveChartData", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskFTPTransfer, "FTPTransfer", 8192, NULL, 1, NULL, 1);
}

void loop() {
    // Prázdné, protože veškerá práce běží v úlohách FreeRTOS
}
