#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_IS31FL3731.h>
#include <TimeLib.h>
#include <Preferences.h>
#include "DebugUtils.h"

#include <Botnet.h>
#include <MenuSystem.h>
#include <BlingSystem.h>
#include <ConsoleSystem.h>

#include <painlessMesh.h>

#define IRSEND 25
#define IRRECV 26

void incrementLoop(); // Prototype
Task taskHandler( TASK_MILLISECOND * 100, TASK_FOREVER, &incrementLoop);
 
Scheduler mainScheduler;

const int BTNA = 14;
const int BTNB = 15;
const int BTNC = 34;
const int BTNU = 33;
const int BTNR = 32;
const int BTNL = 27;
const int BTND = 19;

unsigned int loopCtr = 0;

int upButton = 0;
int downButton = 0;
int leftButton = 0;
int rightButton = 0;
int acceptButton = 0;
int backButton = 0;
int centerButton = 0;

unsigned int badgeID = 0;
unsigned int botState = 0;
unsigned int botLvl = 1;
unsigned int badgeType = 1;
unsigned int numAS = 0; //number attack success
unsigned int numAF = 0; //number attack fail
unsigned int numDS = 0;
unsigned int numDF = 0;

typedef struct {
  unsigned int badgeID;
  unsigned int time;
} Attacked_Badge;

Attacked_Badge attacked_badges[550];

typedef struct {
  unsigned int badgeID;
  unsigned int color;
  unsigned int timeLeft;
} Infection_Details;


void incrementLoop() {
  loopCtr++;
}

unsigned int getLoopCtr() {
  return loopCtr;
}

void setup() {
  Serial.begin(115200);
  DEBUG_PRINTLN("Initialization Starting...");

  randomSeed(analogRead(16));

  // Setup Buttons
  pinMode(BTNB, INPUT);
  pinMode(BTNR, INPUT);
  pinMode(BTND, INPUT);
  pinMode(BTNC, INPUT);
  pinMode(BTNA, INPUT);
  pinMode(BTNL, INPUT);
  pinMode(BTNU, INPUT);

  Botnet::initBotnet();

  DEBUG_PRINTLN("Botnet Initialized");

  ConsoleSystem::initConsoleSystem(
    getLoopCtr);

    DEBUG_PRINTLN("Console System Initialized");

  BlingSystem::initBlingSystem(
    getLoopCtr);  

  DEBUG_PRINTLN("Bling System Initialized");

  mainScheduler.addTask( taskHandler );
  taskHandler.enable();
  DEBUG_PRINTLN("Initialization complete");
  

  DEBUG_PRINT("Badge ID: 0x");
  badgeID = Botnet::getBadgeID();
  DEBUG_PRINTLN_HEX(badgeID);
  DEBUG_PRINT("Badge Type: ");
  badgeType = Botnet::getBadgeType();
  DEBUG_PRINTLN(badgeType);
   
  MenuSystem::initMenuSystem(
    getLoopCtr);
}

void loop() {
  Botnet::executeSchedule();
  MenuSystem::executeSchedule();
  BlingSystem::executeSchedule();
  ConsoleSystem::executeLoop();

  upButton = digitalRead(BTNU);
  downButton = digitalRead(BTND);
  leftButton = digitalRead(BTNL);
  rightButton = digitalRead(BTNR);
  acceptButton = digitalRead(BTNA);
  backButton = digitalRead(BTNB);
  centerButton = digitalRead(BTNC);
  mainScheduler.execute();

  if (upButton) {
    MenuSystem::moveUp();
  }
  if (downButton) {
    MenuSystem::moveDown();
  } 
  if (leftButton) {
    MenuSystem::moveLeft();
  } 
  if (rightButton) {
    MenuSystem::moveRight();
  }
  if (acceptButton) {
    MenuSystem::accept();
  } 
  if (backButton) {
    MenuSystem::back();
  }
  if (centerButton) {
  }
}
