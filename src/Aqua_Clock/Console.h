#pragma once
/*
 * Console Parsing Class
 *
 * Supports basic console command line parsing with up to two parameters per command.
 * Callbacks handle data exchange after the command is issues.\
 * This is designed to connnect to a Stream, like a serial port, and parse the inputs until a carriage return is found.
 * Once a carriage return is receieved it will chop the inputs into:
 *  Command, parameter1, parameter2
 * Where Command is the first word up to a space or comma.
 * parameter1 (optional) is the second word separated by a space or comma.
 * parameter2 (optional) is the third word separated by a space or comma.
 * Once parsed input is received, the specificed callback function is called with three Strings for
 * comamnd, param1, param2
 * To use:
 *  1. Create an instance of the Console class.  "Console console;"
 *  2. Ensure the serial stream you want to use is created: "Serial.begin(115200);"
 *  2. Setup the console instance with the serial stream you want to run it on:  "console.Setup(&Serial);"
 *  3. Create a callback function that will be called when commands come in: "void console_rx_event(String command, String param1, String param2)"
 *  4. Set the callback pointer to your new callback function: "console.SetConsoleRxCallback(&console_rx_event);"
 *  5. Call the console update in the loop() or some other periodic task: "console.Loop();"
 *  6. Fill in the callback handler with switches for the incoming commands and parameters:
 *     Test for someone typing HELP on the command line: "if (command.equals("HELP"))"
 *     Test if the parameter after the command is "CLOCK": "if (param1 == "CLOCK")"
 *     Convert the second parameter into an int: "uint32_t period = param2.toInt();"

 * @author Joe Mohos
 * Contact: jmohos@yahoo.com
 */
#ifndef CONSOLE_H
#define CONSOLE_H

#include "Stream.h"


class Console {
private:

#define CONSOLE_BUF_SIZE 64

  Stream *streamSource;  // Stream source for sensor input (Serial1, Serial2, etc)
  char consoleBuffer[CONSOLE_BUF_SIZE + 1];
  int consoleIndex = 0;

  // Define the callback format
  typedef void (*ConsoleRxCallbackFunct)(String comamnd, String param1, String param2);
  ConsoleRxCallbackFunct console_command_callback;  // Event callback for new reading

public:
  //
  // Class constructor
  //
  Console() {
  }


  //
  // Map external callback handler to console RX event
  //
  void SetConsoleRxCallback(ConsoleRxCallbackFunct callback) {
    console_command_callback = callback;
  }


  //
  // Configure the stream input to utilize for the console.
  // Typically Serial or Serial1.
  //
  void Setup(Stream *serial) {
    this->streamSource = serial;
  }


  //
  // Process all pending serial input parsing.  Will trigger the callback
  // if a valid line of command info is framed.
  //
  void Loop() {
    Digest_Console();
  }


protected:

  //
  // Digest inputs until a carriage return or newline is received.
  //
  void Digest_Console() {
    char c;

    while (this->streamSource->available()) {
      c = this->streamSource->read();
      consoleBuffer[consoleIndex++] = c;

      if ((c == '\n') || (c == '\r')) {
        consoleBuffer[consoleIndex - 1] = 0;
        if (consoleIndex > 1) {
          String command_line = String(consoleBuffer);
          Parse_Line(command_line);
        }
        consoleIndex = 0;
      }

      // Guard for excessively long inputs with no terminator.
      if (consoleIndex >= CONSOLE_BUF_SIZE) {
        consoleIndex = 0;
      }
    }
  }


  // Split a line into the command and up to two optional parameters
  void Parse_Line(String line) {

    line.trim();

    String command = "";
    String params = "";
    String parameter1 = "";
    String parameter2 = "";

    // Grab the first segment as the command
    int split_point = line.indexOf(' ');

    command = line.substring(0, split_point);
    command.toUpperCase();

    if (command.length() == 0) {
      // No valid command characters
      return;
    }

    if (split_point > 0) {

      // Anything after the first space is the parameter space
      params = line.substring(split_point + 1);
      params.trim();

      // Grab the next segment as parameter1
      int split_point2 = params.indexOf(' ');

      parameter1 = params.substring(0, split_point2);
      parameter1.trim();
      parameter1.toUpperCase();

      if (split_point2 > 0) {
        // Anything left is parameter2
        parameter2 = params.substring(split_point2 + 1);
        parameter2.trim();
      }
    }

    // Issue callback with comamnd and two parameters
    if (console_command_callback != NULL) {
      console_command_callback(command, parameter1, parameter2);
    }
  }
};

#endif
