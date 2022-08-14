#include "BlingSystem.h"
#include "Botnet.h"
#include <Adafruit_IS31FL3731.h>
#include <painlessMesh.h>
#include "DebugUtils.h"

Adafruit_IS31FL3731 BlingSystem::ledMatrix = Adafruit_IS31FL3731();
Scheduler blingScheduler;

unsigned int (*BlingSystem::_getLoopCtr)();
unsigned int BlingSystem::_ctr;
short int BlingSystem::_ledArr[LED_X][LED_Y];
unsigned int BlingSystem::_badgeType;
unsigned int BlingSystem::_eyeIntensity;
unsigned int BlingSystem::_bodyIntensity;
unsigned short BlingSystem::_blingMode;
bool BlingSystem::_allLEDsUnlocked;
unsigned int BlingSystem::_eyeColor;
unsigned int BlingSystem::_lightIntensityGrid[LIGHT_INTENSITY_GRID_SIZE];
short int BlingSystem::_unlockedBling[MAX_BLING_MUTATIONS];

Task ledScheduledTask(TASK_MILLISECOND * 100, TASK_FOREVER, &BlingSystem::blingTask);
Task updateUnlockedBlingTask(TASK_SECOND, TASK_FOREVER, &BlingSystem::updateUnlockedBling);

/** Initialize the BlingSystem module
 *
 * Called during badge startup, this initializes the BlingSystem
 * module including LED controller and currently selected bling/color modes
 * 
 */
void BlingSystem::initBlingSystem(
  unsigned int(*getLoopCtr)()
) {
  _ctr = 0;
  _getLoopCtr = getLoopCtr;
  _eyeIntensity = 128;
  _bodyIntensity = 128;
  _allLEDsUnlocked = false;
  _eyeColor = Botnet::getSavedEyeColor();
  _blingMode = Botnet::getSavedBlingMode();
  _badgeType = Botnet::getBadgeType();



  for (int i = 0; i < LIGHT_INTENSITY_GRID_SIZE; i++)
  {
    BlingSystem::_lightIntensityGrid[i] = 128;
  }

  for (int i = 0; i < MAX_BLING_MUTATIONS; i++)
  {
    BlingSystem::_unlockedBling[i] = 0;
  }

  BlingSystem::updateUnlockedBling();

  DEBUG_PRINTLN("Initializing LED Controller...");
  if (!BlingSystem::ledMatrix.begin()) {
    DEBUG_PRINTLN("LED Controller Not Found");
    while (1);
  }
  DEBUG_PRINTLN("LED Controller Initialized");

  blingScheduler.startNow();
  blingScheduler.addTask(ledScheduledTask);
  blingScheduler.addTask(updateUnlockedBlingTask);
  ledScheduledTask.enable();
  updateUnlockedBlingTask.enable();
}

/** Save current mutations and variants
 *
 * Get the current mutations and variants from the Botnet and
 * save them for later reference
 * 
 */
void BlingSystem::updateUnlockedBling()
{
  unsigned long mutations = Botnet::getMutations();
  unsigned long variants = Botnet::getVariants();

  BlingSystem::saveUnlockedBling(mutations, variants);
}

/** Save the unlocked bling options
 *
 * Parse the badge's current mutations and variants and save
 * referencible lookups for specific bling modes.
 * 
 */
void BlingSystem::saveUnlockedBling(unsigned long m, unsigned long v)
{
  if (m == 0x3FFFFFF && v == 0x3FFFFFF)
  {
    BlingSystem::_allLEDsUnlocked = true;
  }

  if (bitRead(m, 0) == 1 && bitRead(m, 1) == 1 && bitRead(m, 2) == 1)
  {
    BlingSystem::_unlockedBling[BLING_EARS] = 1;
  }

  if (bitRead(m, 3) == 1 && bitRead(m, 4) == 1 && bitRead(m, 5) == 1)
  {
    BlingSystem::_unlockedBling[BLING_FEET] = 1;
  }

  if (bitRead(m, 6) == 1 && bitRead(m, 7) == 1 && bitRead(m, 8) == 1)
  {
    BlingSystem::_unlockedBling[BLING_HANDS] = 1;
  }

  if (bitRead(m, 9) == 1 && bitRead(m, 10) == 1 && bitRead(m, 11) == 1 && bitRead(m, 12) == 1)
  {
    BlingSystem::_unlockedBling[BLING_FACE] = 1;
  }

  if (bitRead(m, 13) == 1 && bitRead(m, 14) == 1)
  {
    BlingSystem::_unlockedBling[BLING_FACE_BLINK] = 1;
  }

  if (bitRead(m, 15) == 1 && bitRead(m, 16) == 1 && bitRead(m, 17) == 1 && bitRead(m, 18) == 1)
  {
    BlingSystem::_unlockedBling[BLING_HEAD_STITCHES] = 1;
  }

  if (bitRead(m, 19) == 1 && bitRead(m, 20) == 1 && bitRead(m, 21) == 1 && bitRead(m, 22) == 1)
  {
    BlingSystem::_unlockedBling[BLING_LEG_STITCHES] = 1;
  }

  if (bitRead(m, 23) == 1 && bitRead(m, 24) == 1 && bitRead(m, 25) == 1 && bitRead(m, 26) == 1)
  {
    BlingSystem::_unlockedBling[BLING_ARM_STITCHES] = 1;
  }

  if (bitRead(v, 0) == 1 && bitRead(v, 1) == 1 && bitRead(v, 2) == 1)
  {
    BlingSystem::_unlockedBling[BLING_RED_EYES] = 1;
  }

  if (bitRead(v, 3) == 1 && bitRead(v, 4) == 1 && bitRead(v, 5) == 1)
  {
    BlingSystem::_unlockedBling[BLING_BLUE_EYES] = 1;
  }

  if (bitRead(v, 6) == 1 && bitRead(v, 7) == 1 && bitRead(v, 8) == 1)
  {
    BlingSystem::_unlockedBling[BLING_PULSE_EYES] = 1;
  }

  if (bitRead(v, 9) == 1 && bitRead(v, 10) == 1 && bitRead(v, 11) == 1)
  {
    BlingSystem::_unlockedBling[BLING_BLINK_BODY] = 1;
  }

  if (bitRead(v, 12) == 1 && bitRead(v, 13) == 1 && bitRead(v, 14) == 1 && bitRead(v, 15) == 1 && bitRead(v, 16) == 1)
  {
    BlingSystem::_unlockedBling[BLING_RACE_BODY] = 1;
  }

  if (bitRead(v, 17) == 1 && bitRead(v, 18) == 1 && bitRead(v, 19) == 1 && bitRead(v, 20) == 1 && bitRead(v, 21) == 1)
  {
    BlingSystem::_unlockedBling[BLING_PULSE_BODY] = 1;
  }

  if (bitRead(v, 22) == 1 && bitRead(v, 23) == 1 && bitRead(v, 24) == 1 && bitRead(v, 25) == 1 && bitRead(v, 26) == 1)
  {
    BlingSystem::_unlockedBling[BLING_DAZZLE_BODY] = 1;
  }

  if (bitRead(v, 27) == 1)
  {
    BlingSystem::_unlockedBling[BLING_GREEN_EYES] = 1;
  }

  if (bitRead(v, 28) == 1)
  {
    BlingSystem::_unlockedBling[BLING_RAINBOW_EYES] = 1;
  }
}

void BlingSystem::setEyeColor(unsigned int color)
{
  BlingSystem::_eyeColor = color;
  Botnet::saveEyeColor(color);
}

void BlingSystem::setBlingMode(unsigned short mode)
{
  BlingSystem::_blingMode = mode;
  Botnet::saveBlingMode(mode);
}

void BlingSystem::unlockNewEyeColor(unsigned int color)
{
  BlingSystem::_eyeColor = color;
  Botnet::saveEyeColor(color);
}

void BlingSystem::unlockNewBLingMode(unsigned int mode)
{
  BlingSystem::_blingMode = mode;
  Botnet::saveBlingMode(mode);
}

void BlingSystem::executeSchedule() {
    blingScheduler.execute();
}

void BlingSystem::clearLEDs() {
  for (int x = 0; x < LED_X; x++) {
    for (int y = 0; y < LED_Y; y++) {
      BlingSystem::_ledArr[x][y] = 0;
    }
  }
}

void BlingSystem::writeLEDs() {
  for (int x = 0; x < LED_X; x++) {
    for (int y = 0; y < LED_Y; y++) {
      BlingSystem::ledMatrix.drawPixel(x, y, BlingSystem::_ledArr[x][y]);
    }
  }
}

void BlingSystem::fullBodyLEDs() {
  for (int x = 0; x < 6; x++) {
    for (int y = 0; y < 7; y++) {
      BlingSystem::_ledArr[x][y] = BlingSystem::BlingSystem::_lightIntensityGrid[x%6];
    }
  }
}

void BlingSystem::setEyeLEDs()
{

  switch (BlingSystem::_eyeColor)
  {
    case EYES_WHITE:
      if (_badgeType == BADGE_TYPE_WG)
      {
        BlingSystem::eyeLEDs(GREEN);
      }
      else
      {
        BlingSystem::whiteEyeLEDs();
      }
      break;
    case EYES_BLUE:
      BlingSystem::eyeLEDs(BLUE);
      break;
    case EYES_RED:
      BlingSystem::eyeLEDs(RED);
      break;
    case EYES_GREEN:
      BlingSystem::eyeLEDs(GREEN);
      break;
    case EYES_RAINBOW:
      BlingSystem::rainbowEyeLEDs();
      break;
  }
}

void BlingSystem::rainbowEyeLEDs()
{
  if (BlingSystem::_blingMode == BLING_MODE_OFF)
  {
    BlingSystem::_ledArr[LEFT_EYE][RED] = 0;
    BlingSystem::_ledArr[RIGHT_EYE][RED] = 0;
    BlingSystem::_ledArr[LEFT_EYE][BLUE] = 0;
    BlingSystem::_ledArr[RIGHT_EYE][BLUE] = 0;
    BlingSystem::_ledArr[LEFT_EYE][GREEN] = 0;
    BlingSystem::_ledArr[RIGHT_EYE][GREEN] = 0;
  }
  else
  {
    unsigned int x = (BlingSystem::_ctr % 26);
    if (x > 13)
    {
      x = 26 - x;
    }

    x = x * 16;

    unsigned int y = (BlingSystem::_ctr % 46);
    if (y > 23)
    {
      y = 46 - y;
    }
    y = y * 10;

    unsigned int z = (BlingSystem::_ctr % 74);
    if (z > 37)
    {
      z = 74 - z;
    }
    z = z * 6;

    BlingSystem::_ledArr[LEFT_EYE][RED] = x;
    BlingSystem::_ledArr[RIGHT_EYE][RED] = x;
    BlingSystem::_ledArr[LEFT_EYE][BLUE] = y;
    BlingSystem::_ledArr[RIGHT_EYE][BLUE] = y;
    BlingSystem::_ledArr[LEFT_EYE][GREEN] = z;
    BlingSystem::_ledArr[RIGHT_EYE][GREEN] = z;
  }
}

void BlingSystem::setBodyLEDs()
{
  if (BlingSystem::_allLEDsUnlocked == true || _badgeType == BADGE_TYPE_WG)
  {
    BlingSystem::fullBodyLEDs();
  }
  else
  {
    if (BlingSystem::_unlockedBling[BLING_EARS] == 1)
    {
      if (_badgeType == BADGE_TYPE_LION)
      {
        BlingSystem::lionEars();
      }
      else if (_badgeType == BADGE_TYPE_PANDA)
      {
        BlingSystem::pandaEars();
      }
    }

    if (BlingSystem::_unlockedBling[BLING_FEET] == 1)
    {
      if (_badgeType == BADGE_TYPE_LION)
      {
        BlingSystem::lionFeet();
      }
      else if (_badgeType == BADGE_TYPE_PANDA)
      {
        BlingSystem::pandaFeet();
      }
    }

    if (BlingSystem::_unlockedBling[BLING_HANDS] == 1)
    {
      if (_badgeType == BADGE_TYPE_LION)
      {
        BlingSystem::lionHands();
      }
      else if (_badgeType == BADGE_TYPE_PANDA)
      {
        BlingSystem::pandaArms();
      }
    }

    if (BlingSystem::_unlockedBling[BLING_FACE] == 1)
    {
      if (_badgeType == BADGE_TYPE_LION)
      {
        BlingSystem::lionWhiskers();
      }
      else if (_badgeType == BADGE_TYPE_PANDA)
      {
        BlingSystem::pandaMouth();
      }
    }

    if (BlingSystem::_unlockedBling[BLING_FACE_BLINK] == 1)
    {
      if (_badgeType == BADGE_TYPE_LION)
      {
        BlingSystem::lionWhiskerBlink();
      }
      else if (_badgeType == BADGE_TYPE_PANDA)
      {
        BlingSystem::pandaDrool();
      }
    }

    if (BlingSystem::_unlockedBling[BLING_HEAD_STITCHES] == 1)
    {
      if (_badgeType == BADGE_TYPE_LION)
      {
        BlingSystem::lionHeadStitches();
      }
      else if (_badgeType == BADGE_TYPE_PANDA)
      {
        BlingSystem::pandaHeadStitches();
      }
    }

    if (BlingSystem::_unlockedBling[BLING_LEG_STITCHES] == 1)
    {
      if (_badgeType == BADGE_TYPE_LION)
      {
        BlingSystem::lionLegStitches();
      }
      else if (_badgeType == BADGE_TYPE_PANDA)
      {
        BlingSystem::pandaLegStitches();
      }
    }

    if (BlingSystem::_unlockedBling[BLING_ARM_STITCHES] == 1)
    {
      if (_badgeType == BADGE_TYPE_LION)
      {
        BlingSystem::lionTail();
      }
      else if (_badgeType == BADGE_TYPE_PANDA)
      {
        BlingSystem::pandaArmStitches();
      }
    }
  }
  
}

void BlingSystem::eyeLEDs(unsigned int color) {
  BlingSystem::_ledArr[LEFT_EYE][color] = BlingSystem::_eyeIntensity;
  BlingSystem::_ledArr[RIGHT_EYE][color] = BlingSystem::_eyeIntensity;
}

void BlingSystem::whiteEyeLEDs() {
  BlingSystem::_ledArr[LEFT_EYE][RED] = BlingSystem::_eyeIntensity;
  BlingSystem::_ledArr[RIGHT_EYE][RED] = BlingSystem::_eyeIntensity;
  BlingSystem::_ledArr[LEFT_EYE][BLUE] = BlingSystem::_eyeIntensity;
  BlingSystem::_ledArr[RIGHT_EYE][BLUE] = BlingSystem::_eyeIntensity;
  BlingSystem::_ledArr[LEFT_EYE][GREEN] = BlingSystem::_eyeIntensity;
  BlingSystem::_ledArr[RIGHT_EYE][GREEN] = BlingSystem::_eyeIntensity;
}

void BlingSystem::pandaMouth() {
  BlingSystem::_ledArr[4][1] = BlingSystem::_lightIntensityGrid[0];
  BlingSystem::_ledArr[5][1] = BlingSystem::_lightIntensityGrid[1];
}

void BlingSystem::pandaHeadStitches() {
  for (int x = 4; x < 6; x++) {
    BlingSystem::_ledArr[x][0] = BlingSystem::_lightIntensityGrid[x%2];;
  }
  for (int x = 0; x < 4; x++) {
    BlingSystem::_ledArr[x][1] = BlingSystem::_lightIntensityGrid[x%4];;
  }
}

void BlingSystem::pandaLegStitches() {
  for (int x = 1; x < 6; x++) {
    BlingSystem::_ledArr[x][4] = BlingSystem::_lightIntensityGrid[x%5];;
  }
  BlingSystem::_ledArr[0][5] = BlingSystem::_lightIntensityGrid[6];
}

void BlingSystem::pandaArmStitches()
{
  for (int x = 0; x < 4; x++) {
    BlingSystem::_ledArr[x][3] = BlingSystem::_lightIntensityGrid[x%4];;
  }
}

void BlingSystem::pandaArms()
{
  for (int x = 1; x < 6; x++) {
    BlingSystem::_ledArr[x][2] = BlingSystem::_lightIntensityGrid[x%5];
  }
  for (int x = 4; x < 6; x++) {
    BlingSystem::_ledArr[x][3] = BlingSystem::_lightIntensityGrid[x%2];;
  }
  BlingSystem::_ledArr[0][4] = BlingSystem::_lightIntensityGrid[3];
}

void BlingSystem::pandaFeet()
{
  for (int x = 1; x < 6; x++) {
    BlingSystem::_ledArr[x][5] = BlingSystem::_lightIntensityGrid[x%5];
  }
  for (int x = 0; x < 4; x++) {
    BlingSystem::_ledArr[x][6] = BlingSystem::_lightIntensityGrid[x%4];
  }
}

void BlingSystem::pandaDrool() {
  if (BlingSystem::_blingMode == BLING_MODE_OFF)
  {
    BlingSystem::_ledArr[0][2] = 0;
  }
  else
  {
    const int rand = random(0, 9);
    BlingSystem::_ledArr[0][2] = (rand % 3) * 60;
  }
}

void BlingSystem::pandaEars()
{
  for (int x = 0; x < 4; x++) {
    BlingSystem::_ledArr[x][0] = BlingSystem::_lightIntensityGrid[x%4];
  }
}

void BlingSystem::lionEars()
{
  for (int x = 0; x < 3; x++) {
    BlingSystem::_ledArr[x][0] = BlingSystem::_lightIntensityGrid[x%3];
  }
}

void BlingSystem::lionHeadStitches()
{
  for (int x = 3; x < 5; x++) {
    BlingSystem::_ledArr[x][0] = BlingSystem::_lightIntensityGrid[x%2];
  }
  for (int x = 3; x < 5; x++) {
    BlingSystem::_ledArr[x][1] = BlingSystem::_lightIntensityGrid[x%2];
  }
}

void BlingSystem::lionWhiskers()
{
  if (BlingSystem::_blingMode == BLING_MODE_OFF)
  {
    BlingSystem::_ledArr[5][0] = 0;
    for (int x = 0; x < 3; x++) {
      BlingSystem::_ledArr[x][1] = 0;
    }
  }
  else
  {
    BlingSystem::_ledArr[5][0] = BlingSystem::_lightIntensityGrid[4];
    for (int x = 0; x < 3; x++) {
      BlingSystem::_ledArr[x][1] = BlingSystem::_lightIntensityGrid[x%3];
    }
  }
}

void BlingSystem::lionWhiskerBlink() {
  int rand = random(0, 9);
  BlingSystem::_ledArr[5][0] = (rand % 3) * 60;
  for (int x = 0; x < 3; x++) {
    int rand2 = random(0, 9);
    BlingSystem::_ledArr[x][1] = (rand2 % 3) * 60;
  }
}

void BlingSystem::lionLegStitches()
{
  for (int x = 3; x < 6; x++) {
    BlingSystem::_ledArr[x][2] = BlingSystem::_lightIntensityGrid[x%3];
  }
  for (int x = 0; x < 3; x++) {
    BlingSystem::_ledArr[x][3] = BlingSystem::_lightIntensityGrid[x%3+3];
  }

  for (int x = 1; x < 6; x++) {
    BlingSystem::_ledArr[x][4] = BlingSystem::_lightIntensityGrid[x%5];
  }
}

void BlingSystem::lionTail()
{
  BlingSystem::_ledArr[5][1] = BlingSystem::_lightIntensityGrid[4];
  for (int x = 0; x < 3; x++) {
    BlingSystem::_ledArr[x][2] = BlingSystem::_lightIntensityGrid[x%3];
  }
}

void BlingSystem::lionFeet()
{
  for (int x = 0; x < 2; x++) {
    BlingSystem::_ledArr[x][5] = BlingSystem::_lightIntensityGrid[x%2];
  }
  BlingSystem::_ledArr[0][4] = BlingSystem::_lightIntensityGrid[3];
  BlingSystem::_ledArr[3][3] = BlingSystem::_lightIntensityGrid[4];
  BlingSystem::_ledArr[3][6] = BlingSystem::_lightIntensityGrid[5];
  for (int x = 4; x < 6; x++) {
    BlingSystem::_ledArr[x][3] = BlingSystem::_lightIntensityGrid[x%2+3];
  }
}

void BlingSystem::lionHands()
{
  for (int x = 2; x < 6; x++) {
    BlingSystem::_ledArr[x][5] = BlingSystem::_lightIntensityGrid[x%4];
  }
  for (int x = 0; x < 3; x++) {
    BlingSystem::_ledArr[x][6] = BlingSystem::_lightIntensityGrid[x%3+3];
  }
}

void BlingSystem::setEyeIntensity()
{
  if (BlingSystem::_blingMode == BLING_MODE_OFF)
  {
    BlingSystem::_eyeIntensity = 0;
  }
  else{
    unsigned int i = (BlingSystem::_ctr % 64);
    if (i > 32)
    {
      i = 64 - i;
    }
    i = i * 4;
    BlingSystem::_eyeIntensity = i;
  }
}

void BlingSystem::setBodyIntensity()
{
  unsigned int x = (BlingSystem::_ctr % 8);
  unsigned int y = (BlingSystem::_ctr % 64);
  switch (BlingSystem::_blingMode)
  {
    case BLING_MODE_STEADY:
      for (int i = 0; i < LIGHT_INTENSITY_GRID_SIZE; i++)
      {
        BlingSystem::_lightIntensityGrid[i] = 128;
      }
      break;
    case BLING_MODE_BLINK:
      if (x < 4)
      {
        for (int i = 0; i < LIGHT_INTENSITY_GRID_SIZE; i++)
        {
          BlingSystem::_lightIntensityGrid[i] = 128;
        }
      }
      else
      {
        for (int i = 0; i < LIGHT_INTENSITY_GRID_SIZE; i++)
        {
          BlingSystem::_lightIntensityGrid[i] = 0;
        }
      }
      break;
    case BLING_MODE_PULSE:
      if (y > 32)
      {
        y = 64 - y;
      }
      y = y * 8;
      for (int i = 0; i < LIGHT_INTENSITY_GRID_SIZE; i++)
      {
        BlingSystem::_lightIntensityGrid[i] = y;
      }
      break;
    case BLING_MODE_RACE:
      for (int i = 0; i < LIGHT_INTENSITY_GRID_SIZE; i++)
      {
        if (BlingSystem::_ctr % 4 == (i % 4))
        {
          BlingSystem::_lightIntensityGrid[i] = 256;
        }
        else if (BlingSystem::_ctr % 4 == ((i % 4) - 1))
        {
          BlingSystem::_lightIntensityGrid[i] = 16;
        }
        else if (BlingSystem::_ctr % 4 == ((i % 4) - 2))
        {
          BlingSystem::_lightIntensityGrid[i] = 8;
        }
        else
        {
          BlingSystem::_lightIntensityGrid[i] = 0;
        }
      }
      break;
    case BLING_MODE_DAZZLE:
      for (int i = 0; i < LIGHT_INTENSITY_GRID_SIZE; i++)
      {
        const int rand = random(3);
        if (rand == 0)
        {
          BlingSystem::_lightIntensityGrid[i] = 240;
        }
        else if (rand == 1)
        {
          BlingSystem::_lightIntensityGrid[i] = 90;
        }
        else
        {
          BlingSystem::_lightIntensityGrid[i] = 0;
        }
        
      }
      break;
    case BLING_MODE_OFF:
      for (int i = 0; i < LIGHT_INTENSITY_GRID_SIZE; i++)
      {
        BlingSystem::_lightIntensityGrid[i] = 0;
      }
      break;
  }
}


void BlingSystem::blingTask() {
  BlingSystem::_ctr++;
  BlingSystem::clearLEDs();
  BlingSystem::setEyeIntensity();
  BlingSystem::setBodyIntensity();
  BlingSystem::setEyeLEDs();
  BlingSystem::setBodyLEDs();

  BlingSystem::writeLEDs();
}