/*
 * Recognize Commands for ESP32-S3 Voice Assistant
 * Handles recognition of wake word commands
 */

#ifndef RECOGNIZE_COMMANDS_H_
#define RECOGNIZE_COMMANDS_H_

#include <cstring>

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

// Maximum number of commands to track
#define COMMAND_HISTORY_MAX 50

// Scoring parameters
constexpr int kCategoryCount = 4; // "hey_assistant", "yes", "no", "unknown"
constexpr int kCommandThreshold = 200;
constexpr int kMinimumCount = 3;
constexpr int kSuppressCount = 3;
constexpr int64_t kDetectionThresholdMs = 1000;

// Class to recognize and handle wake word commands
class RecognizeCommands {
 public:
  // Constructor
  explicit RecognizeCommands(tflite::ErrorReporter* error_reporter,
                            int32_t average_window_duration_ms = 1000,
                            uint8_t detection_threshold = 200,
                            int32_t suppression_ms = 1500,
                            int32_t minimum_count = 3)
      : error_reporter_(error_reporter),
        average_window_duration_ms_(average_window_duration_ms),
        detection_threshold_(detection_threshold),
        suppression_ms_(suppression_ms),
        minimum_count_(minimum_count),
        previous_results_(),
        previous_top_label_(),
        previous_top_label_time_(0),
        previous_top_score_(0) {
    previous_top_label_[0] = '\0';
  }

  // Process the latest TensorFlow Lite results
  TfLiteStatus ProcessLatestResults(TfLiteTensor* latest_results,
                                   const int32_t current_time_ms,
                                   const char** found_command,
                                   uint8_t* score,
                                   bool* is_new_command) {
    if ((latest_results->dims->size != 2) ||
        (latest_results->dims->data[0] != 1) ||
        (latest_results->dims->data[1] != kCategoryCount)) {
      error_reporter_->Report(
          "The results for recognition should contain %d elements, but there are "
          "%d in an %d-dimensional shape",
          kCategoryCount, latest_results->dims->data[1],
          latest_results->dims->size);
      return kTfLiteError;
    }

    if (latest_results->type != kTfLiteInt8) {
      error_reporter_->Report(
          "The results for recognition should be int8_t elements, but are %d",
          latest_results->type);
      return kTfLiteError;
    }

    // Store current results
    int8_t* scores = latest_results->data.int8;
    
    // Add latest results to history
    if (previous_results_.size() >= COMMAND_HISTORY_MAX) {
      previous_results_.pop_front();
    }
    
    CommandResult new_result = {current_time_ms, scores[0], scores[1], scores[2], scores[3]};
    previous_results_.push_back(new_result);

    // Analyze results
    const char* top_label = nullptr;
    uint8_t top_score = 0;
    bool is_new = false;
    
    TfLiteStatus analysis_status = AnalyzeResults(&top_label, &top_score, &is_new);
    if (analysis_status != kTfLiteOk) {
      return analysis_status;
    }
    
    *found_command = top_label;
    *score = top_score;
    *is_new_command = is_new;
    
    return kTfLiteOk;
  }

 private:
  // Command result structure for history
  struct CommandResult {
    int32_t time;
    int8_t score_hey_assistant;
    int8_t score_yes;
    int8_t score_no;
    int8_t score_unknown;
  };

  // Analyze the stored results and determine if a new command is detected
  TfLiteStatus AnalyzeResults(const char** found_command, uint8_t* score,
                             bool* is_new_command) {
    if (previous_results_.empty()) {
      *found_command = "silence";
      *score = 0;
      *is_new_command = false;
      return kTfLiteOk;
    }
    
    // Calculate average scores across history
    int32_t hey_assistant_score = 0;
    int32_t yes_score = 0;
    int32_t no_score = 0;
    int32_t unknown_score = 0;
    
    int32_t earliest_time = previous_results_.front().time;
    int32_t latest_time = previous_results_.back().time;
    
    // If window is too small, assume no change
    if ((latest_time - earliest_time) < average_window_duration_ms_) {
      *found_command = previous_top_label_;
      *score = previous_top_score_;
      *is_new_command = false;
      return kTfLiteOk;
    }
    
    // Calculate average scores
    for (const auto& previous_result : previous_results_) {
      hey_assistant_score += previous_result.score_hey_assistant;
      yes_score += previous_result.score_yes;
      no_score += previous_result.score_no;
      unknown_score += previous_result.score_unknown;
    }
    
    // Find the top scoring command
    hey_assistant_score /= previous_results_.size();
    yes_score /= previous_results_.size();
    no_score /= previous_results_.size();
    unknown_score /= previous_results_.size();
    
    // Convert negative values to positive for comparison
    uint8_t score_hey_assistant = hey_assistant_score + 128;
    uint8_t score_yes = yes_score + 128;
    uint8_t score_no = no_score + 128;
    uint8_t score_unknown = unknown_score + 128;
    
    uint8_t top_score = score_hey_assistant;
    const char* top_label = "hey_assistant";
    
    if (score_yes > top_score) {
      top_score = score_yes;
      top_label = "yes";
    }
    
    if (score_no > top_score) {
      top_score = score_no;
      top_label = "no";
    }
    
    if (score_unknown > top_score) {
      top_score = score_unknown;
      top_label = "unknown";
    }
    
    // If the score is too low, use unknown
    if (top_score < detection_threshold_) {
      top_label = "unknown";
    }
    
    // Check if this is a new command
    bool is_new = false;
    if (strcmp(top_label, previous_top_label_) != 0) {
      // This is a new command
      is_new = true;
    } else if ((current_time_ms - previous_top_label_time_) > suppression_ms_) {
      // This is a repeated command but after suppression period
      is_new = true;
    }
    
    // Update historical information
    previous_top_label_time_ = current_time_ms;
    previous_top_score_ = top_score;
    strcpy(previous_top_label_, top_label);
    
    *found_command = top_label;
    *score = top_score;
    *is_new_command = is_new;
    
    return kTfLiteOk;
  }

  // Member variables
  tflite::ErrorReporter* error_reporter_;
  int32_t average_window_duration_ms_;
  uint8_t detection_threshold_;
  int32_t suppression_ms_;
  int32_t minimum_count_;
  std::deque<CommandResult> previous_results_;
  char previous_top_label_[20];
  int32_t previous_top_label_time_;
  uint8_t previous_top_score_;
};

#endif  // RECOGNIZE_COMMANDS_H_ 