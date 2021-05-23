/*  JSON message handling.

    Copyright (C) 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

String bufferedJsonLines = "";

enum message_type {
  MESSAGE_ALERT,
  MESSAGE_WARN,
  MESSAGE_INFO,
  MESSAGE_DEBUG
};

// Translate a message into JSON representation
String JsonMessage(String message, message_type code) {
  const int docSize = JSON_OBJECT_SIZE(1) + // one element
                      JSON_OBJECT_SIZE(2);  // message
  StaticJsonDocument <docSize> doc;
  JsonObject msg = doc.createNestedObject("message");
  msg["text"] = message.c_str();
  switch (code) {
    case MESSAGE_ALERT:
      msg["type"] = "alert";
      break;
    case MESSAGE_WARN:
      msg["type"] = "warning";
      break;
    case MESSAGE_INFO:
      msg["type"] = "info";
      break;
    case MESSAGE_DEBUG:
      msg["type"] = "debug";
      break;
  }

  String result = "";
  serializeJson(doc, result);

  return result;
}



// add a single JSON line to the buffer
void addJsonLine(String message, message_type code) {
  if (code <= MESSAGE_VERBOSITY) {
    // add only those messages according to the configured verbosity
    String doc = JsonMessage(message, code);
    if (bufferedJsonLines == "")
      bufferedJsonLines = doc;
    else if (bufferedJsonLines.length() + doc.length() < MAX_JSON_BUFFER_SIZE)
      bufferedJsonLines += "\n" + doc;
    else
      bufferedJsonLines = doc;
  }
}

// add an existing JSON message line
void addJsonLine(String json_message) {
  if (bufferedJsonLines.length() == 0)
    bufferedJsonLines = json_message;
  else if (bufferedJsonLines.length() + json_message.length() < MAX_JSON_BUFFER_SIZE)
    bufferedJsonLines += "\n" + json_message;
  else
    bufferedJsonLines = json_message;
}

// process the buffered JSON lines
String processJsonLines() {
  String doc = bufferedJsonLines;
  bufferedJsonLines = "";

  return doc;
}
