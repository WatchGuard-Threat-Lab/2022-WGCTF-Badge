
#ifndef HEADER_MenuSystem
#define HEADER_MenuSystem
#include "stdint.h"
#include "Arduino.h"
#include <string>
#include <Adafruit_GFX.h>
#include <painlessMesh.h>
#include <Adafruit_IS31FL3731.h>

#define MAX_DISPLAY_X 128
#define MAX_DISPLAY_Y 160

// TOP LEVEL MENU
#define MAIN_MENU       0
#define BOTNET_MENU     1
#define SETTINGS_MENU   2
#define EXTRAS_MENU     3
#define SECRETS_MENU 99

// BOTNET SUBMENU
#define BOT_MENU_MAIN       0
#define BOT_MENU_MUTATION   1
#define BOT_MENU_INFECT     2
#define BOT_MENU_INFO       3

// BOTNET MAIN MENU OPTIONS
#define BOT_MAIN_MUTATION 0
#define BOT_MAIN_INFECT   1
#define BOT_MAIN_INFO 2

// BOTNET INFECT STATES
#define BOT_INFECT_INFO 0
#define BOT_INFECT_COM 1
#define BOT_INFECT_COMPLETE 2
#define BOT_INFECT_TOO_SOON 3
#define BOT_INFECT_TIMED_OUT 4

// SETTINGS SUBMENU
#define CONF_MENU_MAIN 0
#define CONF_MENU_INFO 1
#define CONF_MENU_NAME 2
#define CONF_MENU_BLING 3
#define CONF_MENU_BLING_EYES 4
#define CONF_MENU_BLING_PATTERN 5
#define CONF_MENU_RESET 6
#define CONF_MENU_RESET_CONFIRM 7

// SETTINGS MAIN MENU OPTIONS
#define CONF_MAIN_INFO 0
#define CONF_MAIN_NAME 1
#define CONF_MAIN_BLING 2
#define CONF_MAIN_RESET 3

// BLING SETTINGS MENU OPTIONS
#define CONF_BLING_EYES 0
#define CONF_BLING_PATTERN 1

// BLING EYE COLOR OPTIONS
#define CONF_EYE_WHITE 0
#define CONF_EYE_RED 1
#define CONF_EYE_BLUE 2
#define CONF_EYE_RAINBOW 3

// BLING LED PATTERN OPTIONS
#define CONF_PATTERN_OFF 0
#define CONF_PATTERN_SOLID 1
#define CONF_PATTERN_BLINKING 2
#define CONF_PATTERN_RACING 3
#define CONF_PATTERN_PULSING 4
#define CONF_PATTERN_DAZZLING 5

// INPUT MENU OPTIONS
#define SELECT_LETTER   0
#define SELECT_OK       1
#define SELECT_DEL      2

#define MAIN_MENU_FRAME_FACTOR 2

class MenuSystem {
public:
    static void initMenuSystem(
        unsigned int (*getLoopCtr)());
    static void moveRight();
    static void moveLeft();
    static void moveDown();
    static void moveUp();
    static void accept();
    static void back();
    static void printMainMenu();
    static void printMainMenuAnimation();
    static void printBotInfectAnimation();
    static void printBotnetMenu();
    static void printBotMutationMenu();
    static void printBotInfoMenu();
    static void printSettingsMenu();
    static void printSecretsMenu();
    static void printExtrasMenu();
    static void printWiFiScan(unsigned int loopCtr);
    static void refreshMenu();
    static void updateWiFiScanMenu();
    static void updateMainMenuAnimation();
    static void updateInfectionAnimation();

    static void secretAccept();
    static void secretReject();

    static void setReceivedInfectMessageParts(unsigned int parts[], bool acked);
    static void setInfectionStatus(unsigned int state);

    static void setFaction(unsigned int faction);

    static bool isOnWiFiScan();
    static bool isNavLocked();
    static void enableNavLock();
    static void enableNavLockLong();
    static void disableNavLock();
    static void executeSchedule();

private:
    static bool _navLock;
    static int _currentMenu;
    static int _currentMenuState;
    static int _currentMenuPage;
    static int _currentMenuOption;
    static int _botState;
    static int _botPower;
    static int _currentTarget;
    static int _seqUp;
    static int _seqDown;
    static int _currentLetter;
    static int _numWiFiNetworks;
    static unsigned int _badgeType;
    static String _currentInput;
    static String _botName;
    static int _currentInputLength;
    static bool _canMoveRight;
    static bool _canMoveLeft;
    static bool _canMoveUp;
    static bool _canMoveDown;
    static unsigned short menuColor;
    static bool _standbyBling;
    static void printNavArrows();
    static void jpegRender(int xpos, int ypos);
    static void drawJpeg(const char *filename, int xpos, int ypos);
    static unsigned int (*_getLoopCtr)();
    static String getMutationName(unsigned int type, unsigned int loc);
    static void drawMainAnimationBlister(unsigned int x, unsigned int y, unsigned int ctr, unsigned int ctrOffset);
};

#endif