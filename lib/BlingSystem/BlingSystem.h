
#ifndef HEADER_BlingSystem
#define HEADER_BlingSystem
#include "stdint.h"
#include "Arduino.h"
#include <string>
#include <Adafruit_IS31FL3731.h>

#define LED_X 16
#define LED_Y 9

#define LEFT_EYE 8
#define RIGHT_EYE 9

#define RED 2
#define BLUE 0
#define GREEN 1

#define EYES_WHITE 0
#define EYES_RED 1
#define EYES_BLUE 2
#define EYES_GREEN 3
#define EYES_RAINBOW 4

#define MAX_BLING_MUTATIONS 17

#define BLING_EARS 0
#define BLING_FEET 1
#define BLING_HANDS 2
#define BLING_FACE 3
#define BLING_FACE_BLINK 4
#define BLING_HEAD_STITCHES 5
#define BLING_LEG_STITCHES 6
#define BLING_ARM_STITCHES 7
#define BLING_RED_EYES 8
#define BLING_BLUE_EYES 9
#define BLING_PULSE_EYES 10
#define BLING_BLINK_BODY 11
#define BLING_RACE_BODY 12
#define BLING_PULSE_BODY 13
#define BLING_DAZZLE_BODY 14
#define BLING_GREEN_EYES 15
#define BLING_RAINBOW_EYES 16

#define LIGHT_INTENSITY_GRID_SIZE 8

#define BLING_MODE_STEADY 0
#define BLING_MODE_BLINK 1
#define BLING_MODE_PULSE 2
#define BLING_MODE_RACE 3
#define BLING_MODE_DAZZLE 4
#define BLING_MODE_OFF 5

class BlingSystem {
public:
    static void initBlingSystem(
        unsigned int (*getLoopCtr)());
    static void blingTask();
    static void executeSchedule();
    static void updateUnlockedBling();
    static void setEyeColor(unsigned int color);
    static void setBlingMode(unsigned short mode);
    static void unlockNewEyeColor(unsigned int color);
    static void unlockNewBLingMode(unsigned int mode);

private:
    static Adafruit_IS31FL3731 ledMatrix;
    static short int _ledArr[16][9];
    static unsigned int _ctr;
    static unsigned int (*_getLoopCtr)();
    static unsigned int _badgeType;
    static unsigned int _eyeIntensity;
    static unsigned int _bodyIntensity;
    static unsigned short _blingMode;
    static unsigned int _eyeColor;
    static unsigned int _lightIntensityGrid[LIGHT_INTENSITY_GRID_SIZE];
    static short int _unlockedBling[MAX_BLING_MUTATIONS];
    static bool _allLEDsUnlocked;
    static void saveUnlockedBling(unsigned long m, unsigned long v);

    static void clearLEDs();
    static void writeLEDs();
    static void fullBodyLEDs();
    static void eyeLEDs(unsigned int color);
    static void whiteEyeLEDs();
    static void rainbowEyeLEDs();
    static void pandaMouth();
    static void pandaHeadStitches();
    static void pandaLegStitches();
    static void pandaArmStitches();
    static void pandaFeet();
    static void pandaArms();
    static void pandaEars();
    static void pandaDrool();
    static void lionWhiskerBlink();

    static void lionEars();
    static void lionHeadStitches();
    static void lionWhiskers();
    static void lionLegStitches();
    static void lionTail();
    static void lionFeet();
    static void lionHands();

    static void setEyeLEDs();
    static void setBodyLEDs();

    static void setEyeIntensity();
    static void setBodyIntensity();
};

#endif