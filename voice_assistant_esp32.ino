/*
 * ESP32-S3 Voice Assistant with RFID Card Access System
 * Uses INMP441 I2S Microphone, Speaker, SD Card and Local API Server
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <I2S.h>
#include <SD.h>
#include <SPI.h>
#include <MFRC522.h>
#include <TensorFlowLite_ESP32.h>
#include "audio_provider.h"
#include "command_responder.h"
#include "feature_provider.h"
#include "micro_features_micro_model_settings.h"
#include "micro_features_tiny_conv_micro_features_model_data.h"
#include "recognize_commands.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

// Network credentials
const char* ssid = "Xiaomi_13_Lite";
const char* password = "123456789....";

// API Endpoints
const char* API_SERVER = "http://10.2.0.2:5000";  // Local server IP
const char* API_TRANSCRIBE = "/transcribe";
const char* API_RESPOND = "/respond";

// Pin Definitions for ESP32-S3
#define I2S_MIC_SERIAL_CLOCK      GPIO_NUM_5
#define I2S_MIC_LEFT_RIGHT_CLOCK  GPIO_NUM_6
#define I2S_MIC_SERIAL_DATA       GPIO_NUM_7
#define I2S_SPEAKER_BCLK          GPIO_NUM_16
#define I2S_SPEAKER_WCLK          GPIO_NUM_15
#define I2S_SPEAKER_DOUT          GPIO_NUM_14
#define SD_MISO                   GPIO_NUM_37
#define SD_MOSI                   GPIO_NUM_35
#define SD_SCK                    GPIO_NUM_36
#define SD_CS                     GPIO_NUM_34
#define STATUS_LED                GPIO_NUM_47
#define RFID_SS                   GPIO_NUM_21
#define RFID_RST                  GPIO_NUM_22

// RFID Initialization
MFRC522 rfid(RFID_SS, RFID_RST);

// Voice recognition parameters
#define SAMPLE_RATE 16000
#define BUFFER_SIZE 512

// I2S configuration for microphone
i2s_config_t i2s_mic_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
};

// I2S pin configuration for microphone
i2s_pin_config_t i2s_mic_pins = {
    .bck_io_num = I2S_MIC_SERIAL_CLOCK,
    .ws_io_num = I2S_MIC_LEFT_RIGHT_CLOCK,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SERIAL_DATA
};

// I2S configuration for speaker
i2s_config_t i2s_speaker_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 22050,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
};

// I2S pin configuration for speaker
i2s_pin_config_t i2s_speaker_pins = {
    .bck_io_num = I2S_SPEAKER_BCLK,
    .ws_io_num = I2S_SPEAKER_WCLK,
    .data_out_num = I2S_SPEAKER_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
};

// TensorFlow Lite parameters for wake word detection
namespace {
  tflite::ErrorReporter* error_reporter = nullptr;
  const tflite::Model* model = nullptr;
  tflite::MicroInterpreter* interpreter = nullptr;
  TfLiteTensor* model_input = nullptr;
  FeatureProvider* feature_provider = nullptr;
  RecognizeCommands* recognizer = nullptr;
  int32_t previous_time = 0;
  
  constexpr int kTensorArenaSize = 10 * 1024;
  uint8_t tensor_arena[kTensorArenaSize];
}

// Authorized RFID cards (UID in hex format)
const String AUTHORIZED_CARDS[] = {
  "11223344",  // Example card 1
  "55667788",  // Example card 2
  "99AABBCC"   // Example card 3
};
const int NUM_AUTHORIZED_CARDS = 3;

// Function prototypes
void initMicrophone();
void initSpeaker();
void initSDCard();
void initRFID();
void initTensorFlow();
void initWiFi();
bool detectWakeWord();
bool recordAudio(const char* filename, int recordSeconds);
bool sendAudioToServer(const char* filename);
bool getResponseFromServer();
void playAudioFile(const char* filename);
void logEvent(const String& event);
void checkRFID();
bool isAuthorizedCard(String uid);
void blinkLED(int times, int delayMs);
void showProcessingStatus();
void showErrorStatus();
void showReadyStatus();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("ESP32-S3 Sesli Asistan ve RFID Sistemi Başlatılıyor...");
  
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);
  
  initWiFi();
  initMicrophone();
  initSpeaker();
  initSDCard();
  initRFID();
  initTensorFlow();
  
  logEvent("Sistem başlatıldı");
  showReadyStatus();
}

void loop() {
  // Check for RFID cards
  checkRFID();
  
  // Check for wake word
  if (detectWakeWord()) {
    digitalWrite(STATUS_LED, HIGH);
    
    Serial.println("Uyandırma kelimesi algılandı: 'Hey Assistant'");
    playAudioFile("/system/wake_detected.wav");
    
    if (recordAudio("/query.wav", 5)) {
      logEvent("Ses kaydı tamamlandı");
      Serial.println("Soru analiz ediliyor...");
      
      showProcessingStatus();
      
      if (sendAudioToServer("/query.wav")) {
        showProcessingStatus();
        
        if (getResponseFromServer()) {
          playAudioFile("/response.wav");
          blinkLED(2, 100);
        } else {
          Serial.println("Yanıt alınamadı");
          showErrorStatus();
        }
      } else {
        Serial.println("Ses dosyası sunucuya gönderilemedi");
        showErrorStatus();
      }
    } else {
      Serial.println("Ses kaydı başarısız oldu");
      showErrorStatus();
    }
    
    digitalWrite(STATUS_LED, LOW);
    showReadyStatus();
  }
  
  delay(100);
}

void initWiFi() {
  Serial.print("WiFi ağına bağlanılıyor: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Bağlandı, IP adresi: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("WiFi bağlantısı kurulamadı! Cihaz çevrimdışı modda çalışacak.");
  }
}

void initMicrophone() {
  Serial.println("I2S mikrofon başlatılıyor...");
  esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_mic_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Mikrofon için I2S sürücüsü yüklenemedi: %d\n", err);
    return;
  }
  
  err = i2s_set_pin(I2S_NUM_0, &i2s_mic_pins);
  if (err != ESP_OK) {
    Serial.printf("Mikrofon için I2S pin ayarları yapılamadı: %d\n", err);
    return;
  }
  
  Serial.println("I2S mikrofon başlatıldı");
}

void initSpeaker() {
  Serial.println("I2S hoparlör başlatılıyor...");
  esp_err_t err = i2s_driver_install(I2S_NUM_1, &i2s_speaker_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Hoparlör için I2S sürücüsü yüklenemedi: %d\n", err);
    return;
  }
  
  err = i2s_set_pin(I2S_NUM_1, &i2s_speaker_pins);
  if (err != ESP_OK) {
    Serial.printf("Hoparlör için I2S pin ayarları yapılamadı: %d\n", err);
    return;
  }
  
  Serial.println("I2S hoparlör başlatıldı");
}

void initSDCard() {
  Serial.println("SD kart başlatılıyor...");
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  
  if (!SD.begin(SD_CS)) {
    Serial.println("SD kart başlatılamadı!");
    return;
  }
  
  if (!SD.exists("/system")) {
    SD.mkdir("/system");
  }
  
  Serial.println("SD kart başlatıldı");
}

void initRFID() {
  Serial.println("RFID okuyucu başlatılıyor...");
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("RFID okuyucu başlatıldı");
}

void initTensorFlow() {
  Serial.println("TensorFlow başlatılıyor...");
  
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;
  
  model = tflite::GetModel(g_tiny_conv_micro_features_model_data);
  
  static tflite::MicroMutableOpResolver<4> micro_op_resolver;
  micro_op_resolver.AddDepthwiseConv2D();
  micro_op_resolver.AddFullyConnected();
  micro_op_resolver.AddSoftmax();
  micro_op_resolver.AddReshape();
  
  static tflite::MicroInterpreter static_interpreter(
      model, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;
  
  interpreter->AllocateTensors();
  
  model_input = interpreter->input(0);
  
  static FeatureProvider static_feature_provider(kFeatureElementCount,
                                               feature_data);
  feature_provider = &static_feature_provider;
  
  static RecognizeCommands static_recognizer(error_reporter);
  recognizer = &static_recognizer;
  
  previous_time = 0;
  
  Serial.println("TensorFlow başlatıldı");
}

bool detectWakeWord() {
  const int32_t current_time = millis();
  
  int how_many_new_slices = 0;
  TfLiteStatus feature_status = feature_provider->PopulateFeatureData(
      error_reporter, previous_time, current_time, &how_many_new_slices);
  
  if (feature_status != kTfLiteOk) {
    error_reporter->Report("Feature generation failed");
    return false;
  }
  
  previous_time = current_time;
  
  if (how_many_new_slices == 0) {
    return false;
  }
  
  for (int i = 0; i < kFeatureElementCount; i++) {
    model_input->data.f[i] = feature_data[i];
  }
  
  TfLiteStatus invoke_status = interpreter->Invoke();
  if (invoke_status != kTfLiteOk) {
    error_reporter->Report("Invoke failed");
    return false;
  }
  
  TfLiteTensor* output = interpreter->output(0);
  
  const char* found_command = nullptr;
  uint8_t score = 0;
  bool is_new_command = false;
  TfLiteStatus process_status = recognizer->ProcessLatestResults(
      output, current_time, &found_command, &score, &is_new_command);
  
  if (process_status != kTfLiteOk) {
    error_reporter->Report("Recognition process failed");
    return false;
  }
  
  if (is_new_command && (score > 200) && (strcmp(found_command, "hey_assistant") == 0)) {
    Serial.println("Wake word detected!");
    return true;
  }
  
  return false;
}

bool recordAudio(const char* filename, int recordSeconds) {
  Serial.printf("Ses kaydı başlatılıyor: %s...\n", filename);
  
  File audioFile = SD.open(filename, FILE_WRITE);
  if (!audioFile) {
    Serial.println("Ses dosyası oluşturulamadı");
    return false;
  }
  
  uint8_t header[44];
  memcpy(header, "RIFF", 4);
  uint32_t fileSize = recordSeconds * SAMPLE_RATE * 2 + 36;
  header[4] = (uint8_t)(fileSize & 0xFF);
  header[5] = (uint8_t)((fileSize >> 8) & 0xFF);
  header[6] = (uint8_t)((fileSize >> 16) & 0xFF);
  header[7] = (uint8_t)((fileSize >> 24) & 0xFF);
  memcpy(header + 8, "WAVE", 4);
  
  memcpy(header + 12, "fmt ", 4);
  uint32_t subchunk1Size = 16;
  header[16] = (uint8_t)(subchunk1Size & 0xFF);
  header[17] = (uint8_t)((subchunk1Size >> 8) & 0xFF);
  header[18] = (uint8_t)((subchunk1Size >> 16) & 0xFF);
  header[19] = (uint8_t)((subchunk1Size >> 24) & 0xFF);
  
  uint16_t audioFormat = 1;
  header[20] = (uint8_t)(audioFormat & 0xFF);
  header[21] = (uint8_t)((audioFormat >> 8) & 0xFF);
  
  uint16_t numChannels = 1;
  header[22] = (uint8_t)(numChannels & 0xFF);
  header[23] = (uint8_t)((numChannels >> 8) & 0xFF);
  
  uint32_t sampleRate = SAMPLE_RATE;
  header[24] = (uint8_t)(sampleRate & 0xFF);
  header[25] = (uint8_t)((sampleRate >> 8) & 0xFF);
  header[26] = (uint8_t)((sampleRate >> 16) & 0xFF);
  header[27] = (uint8_t)((sampleRate >> 24) & 0xFF);
  
  uint32_t byteRate = SAMPLE_RATE * numChannels * 2;
  header[28] = (uint8_t)(byteRate & 0xFF);
  header[29] = (uint8_t)((byteRate >> 8) & 0xFF);
  header[30] = (uint8_t)((byteRate >> 16) & 0xFF);
  header[31] = (uint8_t)((byteRate >> 24) & 0xFF);
  
  uint16_t blockAlign = numChannels * 2;
  header[32] = (uint8_t)(blockAlign & 0xFF);
  header[33] = (uint8_t)((blockAlign >> 8) & 0xFF);
  
  uint16_t bitsPerSample = 16;
  header[34] = (uint8_t)(bitsPerSample & 0xFF);
  header[35] = (uint8_t)((bitsPerSample >> 8) & 0xFF);
  
  memcpy(header + 36, "data", 4);
  uint32_t subchunk2Size = recordSeconds * SAMPLE_RATE * 2;
  header[40] = (uint8_t)(subchunk2Size & 0xFF);
  header[41] = (uint8_t)((subchunk2Size >> 8) & 0xFF);
  header[42] = (uint8_t)((subchunk2Size >> 16) & 0xFF);
  header[43] = (uint8_t)((subchunk2Size >> 24) & 0xFF);
  
  audioFile.write(header, 44);
  
  int32_t buffer[BUFFER_SIZE];
  size_t bytes_read = 0;
  int16_t sample16[BUFFER_SIZE];
  
  unsigned long startTime = millis();
  unsigned long endTime = startTime + (recordSeconds * 1000);
  
  while (millis() < endTime) {
    i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);
    
    for (int i = 0; i < BUFFER_SIZE; i++) {
      sample16[i] = buffer[i] >> 16;
    }
    
    audioFile.write((const uint8_t *)sample16, BUFFER_SIZE * 2);
    
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
  }
  
  audioFile.close();
  Serial.println("Ses kaydı tamamlandı");
  return true;
}

bool sendAudioToServer(const char* filename) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi bağlantısı yok!");
    return false;
  }
  
  Serial.println("Ses dosyası sunucuya gönderiliyor...");
  
  if (!SD.exists(filename)) {
    Serial.println("Ses dosyası bulunamadı");
    return false;
  }
  
  File audioFile = SD.open(filename);
  if (!audioFile) {
    Serial.println("Ses dosyası açılamadı");
    return false;
  }
  
  size_t fileSize = audioFile.size();
  
  HTTPClient http;
  String endpoint = String(API_SERVER) + API_TRANSCRIBE;
  http.begin(endpoint);
  
  String boundary = "ESP32FormBoundary";
  String contentType = "multipart/form-data; boundary=" + boundary;
  http.addHeader("Content-Type", contentType);
  
  uint8_t *buffer = new uint8_t[1024];
  if (!buffer) {
    Serial.println("Bellek tahsis edilemedi");
    audioFile.close();
    return false;
  }
  
  String startData = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
  String endData = "\r\n--" + boundary + "--\r\n";
  
  http.setRequestTimeout(30000); 
  
  http.addHeader("Content-Length", String(startData.length() + fileSize + endData.length()));
  
  WiFiClient *client = http.getStreamPtr();
  
  client->print(startData);
  
  size_t totalBytesRead = 0;
  size_t bytesRead = 0;
  audioFile.seek(0);
  
  while (totalBytesRead < fileSize) {
    bytesRead = audioFile.read(buffer, 1024);
    if (bytesRead == 0) break;
    
    client->write(buffer, bytesRead);
    totalBytesRead += bytesRead;
    
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
  }
  
  client->print(endData);
  
  delete[] buffer;
  audioFile.close();
  
  int httpCode = http.POST("");
  http.end();
  
  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Ses dosyası başarıyla gönderildi");
    return true;
  } else {
    Serial.printf("Ses dosyası gönderimi başarısız. HTTP kodu: %d\n", httpCode);
    return false;
  }
}

bool getResponseFromServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi bağlantısı yok!");
    return false;
  }
  
  Serial.println("Sunucudan yanıt alınıyor...");
  
  HTTPClient http;
  String endpoint = String(API_SERVER) + API_RESPOND;
  http.begin(endpoint);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    File responseFile = SD.open("/response.wav", FILE_WRITE);
    if (!responseFile) {
      Serial.println("Yanıt dosyası oluşturulamadı");
      http.end();
      return false;
    }
    
    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[1024];
    size_t totalBytes = 0;
    size_t bytesRead = 0;
    
    while (http.connected() && (totalBytes < http.getSize())) {
      size_t available = stream->available();
      if (available) {
        size_t readSize = min(available, sizeof(buffer));
        bytesRead = stream->readBytes(buffer, readSize);
        responseFile.write(buffer, bytesRead);
        totalBytes += bytesRead;
        
        digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
      }
      delay(1);
    }
    
    responseFile.close();
    Serial.println("Yanıt dosyası başarıyla kaydedildi");
    http.end();
    return true;
  } else {
    Serial.printf("Yanıt alınamadı. HTTP kodu: %d\n", httpCode);
    http.end();
    return false;
  }
}

void playAudioFile(const char* filename) {
  Serial.printf("Ses dosyası çalınıyor: %s\n", filename);
  
  if (!SD.exists(filename)) {
    Serial.println("Ses dosyası bulunamadı");
    return;
  }
  
  File audioFile = SD.open(filename);
  if (!audioFile) {
    Serial.println("Ses dosyası açılamadı");
    return;
  }
  
  audioFile.seek(44);
  
  uint8_t buffer[BUFFER_SIZE * 2];
  size_t bytes_written = 0;
  
  while (audioFile.available()) {
    size_t bytesRead = audioFile.read(buffer, sizeof(buffer));
    i2s_write(I2S_NUM_1, buffer, bytesRead, &bytes_written, portMAX_DELAY);
  }
  
  audioFile.close();
  Serial.println("Ses çalma tamamlandı");
}

void checkRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }
  
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += (rfid.uid.uidByte[i] < 0x10 ? "0" : "") + String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  
  Serial.print("RFID Kart algılandı: ");
  Serial.println(uid);
  
  if (isAuthorizedCard(uid)) {
    logEvent("Yetkili kart algılandı: " + uid);
    playAudioFile("/success.wav");
  } else {
    logEvent("Yetkisiz kart algılandı: " + uid);
    playAudioFile("/unauthorized.wav");
  }
  
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

bool isAuthorizedCard(String uid) {
  for (int i = 0; i < NUM_AUTHORIZED_CARDS; i++) {
    if (uid.equals(AUTHORIZED_CARDS[i])) {
      return true;
    }
  }
  return false;
}

void logEvent(const String& event) {
  unsigned long now = millis();
  unsigned long seconds = now / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  seconds %= 60;
  minutes %= 60;
  hours %= 24;
  
  char timestamp[20];
  sprintf(timestamp, "%02lu:%02lu:%02lu", hours, minutes, seconds);
  
  String logEntry = String(timestamp) + " - " + event;
  
  if (SD.exists("/assistant_log.txt")) {
    File logFile = SD.open("/assistant_log.txt", FILE_APPEND);
    if (logFile) {
      logFile.println(logEntry);
      logFile.close();
    }
  } else {
    File logFile = SD.open("/assistant_log.txt", FILE_WRITE);
    if (logFile) {
      logFile.println("Zaman - Olay");
      logFile.println(logEntry);
      logFile.close();
    }
  }
  
  Serial.println(logEntry);
}

void blinkLED(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(STATUS_LED, HIGH);
    delay(delayMs);
    digitalWrite(STATUS_LED, LOW);
    delay(delayMs);
  }
}

void showProcessingStatus() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(STATUS_LED, HIGH);
    delay(50);
    digitalWrite(STATUS_LED, LOW);
    delay(50);
  }
}

void showErrorStatus() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(STATUS_LED, HIGH);
    delay(300);
    digitalWrite(STATUS_LED, LOW);
    delay(300);
  }
}

void showReadyStatus() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(STATUS_LED, HIGH);
    delay(100);
    digitalWrite(STATUS_LED, LOW);
    delay(100);
  }
} 