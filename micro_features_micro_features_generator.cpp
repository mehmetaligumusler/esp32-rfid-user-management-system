/*
 * Micro Features Generator Implementation for ESP32-S3 Voice Assistant
 * Implements MFCC feature extraction for wake word detection
 */

#include "micro_features_micro_features_generator.h"

#include <cmath>
#include <cstring>

#include "micro_features_micro_model_settings.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

namespace {
// Mel-frequency Cepstral Coefficients calculation
const int kNoMelBins = 40;
const int kNoMfccFeatures = 40;
const float kFeatureRangeMin = -247.0f;
const float kFeatureRangeMax = 30.0f;

// Allocate working buffers for MFCC calculation
float spectrum_[kFFTHalfSize];
float mel_energies_[kNoMelBins];
float mfcc_output_[kNoMfccFeatures];

// Initialize mel filter bank
// Simplified mel frequency calculation
inline float MelScale(float frequency) {
  return 1127.0f * logf(1.0f + frequency / 700.0f);
}

// Apply FFT to the audio data
void ApplyFFT(const int16_t* input, int input_size, float* output) {
  // Simple real-valued FFT implementation
  // This is a placeholder - a real implementation would use ESP32's FFT functions
  
  for (int i = 0; i < kFFTHalfSize; ++i) {
    float real_val = 0.0f;
    float imag_val = 0.0f;
    
    for (int j = 0; j < input_size; ++j) {
      float angle = 2 * M_PI * i * j / kFFTSize;
      real_val += input[j] * cosf(angle);
      imag_val -= input[j] * sinf(angle);
    }
    
    // Calculate magnitude
    output[i] = sqrtf(real_val * real_val + imag_val * imag_val) / input_size;
  }
}

// Apply mel filter bank to the spectrum
void ApplyMelFilterBank(const float* spectrum, float* mel_energies) {
  // This is a simplified version - a real implementation would use precomputed filters
  float mel_low_freq = MelScale(0.0f);
  float mel_high_freq = MelScale(kAudioSampleFrequency / 2);
  float mel_freq_delta = (mel_high_freq - mel_low_freq) / (kNoMelBins + 1);
  
  // Clear mel energies
  memset(mel_energies, 0, kNoMelBins * sizeof(float));
  
  // For each bin, calculate energy
  for (int bin = 0; bin < kNoMelBins; ++bin) {
    float left_mel = mel_low_freq + bin * mel_freq_delta;
    float center_mel = left_mel + mel_freq_delta;
    float right_mel = center_mel + mel_freq_delta;
    
    // Convert mel frequencies back to Hz
    float left_freq = 700.0f * (expf(left_mel / 1127.0f) - 1.0f);
    float center_freq = 700.0f * (expf(center_mel / 1127.0f) - 1.0f);
    float right_freq = 700.0f * (expf(right_mel / 1127.0f) - 1.0f);
    
    // Convert Hz to FFT bin indices
    int left_bin = static_cast<int>(left_freq * kFFTSize / kAudioSampleFrequency);
    int center_bin = static_cast<int>(center_freq * kFFTSize / kAudioSampleFrequency);
    int right_bin = static_cast<int>(right_freq * kFFTSize / kAudioSampleFrequency);
    
    // Ensure bins are within bounds
    left_bin = (left_bin < 0) ? 0 : left_bin;
    right_bin = (right_bin > kFFTHalfSize - 1) ? kFFTHalfSize - 1 : right_bin;
    
    // Apply triangular filter
    for (int i = left_bin; i <= right_bin; ++i) {
      float weight = 0.0f;
      if (i < center_bin) {
        weight = (i - left_bin) / static_cast<float>(center_bin - left_bin);
      } else {
        weight = (right_bin - i) / static_cast<float>(right_bin - center_bin);
      }
      
      // Add weighted energy
      mel_energies[bin] += weight * spectrum[i];
    }
    
    // Apply log
    if (mel_energies[bin] < 1e-10) {
      mel_energies[bin] = 1e-10;
    }
    mel_energies[bin] = logf(mel_energies[bin]);
  }
}

// Apply DCT to convert mel energies to MFCC
void ApplyDCT(const float* mel_energies, float* mfcc_output) {
  // Simplified DCT implementation
  for (int i = 0; i < kNoMfccFeatures; ++i) {
    float sum = 0.0f;
    for (int j = 0; j < kNoMelBins; ++j) {
      sum += mel_energies[j] * cosf(M_PI * i * (j + 0.5f) / kNoMelBins);
    }
    mfcc_output[i] = sum;
  }
}

// Convert float MFCC values to quantized int8_t
void Quantize(const float* mfcc_output, int8_t* quantized_output) {
  for (int i = 0; i < kNoMfccFeatures; ++i) {
    // Scale to int8_t range
    float scaled_value = (mfcc_output[i] - kFeatureRangeMin) /
                         (kFeatureRangeMax - kFeatureRangeMin) * 255.0f - 128.0f;
    
    // Clamp to int8_t range
    if (scaled_value > 127.0f) {
      scaled_value = 127.0f;
    }
    if (scaled_value < -128.0f) {
      scaled_value = -128.0f;
    }
    
    quantized_output[i] = static_cast<int8_t>(roundf(scaled_value));
  }
}

}  // namespace

// Generate MFCC features from audio data
TfLiteStatus GenerateMicroFeatures(tflite::ErrorReporter* error_reporter,
                                   const int16_t* audio_data,
                                   int audio_data_size,
                                   int8_t* feature_data) {
  // Check if we have enough data
  if (audio_data_size < kWindowSize) {
    error_reporter->Report("Audio data size %d is too small, needs at least %d samples",
                          audio_data_size, kWindowSize);
    return kTfLiteError;
  }
  
  // Apply FFT to get frequency spectrum
  ApplyFFT(audio_data, kWindowSize, spectrum_);
  
  // Apply mel filter bank
  ApplyMelFilterBank(spectrum_, mel_energies_);
  
  // Apply DCT to get MFCC
  ApplyDCT(mel_energies_, mfcc_output_);
  
  // Quantize MFCC values to int8_t
  Quantize(mfcc_output_, feature_data);
  
  return kTfLiteOk;
} 