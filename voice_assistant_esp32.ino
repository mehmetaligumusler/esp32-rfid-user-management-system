/*
 * ESP32-S3 Voice Assistant with LLM Integration
 * Uses INMP441 I2S Microphone, Speaker, SD Card and Gemini Free API
 * Integrates with existing RFID access system
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <I2S.h>
#include <SD.h>
#include <SPI.h>
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

// Network credentials - update to match existing RFID project
const char* ssid = "Xiaomi_13_Lite";
const char* password = "123456789....";

// Gemini API Configuration
const char* GEMINI_API_KEY = "AIzaSyCheTPf70PepZQfuydwi7WUqG4WIuKE1kc";
const char* GEMINI_API_ENDPOINT = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent";
const bool USE_TURKISH = true; // Set to true for Turkish responses
const int MAX_RESPONSE_SIZE = 2048;

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

// Function prototypes
void initMicrophone();
void initSpeaker();
void initSDCard();
void initTensorFlow();
void initWiFi();
bool detectWakeWord();
bool recordAudio(const char* filename, int recordSeconds);
String transcribeAudio(const char* filename);
String sendToGemini(const String& query);
String parseGeminiResponse(const String& jsonResponse);
bool textToSpeech(const String& text, const char* outputFile);
void playAudioFile(const char* filename);
void logEvent(const String& event);

// Simple conversation history for context (1 entry)
String lastQuery = "";
String lastResponse = "";

void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial monitor time to open
  
  Serial.println("ESP32-S3 Sesli Asistan Başlatılıyor...");
  
  // Initialize status LED
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);
  
  // Connect to Wi-Fi
  initWiFi();
  
  // Initialize microphone, speaker, SD card, and TensorFlow
  initMicrophone();
  initSpeaker();
  initSDCard();
  initTensorFlow();
  
  logEvent("Sesli asistan sistemi başlatıldı");
  
  // Blink LED to indicate ready state
  for (int i = 0; i < 3; i++) {
    digitalWrite(STATUS_LED, HIGH);
    delay(100);
    digitalWrite(STATUS_LED, LOW);
    delay(100);
  }
}

void loop() {
  // Check for wake word
  if (detectWakeWord()) {
    digitalWrite(STATUS_LED, HIGH); // Turn on LED to indicate listening
    
    Serial.println("Uyandırma kelimesi algılandı: 'Hey Assistant'");
    
    // Play a sound to indicate wake word detected
    playAudioFile("/system/wake_detected.wav");
    
    // Record user's query
    if (recordAudio("/query.wav", 5)) { // Record for 5 seconds max
      logEvent("Ses kaydı tamamlandı");
      
      // For development purposes, let's use a hardcoded query
      // In a real implementation, we would use the transcribeAudio function
      
      // Ask the user to speak
      Serial.println("Sorunuzu söyleyin...");
      
      // Simulate a query (in production would come from transcribeAudio)
      String query;
      if (USE_TURKISH) {
        query = "Bugün hava nasıl olacak?"; // Default query in Turkish
      } else {
        query = "What's the weather today?"; // Default query in English
      }
      
      Serial.println("Algılanan Soru: " + query);
      logEvent("Soru: " + query);
      
      // Get response from Gemini API
      String response = sendToGemini(query);
      logEvent("Gemini cevabı: " + response);
      
      // Store for conversation history
      lastQuery = query;
      lastResponse = response;
      
      // In a real implementation, we would convert text to speech here
      // textToSpeech(response, "/response.wav");
      // playAudioFile("/response.wav");
      
      // For now, just print the response
      Serial.println("Asistan Cevabı: " + response);
    }
    
    digitalWrite(STATUS_LED, LOW); // Turn off LED
  }
  
  delay(100); // Short delay to prevent busy-waiting
}

// Initialize WiFi connection
void initWiFi() {
  Serial.print("WiFi ağına bağlanılıyor: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  // Wait for connection with timeout
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED)); // Blink LED while connecting
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

// Initialize I2S microphone
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

// Initialize I2S speaker
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

// Initialize SD card
void initSDCard() {
  Serial.println("SD kart başlatılıyor...");
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  
  if (!SD.begin(SD_CS)) {
    Serial.println("SD kart başlatılamadı!");
    return;
  }
  
  // Create system directories if they don't exist
  if (!SD.exists("/system")) {
    SD.mkdir("/system");
  }
  
  Serial.println("SD kart başlatıldı");
}

// Initialize TensorFlow Lite for wake word detection
void initTensorFlow() {
  Serial.println("TensorFlow başlatılıyor...");
  
  // Set up logging
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;
  
  // Map the model into a usable data structure
  model = tflite::GetModel(g_tiny_conv_micro_features_model_data);
  
  // Pull in only needed operations
  static tflite::MicroMutableOpResolver<4> micro_op_resolver;
  micro_op_resolver.AddDepthwiseConv2D();
  micro_op_resolver.AddFullyConnected();
  micro_op_resolver.AddSoftmax();
  micro_op_resolver.AddReshape();
  
  // Build an interpreter to run the model
  static tflite::MicroInterpreter static_interpreter(
      model, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;
  
  // Allocate memory from the tensor_arena for the model's tensors
  interpreter->AllocateTensors();
  
  // Get information about the memory area to use for the model's input
  model_input = interpreter->input(0);
  
  // Set up feature provider and command recognizer
  static FeatureProvider static_feature_provider(kFeatureElementCount,
                                               feature_data);
  feature_provider = &static_feature_provider;
  
  static RecognizeCommands static_recognizer(error_reporter);
  recognizer = &static_recognizer;
  
  previous_time = 0;
  
  Serial.println("TensorFlow başlatıldı");
}

// Detect wake word using TensorFlow Lite
bool detectWakeWord() {
  // Get current time for determining if a command was recognized
  const int32_t current_time = millis();
  
  // Fetch the spectrogram for the current time
  int how_many_new_slices = 0;
  TfLiteStatus feature_status = feature_provider->PopulateFeatureData(
      error_reporter, previous_time, current_time, &how_many_new_slices);
  
  if (feature_status != kTfLiteOk) {
    error_reporter->Report("Feature generation failed");
    return false;
  }
  
  previous_time = current_time;
  
  // If not enough new data, return
  if (how_many_new_slices == 0) {
    return false;
  }
  
  // Copy feature data to the input tensor
  for (int i = 0; i < kFeatureElementCount; i++) {
    model_input->data.f[i] = feature_data[i];
  }
  
  // Run the model
  TfLiteStatus invoke_status = interpreter->Invoke();
  if (invoke_status != kTfLiteOk) {
    error_reporter->Report("Invoke failed");
    return false;
  }
  
  // Get output tensor data
  TfLiteTensor* output = interpreter->output(0);
  
  // Determine whether a command was recognized
  const char* found_command = nullptr;
  uint8_t score = 0;
  bool is_new_command = false;
  TfLiteStatus process_status = recognizer->ProcessLatestResults(
      output, current_time, &found_command, &score, &is_new_command);
  
  if (process_status != kTfLiteOk) {
    error_reporter->Report("Recognition process failed");
    return false;
  }
  
  // Return true if the wake word was detected
  if (is_new_command && (score > 200) && (strcmp(found_command, "hey_assistant") == 0)) {
    Serial.println("Wake word detected!");
    return true;
  }
  
  return false;
}

// Record audio to SD card
bool recordAudio(const char* filename, int recordSeconds) {
  Serial.printf("Ses kaydı başlatılıyor: %s...\n", filename);
  
  File audioFile = SD.open(filename, FILE_WRITE);
  if (!audioFile) {
    Serial.println("Ses dosyası oluşturulamadı");
    return false;
  }
  
  // WAV file header
  uint8_t header[44];
  // RIFF chunk descriptor
  memcpy(header, "RIFF", 4);
  uint32_t fileSize = recordSeconds * SAMPLE_RATE * 2 + 36;  // 16-bit mono
  header[4] = (uint8_t)(fileSize & 0xFF);
  header[5] = (uint8_t)((fileSize >> 8) & 0xFF);
  header[6] = (uint8_t)((fileSize >> 16) & 0xFF);
  header[7] = (uint8_t)((fileSize >> 24) & 0xFF);
  memcpy(header + 8, "WAVE", 4);
  
  // "fmt " subchunk
  memcpy(header + 12, "fmt ", 4);
  uint32_t subchunk1Size = 16;  // PCM
  header[16] = (uint8_t)(subchunk1Size & 0xFF);
  header[17] = (uint8_t)((subchunk1Size >> 8) & 0xFF);
  header[18] = (uint8_t)((subchunk1Size >> 16) & 0xFF);
  header[19] = (uint8_t)((subchunk1Size >> 24) & 0xFF);
  
  uint16_t audioFormat = 1;  // PCM
  header[20] = (uint8_t)(audioFormat & 0xFF);
  header[21] = (uint8_t)((audioFormat >> 8) & 0xFF);
  
  uint16_t numChannels = 1;  // Mono
  header[22] = (uint8_t)(numChannels & 0xFF);
  header[23] = (uint8_t)((numChannels >> 8) & 0xFF);
  
  uint32_t sampleRate = SAMPLE_RATE;
  header[24] = (uint8_t)(sampleRate & 0xFF);
  header[25] = (uint8_t)((sampleRate >> 8) & 0xFF);
  header[26] = (uint8_t)((sampleRate >> 16) & 0xFF);
  header[27] = (uint8_t)((sampleRate >> 24) & 0xFF);
  
  uint32_t byteRate = SAMPLE_RATE * numChannels * 2;  // SampleRate * NumChannels * BitsPerSample/8
  header[28] = (uint8_t)(byteRate & 0xFF);
  header[29] = (uint8_t)((byteRate >> 8) & 0xFF);
  header[30] = (uint8_t)((byteRate >> 16) & 0xFF);
  header[31] = (uint8_t)((byteRate >> 24) & 0xFF);
  
  uint16_t blockAlign = numChannels * 2;  // NumChannels * BitsPerSample/8
  header[32] = (uint8_t)(blockAlign & 0xFF);
  header[33] = (uint8_t)((blockAlign >> 8) & 0xFF);
  
  uint16_t bitsPerSample = 16;
  header[34] = (uint8_t)(bitsPerSample & 0xFF);
  header[35] = (uint8_t)((bitsPerSample >> 8) & 0xFF);
  
  // "data" subchunk
  memcpy(header + 36, "data", 4);
  uint32_t subchunk2Size = recordSeconds * SAMPLE_RATE * 2;  // NumSamples * NumChannels * BitsPerSample/8
  header[40] = (uint8_t)(subchunk2Size & 0xFF);
  header[41] = (uint8_t)((subchunk2Size >> 8) & 0xFF);
  header[42] = (uint8_t)((subchunk2Size >> 16) & 0xFF);
  header[43] = (uint8_t)((subchunk2Size >> 24) & 0xFF);
  
  // Write header to file
  audioFile.write(header, 44);
  
  // Buffer for audio data
  int32_t buffer[BUFFER_SIZE];
  size_t bytes_read = 0;
  int16_t sample16[BUFFER_SIZE];
  
  // Record for specified duration
  unsigned long startTime = millis();
  unsigned long endTime = startTime + (recordSeconds * 1000);
  
  while (millis() < endTime) {
    // Read audio data from I2S
    i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);
    
    // Convert 32-bit samples to 16-bit
    for (int i = 0; i < BUFFER_SIZE; i++) {
      // Convert 24-bit signed value to 16-bit signed value
      sample16[i] = buffer[i] >> 16;
    }
    
    // Write audio data to file
    audioFile.write((const uint8_t *)sample16, BUFFER_SIZE * 2);
    
    // Blink LED to indicate recording
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
  }
  
  audioFile.close();
  Serial.println("Ses kaydı tamamlandı");
  return true;
}

// Send query to Gemini API and get response
String sendToGemini(const String& query) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi bağlantısı yok!");
    return "İnternet bağlantısı yok. Lütfen WiFi bağlantınızı kontrol edin.";
  }
  
  Serial.println("Gemini API'ye sorgu gönderiliyor...");
  
  // Prepare complete URL with API key
  String url = String(GEMINI_API_ENDPOINT) + "?key=" + GEMINI_API_KEY;
  
  // Create HTTP client
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON payload
  // If Turkish is enabled, append a language instruction
  String systemInstruction = "";
  if (USE_TURKISH) {
    systemInstruction = "Sen bir Türkçe konuşan sesli asistandır. Türkçe cevap ver. ";
  }
  
  // Include conversation history if available
  String fullQuery = query;
  if (lastQuery.length() > 0 && lastResponse.length() > 0) {
    fullQuery = systemInstruction + "Önceki konuşma: Kullanıcı: " + lastQuery + " Asistan: " + 
                lastResponse + " Şimdi: Kullanıcı: " + query;
  } else {
    fullQuery = systemInstruction + query;
  }
  
  // Create JSON document
  DynamicJsonDocument doc(1024);
  JsonArray contents = doc.createNestedArray("contents");
  JsonObject content = contents.createNestedObject();
  JsonArray parts = content.createNestedArray("parts");
  JsonObject part = parts.createNestedObject();
  part["text"] = fullQuery;
  
  // Serialize JSON to string
  String payload;
  serializeJson(doc, payload);
  
  Serial.println("API İsteği: " + payload);
  
  // Send POST request
  int httpResponseCode = http.POST(payload);
  String response = "";
  
  // Check response
  if (httpResponseCode > 0) {
    Serial.printf("HTTP Cevap kodu: %d\n", httpResponseCode);
    response = http.getString();
    Serial.println("Ham API cevabı: " + response);
    
    // Parse the JSON response
    return parseGeminiResponse(response);
  } else {
    Serial.printf("HTTP Hata kodu: %d\n", httpResponseCode);
    return "API'ye bağlanırken bir hata oluştu.";
  }
  
  http.end();
}

// Parse Gemini API response to extract the assistant's message
String parseGeminiResponse(const String& jsonResponse) {
  // Create a dynamic JSON document with enough capacity for the response
  DynamicJsonDocument doc(MAX_RESPONSE_SIZE);
  
  // Parse the JSON response
  DeserializationError error = deserializeJson(doc, jsonResponse);
  
  // Check for parsing errors
  if (error) {
    Serial.print("JSON ayrıştırma hatası: ");
    Serial.println(error.c_str());
    return "Yanıt ayrıştırılamadı. Hata: " + String(error.c_str());
  }
  
  // Extract the text response from the JSON structure
  try {
    // Navigate through the JSON structure to find the text content
    // Structure: {"candidates":[{"content":{"parts":[{"text":"Response text"}]}}]}
    
    if (doc.containsKey("candidates") && doc["candidates"].is<JsonArray>()) {
      JsonArray candidates = doc["candidates"].as<JsonArray>();
      
      if (candidates.size() > 0) {
        JsonObject content = candidates[0]["content"];
        
        if (content.containsKey("parts") && content["parts"].is<JsonArray>()) {
          JsonArray parts = content["parts"].as<JsonArray>();
          
          if (parts.size() > 0 && parts[0].containsKey("text")) {
            String text = parts[0]["text"].as<String>();
            return text;
          }
        }
      }
    }
    
    // If we get here, the JSON structure wasn't as expected
    Serial.println("Beklenmeyen JSON yapısı");
    return "Gemini yanıtı istenen formatta değil.";
    
  } catch (const std::exception& e) {
    Serial.printf("JSON ayrıştırma istisna hatası: %s\n", e.what());
    return "JSON ayrıştırma sırasında bir hata oluştu.";
  }
}

// Transcribe audio to text (not implemented - would require external API)
String transcribeAudio(const char* filename) {
  // In a real implementation, this function would send the audio file to a speech-to-text API
  // and return the transcribed text. For this example, we'll return a placeholder.
  
  if (USE_TURKISH) {
    return "Merhaba, bugün hava nasıl olacak?";
  } else {
    return "Hello, what's the weather today?";
  }
}

// Convert text to speech (not implemented - would require external API)
bool textToSpeech(const String& text, const char* outputFile) {
  // This function would normally send the text to a TTS API and save the response
  // to the specified output file. For this example, just return true.
  Serial.println("Metin sese dönüştürülüyor (simüle edildi): " + text);
  return true;
}

// Play audio file from SD card
void playAudioFile(const char* filename) {
  Serial.printf("Ses dosyası çalınıyor: %s\n", filename);
  
  // Check if file exists
  if (!SD.exists(filename)) {
    Serial.println("Ses dosyası bulunamadı");
    return;
  }
  
  // Open the audio file
  File audioFile = SD.open(filename);
  if (!audioFile) {
    Serial.println("Ses dosyası açılamadı");
    return;
  }
  
  // Skip WAV header (44 bytes)
  audioFile.seek(44);
  
  // Buffer for audio data
  uint8_t buffer[BUFFER_SIZE * 2]; // * 2 because 16-bit samples
  size_t bytes_written = 0;
  
  // Read and play audio data
  while (audioFile.available()) {
    size_t bytesRead = audioFile.read(buffer, sizeof(buffer));
    i2s_write(I2S_NUM_1, buffer, bytesRead, &bytes_written, portMAX_DELAY);
  }
  
  audioFile.close();
  Serial.println("Ses çalma tamamlandı");
}

// Log event to SD card and serial monitor
void logEvent(const String& event) {
  // Get current timestamp (approximate since we don't have RTC)
  unsigned long now = millis();
  unsigned long seconds = now / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  seconds %= 60;
  minutes %= 60;
  hours %= 24;
  
  char timestamp[20];
  sprintf(timestamp, "%02lu:%02lu:%02lu", hours, minutes, seconds);
  
  // Format log entry
  String logEntry = String(timestamp) + " - " + event;
  
  // Open log file on SD card
  if (SD.exists("/assistant_log.txt")) {
    File logFile = SD.open("/assistant_log.txt", FILE_APPEND);
    if (logFile) {
      logFile.println(logEntry);
      logFile.close();
    }
  } else {
    // Create new log file
    File logFile = SD.open("/assistant_log.txt", FILE_WRITE);
    if (logFile) {
      logFile.println("Zaman - Olay");
      logFile.println(logEntry);
      logFile.close();
    }
  }
  
  // Also print to serial console
  Serial.println(logEntry);
} 