/*
 * Feature Provider for ESP32-S3 Voice Assistant
 * Handles audio features extraction for wake word detection
 */

#ifndef FEATURE_PROVIDER_H_
#define FEATURE_PROVIDER_H_

#include "audio_provider.h"
#include "micro_features_micro_features_generator.h"
#include "micro_features_micro_model_settings.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

// Feature Provider class to handle audio feature generation
class FeatureProvider {
 public:
  // Constructor
  FeatureProvider(int feature_size, float* feature_data)
      : feature_size_(feature_size),
        feature_data_(feature_data),
        is_first_run_(true) {
    // Initialize the feature data array
    for (int i = 0; i < feature_size_; ++i) {
      feature_data_[i] = 0.0f;
    }
  }

  // Populate feature data from audio input
  TfLiteStatus PopulateFeatureData(tflite::ErrorReporter* error_reporter,
                                  const int32_t previous_time,
                                  const int32_t current_time,
                                  int* how_many_new_slices) {
    // Calculate how many new audio samples we need for this update
    int32_t recording_win_ms;
    int32_t recording_offset_ms;
    
    GetAudioFeatures(error_reporter, current_time, &recording_win_ms,
                     &recording_offset_ms);
    
    // If this is the first run, make sure we generate a full feature set
    if (is_first_run_) {
      TfLiteStatus generate_status = GenerateFeatures(
          error_reporter, g_audio_output_buffer, g_audio_capture_buffer, 
          recording_win_ms, recording_offset_ms, feature_data_, how_many_new_slices);
      if (generate_status != kTfLiteOk) {
        return generate_status;
      }
      is_first_run_ = false;
      return kTfLiteOk;
    }
    
    // If we're not at the first run, we only want to use the newly available data
    const int32_t time_since_last_run_ms = current_time - previous_time;
    
    // If there's been less than our minimum waiting period, don't trigger an update
    if (time_since_last_run_ms < recording_win_ms) {
      *how_many_new_slices = 0;
      return kTfLiteOk;
    }
    
    // Calculate how many new slices need to be calculated
    const int32_t new_slices = time_since_last_run_ms / recording_win_ms;
    
    // If there are no new audio samples, return without processing
    if (new_slices == 0) {
      *how_many_new_slices = 0;
      return kTfLiteOk;
    }
    
    // Process the audio samples to generate new feature data
    TfLiteStatus generate_status = GenerateFeatures(
        error_reporter, g_audio_output_buffer, g_audio_capture_buffer, 
        recording_win_ms, recording_offset_ms, feature_data_, how_many_new_slices);
    
    if (generate_status != kTfLiteOk) {
      return generate_status;
    }
    
    return kTfLiteOk;
  }

 private:
  int feature_size_;
  float* feature_data_;
  bool is_first_run_;
  
  // Generate MFCC features from audio data
  TfLiteStatus GenerateFeatures(tflite::ErrorReporter* error_reporter,
                               const int16_t* audio_data,
                               const int16_t* audio_samples,
                               int32_t recording_win_ms,
                               int32_t recording_offset_ms,
                               float* feature_data,
                               int* num_samples_read) {
    // Clear feature data
    for (int i = 0; i < feature_size_; ++i) {
      feature_data[i] = 0.0f;
    }
    
    // Generate MFCC features
    if (recording_win_ms > 0) {
      // Use mini_speech to extract MFCC features
      size_t num_samples_per_slice = 480; // 30ms at 16kHz
      int8_t mfcc_output[kFeatureElementCount];
      
      // Extract features directly from the audio samples
      int slices_needed = kFeatureSliceCount;
      if (slices_needed > 0) {
        *num_samples_read = 0;
        for (int i = 0; i < slices_needed; ++i) {
          // Get features from audio samples
          uint32_t mfcc_status = GenerateMicroFeatures(
              error_reporter, audio_samples + (i * num_samples_per_slice),
              num_samples_per_slice, mfcc_output);
              
          // Copy to the output array
          for (int j = 0; j < kFeatureSize; ++j) {
            feature_data[i * kFeatureSize + j] = mfcc_output[j] / 128.0f;
          }
          
          *num_samples_read += 1;
        }
      }
    }
    
    return kTfLiteOk;
  }
};

#endif  // FEATURE_PROVIDER_H_ 