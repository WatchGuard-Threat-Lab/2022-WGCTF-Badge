#include "DebugUtils.h"
#include "MenuSystem.h"
#include "BlingSystem.h"
#include "Botnet.h"
#define FS_NO_GLOBALS
#include <FS.h>
#include "SPIFFS.h"
#include "JPEGDecoder.h"
#include "WiFi.h"
#include <BLEDevice.h>
#include <TFT_eFEX.h> // Include the function extension library
#include <TFT_eSPI.h> // Hardware-specific library
#include <painlessMesh.h>
#include <Adafruit_IS31FL3731.h>

TFT_eSPI tft = TFT_eSPI();

Scheduler menuScheduler;


const char ALPHABET[45] = {
    ' ', ' ', ' ', ' ',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
    'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
    'W', 'X', 'Y', 'Z', '0', '1', '2', '3', '4', '5', '6',
    '7', '8', '9',
    ' ', ' ', ' ', ' ', ' '};


bool MenuSystem::_navLock;
int MenuSystem::_currentMenu;
int MenuSystem::_currentMenuPage;
int MenuSystem::_currentMenuOption;
int MenuSystem::_currentMenuState;
int MenuSystem::_botState;
int MenuSystem::_botPower;
int MenuSystem::_currentLetter;
String MenuSystem::_currentInput;
int MenuSystem::_currentInputLength;
int MenuSystem::_numWiFiNetworks;
bool MenuSystem::_canMoveLeft;
bool MenuSystem::_canMoveRight;
bool MenuSystem::_canMoveUp;
bool MenuSystem::_canMoveDown;
int MenuSystem::_seqUp;
int MenuSystem::_seqDown;
int MenuSystem::_currentTarget;
unsigned int MenuSystem::_badgeType;
unsigned short MenuSystem::menuColor;
unsigned int (*MenuSystem::_getLoopCtr)();

Task t1(0, TASK_ONCE , &MenuSystem::disableNavLock);
Task t2(TASK_MILLISECOND * 500, TASK_FOREVER, &MenuSystem::updateWiFiScanMenu);
Task t3(TASK_MILLISECOND * 100, TASK_FOREVER, &MenuSystem::updateMainMenuAnimation);
Task t4(TASK_MILLISECOND * 500, TASK_FOREVER, &MenuSystem::updateInfectionAnimation);

// Return the minimum of two values a and b
#define minimum(a, b) (((a) < (b)) ? (a) : (b))

/** Render a jpeg image on the TFT display
 *
 * Render a decoded jpeg image to the TFT display
 * 
 */
void MenuSystem::jpegRender(int xpos, int ypos) {

    uint16_t *pImg;
    int16_t mcu_w = JpegDec.MCUWidth;
    int16_t mcu_h = JpegDec.MCUHeight;
    int32_t max_x = JpegDec.width;
    int32_t max_y = JpegDec.height;

    int32_t min_w = minimum(mcu_w, max_x % mcu_w);
    int32_t min_h = minimum(mcu_h, max_y % mcu_h);

    int32_t win_w = mcu_w;
    int32_t win_h = mcu_h;

    uint32_t drawTime = millis();

    max_x += xpos;
    max_y += ypos;

    while (JpegDec.readSwappedBytes()) {

        pImg = JpegDec.pImage;

        int mcu_x = JpegDec.MCUx * mcu_w + xpos;
        int mcu_y = JpegDec.MCUy * mcu_h + ypos;

        if (mcu_x + mcu_w <= max_x)
            win_w = mcu_w;
        else
            win_w = min_w;

        if (mcu_y + mcu_h <= max_y)
            win_h = mcu_h;
        else
            win_h = min_h;

        if (win_w != mcu_w) {
            for (int h = 1; h < win_h - 1; h++) {
                memcpy(pImg + h * win_w, pImg + (h + 1) * mcu_w, win_w << 1);
            }
        }

        if (mcu_x < tft.width() && mcu_y < tft.height()) {
            tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
        }

        else if ((mcu_y + win_h) >= tft.height())
            JpegDec.abort();
    }

    drawTime = millis() - drawTime;
}

/** Draw a jpeg to the TFT display
 *
 * Decode and draw a jpeg image to the TFT display by calling jpegRender()
 * 
 */
void MenuSystem::drawJpeg(const char *filename, int xpos, int ypos) {

    File jpegFile = SPIFFS.open(filename, "r");
    if (!jpegFile) {
        DEBUG_PRINT("ERROR: File \"");
        DEBUG_PRINT(filename);
        DEBUG_PRINTLN("\" not found!");
        return;
    }

    boolean decoded = JpegDec.decodeFsFile(filename);

    if (decoded) {
        MenuSystem::jpegRender(xpos, ypos);
    } else {
        DEBUG_PRINTLN("Jpeg file format not supported!");
    }
}

/** Initialize the MenuSystem module
 *
 * Called during badge startup, this initializes the MenuSystem
 * module including TFT display and scheduled task setup.
 * 
 */
void MenuSystem::initMenuSystem(
        unsigned int(*getLoopCtr)()
    ) {
    DEBUG_PRINTLN("Initializing Screen...");

    menuScheduler.startNow();
    menuScheduler.addTask( t1 );
    menuScheduler.addTask( t2 );
    menuScheduler.addTask( t3 );
    menuScheduler.addTask( t4 );
    t3.enable();
    t2.enable();
    t4.enable();

    tft.init();
    tft.setRotation(0); // 0 & 2 Portrait. 1 & 3 landscape
    tft.writecommand(TFT_MAD_RGB);
    SPIFFS.begin(true);

    tft.fillScreen(TFT_BLACK);
    

    if (!SPIFFS.begin()) {
        DEBUG_PRINTLN("SPIFFS initialisation failed!");
        while (1)
            yield(); // Stay here twiddling thumbs waiting
    }
    DEBUG_PRINTLN("\r\nInitialisation done.");
    _navLock = false;
    _getLoopCtr = getLoopCtr;
    _currentMenu = 0;
    _currentMenuPage = 0;
    _currentMenuOption = 0;
    _currentMenuState = 0;
    _botState = 0;
    _botPower = 0;
    _seqUp = 0;
    _seqDown = 0;
    _currentTarget = 0;
    _currentLetter = 4;
    _currentInputLength = 0;
    _numWiFiNetworks = 0;
    _canMoveLeft = false;
    _canMoveRight = true;
    _canMoveUp = false;
    _canMoveDown = false;
    _badgeType = Botnet::getBadgeType();
    switch (_badgeType) {
        case 0:
            menuColor = TFT_RED;
            break;
        case 1:
            menuColor = TFT_BLUE;
            break;
        case 2:
            menuColor = TFT_GREEN;
            break;
    }
    printMainMenu();
}  

/** Check if navigation handling is currently locked
 *
 * Navigation handling is locked while handling the previous user input
 * to prevent double-input.
 * 
 */
bool MenuSystem::isNavLocked() {
    return _navLock;
}

/*
bool MenuSystem::isOnWiFiScan() { 
    if (_currentMenu == EXTRAS_MENU && _currentMenuPage == 0 && _currentMenuOption == 1) {
        return true;
    } else {
        return false;
    }
}
*/

/** Move the menu to the right
 *
 * Handle a right directional button press in the menu system
 * 
 */
void MenuSystem::moveRight() {
    if (!_navLock) {
        enableNavLock();
        switch (_currentMenu) {
            case MAIN_MENU:
                if (_currentMenuPage < EXTRAS_MENU) {
                    _currentMenuPage += 1;
                    if (_currentMenuPage > 0) {
                        _canMoveLeft = true;
                    }
                    if (_currentMenuPage == EXTRAS_MENU) {
                        _canMoveRight = false;
                    }
                    printMainMenu();
                }
                break;
            case BOTNET_MENU:
                break;
            case SECRETS_MENU:
                switch (_currentMenuOption) {
                    case SELECT_LETTER:
                        if (_currentLetter < 39) {
                            _currentLetter += 1;
                            printSecretsMenu();
                        }
                        break;
                    case SELECT_OK:
                        _currentMenuOption = SELECT_DEL;
                        printSecretsMenu();
                        break;
                    case SELECT_DEL:
                        break;
                    }
                break;
            case SETTINGS_MENU:
                switch(_currentMenuPage)
                {
                    case CONF_MENU_NAME:
                        switch(_currentMenuOption)
                        {
                                case SELECT_LETTER:
                                if (_currentLetter < 39) {
                                        _currentLetter += 1;
                                        printSettingsMenu();
                                    }
                                    break;
                                case SELECT_OK:
                                    _currentMenuOption = SELECT_DEL;
                                    printSettingsMenu();
                                    break;
                        }
                        break;
                    case CONF_MENU_RESET_CONFIRM:
                        if (_currentMenuOption == 0)
                        {
                            _currentMenuOption = 1;
                        }
                        printSettingsMenu();
                        break;
                }
                break;
            case EXTRAS_MENU:
                break;
        }
    } 
}

/** Move the menu to the left
 *
 * Handle a left directional button press in the menu system
 * 
 */
void MenuSystem::moveLeft() {
    if (!_navLock) {
        enableNavLock();
        switch (_currentMenu) {
            case MAIN_MENU:
                if (_currentMenuPage > 0) {
                    _currentMenuPage -= 1;
                    if (_currentMenuPage < EXTRAS_MENU) {
                        _canMoveRight = true;
                    }
                    if (_currentMenuPage == MAIN_MENU) {
                        _canMoveLeft = false;
                    }
                    printMainMenu();
                }
                break;
        case BOTNET_MENU:
            break;
        case SECRETS_MENU:
            switch (_currentMenuOption) {
                case SELECT_LETTER:
                    if (_currentLetter > 4) {
                        _currentLetter -= 1;
                        printSecretsMenu();
                    }
                    break;
                case SELECT_OK:
                    break;
                case SELECT_DEL:
                    _currentMenuOption = SELECT_OK;
                    printSecretsMenu();
                    break;
            }
            break;
        case SETTINGS_MENU:
            switch(_currentMenuPage)
            {
                case CONF_MENU_NAME:
                    switch(_currentMenuOption) {
                        case SELECT_LETTER:
                            if (_currentLetter > 4) {
                                _currentLetter -= 1;
                                printSettingsMenu();
                            }
                            break;
                        case SELECT_DEL:
                            _currentMenuOption = SELECT_OK;
                            printSettingsMenu();
                    }
                    break;
                case CONF_MENU_RESET_CONFIRM:
                    if (_currentMenuOption == 1)
                    {
                        _currentMenuOption = 0;
                    }
                    printSettingsMenu();
                    break;
            }
            break;
        case EXTRAS_MENU:
            break;
        }
    }
}

/** Move the menu up
 *
 * Handle an up directional button press in the menu system
 * 
 */
void MenuSystem::moveUp() {
    if (!_navLock) {
        enableNavLock();
        switch (_currentMenu) {
            case BOTNET_MENU:
                switch (_currentMenuPage) {
                    case BOT_MENU_MAIN:
                        if (_currentMenuOption > BOT_MAIN_MUTATION)
                        {
                            _currentMenuOption--;
                            printBotnetMenu();
                        }
                        break;
                    case BOT_MENU_MUTATION:
                        break;
                    case BOT_MENU_INFECT:
                        break;
                }
                break;
            case SETTINGS_MENU:
                switch(_currentMenuPage)
                {
                    case CONF_MENU_MAIN:
                        if (_currentMenuOption > CONF_MAIN_INFO)
                        {
                            _currentMenuOption--;
                            printSettingsMenu();
                        }
                        break;
                    case CONF_MENU_NAME:
                        switch (_currentMenuOption)
                        {
                            case SELECT_OK:
                            case SELECT_DEL:
                                _currentMenuOption = SELECT_LETTER;
                                printSettingsMenu();
                                break;
                        }
                        break;
                    case CONF_MENU_BLING:
                        if (_currentMenuOption == CONF_BLING_PATTERN)
                        {
                            _currentMenuOption = CONF_BLING_EYES;
                            printSettingsMenu();
                        }
                        break;
                    case CONF_MENU_BLING_EYES:
                        if (_currentMenuOption > CONF_EYE_WHITE)
                        {
                            _currentMenuOption--;
                            printSettingsMenu();
                        }
                        break;
                    case CONF_MENU_BLING_PATTERN:
                        if (_currentMenuOption > CONF_PATTERN_OFF)
                        {
                            _currentMenuOption--;
                            printSettingsMenu();
                        }
                        break;
                }
                break;
            case SECRETS_MENU:
                if (_currentMenuOption != SELECT_LETTER) {
                    _currentMenuOption = SELECT_LETTER;
                    printSecretsMenu();
                }
                break;
        }
    }
}

/** Move the menu down
 *
 * Handle a down directional button press in the menu system
 * 
 */
void MenuSystem::moveDown() {
    if (!_navLock) {
        enableNavLock();
        switch (_currentMenu) {
            case BOTNET_MENU:
                switch (_currentMenuPage) {
                    case BOT_MENU_MAIN:
                        if (_currentMenuOption < BOT_MAIN_INFO)
                        {
                            _currentMenuOption++;
                            printBotnetMenu();
                        }
                        break;
                    case BOT_MENU_MUTATION:
                        break;
                    case BOT_MENU_INFECT:
                        break;
                }
                break;
            case SETTINGS_MENU:
                switch (_currentMenuPage)
                {
                    case CONF_MENU_MAIN:
                        if (_currentMenuOption < CONF_MAIN_RESET)
                        {
                            _currentMenuOption += 1;
                            printSettingsMenu();
                        }
                        break;
                    case CONF_MENU_NAME:
                        if (_currentMenuOption == SELECT_LETTER)
                        {
                            _currentMenuOption = SELECT_OK;
                            printSettingsMenu();
                        }
                        break;
                    case CONF_MENU_BLING:
                        if (_currentMenuOption == CONF_BLING_EYES)
                        {
                            _currentMenuOption = CONF_BLING_PATTERN;
                            printSettingsMenu();
                        }
                        break;
                    case CONF_MENU_BLING_EYES:
                        if (_currentMenuOption < CONF_EYE_RAINBOW)
                        {
                            _currentMenuOption++;
                            printSettingsMenu();
                        }
                        break;
                    case CONF_MENU_BLING_PATTERN:
                        if (_currentMenuOption < CONF_PATTERN_DAZZLING)
                        {
                            _currentMenuOption++;
                            printSettingsMenu();
                        }
                        break;
                }

                break;
            case SECRETS_MENU:
                if (_currentMenuOption == SELECT_LETTER) {
                    _currentMenuOption = SELECT_OK;
                    printSecretsMenu();
                }
                break;
        }
    }
}

/** Handle accept button
 *
 * Handle an accept button press in the menu system
 * 
 */
void MenuSystem::accept() {
    if (!_navLock) {
        enableNavLockLong();
        switch (_currentMenu) {
        case MAIN_MENU:
            switch (_currentMenuPage) {
                case MAIN_MENU:
                    break;
                case BOTNET_MENU:
                    _currentMenu = BOTNET_MENU;
                    _currentMenuPage = BOT_MENU_MAIN;
                    _canMoveLeft = false;
                    _canMoveRight = false;
                    printBotnetMenu();
                    break;
                case SECRETS_MENU:
                    _currentMenu = SECRETS_MENU;
                    _currentMenuPage = 0;
                    _currentMenuOption = 0;
                    _currentLetter = 4;
                    _currentInput = "";
                    _currentInputLength = 0;
                    printSecretsMenu();
                    break;
                case SETTINGS_MENU:
                    _currentMenu = SETTINGS_MENU;
                    _currentMenuPage = CONF_MENU_MAIN;
                    _canMoveLeft = false;
                    _canMoveRight = true;
                    printSettingsMenu();
                    break;
                case EXTRAS_MENU:
                    _currentMenu = EXTRAS_MENU;
                    _currentMenuPage = 0;
                    _currentMenuOption = 0;
                    _canMoveRight = true;
                    _canMoveLeft = false;
                    printExtrasMenu();
                    break;
            }
            break;
        case BOTNET_MENU:
            switch (_currentMenuPage) {
                case BOT_MENU_MAIN:
                    switch (_currentMenuOption)
                    {
                        case BOT_MAIN_MUTATION:
                            _currentMenuPage = BOT_MENU_MUTATION;
                            _currentMenuOption = 0;
                            printBotMutationMenu();
                            break;
                        case BOT_MAIN_INFECT:
                            _currentMenuPage = BOT_MENU_INFECT;
                            _currentMenuState = BOT_INFECT_INFO;
                            _currentMenuOption = 0;
                            printBotnetMenu();
                            break;
                        case BOT_MAIN_INFO:
                            _currentMenuPage = BOT_MENU_INFO;
                            printBotInfoMenu();
                            break;
                    }
                    break;
                case BOT_MENU_MUTATION:
                    break;
                case BOT_MENU_INFECT:
                    _currentMenuOption = 0;
                    switch (_currentMenuState)
                    {
                        case BOT_INFECT_INFO:
                            _currentMenuState = BOT_INFECT_COM;
                            Botnet::startCrossInfect();
                            printBotnetMenu();
                            break;
                        case BOT_INFECT_COM:
                            _currentMenuState = BOT_INFECT_INFO;
                            if (Botnet::crossInfectTimedOut)
                            {
                                _currentMenuState = BOT_INFECT_TIMED_OUT;
                            }
                            printBotnetMenu();
                            break;
                        case BOT_INFECT_COMPLETE:
                        case BOT_INFECT_TOO_SOON:
                        case BOT_INFECT_TIMED_OUT:
                            _currentMenuState = BOT_INFECT_INFO;
                            _currentMenuPage = BOT_MENU_MAIN;
                            _currentMenuOption = BOT_MAIN_MUTATION;
                            printBotnetMenu();
                            break;
                    }
                    break;
            }
            break;
        case SECRETS_MENU:
            switch (_currentMenuOption) {
                case SELECT_LETTER:
                    // Add letter to code
                    if (_currentInput.length() < 8) {
                        _currentInput += ALPHABET[_currentLetter];
                        _currentInputLength += 1;
                        printSecretsMenu();
                    }
                    break;
                case SELECT_OK:
                    // Enter completed code
                    {
                        if (Botnet::checkCode(_currentInput)) {
                            // Code Accepted
                            secretAccept();
                            _currentInput = "";
                            _currentInputLength = 0;
                            tft.fillScreen(TFT_BLACK);
                            printSecretsMenu();
                        } else {
                            secretReject();
                            tft.fillScreen(TFT_BLACK);
                            printSecretsMenu();
                            // Entered an invalid code
                        }
                    }
                    break;
                case SELECT_DEL:
                    // Remove letter from code
                    if (_currentInput.length() > 0) {
                        _currentInput.remove(_currentInput.length() - 1);
                        _currentInputLength -= 1;
                        printSecretsMenu();
                    }
                    break;
            }
            break;
        case SETTINGS_MENU:
            switch (_currentMenuPage) {
                case CONF_MENU_MAIN:
                    switch (_currentMenuOption) {
                        case CONF_MAIN_INFO:
                            _currentMenuPage = CONF_MENU_INFO;
                            _currentMenuOption = 0;
                            printSettingsMenu();
                            break;
                        case CONF_MAIN_NAME:
                            _currentMenuPage = CONF_MENU_NAME;
                            _currentMenuOption = SELECT_LETTER;
                            _currentLetter = 4;
                            _currentInput = "";
                            _currentInputLength = 0;
                            printSettingsMenu();
                            break;
                        case CONF_MAIN_BLING:
                            _currentMenuPage = CONF_MENU_BLING;
                            _currentMenuOption = CONF_BLING_EYES;
                            printSettingsMenu();
                            break;
                        case CONF_MAIN_RESET:
                            _currentMenuPage = CONF_MENU_RESET;
                            printSettingsMenu();
                            break;
                    }
                    break;
                case CONF_MENU_NAME:
                    switch (_currentMenuOption)
                    {
                        case SELECT_LETTER:
                            if (_currentInput.length() < 8)
                            {
                                _currentInput += ALPHABET[_currentLetter];
                                _currentInputLength += 1;
                                printSettingsMenu();
                            }
                            break;
                        case SELECT_OK:
                            Botnet::setBotName(_currentInput);
                            _currentInput = "";
                            _currentInputLength = 0;
                            tft.fillScreen(TFT_BLACK);
                            printSettingsMenu();
                            break;
                        case SELECT_DEL:
                            if (_currentInput.length() > 0)
                            {
                                _currentInput.remove(_currentInput.length() - 1);
                                _currentInputLength -= 1;
                                printSettingsMenu();
                            }
                            break;
                    }
                    break;
                case CONF_MENU_BLING:
                    switch (_currentMenuOption) {
                        case CONF_BLING_EYES:
                            _currentMenuPage = CONF_MENU_BLING_EYES;
                            _currentMenuOption = CONF_EYE_WHITE;
                            printSettingsMenu();
                            break;
                        case CONF_BLING_PATTERN:
                            _currentMenuPage = CONF_MENU_BLING_PATTERN;
                            _currentMenuOption = CONF_PATTERN_OFF;
                            printSettingsMenu();
                            break;
                    }
                    break;
                case CONF_MENU_BLING_EYES:
                    switch (_currentMenuOption)
                    {
                        case CONF_EYE_WHITE:
                            BlingSystem::setEyeColor(EYES_WHITE);
                            break;
                        case CONF_EYE_RED:
                            if (Botnet::redEyesUnlocked())
                            {
                                BlingSystem::setEyeColor(EYES_RED);
                            }
                            break;
                        case CONF_EYE_BLUE:
                            if (Botnet::blueEyesUnlocked())
                            {
                                BlingSystem::setEyeColor(EYES_BLUE);
                            }
                            break;
                        case CONF_EYE_RAINBOW:
                            if (Botnet::rainbowEyesUnlocked())
                            {
                                BlingSystem::setEyeColor(EYES_RAINBOW);
                            }
                            break;
                    }
                    break;
                case CONF_MENU_BLING_PATTERN:
                    switch (_currentMenuOption)
                    {
                        case CONF_PATTERN_OFF:
                            BlingSystem::setBlingMode(BLING_MODE_OFF);
                            break;
                        case CONF_PATTERN_SOLID:
                            BlingSystem::setBlingMode(BLING_MODE_STEADY);
                            break;
                        case CONF_PATTERN_BLINKING:
                            if (Botnet::blinkingBodyUnlocked())
                            {
                                BlingSystem::setBlingMode(BLING_MODE_BLINK);
                            }
                            break;
                        case CONF_PATTERN_RACING:
                            if (Botnet::racingBodyUnlocked())
                            {
                                BlingSystem::setBlingMode(BLING_MODE_RACE);
                            }
                            break;
                        case CONF_PATTERN_PULSING:
                            if (Botnet::pulsingBodyUnlocked())
                            {
                                BlingSystem::setBlingMode(BLING_MODE_PULSE);
                            }
                            break;
                        case CONF_PATTERN_DAZZLING:
                            if (Botnet::dazzlingBodyUnlocked())
                            {
                                BlingSystem::setBlingMode(BLING_MODE_DAZZLE);
                            }
                    }
                    break;
                case CONF_MENU_RESET:
                    _currentMenuPage = CONF_MENU_RESET_CONFIRM;
                    _canMoveLeft = false;
                    _canMoveRight = false;
                    _currentMenuOption = 0;
                    printSettingsMenu();
                    break;
                case CONF_MENU_RESET_CONFIRM:
                    if (_currentMenuOption == 0) {
                        _currentMenuPage = CONF_MENU_RESET;
                        _canMoveLeft = true;
                        _canMoveRight = false;
                        printSettingsMenu();
                    }
                    else if (_currentMenuOption == 1) {
                        Botnet::resetBadge();
                    }
                    break;
            }
            break;
        case EXTRAS_MENU:
            if(_currentMenuPage == 0) {
                _currentMenuOption = 0;
                printWiFiScan(_getLoopCtr());
            }
            break;
        }
    }
}

/** Handle back button
 *
 * Handle a back button press in the menu system
 * 
 */
void MenuSystem::back() {
    if (!_navLock) {
        enableNavLockLong();
        switch (_currentMenu) {
        case MAIN_MENU:
            break;
        case BOTNET_MENU:
            switch (_currentMenuPage) {
                case BOT_MENU_MAIN:
                    _currentMenu = MAIN_MENU;
                    _currentMenuPage = BOTNET_MENU;
                    _canMoveLeft = true;
                    _canMoveRight = true;
                    printMainMenu();
                    break;
                case BOT_MENU_MUTATION:
                    _currentMenuPage = BOT_MENU_MAIN;
                    _currentMenuOption = BOT_MAIN_MUTATION;
                    printBotnetMenu();
                    break;
                case BOT_MENU_INFECT:
                    _currentMenuOption = BOT_MAIN_MUTATION;
                    switch (_currentMenuState)
                    {
                        case BOT_INFECT_INFO:
                            _currentMenuPage = BOT_MENU_MAIN;
                            _currentMenuOption = BOT_MAIN_INFECT;
                            printBotnetMenu();
                            break;
                        case BOT_INFECT_COM:
                            _currentMenuState = BOT_INFECT_INFO;
                            Botnet::stopCrossInfect();
                            printBotnetMenu();
                            break;
                        case BOT_INFECT_COMPLETE:
                        case BOT_INFECT_TOO_SOON:
                        case BOT_INFECT_TIMED_OUT:
                            _currentMenuState = BOT_INFECT_INFO;
                            _currentMenuPage = BOT_MENU_MAIN;
                            printBotnetMenu();
                            break;
                    }
                    break;
                case BOT_MENU_INFO:
                    _currentMenuPage = BOT_MENU_MAIN;
                    printBotnetMenu();
                    break;
            }
            break;
        case SECRETS_MENU:
            switch (_currentMenuOption) {
                case SELECT_LETTER:
                    _currentMenu = MAIN_MENU;
                    _currentMenuPage = SECRETS_MENU;
                    _canMoveLeft = true;
                    _canMoveRight = true;
                    _currentLetter = 4;
                    _currentMenuOption = 0;
                    _currentInput = "";
                    _currentInputLength = 0;
                    printMainMenu();
                    break;
                case SELECT_OK:
                case SELECT_DEL:
                    _currentMenuOption = 0;
                    printSecretsMenu();
                    break;
            }
            break;
        case SETTINGS_MENU:
            switch (_currentMenuPage)
            {
                case CONF_MENU_MAIN:
                    _currentMenu = MAIN_MENU;
                    _currentMenuPage = SETTINGS_MENU;
                    _currentMenu = MAIN_MENU;
                    _currentMenuPage = SETTINGS_MENU;
                    _currentMenuOption = 0;
                    _canMoveLeft = true;
                    _canMoveRight = true;
                    _numWiFiNetworks = 0;       
                    printMainMenu();
                    break;
                case CONF_MENU_BLING_EYES:
                case CONF_MENU_BLING_PATTERN:
                    _currentMenuPage = CONF_MENU_BLING;
                    _currentMenuOption = CONF_BLING_EYES;
                    printSettingsMenu();
                    break;
                case CONF_MENU_INFO:
                case CONF_MENU_NAME:
                case CONF_MENU_BLING:
                case CONF_MENU_RESET:
                    _currentMenuPage = CONF_MENU_MAIN;
                    _currentMenuOption = CONF_MAIN_INFO;
                    _numWiFiNetworks = 0;       
                    printSettingsMenu();
                    break;
                case CONF_MENU_RESET_CONFIRM:
                    _currentMenuPage = CONF_MENU_RESET;
                    _currentMenuOption = 0;
                    printSettingsMenu();
                    break;
            }
            break;
        case EXTRAS_MENU:
            _currentMenu = MAIN_MENU;
            _currentMenuPage = EXTRAS_MENU;
            _canMoveLeft = true;
            _canMoveRight = false;
            printMainMenu();
            break;
        }
    }
}

/** Execute the MenuSystem scheduler
 *
 * Executed every loop interation in main.cpp
 * 
 */
void MenuSystem::executeSchedule() {
    menuScheduler.execute();
}

/** Temporarily lock menu navigation
 *
 * Temporarily lock menu navigation after accepting a button press to
 * prevent multiple reads from the same press
 * 
 */
void MenuSystem::enableNavLock() {
    _navLock = true;
    t1.restartDelayed(200);
}

/** Temporarily lock menu navigation - extended
 *
 * Temporarily lock menu navigation after accepting a button press to
 * prevent multiple reads from the same press
 * 
 * This locks the menu for a full half a second
 * 
 */
void MenuSystem::enableNavLockLong() {
    _navLock = true;
    t1.restartDelayed(500);
}

/** Disable the menu navigation lock
 *
 * Disable the menu navigation lock to begin accepting button presses
 * again
 * 
 */
void MenuSystem::disableNavLock() {
    _navLock = false;
}

/** Print navigation arrow helpers
 *
 * Print arrows on the menu depending on whether the user can navigate
 * with a directional button press.
 * 
 */
void MenuSystem::printNavArrows() {
    if (_canMoveLeft) {
        tft.fillTriangle(1, 80, 8, 72, 8, 88, TFT_RED);
    }

    if (_canMoveRight) {
        tft.fillTriangle(127, 80, 120, 72, 120, 88, TFT_RED);
    }

    if (_canMoveUp) {
        tft.fillTriangle(64, 1, 57, 9, 71, 9, TFT_RED);
    }

    if (_canMoveDown) {
        tft.fillTriangle(64, 159, 57, 150, 71, 150, TFT_RED);
    }
}

/** Update cross-infection status
 *
 * Notify the MenuSystem of an update to the cross-infection status
 * 
 */
void MenuSystem::setInfectionStatus(unsigned int state)
{
    if (_currentMenu == BOTNET_MENU && _currentMenuPage == BOT_MENU_INFECT && _currentMenuState == BOT_INFECT_COM)
    {
        _currentMenuState = state;
        printBotnetMenu();
    }
}

/**
 * Update cross-infect message status
 * 
 * Notify the MenuSystem as pieces of the cross infect message are received
 * 
 */
void MenuSystem::setReceivedInfectMessageParts(unsigned int parts[], bool acked)
{
    if (_currentMenu == BOTNET_MENU && _currentMenuPage == BOT_MENU_INFECT && _currentMenuState == BOT_INFECT_COM)
    {
        tft.fillRect(0, 116, 128, 16, TFT_BLACK);
        for (int i = 0; i < CI_MAX_MESSAGE_SIZE; i++)
        {
            if (parts[i] != 1)
            {
                tft.drawRect(10+(i*15), 116, 10, 16, TFT_WHITE);
            }
            else
            {
                tft.fillRect(10+(i*15), 116, 10, 16, TFT_WHITE);
            }
        }
        if (!acked)
        {
            tft.fillRect(0, 135, 128, 3, TFT_RED);
        }
        else
        {
            tft.fillRect(0, 135, 128, 3, TFT_GREEN);
        }
        /*
        tft.drawRect(5, 116, 10, 18, TFT_BLACK);
        tft.drawRect(20, 116, 10, 18, TFT_BLACK);
        tft.drawRect(35, 116, 10, 18, TFT_BLACK);
        tft.drawRect(50, 116, 10, 18, TFT_BLACK);
        tft.drawRect(65, 116, 10, 18, TFT_BLACK);
        tft.drawRect(80, 116, 10, 18, TFT_BLACK);
        tft.drawRect(95, 116, 10, 18, TFT_BLACK);
        tft.drawRect(110, 116, 10, 18, TFT_BLACK);
        */

    }
}


/** Get the name of a mutation based off the bit number
 *
 * Return the name of a mutation or variant based off the bit number
 * 
 */
String MenuSystem::getMutationName(unsigned int type, unsigned int loc)
{
    String name;
    if (type == MUTATION_TYPE_M)
    {
        switch (loc)
        {
            case 0:
                name = "A1";
                break;
            case 1:
                name = "A2";
                break;
            case 2:
                name = "A3";
                break;
            case 3:
                name = "B1";
                break;
            case 4:
                name = "B2";
                break;
            case 5:
                name = "B3";
                break;
            case 6:
                name = "C1";
                break;
            case 7:
                name = "C2";
                break;
            case 8:
                name = "C3";
                break;
            case 9:
                name = "D1";
                break;
            case 10:
                name = "D2";
                break;
            case 11:
                name = "D3";
                break;
            case 12:
                name = "D4";
                break;
            case 13:
                name = "E1";
                break;
            case 14:
                name = "E2";
                break;
            case 15:
                name = "F1";
                break;
            case 16:
                name = "F2";
                break;
            case 17:
                name = "F3";
                break;
            case 18:
                name = "F4";
                break;
            case 19:
                name = "G1";
                break;
            case 20:
                name = "G2";
                break;
            case 21:
                name = "G3";
                break;
            case 22:
                name = "G4";
                break;
            case 23:
                name = "H1";
                break;
            case 24:
                name = "H2";
                break;
            case 25:
                name = "H3";
                break;
            case 26:
                name = "H4";
                break;
        }
    }
    else if (type == MUTATION_TYPE_V)
    {
        switch (loc)
        {
            case 0:
                name = "I1";
                break;
            case 1:
                name = "I2";
                break;
            case 2:
                name = "I3";
                break;
            case 3:
                name = "J1";
                break;
            case 4:
                name = "J2";
                break;
            case 5:
                name = "J3";
                break;
            case 6:
                name = "K1";
                break;
            case 7:
                name = "K2";
                break;
            case 8:
                name = "K3";
                break;
            case 9:
                name = "L1";
                break;
            case 10:
                name = "L2";
                break;
            case 11:
                name = "L3";
                break;
            case 12:
                name = "M1";
                break;
            case 13:
                name = "M2";
                break;
            case 14:
                name = "M3";
                break;
            case 15:
                name = "M4";
                break;
            case 16:
                name = "M5";
                break;
            case 17:
                name = "N1";
                break;
            case 18:
                name = "N2";
                break;
            case 19:
                name = "N3";
                break;
            case 20:
                name = "N4";
                break;
            case 21:
                name = "N5";
                break;
            case 22:
                name = "O1";
                break;
            case 23:
                name = "O2";
                break;
            case 24:
                name = "O3";
                break;
            case 25:
                name = "O4";
                break;
            case 26:
                name = "O5";
                break;
            case 27:
                name = "P1";
                break;
        }
    }
    return name;
}

/** Redraw the WiFiScan menu
 *
 * Redraw's the WiFi Scan menu to display any potential updates
 * 
 */
void MenuSystem::updateWiFiScanMenu()
{
    if (_currentMenu == EXTRAS_MENU && _currentMenuPage == 0 && _currentMenuOption == 1)
    {
        printWiFiScan(_getLoopCtr());
    }
}

/** Redraw the current menu
 *
 * Redraw's the current menu to display any potential updates or animation
 * 
 */
void MenuSystem::refreshMenu() { 
    if (_currentMenu == MAIN_MENU && _currentMenuPage == MAIN_MENU) {
        printMainMenu();
    }
    else if (_currentMenu == BOTNET_MENU && _currentMenuPage == BOT_MENU_MUTATION) { //TODO: Add check for if anything has updated
        printBotMutationMenu();
    }
    /*
    else if (_currentMenu == BOTNET_MENU && _currentMenuPage == BOT_MENU_INFECT && _currentMenuState == BOT_INFECT_COM)
    {
        printBotnetMenu();
    }
    */
}


/** Redraw the main menu animation
 *
 * Redraws the main menu animation of that is the current active menu
 * 
 */
void MenuSystem::updateMainMenuAnimation()
{
    if (_currentMenu == MAIN_MENU && _currentMenuPage == 0)
    {
        printMainMenuAnimation();
    }
}

/** Redraw the cross-infect pending animation
 * 
 * Redraws the pending message animation if the botnet cross infect
 * process is running
 * 
 */
void MenuSystem::updateInfectionAnimation()
{
    if (_currentMenu == BOTNET_MENU && _currentMenuPage == BOT_MENU_INFECT && _currentMenuState == BOT_INFECT_COM)
    {
        printBotInfectAnimation();
    }
}

/** Print an animated virus 'blister' on the main screen
* 
 * Draws a circle at x,y and a line connecting to the center circle on the
 * main menu screen. Animates the circle growing and shrinking based off
 * the ctr and a provided offset.
 * 
 */
void MenuSystem::drawMainAnimationBlister(unsigned int x, unsigned int y, unsigned int ctr, unsigned int ctrOffset)
{
    if (((ctr+ctrOffset) % (20 * MAIN_MENU_FRAME_FACTOR)) < 8 * MAIN_MENU_FRAME_FACTOR)
    {
        tft.drawLine(64, 80, x, y, MenuSystem::menuColor);
        tft.fillCircle(x, y, (((ctr+ctrOffset)%(20 * MAIN_MENU_FRAME_FACTOR)) / MAIN_MENU_FRAME_FACTOR), MenuSystem::menuColor);   
    }
    else if (((ctr+ctrOffset) % (20 * MAIN_MENU_FRAME_FACTOR)) < 16 * MAIN_MENU_FRAME_FACTOR)
    {
        tft.fillCircle(x, y, 8, TFT_BLACK);
        tft.drawLine(64, 80, x, y, MenuSystem::menuColor);
        tft.fillCircle(x, y, 16-((((ctr+ctrOffset)%(20 * MAIN_MENU_FRAME_FACTOR))/MAIN_MENU_FRAME_FACTOR)), MenuSystem::menuColor);   
    }
    else if (((ctr+ctrOffset) % (20 * MAIN_MENU_FRAME_FACTOR)) < 17 * MAIN_MENU_FRAME_FACTOR)
    {
        tft.fillCircle(x, y, 8, TFT_BLACK);
        tft.drawLine(64, 80, x, y, TFT_BLACK);
    }
}

/** Draw an idle animation for cross infect
 * 
 * Draw an idle animation while waiting for the cross infect message to complete
 * 
 */
void MenuSystem::printBotInfectAnimation()
{
    unsigned int ctr = _getLoopCtr();
    tft.fillRect(0, 80, 128, 8, TFT_BLACK);
    //tft.setCursor(0, 80);

    if (ctr % 60 < 20)
    {
        //tft.println("  o       ");
        tft.drawCircle(30, 84, 3, TFT_WHITE);
    }
    else if (ctr % 60 < 40)
    {
        //tft.println("     o     ");
        tft.drawCircle(64, 84, 3, TFT_WHITE);
    }
    else {
        //tft.println("        o   ");
        tft.drawCircle(98, 84, 3, TFT_WHITE);
    }
}

/** Draw the main menu animation
 *
 * Draw the current iteration of the main menu animation 
 * 
 */
void MenuSystem::printMainMenuAnimation()
{
    tft.fillCircle(64, 80, 8, menuColor);
    unsigned int numBadges = Botnet::getNumBadgesEncountered();
    unsigned int badgeType = Botnet::getBadgeType();

    unsigned int ctr = _getLoopCtr();


    if (badgeType == BADGE_TYPE_WG || numBadges >= 1){
        tft.fillCircle(64, 80, 8, menuColor);
        MenuSystem::drawMainAnimationBlister(40, 72, ctr, 0);
    }

    if (badgeType == BADGE_TYPE_WG || numBadges >= 5)
    {
        MenuSystem::drawMainAnimationBlister(30, 102, ctr, 19);
    }

    if (badgeType == BADGE_TYPE_WG || numBadges >= 10)
    {
        MenuSystem::drawMainAnimationBlister(90, 50, ctr, 5);
    }

    if (badgeType == BADGE_TYPE_WG || numBadges >= 15)
    {
        MenuSystem::drawMainAnimationBlister(70, 112, ctr, 33);
    }

    if (badgeType == BADGE_TYPE_WG || numBadges >= 20)
    {
        MenuSystem::drawMainAnimationBlister(67, 58, ctr, 29);
    }

    if (badgeType == BADGE_TYPE_WG || numBadges >= 25)
    {
        MenuSystem::drawMainAnimationBlister(100, 94, ctr, 15);
    }

    if (badgeType == BADGE_TYPE_WG || numBadges >= 30)
    {
        MenuSystem::drawMainAnimationBlister(52, 40, ctr, 41);
    }

    if (badgeType == BADGE_TYPE_WG || numBadges >= 35)
    {
        MenuSystem::drawMainAnimationBlister(35, 125, ctr, 67);
    }

    if (badgeType == BADGE_TYPE_WG || numBadges >= 40)
    {
        MenuSystem::drawMainAnimationBlister(105, 60, ctr, 75);
    }

    if (badgeType == BADGE_TYPE_WG || numBadges > 45)
    {
        MenuSystem::drawMainAnimationBlister(92, 140, ctr, 63);
    }

    if (badgeType == BADGE_TYPE_WG || numBadges >= 50)
    {
        MenuSystem::drawMainAnimationBlister(30, 35, ctr, 93);
    }

    if (badgeType == BADGE_TYPE_WG || numBadges >= 55)
    {
        MenuSystem::drawMainAnimationBlister(20, 85, ctr, 10);
    }

    tft.fillCircle(64, 80, 8, menuColor); // Redraw the center circle

}

/** Draw the main menu
 *
 * Draw the full main menu page
 * 
 */
void MenuSystem::printMainMenu() {
    // Menu System
    switch (_currentMenuPage) {
    case MAIN_MENU:
        {
        unsigned int numNodes = Botnet::getNumBadgesEncountered();
        unsigned int numMutx = Botnet::getMutationCount();
        unsigned int numInf = Botnet::getNumBadgesInfected();
        unsigned int badgeType = Botnet::getBadgeType();


        tft.setCursor(0, 0);
        tft.setTextColor(menuColor);
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1);
        if (badgeType == BADGE_TYPE_WG)
        {
            tft.println("Badges Seen    ");
            tft.drawEllipse(94, 4, 2, 2, menuColor);
            tft.drawEllipse(98, 4, 2, 2, menuColor);
            //tft.println(numNodes);
            tft.println("Mutations      ");
            tft.drawEllipse(94, 12, 2, 2, menuColor);
            tft.drawEllipse(98, 12, 2, 2, menuColor);
            tft.println("X-Infections   ");
            tft.drawEllipse(94, 20, 2, 2, menuColor);
            tft.drawEllipse(98, 20, 2, 2, menuColor);
            tft.drawLine(0,24,128,24, menuColor);
        }
        else
        {
            tft.print("Badges Seen    ");
            tft.println(numNodes);
            tft.print("Mutations      ");
            tft.println(numMutx);
            tft.print("X-Infections   ");
            tft.println(numInf);
            tft.drawLine(0,24,128,24, menuColor);
        }
        
        printMainMenuAnimation();
        }
        break;
    case BOTNET_MENU:
        drawJpeg("/Botnet.jpg", 0, 0);
        break;
    case SECRETS_MENU:
        drawJpeg("/Secrets.jpg", 0, 0);
        break;
    case SETTINGS_MENU:
        drawJpeg("/Settings.jpg", 0, 0);
        break;
    case EXTRAS_MENU:
        drawJpeg("/Extras.jpg", 0, 0);
        break;
    }
    // Nav Arrows
    MenuSystem::printNavArrows();
}

/** Draw the botnet mutation menu
 *
 * Draw the botnet mutation menu, showing the currently unlocked
 * mutations and variants
 * 
 */
void MenuSystem::printBotMutationMenu() {
    // Menu System
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextWrap(true);
    tft.setTextSize(1);
    tft.println("      Mutations\n");
    tft.print("A");
    for (int i = 0; i < 3; i++)
    {
        if (Botnet::getMutation(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
    tft.print("B");
    for (int i = 3; i < 6; i++)
    {
        if (Botnet::getMutation(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
    tft.print("C");
    for (int i = 6; i < 9; i++)
    {
        if (Botnet::getMutation(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
    tft.print("D");
    for (int i = 9; i < 13; i++)
    {
        if (Botnet::getMutation(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
    tft.print("E");
    for (int i = 13; i < 15; i++)
    {
        if (Botnet::getMutation(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
    tft.print("F");
    for (int i = 15; i < 19; i++)
    {
        if (Botnet::getMutation(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
    tft.print("G");
    for (int i = 19; i < 23; i++)
    {
        if (Botnet::getMutation(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
    tft.print("H");
    for (int i = 23; i < 27; i++)
    {
        if (Botnet::getMutation(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
    tft.print("I");
    for (int i = 0; i < 3; i++)
    {
        if (Botnet::getVariant(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
    tft.print("J");
    for (int i = 3; i < 6; i++)
    {
        if (Botnet::getVariant(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
    tft.print("K");
    for (int i = 6; i < 9; i++)
    {
        if (Botnet::getVariant(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
    tft.print("L");
    for (int i = 9; i < 12; i++)
    {
        if (Botnet::getVariant(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
    tft.print("M");
    for (int i = 12; i < 17; i++)
    {
        if (Botnet::getVariant(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
    tft.print("N");
    for (int i = 17; i < 22; i++)
    {
        if (Botnet::getVariant(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
    tft.print("O");
    for (int i = 22; i <27; i++)
    {
        if (Botnet::getVariant(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
    tft.print("P");
    for (int i = 27; i < 28; i++)
    {
        if (Botnet::getVariant(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
    tft.print("Q");
    for (int i = 28; i < 29; i++)
    {
        if (Botnet::getVariant(i) == true)
        {
            tft.print(" [X]");
        }
        else
        {
            tft.print(" [ ]");
        }
    }
    tft.println("");
}

/** Draw the botnet info display
 *
 * Draw the botnet help info/description page
 * 
 */
void MenuSystem::printBotInfoMenu()
{
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextWrap(true);
    tft.println("     Botnet Info");
    tft.println("");
    tft.println("");
    tft.println("  Unlock additional");
    tft.println(" bling on your badge");
    tft.println(" by interacting with");
    tft.println("     other badges!");
    tft.println("");
    tft.println(" Some bling can only");
    tft.println("be unlocked by cross-");
    tft.println("infecting a different");
    tft.println("     type of badge.");
    tft.println("");
    tft.println(" Keep an eye out for");
    tft.println(" other WGCTF badges");
    tft.println(" and don't be afraid");
    tft.println("     to say hello!");
}

/** Draw the botnet main menu
 *
 * Draw the main botnet menu
 * 
 */
void MenuSystem::printBotnetMenu() {
    // Menu System
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextWrap(true);
    switch(_currentMenuPage) {
        case BOT_MENU_MAIN:
            tft.setTextSize(1);
            tft.println("     Botnet Menu");
            switch (_currentMenuOption) {
                case BOT_MAIN_MUTATION:
                    tft.drawRect(30, 49, 65, 9, TFT_WHITE);
                    break;
                case BOT_MAIN_INFECT:
                    tft.drawRect(18, 57, 83, 9, TFT_WHITE);
                    break;
                case BOT_MAIN_INFO:
                    tft.drawRect(24, 65, 76, 9, TFT_WHITE);
                    break;
            }
            tft.setCursor(0, 50);
            tft.println("      Mutations");
            tft.println("    Cross Infect");
            tft.println("     Botnet Info");
            break;
        case BOT_MENU_MUTATION:
            // Covered in own menu
            break;
        case BOT_MENU_INFECT:
            switch (MenuSystem::_currentMenuState)
            {
                case BOT_INFECT_INFO:
                    tft.setTextSize(1);
                    tft.println("    Cross Infect");
                    tft.println("");
                    tft.println("  Attempt to cross");
                    tft.println(" infect with another");
                    tft.println(" badge to obtain one");
                    tft.println(" of their mutations");
                    tft.println("");
                    tft.println(" Press OK and point");
                    tft.println(" the bottoms of your");
                    tft.println("   badges together");
                    tft.println("  while holding them");
                    tft.println("    one foot apart");
                    tft.setTextSize(2);
                    tft.setCursor(0, 145);
                    tft.println("   [OK]   ");
                    break;
                case BOT_INFECT_COM:
                    tft.setTextSize(1);
                    tft.println("    Cross Infect");
                    tft.println("");
                    tft.println("");
                    tft.println("    ATTEMPTING TO");
                    tft.println(" INFECT NEARBY BADGE");
                    tft.println("");
                    tft.setTextSize(2);
                    tft.println("");
                    MenuSystem::printBotInfectAnimation();
                    /*
                    if (_getLoopCtr() % 90 < 30)
                    {
                        tft.println("  o       ");
                    }
                    else if (_getLoopCtr() % 90 < 60)
                    {
                        tft.println("     o     ");
                    }
                    else {
                        tft.println("        o   ");
                    }
                    */

                    for (int i = 0; i < CI_MAX_MESSAGE_SIZE; i++)
                    {
                        tft.drawRect(10+(i*15), 116, 10, 16, TFT_WHITE);
                    }
                    tft.fillRect(0, 135, 128, 3, TFT_RED);
                    
                    tft.setCursor(0, 145);
                    tft.println(" [CANCEL]");
                    break;
                case BOT_INFECT_COMPLETE:
                    tft.setTextSize(1);
                    tft.println("    Cross Infect");
                    tft.println("");
                    tft.setTextSize(2);
                    if (Botnet::crossInfectNewMutationFound || Botnet::crossInfectNewVariantFound)
                    {
                        tft.println("    NEW");
                        tft.println(" MUTATION!");
                        tft.println("");
                        tft.print("    ");
                        if (Botnet::crossInfectNewMutationFound)
                        {
                            tft.println(MenuSystem::getMutationName(MUTATION_TYPE_M, Botnet::crossInfectNewMutation));
                        }
                        else
                        {
                            tft.println(MenuSystem::getMutationName(MUTATION_TYPE_V, Botnet::crossInfectNewVariant));

                        }
                    }
                    else
                    {
                        tft.println("");
                        tft.println("  NO NEW");
                        tft.println(" MUTATION");
                        tft.println("");
                        tft.print("   ");
                    }
                    tft.setCursor(0, 145);
                    tft.println("   [OK]");
                    break;
                case BOT_INFECT_TOO_SOON:
                    tft.setTextSize(1);
                    tft.println("    Cross Infect");
                    tft.println("");
                    tft.setTextSize(2);
                    tft.println(" TO SOON");
                    tft.setTextSize(1);
                    tft.println("");
                    tft.println(" You can only cross ");
                    tft.println("  infect each badge");
                    tft.println(" once every 12 hours");
                    tft.setTextSize(2);
                    tft.setCursor(0, 145);
                    tft.println("   [OK]");
                    break;
                case BOT_INFECT_TIMED_OUT:
                    tft.setTextSize(1);
                    tft.println("    Cross Infect");
                    tft.println("");
                    tft.setTextSize(2);
                    tft.println("");
                    tft.println("");
                    tft.println(" NO BADGE");
                    tft.println("   FOUND");
                    tft.setTextSize(1);
                    tft.println("");
                    tft.println(" Unable to establish ");
                    tft.println("  a connection with");
                    tft.println("    another badge");
                    tft.setTextSize(2);
                    tft.setCursor(0, 145);
                    tft.println("   [OK]");
                    break;
            }
            break;
    }
}

/** Draw the settings menu
 *
 * Draw the main settings menu
 * 
 */
void MenuSystem::printSettingsMenu() {
    tft.setTextWrap(false);
    tft.setCursor(0, 0);
    tft.setTextColor(menuColor);
    tft.setTextSize(2);
    tft.fillScreen(TFT_BLACK);
    switch (_currentMenuPage) {
        case CONF_MENU_MAIN:
            tft.setTextSize(1);
            tft.println("   Settings Menu");
            switch (_currentMenuOption)
            {
                case CONF_MAIN_INFO:
                    tft.drawRect(24, 49, 70, 9, TFT_WHITE);
                    break;
                case CONF_MAIN_NAME:
                    tft.drawRect(12, 57, 94, 9, TFT_WHITE);
                    break;
                case CONF_MAIN_BLING:
                    tft.drawRect(12, 65, 100, 9, TFT_WHITE);
                    break;
                case CONF_MAIN_RESET:
                    tft.drawRect(24, 73, 76, 9, TFT_WHITE);
                    break;
            }
            tft.setCursor(0, 50);
            tft.println("     Badge Info");
            tft.println("   Set Badge Name");
            tft.println("   Configure Bling");
            tft.println("     Reset Badge");
            break;
        case CONF_MENU_INFO:
            tft.setTextSize(1);
            tft.println("Badge Name");
            tft.print(" ");
            tft.println(Botnet::getThisBotName());
            tft.println("Badge ID");
            tft.print(" ");
            tft.println(Botnet::getBadgeID(), HEX);
            tft.println("Node ID");
            tft.print(" ");
            tft.println(Botnet::getBadgeID());
            MenuSystem::printNavArrows();
            break;
        case CONF_MENU_NAME:
            tft.setTextWrap(false);
            tft.setCursor(5, 0);
            tft.setTextColor(menuColor);
            tft.setTextSize(2);
            if (_currentLetter == 4) {
                tft.fillScreen(TFT_BLACK);
            }
            tft.println( "SET NAME");
            tft.fillRect(3, 58, 122, 18, TFT_BLACK);
            tft.drawRect(3, 58, 122, 18, TFT_WHITE);

            tft.setCursor(16, 60);
            tft.print(_currentInput);
            tft.fillRect(3,84,122,18,TFT_BLACK);
            switch (_currentMenuOption) {
                case SELECT_LETTER:
                    tft.fillRect(3,109,122,18,TFT_BLACK);
                    tft.drawRect(52, 84, 12, 16, TFT_GREEN);
                    break;
                case SELECT_OK:
                    tft.fillRect(3,109,122,18,TFT_BLACK);
                    tft.drawRect(22, 109, 26, 16, TFT_GREEN);
                    break;
                case SELECT_DEL:
                    tft.fillRect(3,109,122,18,TFT_BLACK);
                    tft.drawRect(58, 109, 38, 16, TFT_GREEN);
                    break;
            }
            tft.println("");
            tft.setCursor(5, 85);
            for (int i = _currentLetter - 4; i < _currentLetter + 6; i++) {
                tft.print(ALPHABET[i]);
            }
            tft.println("");
            tft.setCursor(0, 110);
            tft.print("  OK DEL  ");
            break;
        case CONF_MENU_BLING:
            tft.setTextSize(1);
            tft.println("   Bling Settings");
            switch (_currentMenuOption)
            {
                case CONF_BLING_EYES:
                    tft.drawRect(30, 49, 65, 9, TFT_WHITE);
                    break;
                case CONF_BLING_PATTERN:
                    tft.drawRect(24, 57, 76, 9, TFT_WHITE);
                    break;
            }
            tft.setCursor(0, 50);
            tft.println("      Eye Color");
            tft.println("     LED Pattern");
            break;
        case CONF_MENU_BLING_EYES:
            tft.setTextSize(1);
            tft.println("      Eye Color");
            switch (_currentMenuOption)
            {
                case CONF_EYE_WHITE:
                    tft.drawRect(36, 49, 41, 9, TFT_WHITE);
                    break;
                case CONF_EYE_RED:
                    tft.drawRect(42, 57, 29, 9, TFT_WHITE);
                    break;
                case CONF_EYE_BLUE:
                    tft.drawRect(36, 65, 35, 9, TFT_WHITE);
                    break;
                case CONF_EYE_RAINBOW:
                    tft.drawRect(30, 73, 52, 9, TFT_WHITE);
                    break;
            }
            tft.setCursor(0, 50);
            tft.println("       White");
            if (Botnet::redEyesUnlocked())
            {
                tft.println("        Red");
            }
            else
            {
                tft.println("     --LOCKED--");
            }
            if (Botnet::blueEyesUnlocked())
            {
                tft.println("       Blue");
            }
            else
            {
                tft.println("     --LOCKED--");
            }
            if (Botnet::rainbowEyesUnlocked())
            {
                tft.println("      Rainbow");
            }
            else
            {
                tft.println("     --LOCKED--");
            }
            break;
        case CONF_MENU_BLING_PATTERN:
            tft.setTextSize(1);
            tft.println("     LED Pattern");
            switch (_currentMenuOption)
            {
                case CONF_PATTERN_OFF:
                    tft.drawRect(42, 49, 29, 9, TFT_WHITE);
                    break;
                case CONF_PATTERN_SOLID:
                    tft.drawRect(36, 57, 41, 9, TFT_WHITE);
                    break;
                case CONF_PATTERN_BLINKING:
                    tft.drawRect(30, 65, 59, 9, TFT_WHITE);
                    break;
                case CONF_PATTERN_RACING:
                    tft.drawRect(36, 73, 47, 9, TFT_WHITE);
                    break;
                case CONF_PATTERN_PULSING:
                    tft.drawRect(36, 81, 41, 9, TFT_WHITE);
                    break;
                case CONF_PATTERN_DAZZLING:
                    tft.drawRect(36, 89, 47, 9, TFT_WHITE);
                    break;
            }
            tft.setCursor(0, 50);
            tft.println("        Off");
            tft.println("       Solid");
            if (Botnet::blinkingBodyUnlocked())
            {
                tft.println("      Blinking");
            }
            else
            {
                tft.println("     --LOCKED--");
            }
            if (Botnet::racingBodyUnlocked())
            {
                tft.println("       Racing");
            }
            else
            {
                tft.println("     --LOCKED--");
            }
            if (Botnet::pulsingBodyUnlocked())
            {
                tft.println("       Pulse");
            }
            else
            {
                tft.println("     --LOCKED--");
            }
            if (Botnet::dazzlingBodyUnlocked())
            {
                tft.println("       Dazzle");
            }
            else
            {
                tft.println("     --LOCKED--");
            }
            break;
        case CONF_MENU_RESET:
            tft.println("***RESET***");
            tft.setTextSize(1);
            tft.setCursor(0, 30);
            tft.setTextColor(TFT_WHITE);
            tft.println("   This will reset");
            tft.println("     EVERYTHING!");
            tft.println(" ");
            tft.println("   You will need to");
            tft.println("  re-sync your badge");
            tft.println("   with the website.");
            tft.setTextColor(menuColor);
            tft.setTextSize(2);
            tft.setCursor(0, 145);
            tft.println("   [OK]   ");
            break;
        case CONF_MENU_RESET_CONFIRM:
            tft.println("***RESET***");
            tft.setCursor(0, 108);
            tft.println(" Confirm?");
            switch (_currentMenuOption) {
                case 0:
                    tft.fillRect(24, 123, 24, 15, TFT_WHITE);
                    break;
                case 1:
                    tft.fillRect(60, 123, 36, 15, TFT_WHITE);
                    break;
                }
            tft.println("  NO YES  ");
            break;
        }
}


/** Draw the secrets menu
 *
 *  --- DEPRECATED ---
 * 
 * Draw the main secrets menu
 * 
 */
void MenuSystem::printSecretsMenu() {
    tft.setTextWrap(false);
    tft.setCursor(5, 0);
    tft.setTextColor(menuColor);
    tft.setTextSize(2);
    if (_currentLetter == 4) {
        tft.fillScreen(TFT_BLACK);
    }
    tft.println("ENTER CODE");

    tft.fillRect(3, 58, 122, 18, TFT_BLACK);
    tft.drawRect(3, 58, 122, 18, TFT_WHITE);

    tft.setCursor(16, 60);
    tft.print(_currentInput);
    tft.fillRect(3,84,122,18,TFT_BLACK);
    switch (_currentMenuOption) {
        case SELECT_LETTER:
            tft.fillRect(3,109,122,18,TFT_BLACK);
            tft.drawRect(52, 84, 12, 16, TFT_GREEN);
            break;
        case SELECT_OK:
            tft.fillRect(3,109,122,18,TFT_BLACK);
            tft.drawRect(22, 109, 26, 16, TFT_GREEN);
            break;
        case SELECT_DEL:
            tft.fillRect(3,109,122,18,TFT_BLACK);
            tft.drawRect(58, 109, 38, 16, TFT_GREEN);
            break;
    }

    tft.println(""); //TODO: Check if this is necessary?
    tft.setCursor(5, 85);
    for (int i = _currentLetter - 4; i < _currentLetter + 6; i++) {
        tft.print(ALPHABET[i]);
    }
    tft.println("");
    tft.setCursor(0, 110);
    tft.print("  OK DEL  ");
}

/** Draw the settings menu accepted message
 *
 * Draw ACCEPTED to the screen to indicate the user-entered secret
 * was accepted by the badge
 * 
 */
void MenuSystem::secretAccept() {
    tft.setCursor(16, 35);
    tft.setTextColor(TFT_GREEN);
    tft.print("ACCEPTED");
}

/** Draw the settings menu rejected message
 *
 * Draw REJECTED to the screen to indicate the user-entered secret
 * was rejected by the badge
 * 
 */
void MenuSystem::secretReject() {
    tft.setCursor(16, 35);
    tft.setTextColor(TFT_RED);
    tft.print("REJECTED");
}

/** Draw the extras menu
 *
 * Draw the main extras menu
 * 
 */
void MenuSystem::printExtrasMenu() {
    tft.setTextWrap(false);
    tft.setCursor(5, 0);
    tft.setTextColor(menuColor);
    tft.setTextSize(2);
    tft.fillScreen(TFT_BLACK);
    if (_currentMenuPage == 0) {
        drawJpeg("/Wi-Fi-Scanner.jpg", 0, 0);
        //printNavArrows();
    }
}

/** Draw the WiFi Scan menu
 *
 * Draw the WiFi Scan menu or its results if completed
 * 
 */
void MenuSystem::printWiFiScan(unsigned int loopCtr) {
    tft.setTextWrap(false);
    tft.setCursor(5,0);
    tft.setTextColor(menuColor);
    tft.setTextSize(2);
    if (_currentMenuOption == 0) {
        _numWiFiNetworks = 0;
        tft.fillScreen(TFT_BLACK);
        tft.println("Scanning..");
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        DEBUG_PRINTLN("Scanning Networks");
        _numWiFiNetworks = WiFi.scanNetworks();
        DEBUG_PRINTLN("Scan Done");
        _currentMenuOption = 1;
        tft.fillScreen(TFT_BLACK);
        printWiFiScan(_getLoopCtr());
    }else if (_currentMenuOption == 1) {
        if (_numWiFiNetworks == 0) {
            DEBUG_PRINTLN("No Networks Found");
            tft.println("No Networks");       
        } else {
            tft.println("Networks:");  
            DEBUG_PRINT(_numWiFiNetworks);
            DEBUG_PRINTLN(" networks found");
            for (int i = 0; i < _numWiFiNetworks; ++i) {
                String text = WiFi.SSID(i) + " (" + WiFi.RSSI(i) + ")";
                (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? text+" " : text+"*";
                if (text.length() > 17) {
                    unsigned int l = text.length();
                    tft.fillRect(24,16+(i*8),104,8,TFT_BLACK);
                    text += "   " + text;
                    text = text.substring(loopCtr % (l+3));
                }
                tft.setTextSize(1);
                if (i < 9) {
                    tft.print(0);
                    tft.print(i + 1);   
                } else {
                    tft.print(i + 1);
                }
                tft.print(": ");
                tft.println(text);
            }
        }
    }
}