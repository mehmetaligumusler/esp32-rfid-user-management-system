#include <Arduino.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include <SPI.h>
#include <time.h>
#include <WiFi.h>

MFRC522DriverPinSimple ss_pin(10); // SDA
MFRC522DriverSPI driver{ss_pin};
MFRC522 mfrc522{driver};

const char* ssid = "MehmetAliPC";
const char* password = "gokay5353";

long timezone = 0;
byte daysavetime = 1;

AsyncWebServer server(80);

const char* PARAM_INPUT_1 = "uid";
const char* PARAM_INPUT_2 = "role";
const char* PARAM_INPUT_3 = "name";
const char* PARAM_INPUT_4 = "delete";
const char* PARAM_INPUT_5 = "delete-user";

String inputMessage;
String inputParam;

void initRFIDReader() {
  pinMode(9, OUTPUT);
  digitalWrite(9, HIGH);
  SPI.begin(11, 13, 12);
  mfrc522.PCD_Init();
  MFRC522Debug::PCD_DumpVersionToSerial(mfrc522, Serial);
  Serial.println(F("Scan PICC to see UID"));
}

void initLittleFS() {
  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  if(!LittleFS.exists("/users.txt")) {
    File file = LittleFS.open("/users.txt", FILE_WRITE);
    if(file) {
      file.println("UID,Role,Name");
      file.close();
    }
  }
  if(!LittleFS.exists("/log.txt")) {
    File file = LittleFS.open("/log.txt", FILE_WRITE);
    if(file) {
      file.println("Date,Time,UID,Role,Name");
      file.close();
    }
  }
}

void initWifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.print("ESP IP Address: ");
  Serial.println(WiFi.localIP());
}

void initTime() {
  Serial.println("Initializing Time");
  struct tm tmstruct;
  delay(2000);
  tmstruct.tm_year = 0;
  getLocalTime(&tmstruct, 5000);
  Serial.printf("Time and Date right now is : %d-%02d-%02d %02d:%02d:%02d\n",
                (tmstruct.tm_year) + 1900, (tmstruct.tm_mon) + 1, tmstruct.tm_mday,
                tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
}

String getNameFromFile(const char* filename, String uid) {
  File file = LittleFS.open(filename);
  if (!file) return "";
  file.readStringUntil('\n');
  while (file.available()) {
    String line = file.readStringUntil('\n');
    int comma1 = line.indexOf(',');
    int comma2 = line.lastIndexOf(',');
    if (comma1 > 0 && comma2 > comma1) {
      String fileUID = line.substring(0, comma1);
      String name = line.substring(comma2 + 1);
      if (fileUID == uid) {
        file.close();
        name.trim();
        return name;
      }
    }
  }
  file.close();
  return "";
}

String getRoleFromFile(const char* filename, String uid) {
  File file = LittleFS.open(filename);
  if (!file) return "";
  file.readStringUntil('\n');
  while (file.available()) {
    String line = file.readStringUntil('\n');
    int comma1 = line.indexOf(',');
    int comma2 = line.lastIndexOf(',');
    if (comma1 > 0 && comma2 > comma1) {
      String fileUID = line.substring(0, comma1);
      String role = line.substring(comma1 + 1, comma2);
      if (fileUID == uid) {
        file.close();
        role.trim();
        return role;
      }
    }
  }
  file.close();
  return "";
}

String processor(const String& var){
  return String("HTTP GET request sent to your ESP on input field (" + inputParam + ") with value: " + inputMessage + "<br><a href=\"/\"><button class=\"button button-home\">Return to Home Page</button></a>");
}

void deleteLineByUID(const char* path, String uidToDelete) {
  File file = LittleFS.open(path);
  if (!file) return;
  String newContent = "";
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (!line.startsWith(uidToDelete + ",")) {
      newContent += line + "\n";
    }
  }
  file.close();
  file = LittleFS.open(path, FILE_WRITE);
  file.print(newContent);
  file.close();
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  initRFIDReader();
  initLittleFS();
  initWifi();
  configTime(3600 * timezone, daysavetime * 3600, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");
  initTime();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/full-log.html");
  });

  server.on("/add-user", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/add-user.html");
  });

  server.on("/manage-users", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/manage-users.html");
  });

  server.serveStatic("/", LittleFS, "/");

  server.on("/view-users", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/users.txt", "text/plain");
  });

  server.on("/view-log", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/log.txt", "text/plain");
  });

  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2) && request->hasParam(PARAM_INPUT_3)) {
      String uid = request->getParam(PARAM_INPUT_1)->value();
      String role = request->getParam(PARAM_INPUT_2)->value();
      String name = request->getParam(PARAM_INPUT_3)->value();
      inputParam = "add-user";
      inputMessage = uid + "," + role + "," + name;
      File file = LittleFS.open("/users.txt", FILE_APPEND);
      if (file) {
        file.println(inputMessage);
        file.close();
      }
    }
    else if (request->hasParam(PARAM_INPUT_4)) {
      String target = request->getParam(PARAM_INPUT_4)->value();
      inputParam = "delete";
      if (target == "log") {
        LittleFS.remove("/log.txt");
      } else if (target == "users") {
        LittleFS.remove("/users.txt");
      }
    }
    else if (request->hasParam(PARAM_INPUT_5)) {
      String uidToDelete = request->getParam(PARAM_INPUT_5)->value();
      deleteLineByUID("/users.txt", uidToDelete);
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    request->send(LittleFS, "/get.html", "text/html", false, processor);
  });

  server.begin();
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  String uidString = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) {
      uidString += "0";
    }
    uidString += String(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.print("Card UID: ");
  Serial.println(uidString);

  String role = getRoleFromFile("/users.txt", uidString);
  String name = getNameFromFile("/users.txt", uidString);
  if (role != "") {
    Serial.printf("Role for UID %s is %s (%s)\n", uidString.c_str(), role.c_str(), name.c_str());
  } else {
    role = "unknown";
    Serial.printf("UID %s not found, set user role to unknown\n", uidString.c_str());
  }

  time_t now = time(nullptr);
  struct tm *tm_struct = localtime(&now);
  char bufferDate[20], bufferTime[20];
  strftime(bufferDate, sizeof(bufferDate), "%Y-%m-%d", tm_struct);
  strftime(bufferTime, sizeof(bufferTime), "%H:%M:%S", tm_struct);
  String logEntry = String(bufferDate) + "," + String(bufferTime) + "," + uidString + "," + role + "," + name;
if (!LittleFS.exists("/log.txt")) {
  File newLog = LittleFS.open("/log.txt", FILE_WRITE);
  if (newLog) {
    newLog.println("Date,Time,UID,Role,Name");
    newLog.close();
  }
}

File logFile = LittleFS.open("/log.txt", FILE_APPEND);
if (logFile) {
  logFile.println(logEntry);
  logFile.close();
}

  delay(2500);
}
