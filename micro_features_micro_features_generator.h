/*
 * Micro Features Generator for ESP32-S3 Voice Assistant
 * Handles generation of MFCC features from audio data
 */

#ifndef MICRO_FEATURES_MICRO_FEATURES_GENERATOR_H_
#define MICRO_FEATURES_MICRO_FEATURES_GENERATOR_H_

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

// Generates MFCC features from audio data.
// Takes an audio buffer and converts it to features using MFCC algorithm.
// This is a simplified, resource-efficient version to run on ESP32-S3.
TfLiteStatus GenerateMicroFeatures(tflite::ErrorReporter* error_reporter,
                                  const int16_t* audio_data,
                                  int audio_data_size,
                                  int8_t* feature_data);

#endif  // MICRO_FEATURES_MICRO_FEATURES_GENERATOR_H_ 