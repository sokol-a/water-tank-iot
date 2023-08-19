#include <WiFi.h>
#include "time.h"
#include <ESPAsyncWebSrv.h>
#include <SPIFFS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>

// Global variables
uint32_t startTimestamp;
const char *ssid = "Your WiFi SSID";
const char *password = "Your WiFi password";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;   // Adjust based on your time zone (in seconds)
const int   daylightOffset_sec = 3600;  // Adjust if you have daylight saving
uint32_t lastCleaningTime = 0;

struct TempTimestamp {
    float temperature;
    uint32_t timestamp;
};


#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

AsyncWebServer server(80);
Preferences preferences;


void setup() {
  Serial.begin(115200);

  sensors.begin();
//  SPIFFS.format(); //only run once if having issues with mounting SPIFFS

  if (!SPIFFS.begin()) {
    Serial.println("An error occurred while mounting SPIFFS");
    return;
  }
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi!");
  Serial.println(WiFi.localIP());
  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  // Initialize the start timestamp
  startTimestamp = (uint32_t)time(nullptr);

  // Initialize the Preferences library
  preferences.begin("temperature_data", false);  // false indicates read/write mode

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });
  server.on("/history", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/history.html", "text/html");
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });
  server.on("/main_page_script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/main_page_script.js", "text/javascript");
  });
  server.on("/history_page_script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/history_page_script.js", "text/javascript");
  });
  server.on("/get-current-temp", HTTP_GET, handleGetCurrentTemp);
  server.on("/get-temperature-history", HTTP_GET, handleGetTemperatureHistory);

  server.begin();
}

void loop() {
 
  // Periodically read temperature
  sensors.requestTemperatures();
  float currentTemp = sensors.getTempCByIndex(0); // gets the temperature reading from the DS18B20
//  float currentTemp = random(200, 300) / 10.0;          // This will generate a random number between 20 and 30 - ONLY USE FOR TESTING
  uint32_t currentTimestamp = (uint32_t)time(nullptr);  // Assuming you have set up NTP or another time source

  saveTemperature(SPIFFS, currentTemp, currentTimestamp);


  if ((uint32_t)time(nullptr) - lastCleaningTime > 86400) {  // 86400 seconds = 24 hours
    cleanOldTemperatures(SPIFFS);
    lastCleaningTime = (uint32_t)time(nullptr);
  }
  
  delay(60000);  // Wait for 60 seconds
}

void handleGetCurrentTemp(AsyncWebServerRequest *request) {
    TempTimestamp latest = getLatestTemperature(SPIFFS);

    if (latest.temperature == -100.0) {
        request->send(500, "text/plain", "Failed to fetch temperature or no data available");
        return;
    }

    String jsonResponse = String("{\"temperature\": ") + String(latest.temperature) + ", \"timestamp\": " + String(latest.timestamp) + "}";
    request->send(200, "application/json", jsonResponse);
}


void handleGetTemperatureHistory(AsyncWebServerRequest *request) {
    if (SPIFFS.exists("/temperatures.csv")) {
        request->send(SPIFFS, "/temperatures.csv", "text/csv");
    } else {
        request->send(404, "text/plain", "Data not found");
    }
}


void saveTemperature(fs::FS &fs, float temperature, uint32_t timestamp) {
    Serial.println("Saving temperature...");
    String data = String(timestamp) + "," + String(temperature) + "\n";
    
    File file = fs.open("/temperatures.csv", "a");
    if (!file) {
        Serial.println("- failed to open temperatures.csv for writing");
        return;
    }
    
    if (file.print(data)) {
        Serial.println("- temperature data written");
    } else {
        Serial.println("- write failed");
    }
    file.close();
}

TempTimestamp getLatestTemperature(fs::FS &fs) {
    Serial.println("Fetching latest temperature...");
    TempTimestamp result = {-100.0, 0};  // Default values

    File file = fs.open("/temperatures.csv", "r");
    
    if (!file || file.size() == 0) {
        Serial.println("- empty file or failed to open file");
        return result; 
    }

    // Read all lines into a list.
    String line;
    String lastLine;
    while (file.available()) {
        line = file.readStringUntil('\n');
        if (line.length() > 0) {
            lastLine = line;
        }
    }

    Serial.println("Last line from file: " + lastLine);

    // Parse the temperature and timestamp from the last line.
    int commaPos = lastLine.indexOf(',');
    if (commaPos != -1) {
        result.timestamp = lastLine.substring(0, commaPos).toInt();
        result.temperature = lastLine.substring(commaPos + 1).toFloat();
    }
    
    file.close();
    return result;
}


void cleanOldTemperatures(fs::FS &fs) {
    Serial.println("Cleaning old temperature entries...");
    
    File file = fs.open("/temperatures.csv", "r");
    if (!file) {
        Serial.println("- failed to open temperatures.csv for reading");
        return;
    }

    String newData = "";  // A string to store the filtered data
    uint32_t currentTime = (uint32_t)time(nullptr);

    while (file.available()) {
        String line = file.readStringUntil('\n');
        int commaPos = line.indexOf(',');
        uint32_t timestamp = (commaPos != -1) ? line.substring(0, commaPos).toInt() : 0;

        if (currentTime - timestamp <= 432000) {  // 432000 seconds = 5 days
            newData += line + "\n";
        }
    }
    file.close();

    // Write the filtered data back to the file
    File outFile = fs.open("/temperatures.csv", "w");
    if (!outFile) {
        Serial.println("- failed to open temperatures.csv for writing");
        return;
    }
    outFile.print(newData);
    outFile.close();

    Serial.println("Old temperature entries cleaned.");
}




void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}
