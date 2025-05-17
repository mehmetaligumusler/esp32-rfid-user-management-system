/*
 * Audio Provider for ESP32-S3 Voice Assistant
 * Handles audio capture from I2S microphone
 */

#ifndef AUDIO_PROVIDER_H_
#define AUDIO_PROVIDER_H_

#include <Arduino.h>
#include <I2S.h>
#include "micro_features_micro_model_settings.h"

// Feature data array used for wake word detection
float feature_data[kFeatureElementCount];

// Stores current audio data
int16_t g_audio_capture_buffer[kMaxAudioSampleSize];
// Working buffer for audio processing
int16_t g_audio_output_buffer[kMaxAudioSampleSize];

// Fill the feature data from audio input
void GetAudioSamples(int16_t* audio_samples, int audio_samples_size,
                    int16_t* audio_samples_size_out) {
  // Check if size is appropriate
  if (audio_samples_size != kMaxAudioSampleSize) {
    return;
  }
  
  // Read audio samples from I2S
  size_t bytes_read = 0;
  int32_t samples_buffer[kMaxAudioSampleSize];
  
  // Read I2S data
  i2s_read(I2S_NUM_0, samples_buffer, kMaxAudioSampleSize * sizeof(int32_t), &bytes_read, portMAX_DELAY);
  
  // Convert 32-bit samples to 16-bit and copy to output buffer
  for (int i = 0; i < kMaxAudioSampleSize; i++) {
    // Shift 32-bit data to get 16-bit sample
    audio_samples[i] = samples_buffer[i] >> 16;
  }
  
  *audio_samples_size_out = kMaxAudioSampleSize;
}

// Extract audio features for model inference
TfLiteStatus GetAudioFeatures(tflite::ErrorReporter* error_reporter,
                             int32_t time_ms, int32_t* recording_win_ms,
                             int32_t* recording_offset_ms) {
  // Set window parameters (30ms window)
  *recording_win_ms = 30;
  *recording_offset_ms = time_ms % *recording_win_ms;
  
  // Get audio samples
  int16_t audio_samples_size = 0;
  GetAudioSamples(g_audio_capture_buffer, kMaxAudioSampleSize, &audio_samples_size);
  
  // Copy the samples into input buffer
  for (int i = 0; i < audio_samples_size; ++i) {
    g_audio_output_buffer[i] = g_audio_capture_buffer[i];
  }
  
  return kTfLiteOk;
}

#endif  // AUDIO_PROVIDER_H_ 