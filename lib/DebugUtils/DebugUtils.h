#ifndef HEADER_DebugUtils
#define Header_DebugUtils


//#define DEBUG_ENABLED //Comment out to disable debug logging

#ifdef DEBUG_ENABLED
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTLN_HEX(x) Serial.println(x, HEX)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTLN_HEX(x)
#endif

#endif