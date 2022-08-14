#include "DebugUtils.h"
#include "Botnet.h"
#include "ConsoleSystem.h"
#include <painlessMesh.h>
#include "mbedtls/md.h"
#include "WiFi.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "CRC32.h"

Scheduler consoleScheduler;

Task delayedDisableBeacon(TASK_SECOND, TASK_ONCE, &ConsoleSystem::endBTBeacon);

char ConsoleSystem::_uid[6];
String ConsoleSystem::_username;
String ConsoleSystem::_password;
String ConsoleSystem::_usernameInput;
String ConsoleSystem::_passwordInput;
String ConsoleSystem::_commandInput;
String ConsoleSystem::_CHALLENGE_CTF = "CHALLENGE_CTF_FLAG_HERE_[[WGCTF-1006032E18]]";
char ConsoleSystem::_userInput[MAX_MESSAGE_LENGTH];
unsigned int ConsoleSystem::_state;
unsigned int ConsoleSystem::_userInputLen;
bool ConsoleSystem::_acceptInput;
bool ConsoleSystem::_pendingMessage;
unsigned int (*ConsoleSystem::_getLoopCtr)();

void ConsoleSystem::initConsoleSystem(
    unsigned int(*getLoopCtr)()
    ) {

    consoleScheduler.startNow();
    consoleScheduler.addTask(delayedDisableBeacon);

    ConsoleSystem::initBluetooth();

    _state = 0;
    _userInputLen = 0;
    _getLoopCtr = getLoopCtr;
    _acceptInput = false;
    _pendingMessage = true;
    _username = "admin";

    ConsoleSystem::printNextMessage();
  }

  
void ConsoleSystem::initBluetooth() {
  if (!btStart()) {
      DEBUG_PRINTLN("Failed to initialize controller");
    }
    if (esp_bluedroid_init()!= ESP_OK) {
      //Serial.println("Failed to initialize bluedroid");
    }
    
    esp_bluedroid_disable();
}

void ConsoleSystem::startBTBeacon()
{
  if (esp_bluedroid_enable()!= ESP_OK) {
      //Serial.println("Failed to enable bluedroid");
    }

    ConsoleSystem::_password = Botnet::getBTPassword();

    char _btname[8];

    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    CRC32 crc;
    
    for(int i = 0; i < 5; i++){
      crc.update(ConsoleSystem::_password[i]);
    }

    uint32_t checksum = crc.finalize();

    sprintf(_btname, "%08X", checksum);

    esp_bt_dev_set_device_name(_btname);
    DEBUG_PRINTLN("Beacon On");
}

void ConsoleSystem::endBTBeacon()
{
  esp_bluedroid_disable();
  DEBUG_PRINTLN("Beacon Off");
}

void ConsoleSystem::printNextMessage() {

  ConsoleSystem::clearUserInputBuffer();

  switch(ConsoleSystem::_state) {
    case INIT:
      Serial.println("!!!UNKNOWN USER DETECTED!!!");
      Serial.print("> ");
      break;
    case INIT_HELP:
      Serial.println("\n\nAvailable Actions:");
      Serial.println("help - Display available actions");
      Serial.println("login - Authenticate to the badge");
      Serial.println("beacon - Debug CRC Beacon");
      Serial.print("> ");
      break;
    case INIT_BEACON:
      Serial.println("\n\nDEBUG CRC BEACON STARTED: 30s");
      Serial.print("> ");
      break;
    case AUTH_UNAME_INPUT:
      Serial.print("\n\nUsername: ");
      break;
    case AUTH_PASSWORD_INPUT:
      Serial.print("\n\nPassword: ");
      break;
    case AUTH_WRONG_CREDS:
      Serial.println("\n\nINVALID CREDENTIALS");
      Serial.print("> ");
      break;
    case AUTHENTICATED:
      Serial.println("\n\nSuccessfully authenticated");
      Serial.print("> ");
      break;
    case FLAG:
      Serial.print("\n\n[[WGCTF-");
      Serial.print(976319262553,HEX);
      Serial.println("]]");
      Serial.print("> ");
      break;
    case INIT_UNKNOWN_CMD:
    case UNKNOWN_COMMAND:
      Serial.println("\n\nUnknown Action");
      Serial.print("> ");
      break;
    case HELP:
      Serial.println("\n\nAvailable Actions:");
      Serial.println("help - Display available actions");
      Serial.println("info - Display badge information");
      Serial.println("flag - Get your flag for logging in!");
      Serial.print("> ");
      break;
    case INFO:
      Serial.println("\n\nBadge Information");
      Serial.print("> ");
      break;
  }
  ConsoleSystem::_pendingMessage = false;
}

void ConsoleSystem::clearUserInputBuffer() {
  ConsoleSystem::_userInput[0] = '\0';
  ConsoleSystem::_userInputLen = 0;
}

void ConsoleSystem::executeUserInput() {
  switch(ConsoleSystem::_state) {
    case AUTH_UNAME_INPUT:
      ConsoleSystem::_usernameInput = String(ConsoleSystem::_userInput);
      ConsoleSystem::_state = AUTH_PASSWORD_INPUT;
      ConsoleSystem::_pendingMessage = true;
      break;
    case AUTH_PASSWORD_INPUT:
      ConsoleSystem::_passwordInput = String(ConsoleSystem::_userInput);
      if (ConsoleSystem::_passwordInput.equals(ConsoleSystem::_password)
       && ConsoleSystem::_usernameInput.equals(ConsoleSystem::_username)) {
          ConsoleSystem::_state = AUTHENTICATED;
        }
      else {
        ConsoleSystem::_state = AUTH_WRONG_CREDS;
        ConsoleSystem::_usernameInput = "";
        ConsoleSystem::_passwordInput = "";
      }
      ConsoleSystem::_pendingMessage = true;
      break;
    case INIT:
    case INIT_HELP:
    case INIT_BEACON:
    case INIT_UNKNOWN_CMD:
    case AUTH_WRONG_CREDS:
      ConsoleSystem::_commandInput = ConsoleSystem::_userInput;
      if (ConsoleSystem::_commandInput.equals("help")
        || ConsoleSystem::_commandInput.equals("?")) {
        ConsoleSystem::_state = INIT_HELP;
      }
      else if (ConsoleSystem::_commandInput.equals("beacon"))
      {
        ConsoleSystem::startBTBeacon();
        delayedDisableBeacon.restartDelayed(BEACON_TTL * TASK_SECOND);
        ConsoleSystem::_state = INIT_BEACON;
      }
      else if (ConsoleSystem::_commandInput.equals("login"))
      {
        ConsoleSystem::_state = AUTH_UNAME_INPUT;
      }
      else
      {
        ConsoleSystem::_state = INIT_UNKNOWN_CMD;
      }
      ConsoleSystem::_pendingMessage = true;
      break;
    case HELP:
    case INFO:
    case UNKNOWN_COMMAND:
    case AUTHENTICATED:
      ConsoleSystem::_commandInput = ConsoleSystem::_userInput;
      if (ConsoleSystem::_commandInput.equals("help")
        || ConsoleSystem::_commandInput.equals("?")) {
        ConsoleSystem::_state = HELP;
      }
      else if (ConsoleSystem::_commandInput.equals("info")) {
        ConsoleSystem::_state = INFO;
      }
      else if (ConsoleSystem::_commandInput.equals("flag")) {
        ConsoleSystem::_state = FLAG;
      }
      else {
        ConsoleSystem::_state = UNKNOWN_COMMAND;
      }
      ConsoleSystem::_pendingMessage = true;
  }
  ConsoleSystem::clearUserInputBuffer();
}

void ConsoleSystem::acceptUserInput() {
  while (Serial.available() > 0)
  {
    //Read the next available byte in the serial receive buffer
    char inByte = Serial.read();

    if (inByte == '\n') {
      ConsoleSystem::_userInput[_userInputLen] = '\0';
      ConsoleSystem::executeUserInput();
    }
    else if (inByte == '\r') {
      // Don't save carriage returns
    }
    else if (inByte == '\b') {
      if (ConsoleSystem::_userInputLen > 0) {
        Serial.print('\b');
        Serial.print(' ');
        Serial.print('\b');
        ConsoleSystem::_userInputLen--;
        ConsoleSystem::_userInput[_userInputLen] = '\0';
      }
    }
    else {
      if (ConsoleSystem::_userInputLen < MAX_MESSAGE_LENGTH - 1)
      {
        Serial.print(inByte);
        ConsoleSystem::_userInput[_userInputLen] = inByte;
        ConsoleSystem::_userInputLen++;
      }
    }
  }
}

void ConsoleSystem::executeLoop() {
  consoleScheduler.execute();

  if (ConsoleSystem::_pendingMessage) {
    ConsoleSystem::printNextMessage();
  } else {
    ConsoleSystem::acceptUserInput();
  }
}