/*
 * Command Responder for ESP32-S3 Voice Assistant
 * Handles response to recognized wake word commands
 */

#ifndef COMMAND_RESPONDER_H_
#define COMMAND_RESPONDER_H_

#include <Arduino.h>

// This is called when a command is recognized
void RespondToCommand(tflite::ErrorReporter* error_reporter,
                     int32_t current_time, const char* found_command,
                     uint8_t score, bool is_new_command) {
  // Print the command that was found
  if (is_new_command) {
    error_reporter->Report("Heard %s (%d) @%dms", found_command, score, current_time);
    
    // Light LED according to command
    if (strcmp(found_command, "hey_assistant") == 0) {
      // Wake word detected
    } else if (strcmp(found_command, "yes") == 0) {
      // Yes command
    } else if (strcmp(found_command, "no") == 0) {
      // No command
    } else if (strcmp(found_command, "unknown") == 0 || score < 180) {
      // Unknown command or low score, do nothing
    }
  }
}

#endif // COMMAND_RESPONDER_H_ 