/*
 * Micro Features Model Settings for ESP32-S3 Voice Assistant
 * Defines common settings for the wake word detection model
 */

#ifndef MICRO_FEATURES_MICRO_MODEL_SETTINGS_H_
#define MICRO_FEATURES_MICRO_MODEL_SETTINGS_H_

// Keeping these as constant expressions allows them to be used
// as compile-time constants and in arrays.
constexpr int kMaxAudioSampleSize = 512;
constexpr int kAudioSampleFrequency = 16000;

// The settings for the feature extractor
constexpr int kFeatureSize = 40;
constexpr int kFeatureSliceSize = 40;
constexpr int kFeatureSliceCount = 49;
constexpr int kFeatureElementCount = (kFeatureSliceCount * kFeatureSize);
constexpr int kFeatureSliceDurationMs = 30;
constexpr int kFeatureSliceStrideMs = 20;

// Parameters for the spectral analysis
constexpr int kSpectrogramChannels = 1;
constexpr int kWindowSize = 640;
constexpr int kWindowStepSamples = 320;
constexpr int kFFTSize = 1024;
constexpr int kFFTHalfSize = 513;

// Labels for the model categories
constexpr char kCategoryLabels[4][20] = {
    "hey_assistant",
    "yes",
    "no",
    "unknown",
};

#endif  // MICRO_FEATURES_MICRO_MODEL_SETTINGS_H_ 