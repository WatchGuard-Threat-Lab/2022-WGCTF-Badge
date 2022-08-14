#ifndef HEADER_Botnet
#define HEADER_Botnet
#include "stdint.h"
#include "Arduino.h"
#include <string>
#include <painlessMesh.h>
#include "DebugUtils.h"
#include <Adafruit_IS31FL3731.h>

#define MUTATION_TYPE_M 0
#define MUTATION_TYPE_V 1

#ifdef DEBUG_ENABLED
  #define BASE_MUTATION_RATE 2 
  #define INFECTION_TIMEOUT 1
#else
  #define BASE_MUTATION_RATE 300
  #define INFECTION_TIMEOUT 43200
#endif

#define CI_MAX_MESSAGE_SIZE 7

#define BADGE_TYPE_LION 0
#define BADGE_TYPE_PANDA 1
#define BADGE_TYPE_WG 2

#define BADGE_TYPE BADGE_TYPE_LION  // NOTE: Change to the correct badge type before flashing

#define CISTATE_OFF 0
#define CISTATE_SEND 1
#define CISTATE_LISTEN 2
#define CISTATE_ACK_RECEIVED_NO_MESSAGE 3
#define CISTATE_MESSAGE_RECEIVED_NO_ACK 4
#define CISTATE_MESSAGE_AND_ACK_RECEIVED 5
#define CISTATE_COMPLETED 6
#define CISTATE_SEARCH_TIMEOUT 7

class Botnet {
public:
    static void initBotnet();
    static void resetBadge();

    static void sendGreeting();
    static void receivedCallback(uint32_t from, String & msg);
    static void newConnectionCallback(uint32_t nodeId);
    static void changedConnectionCallback();
    static void nodeTimeAdjustedCallback(int32_t offset);
    static void delayReceivedCallback(uint32_t from, int32_t delay);
    static void executeSchedule();
    static void setBotName(String input);
    static unsigned int getNumNodes();
    static SimpleList<uint32_t> getNodeList();
    
    static String getThisBotName();
    static String getBotName(unsigned int id);
    static unsigned int getBadgeID();
    static unsigned int getBadgeLevel();
    static unsigned int getBadgeXP();
    static unsigned int getBadgeType();
    static unsigned int getXPPercentage();
    static bool getStandbyBling();
    static bool checkCode(String code);

    static void attemptMutation();
    static unsigned int getMutationCount();
    static bool getMutation(unsigned int loc);
    static bool getVariant(unsigned int loc);
    static unsigned long getMutations();
    static unsigned long getVariants();
    static bool redEyesUnlocked();
    static bool blueEyesUnlocked();
    static bool rainbowEyesUnlocked();
    static bool blinkingBodyUnlocked();
    static bool racingBodyUnlocked();
    static bool pulsingBodyUnlocked();
    static bool dazzlingBodyUnlocked();
    static void saveMutation(unsigned int type, unsigned int loc);

    static void saveEyeColor(unsigned int color);
    static void saveBlingMode(unsigned short mode);
    static unsigned int getSavedEyeColor();
    static unsigned short getSavedBlingMode();
    static unsigned int getNumBadgesEncountered();
    static unsigned int getNumBadgesInfected();

    static int mutate();
    static void startCrossInfect();
    static void stopCrossInfect();
    static void timeoutCrossInfect();
    static void sendQueuedMessage();
    static void sendCrossInfectMessage();
    static void sendCrossInfectAck();
    static void crossInfectQueueRetry();
    static bool pendingMessageComplete();
    static void processCompleteMessage();

    static void saveBotnetIterations();
    static void incrementBotnetIterations();
    static unsigned int getBotnetIterations();

    static String getBTPassword();

    static bool crossInfectTimedOut;
    static bool crossInfectCompleted;
    static bool crossInfectNewMutationFound;
    static bool crossInfectNewVariantFound;
    static bool crossInfectTooSoon;
    static unsigned short crossInfectNewMutation;
    static unsigned short crossInfectNewVariant;

    static bool mouthComplete;
    static bool baseBodyComplete;
    static bool fullBodyComplete;
    static bool badgeEyesComplete;
    static bool baseEyesComplete;
    static bool fullEyesComplete;
    static bool fullBodyAndBaseEyesComplete;
    static bool allBlingComplete;

private:
    static void initPreferences();
    static String generateBTPassword();
    static void listenCrossInfect();

    static unsigned int botnetIterations;
    static unsigned int bootCount;

    static unsigned int badgeID;
    static unsigned int badgeType;
    static unsigned int badgeLevel;
    static unsigned int badgeXP;
    static unsigned int numBadgesEncountered;
    static unsigned int numInfectedBadges;
    static String botName;
    static String btPassword;

    static unsigned long mutation;
    static unsigned long variant;

    static unsigned int crossInfectTarget;
    static unsigned int crossInfectState;
    static unsigned int crossInfectSendAttempts;
    static bool crossInfectPendingMessageStarted;

    static uint16_t crossInfectPendingMessage[CI_MAX_MESSAGE_SIZE];
    static unsigned int crossInfectPendingMessageParts[CI_MAX_MESSAGE_SIZE];
    static bool crossInfectPendingMessageComplete;
    static bool crossInfectRetryQueued;
    static bool crossInfectMessageAcked;


};

#endif