#ifndef HEADER_ConsoleSystem
#define HEADER_ConsoleSystem
#include "stdint.h"
#include "Arduino.h"
#include <string>
#define MAX_MESSAGE_LENGTH 64

#define INIT 0
#define AUTH_UNAME_INPUT 1
#define AUTH_PASSWORD_INPUT 2
#define AUTH_WRONG_CREDS 3
#define AUTHENTICATED 4
#define UNKNOWN_COMMAND 5
#define HELP 6
#define INFO 7
#define CHALLENGE1 8
#define FLAG 9
#define INIT_HELP 10
#define INIT_BEACON 11
#define INIT_UNKNOWN_CMD 12

#define BEACON_TTL 30

class ConsoleSystem {
public:
    static void initConsoleSystem(
        unsigned int (*getLoopCtr)());
    static void executeLoop();
    static void endBTBeacon();

private:
  static char _uid[6];
  static String _username;
  static String _password;
  static String _usernameInput;
  static String _passwordInput;
  static String _commandInput;
  static String _CHALLENGE_CTF;
  static String _hidf;
  static char _btname[8];
  static char _userInput[MAX_MESSAGE_LENGTH];
  static unsigned int _userInputLen;
  static bool _acceptInput;
  static bool _pendingMessage;
  static unsigned int _state;
  static unsigned int (*_getLoopCtr)();
  static void initBluetooth();
  static void printNextMessage();
  static void acceptUserInput();
  static void clearUserInputBuffer();
  static void executeUserInput();
  static void startBTBeacon();

};

#endif