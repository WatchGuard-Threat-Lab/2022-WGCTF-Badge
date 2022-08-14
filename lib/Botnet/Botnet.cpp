#include "Botnet.h"
#include "DebugUtils.h"
#include "MenuSystem.h"
#include "BlingSystem.h"

#include <Preferences.h>
#include <TimeLib.h>

#include <painlessMesh.h>
#include <Adafruit_IS31FL3731.h>
#include <IRremote.h>

#define MESH_SSID "WGCTFMESH"
#define MESH_PASSWORD "WGCTF2022WelcomeBack"
#define MESH_PORT 5555

typedef struct {
  unsigned int badgeID;
  unsigned int timeLeft;
} Infected_Badge;

Infected_Badge infected_badges[550];


char possibleValues[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"; // 62

Scheduler meshScheduler;
Scheduler botnetScheduler;
SimpleList<uint32_t> nodes;
painlessMesh mesh;
Preferences preferences;

union split32 {
    uint32_t fullHex;
    uint16_t splitHex[2];
};

Task incrementIterationsTask(TASK_SECOND, TASK_FOREVER, &Botnet::incrementBotnetIterations);
Task saveIterationsTask(TASK_SECOND * 10, TASK_FOREVER, &Botnet::saveBotnetIterations);
Task taskAttemptMutation(TASK_SECOND * 2, TASK_FOREVER, &Botnet::attemptMutation);
Task sendCrossInfectMessageTask(TASK_SECOND, TASK_ONCE, &Botnet::sendQueuedMessage);
Task timeoutCrossInfectTask(TASK_SECOND, TASK_ONCE, &Botnet::timeoutCrossInfect);

unsigned int Botnet::botnetIterations;
unsigned int Botnet::bootCount;

unsigned int Botnet::badgeID;
unsigned int Botnet::badgeType;
unsigned int Botnet::numBadgesEncountered;
unsigned int Botnet::numInfectedBadges;
String Botnet::botName;
String Botnet::btPassword;

unsigned long Botnet::mutation;
unsigned long Botnet::variant;
bool Botnet::mouthComplete;
bool Botnet::baseBodyComplete;
bool Botnet::fullBodyComplete;
bool Botnet::badgeEyesComplete;
bool Botnet::baseEyesComplete;
bool Botnet::fullEyesComplete;
bool Botnet::fullBodyAndBaseEyesComplete;
bool Botnet::allBlingComplete;

unsigned int Botnet::crossInfectTarget;
unsigned int Botnet::crossInfectState;
unsigned int Botnet::crossInfectSendAttempts;
bool Botnet::crossInfectPendingMessageStarted;
uint16_t Botnet::crossInfectPendingMessage[CI_MAX_MESSAGE_SIZE];
unsigned int Botnet::crossInfectPendingMessageParts[CI_MAX_MESSAGE_SIZE];
bool Botnet::crossInfectPendingMessageComplete;
bool Botnet::crossInfectRetryQueued;
bool Botnet::crossInfectMessageAcked;
bool Botnet::crossInfectTimedOut;
bool Botnet::crossInfectCompleted;
bool Botnet::crossInfectTooSoon;
bool Botnet::crossInfectNewMutationFound;
bool Botnet::crossInfectNewVariantFound;
unsigned short Botnet::crossInfectNewMutation;
unsigned short Botnet::crossInfectNewVariant;

void (*resetFunc)(void) = 0;

decode_results results;

/** Initialize all values and handlers used by the botnet
 *
 * This fires once on badge startup and is responsible for enabling
 * the IR sender and receiver, configuring the wireless mesh, and
 * enabling the botnet scheduler
 * 
 */
void Botnet::initBotnet()
{
    IrSender.begin(25);
    IrReceiver.begin(26);
    //IrReceiver.enableIRIn();
 
    mesh.init(MESH_SSID, MESH_PASSWORD, &meshScheduler, MESH_PORT);
    
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
    mesh.onNodeDelayReceived(&delayReceivedCallback);

    botnetScheduler.startNow();
    botnetScheduler.addTask(taskAttemptMutation);
    botnetScheduler.addTask(sendCrossInfectMessageTask);
    botnetScheduler.addTask(timeoutCrossInfectTask);
    botnetScheduler.addTask(incrementIterationsTask);
    botnetScheduler.addTask(saveIterationsTask);

    Botnet::crossInfectTarget = 0;
    Botnet::crossInfectSendAttempts = 0;
    Botnet::crossInfectState = 0;
    Botnet::crossInfectPendingMessageComplete = false;
    Botnet::crossInfectPendingMessageStarted = false;
    Botnet::crossInfectMessageAcked = false;
    Botnet::crossInfectCompleted = false;
    Botnet::crossInfectTimedOut = false;
    Botnet::crossInfectTooSoon = false;

    Botnet::initPreferences();

    taskAttemptMutation.enable();
    incrementIterationsTask.enable();
    saveIterationsTask.enable();
}

/** Initialize and retrieve all stored values from the filesystem
 *
 * This fires once on badge startup. It retrieves all saved key/value
 * pairs, initializing them with default values if not present.
 * 
 * Uncomment the preferences.clear() line to reset the stored values
 * on boot.
 * 
 */
void Botnet::initPreferences()
{
    preferences.begin("wgctf", false);

    // UNCOMMENT TO CLEAR PREFERENCES
    // preferences.clear();

    Botnet::bootCount = preferences.getUInt("boot", 0);

    Botnet::bootCount += 1;
    //Serial.println("\n\n");
    //Serial.println(Botnet::bootCount);

    if (bootCount <= 2) {
        preferences.clear();
        preferences.putUInt("boot", Botnet::bootCount);
    }
    else
    {
        preferences.putUInt("boot", Botnet::bootCount);
    }

    // Get badge info from preferences, initialize if not set
    botnetIterations = preferences.getUInt("iterations", 0);
    badgeID = preferences.getUInt("badge_id", mesh.getNodeId());
    //badgeType = preferences.getUInt("badge_type", BADGE_TYPE); // Change in Botnet.h
    badgeType = BADGE_TYPE;
    botName = preferences.getString("bot_name", String(mesh.getNodeId(), HEX));
    btPassword = preferences.getString("bt_password", Botnet::generateBTPassword());
    numBadgesEncountered = preferences.getUInt("num_badges", 0);
    numInfectedBadges = preferences.getUInt("num_infected", 0);

    DEBUG_PRINT("NUM BADGES: ");
    DEBUG_PRINTLN(numBadgesEncountered);

    if (badgeType == BADGE_TYPE_WG)
    {
        mutation = 0xFFFFFFFF;
        variant = 0xFFFFFFFF;
        mouthComplete = true;
        baseBodyComplete = true;
        badgeEyesComplete = true;
        baseEyesComplete = true;
        fullEyesComplete = true;
        fullBodyAndBaseEyesComplete = true;
        allBlingComplete = true;
    }
    else
    {
        mutation = preferences.getULong("mutation", 0);
        variant = preferences.getULong("variant", 0);
        mouthComplete = preferences.getBool("mouth_complete", false);
        baseBodyComplete = preferences.getBool("base_body", false);
        fullBodyComplete = preferences.getBool("full_body", false);
        badgeEyesComplete = preferences.getBool("badge_eyes", false);
        baseEyesComplete = preferences.getBool("base_eyes", false);
        fullEyesComplete = preferences.getBool("full_eyes", false);
        fullBodyAndBaseEyesComplete = preferences.getBool("full_body_eyes", false);
        allBlingComplete = preferences.getBool("all_bling", false);
    }

    // Save values back
    preferences.putUInt("iterations", botnetIterations);

    preferences.putString("bot_name", Botnet::botName);
    preferences.putUInt("badge_id", Botnet::badgeID);
    preferences.putUInt("badge_type", Botnet::badgeType);
    preferences.putUInt("num_badges", numBadgesEncountered);
    preferences.putUInt("num_infected", numInfectedBadges);

    preferences.putULong("mutation", Botnet::mutation);
    preferences.putULong("variant", Botnet::variant);
    preferences.putBool("mouth_complete", mouthComplete);
    preferences.putBool("base_body", baseBodyComplete);
    preferences.putBool("full_body", fullBodyComplete);
    preferences.putBool("badge_eyes", badgeEyesComplete);
    preferences.putBool("base_eyes", baseEyesComplete);
    preferences.putBool("full_eyes", fullEyesComplete);
    preferences.putBool("full_body_eyes", fullBodyAndBaseEyesComplete);
    preferences.putBool("all_bling", allBlingComplete);
    preferences.putString("bt_password", Botnet::btPassword);
}

/** Generate a bluetooth password
 *  
 * Initialize the password if one has not already been been created
 * 
 */
String Botnet::generateBTPassword()
{
    String pwd;

    for(int i = 0; i < 5; i++){
      int r = random(0, strlen(possibleValues));
      pwd += possibleValues[r];
    }

    return pwd;
}

/** Get the BlueTooth password
 *  
 * Get the generated bluetooth password 
 * 
 */
String Botnet::getBTPassword()
{
    return Botnet::btPassword;
}

/** Increment an iteration counter used for tracking time
 *
 * This fires once every second using a scheduled task to increment a
 * counter. The counter is used for tracking elapsed time since last
 * infecting any given badge to prevent spamming.
 * 
 */
void Botnet::incrementBotnetIterations()
{
    botnetIterations += 1;
}

/** Save the current iteration counter to the filesystem
 *
 * This fires once every 10 seconds using a scheduled task to persist
 * the current iteration counter to the filesystem. This prevents
 * the user from restarting the badge to get around the counter.
 * 
 * The delayed save is to prevent excessive IOPS
 * 
 */
void Botnet::saveBotnetIterations()
{
    preferences.putUInt("iterations", Botnet::botnetIterations);
}

/** Get the current iteration counter
 *
 * This returns the current number of iterations (seconds) since
 * the badge first powered on.
 * 
 */
unsigned int Botnet::getBotnetIterations()
{
    return botnetIterations;
}

/** Default the badge saved data
 *
 * This resets all of the saved data on the badge and then calls
 * the built in reboot function.
 * 
 */
void Botnet::resetBadge()
{
    preferences.clear();
    preferences.putUInt("boot",1); //Set the boot count to 1 to allow initBotnet to continue
    resetFunc();
}

/** Handle a new connection to the wireless mesh
 *
 * This fires when a new badge connects to the wireless mesh network.
 * It calls the sendGreeting() botnet function, to broadcast this
 * badge's information into the mesh.
 * 
 */
void Botnet::newConnectionCallback(uint32_t nodeId)
{
    DEBUG_PRINTLN("New Connection, nodeId = " + String(nodeId));
    DEBUG_PRINTLN("New Connection, " + String(mesh.subConnectionJson(true).c_str()));

    Botnet::sendGreeting();
    MenuSystem::refreshMenu();
}

/** Handle a badge leaving the mesh
 *
 * This fires when a badge disconnects or times out from the mesh
 * network. Its primary job is for debug logging purposes.
 * 
 */
void Botnet::changedConnectionCallback()
{
    DEBUG_PRINTLN("Changed connections");
    nodes = mesh.getNodeList();

    DEBUG_PRINTLN("Num nodes: " + String(nodes.size()));
    DEBUG_PRINT("Connection list: ");

    SimpleList<uint32_t>::iterator node = nodes.begin();
    while (node != nodes.end())
    {
        DEBUG_PRINT(" " + String(*node));
        node++;
    }
    DEBUG_PRINTLN("");
    MenuSystem::refreshMenu();
}

/** Handle a messages received from the mesh network
 *
 * This fires when a badge receives a message (direct or broadcast)
 * from another badge in the wireless mesh. It parses the message
 * contents and persists the sender's badge information to the
 * filesystem.
 * 
 */
void Botnet::receivedCallback(uint32_t from, String &msg)
{
    DEBUG_PRINTLN("Received from " + String(from) + "msg=" + String(msg.c_str()));

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, msg);
    const char *type = doc["type"];
    if (strcmp(type, "greet") == 0)
    {
        const char *name = doc["name"];
        uint32_t bType = doc["badge"];
        ulong bMutation = doc["mutation"];


        DEBUG_PRINTLN("Greeting received from " + String(from) + "msg=" + String(msg.c_str()));
        DEBUG_PRINTLN("Associating " + String(name) + " with " + String(from));

        String key = "b_i_" + String(from, HEX);  // Badge_Id
        String key2 = "b_t_" + String(from, HEX); // Badge_Type
        String key3 = "b_m_" + String(from, HEX); // Botnet_Mutation

        String badgeKnown = preferences.getString(key.c_str(), "--UNKNOWN--");
        if (badgeKnown == "--UNKNOWN--")
        {
            DEBUG_PRINTLN("New badge! incrementing counter");
            // This is the first time we've seen this badge, increment the encounter counter
            Botnet::numBadgesEncountered++;
            preferences.putUInt("num_badges", Botnet::numBadgesEncountered);
            DEBUG_PRINT("NUM BADGES: ");
            DEBUG_PRINTLN(numBadgesEncountered);
        }

        preferences.putString(key.c_str(), String(name));
        preferences.putUInt(key2.c_str(), bType);
        preferences.putULong(key3.c_str(), bMutation);      
    }
    else
    {
        DEBUG_PRINTLN("Unknown message type: " + String(type));
    }
}

/** Handle a mesh time adjustment
 *
 * This fires when the badge readjusts its mesh "clock" time based
 * off the times of other badges in the mesh. It is primarily for
 * debug logging purposes.
 * 
 */
void Botnet::nodeTimeAdjustedCallback(int32_t offset)
{
    DEBUG_PRINTLN("Adjusted time " + String(mesh.getNodeTime()) + ". Offset = " + String(offset));
}

/** Handle a mesh delay determination
 *
 * This fires when the badge calculates its delay to other badges
 * in the mesh. It is primarily for debug logging purposes.
 * 
 */
void Botnet::delayReceivedCallback(uint32_t from, int32_t delay)
{
    DEBUG_PRINTLN("Delay to node " + String(from) + " is " + String(delay) + " ms");
}

/** Send badge info to all badges in the mesh
 *
 * This fires when the the mesh detects a new badge joined. It
 * sends a broadcast message to all badges in the mesh with
 * the basic badge information as serializsed JSON.
 * 
 */
void Botnet::sendGreeting()
{
    DynamicJsonDocument jsonBuffer(1024);
    JsonObject msg = jsonBuffer.to<JsonObject>();
    msg["type"] = "greet";
    msg["name"] = botName;
    msg["badge"] = badgeType;
    msg["mutation"] = mutation;
    String str;
    serializeJson(msg, str);
    mesh.sendBroadcast(str);
    return;
}

/** Execute the botnet system scheduler
 *
 * This fires every loop iteration from main.cpp. It is responsible
 * for executing both the wireless mesh and the botnet schedulers.
 * 
 * If the badge is attempting to cross infect another badge, it
 * calls required functions depending on the cross infect state.
 * 
 */
void Botnet::executeSchedule()
{
    mesh.update();
    botnetScheduler.execute();

    if (Botnet::crossInfectState != CISTATE_OFF)
    {
        switch (Botnet::crossInfectState)
        {
            case CISTATE_LISTEN:
                Botnet::listenCrossInfect();
                break;
            case CISTATE_SEND:
                Botnet::sendCrossInfectMessage();
                break;
            case CISTATE_ACK_RECEIVED_NO_MESSAGE:
                Botnet::listenCrossInfect();
                break;
            case CISTATE_MESSAGE_RECEIVED_NO_ACK:
                Botnet::listenCrossInfect();
                break;
            case CISTATE_MESSAGE_AND_ACK_RECEIVED:
                Botnet::processCompleteMessage();
                break;
            case CISTATE_COMPLETED:
                break;
            case CISTATE_SEARCH_TIMEOUT:
                Botnet::crossInfectTimedOut = true;
                break;
        }
    }

}

/** Get the number of badges connected to the mesh network
* 
 * Returns the number of badges currently connected to
 * the wireless mesh network.
 * 
 */
uint32_t Botnet::getNumNodes()
{
    return nodes.size();
}

/** Get the badges connected to the mesh network
 *
 * Returns the node IDs of the badges connected to the
 * wireless mesh network†.
 * 
 */
SimpleList<uint32_t> Botnet::getNodeList()
{
    return nodes;
}

/** Get the number of badges this badge has encountered
 *
 * This returns the total number of badges that have joined the
 * same wireless mesh network as this badge.
 * 
 */
unsigned int Botnet::getNumBadgesEncountered()
{
    return Botnet::numBadgesEncountered;
}

/** Get the number of badges this badge has cross-infected
 *
 * This returns the total number of badges that this badge has
 * cross-infected with via infrared.
 * 
 */
unsigned int Botnet::getNumBadgesInfected()
{
    return Botnet::numInfectedBadges;
}

/** Get the configured name for this badge
 *
 * Returns the user-configured name for this badge or its
 * badgeID as default.
 * 
 */
String Botnet::getThisBotName() { return botName; }

/** Get this badge's ID
 *
 * Get the badge ID for this badge.
 * 
 */
unsigned int Botnet::getBadgeID() { return badgeID; }

/** Get the name of a specific badge by its badge ID
 *
 * This returns the name of another badge based off its
 * badge ID.
 * 
 */
String Botnet::getBotName(unsigned int id)
{
    String key = "b_i_" + String(id, HEX);
    return preferences.getString(key.c_str(), String(id, HEX));
}

/** Get this badge's type
 *
 * Get the badge type for this badge.
 * 
 * BADGE_TYPE_LION == Lion badge
 * BADGE_TYPE_PANDA == Panda badge
 * BADGE_TYPE_WG == A WatchGuard employee's badge
 * 
 */
unsigned int Botnet::getBadgeType() { return badgeType; }


/** Set the name of this badge
 *
 * Save the user-configured name for this badge and send a
 * new greeting into the mesh to inform other badges.
 * 
 */
void Botnet::setBotName(String input)
{
    botName = input;
    preferences.putString("bot_name", botName);
    sendGreeting();
}

/** Check a user-entered code
 *
 * --- DEPRECATED ---
 * 
 * This accepts a user-entered "secret" and compares it against a
 * list. If the code matches an expected value, triggers another
 * function.
 * 
 */
bool Botnet::checkCode(String code)
{

    char *p;
    const char *c = code.c_str();
    unsigned long ul;
    ul = strtoul(c, &p, 16);

    if ((ul ^ badgeID) == 320017171)
    { // 13131313 in hex
        return true;
    }
    return false;
}

/** Check if red eyes have been unlocked
 *
 * Returns true if the red eye mutation has been unlocked
 * 
 */
bool Botnet::redEyesUnlocked()
{
    if (bitRead(variant,0) == 1 && bitRead(variant,1) == 1 && bitRead(variant,2) == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/** Check if blue eyes have been unlocked
 *
 * Returns true if the blue eye mutation has been unlocked
 * 
 */
bool Botnet::blueEyesUnlocked()
{
    if (bitRead(variant,3) == 1 && bitRead(variant,4) == 1 && bitRead(variant,5) == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/** Check if rainbow eyes have been unlocked
 *
 * Returns true if the rainbow eye mutation has been unlocked
 * 
 */
bool Botnet::rainbowEyesUnlocked()
{
    if (bitRead(variant, 28) == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/** Check if blinking body mutation has been unlocked
 *
 * Returns true if the blinking body mutation has been unlocked
 * 
 */
bool Botnet::blinkingBodyUnlocked()
{
    if (bitRead(variant,9) == 1 && bitRead(variant,10) == 1 && bitRead(variant,11) == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}


/** Check if racing body mutation has been unlocked
 *
 * Returns true if the racing body mutation has been unlocked
 * 
 */
bool Botnet::racingBodyUnlocked()
{
    if (bitRead(variant,12) == 1 && bitRead(variant,13) == 1 && bitRead(variant,14) == 1 && bitRead(variant,15) == 1 && bitRead(variant,16) == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/** Check if pulsing body mutation has been unlocked
 *
 * Returns true if the pulsing body mutation has been unlocked
 * 
 */
bool Botnet::pulsingBodyUnlocked()
{
    if (bitRead(variant,17) == 1 && bitRead(variant,18) == 1 && bitRead(variant,19) == 1 && bitRead(variant,20) == 1 && bitRead(variant,21) == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/** Check if dazzling body mutation has been unlocked
 *
 * Returns true if the dazzling body mutation has been unlocked
 * 
 */
bool Botnet::dazzlingBodyUnlocked()
{
    if (bitRead(variant,22) == 1 && bitRead(variant,23) == 1 && bitRead(variant,24) == 1 && bitRead(variant,25) == 1 && bitRead(variant,26) == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/** Get the current mutation count
 *
 * Returns the current number of mutations and variants on this badge
 * 
 */
unsigned int Botnet::getMutationCount()
{
    unsigned int count = 0;
    for (int i = 0; i < 32; i++)
    {
        count += bitRead(Botnet::mutation, i);
    }
    for (int i = 0; i < 32; i++)
    {
        count += bitRead(Botnet::variant, i);
    }

    return count;
}

/** Get all current mutations
 *
 * Returns the currently unlocked mutations as an integer
 * 
 */
unsigned long Botnet::getMutations()
{
    return Botnet::mutation;
}

/** Get all current variants
 *
 * Returns the currently unlocked variants as an integer
 * 
 */
unsigned long Botnet::getVariants()
{
    return Botnet::variant;
}

/** Get a specific mutation
 *
 * Returns true if a specific mutation has been unlocked
 * 
 */
bool Botnet::getMutation(unsigned int loc)
{
    return bitRead(mutation, loc);
}

/** Get a specific variant
 *
 * Returns true if a specific variant has been unlocked
 * 
 */
bool Botnet::getVariant(unsigned int loc)
{
    return bitRead(variant, loc);
}

/** Save a mutation or variant
 *
 * Enable and save a specific mutation or variant
 * 
 */
void Botnet::saveMutation(unsigned int type, unsigned int loc)
{
    if (type == MUTATION_TYPE_M)
    {
        bitSet(mutation, loc);
        preferences.putULong("mutation", mutation);
    }
    else if (type == MUTATION_TYPE_V)
    {
        bitSet(variant, loc);
        preferences.putULong("variant", variant);
    }
}

/** Persist the selected eye color
 *
 * Save the currently selected eye color and persist it to
 * storage via preferences
 * 
 */
void Botnet::saveEyeColor(unsigned int color)
{
    preferences.putUInt("eye_color", color);
}

/** Persist the selected bling mode
 *
 * Save the currently selected bling mode and persist it to
 * storage via preferences
 * 
 */
void Botnet::saveBlingMode(unsigned short mode)
{
    preferences.putUShort("bling_mode", mode);
}

/** Get the selected eye color
 *
 * Get the currently saved eye color from storage
 * 
 */
unsigned int Botnet::getSavedEyeColor()
{
    return preferences.getUInt("eye_color", EYES_WHITE);
}

/** Get the selected bling mode
 *
 * Get the currently saved bling mode from storage
 * 
 */
unsigned short Botnet::getSavedBlingMode()
{
    return preferences.getUShort("bling_mode", 0);
}

/** Trigger and save a mutation
 *
 * Randomly select a currently unlocked but not earned mutation
 * and enable it.
 * 
 */
int Botnet::mutate()
{
    DEBUG_PRINTLN("Mutating!");
    if (Botnet::baseBodyComplete == false)
    {
        DEBUG_PRINTLN("Mutating in base body range");
        // Unlock the base body LEDs first
        unsigned int unset[13];
        unsigned int ctr = 0;

        for (int i = 0; i < 13; i++)
        {
            if (bitRead(mutation, i) == 0)
            {
                unset[ctr] = i;
                ctr++;
            }
        }

        int toSet = unset[random((ctr))];
        Botnet::saveMutation(MUTATION_TYPE_M, toSet);

        if (ctr <= 1)
        {
            DEBUG_PRINTLN("Done mutating base body!");
            Botnet::baseBodyComplete = true;
            preferences.putBool("base_body", true);
        }
        MenuSystem::refreshMenu();
        return toSet;
    }    
    else if (Botnet::fullBodyComplete == false)
    {
        DEBUG_PRINTLN("Mutating in extended body range");
        // Next unlock the stitches and blinking mouth
        unsigned int unset[27];
        unsigned int ctr = 0;

        for (int i = 0; i < 27; i++)
        {
            if (bitRead(mutation, i) == 0)
            {
                unset[ctr] = i;
                ctr++;
            }
        }
        
        int toSet = unset[random((ctr))];
        Botnet::saveMutation(MUTATION_TYPE_M, toSet);

        DEBUG_PRINT("Mutating position: ");
        DEBUG_PRINTLN(toSet);

        if (ctr <= 1)
        {
            DEBUG_PRINTLN("Done mutating full body!");
            Botnet::fullBodyComplete = true;
            preferences.putBool("full_body", true);
        }
        MenuSystem::refreshMenu();
        return -1;
    }
    else if (Botnet::badgeEyesComplete == false)
    {
        DEBUG_PRINTLN("Mutating eyes");
        // Unlock the badge-specific eye color
        unsigned int unset[3];
        unsigned int ctr = 0;
        unsigned int offset = 0;

        if (badgeType == BADGE_TYPE_PANDA)
        {
            offset = 3;
        }

        for (int i = 0; i < 3; i++)
        {
            if (bitRead(variant, i + offset) == 0)
            {
                unset[ctr] = i + offset;
                ctr++;
            }
        }
        
        int toSet = unset[random((ctr))];
        Botnet::saveMutation(MUTATION_TYPE_V, toSet);

        DEBUG_PRINT("Mutating position: ");
        DEBUG_PRINTLN(toSet);

        if (ctr <= 1)
        {
            DEBUG_PRINTLN("Done mutating badge eyes!");
            Botnet::badgeEyesComplete = true;
            preferences.putBool("badge_eyes", true);

            if (badgeType == BADGE_TYPE_LION)
            {
                BlingSystem::unlockNewEyeColor(EYES_RED);
            }
            else if (badgeType == BADGE_TYPE_PANDA)
            {
                BlingSystem::unlockNewEyeColor(EYES_BLUE);
            }

            if (bitRead(variant,0) == 1 && bitRead(variant, 1) == 1 && bitRead(variant, 2) == 1
                && bitRead(variant, 3) == 1 && bitRead(variant, 4) == 1 && bitRead(variant, 5) == 1)
            {
                Botnet::baseEyesComplete = true;
                Botnet::fullBodyAndBaseEyesComplete = true;
                preferences.putBool("base_eyes", true);
                preferences.putBool("full_body_eyes", true);
            }
        }
        MenuSystem::refreshMenu();
        return -1;
    }
    else if (Botnet::fullBodyAndBaseEyesComplete && Botnet::allBlingComplete == false)
    {
        DEBUG_PRINTLN("Mutating additional bling patterns");
        // User has completed base body, unlock extra stuff
        unsigned int unset[27];
        unsigned int ctr = 0;

        for (int i = 0; i < 27; i++)
        {
            if (bitRead(variant, i) == 0)
            {
                unset[ctr] = i;
                ctr++;
            }
        }

        int toSet = unset[random(ctr)];
        Botnet::saveMutation(MUTATION_TYPE_V, toSet);

        DEBUG_PRINT("Mutating position: ");
        DEBUG_PRINTLN(toSet);

        if (ctr <= 1)
        {
            DEBUG_PRINTLN("All bling complete!");
            Botnet::allBlingComplete = true;
            preferences.putBool("all_bling", true);
        }
        MenuSystem::refreshMenu();
        return -1;
    }
    else
    {
        DEBUG_PRINTLN("No mutations available");
        return -1;
    }

}


/** Trigger a chance at a mutation
 *
 * Randomly check to see if a mutation has been earned based off a base
 * mutation rate and the number of badges this badge has encountered.
 * 
 */
void Botnet::attemptMutation()
{
    // TODO: RETURN TO CORRECT VALUE
    // Base rate should be about one mutation every hour plus an additional
    // mutation for every 100 badges met
    //if (random(BASE_MUTATION_RATE) <=1 )
    if (random(BASE_MUTATION_RATE * 100) <= 100 + numBadgesEncountered)
    {
        Botnet::mutate();
    }
}


/** Start the cross infection process
 *
 * Initialize the cross infection process and set a timeout.
 * 
 * This process attempts to communicate with a nearby badge via
 * infrared and share a known mutation.
 * 
 */
void Botnet::startCrossInfect()
{
    DEBUG_PRINTLN("Starting crossInfect");
    Botnet::crossInfectPendingMessageStarted = false;
    Botnet::crossInfectPendingMessageComplete = false;
    Botnet::crossInfectMessageAcked = false;
    Botnet::crossInfectTimedOut = false;
    Botnet::crossInfectCompleted = false;
    Botnet::crossInfectNewMutationFound = false;
    Botnet::crossInfectNewVariantFound = false;
    Botnet::crossInfectTooSoon = false;
    Botnet::crossInfectTarget = 0;
    Botnet::crossInfectSendAttempts = 0;

    for (int i = 0; i < CI_MAX_MESSAGE_SIZE; i++)
    {
        Botnet::crossInfectPendingMessage[i] = 0;
        Botnet::crossInfectPendingMessageParts[i] = 0;
    }
    Botnet::crossInfectState = CISTATE_SEND;

    timeoutCrossInfectTask.enableDelayed(1000 * 25); //Set 15 second timeout for cross infect attempt
    MenuSystem::setReceivedInfectMessageParts(Botnet::crossInfectPendingMessageParts, Botnet::crossInfectMessageAcked);

}

/** Stop the cross infection process
 *
 * Halt the cross infection process and any pending
 * infrared message tasks.
 * 
 */
void Botnet::stopCrossInfect()
{
    DEBUG_PRINTLN("Stopping cross infect attempt");
    Botnet::crossInfectState = CISTATE_OFF;
    sendCrossInfectMessageTask.disable();
}

/** Timeout and stop the cross infection process
 *
 * Stop the cross infection process because of an overall timeout.
 * 
 */
void Botnet::timeoutCrossInfect()
{
    DEBUG_PRINTLN("Cross infect timed out");
    sendCrossInfectMessageTask.disable();
    Botnet::crossInfectState = CISTATE_SEARCH_TIMEOUT;

    MenuSystem::setInfectionStatus(BOT_INFECT_TIMED_OUT);
}

/** Send a cross-infect acknowledgement via infrared
 *
 * Send an acknowledgement message via infrared to the badge
 * this badge is communicating with.
 * 
 */
void Botnet::sendCrossInfectAck()
{
    DEBUG_PRINTLN("Sending cross infect message ACK");
    uint16_t msgBadgeId = Botnet::crossInfectPendingMessage[1];
    IrSender.sendNEC(msgBadgeId, 0x08, 2);
    delay(50);
}

/** Send a cross-infect message via infrared
 *
 * Build and send a cross-infect message via infrared, two bytes
 * at a time, repeating 2 extra times for each message part.
 * 
 * This message includes the badge'es ID, current mutations
 * and current variants. 
 * 
 */
void Botnet::sendCrossInfectMessage()
{
    DEBUG_PRINTLN("Sending cross infect greet");

    //IrReceiver.disableIRIn();
    
    split32 badgeIdSplit;
    badgeIdSplit.fullHex = Botnet::badgeID;
    split32 mutationSplit;
    mutationSplit.fullHex = Botnet::mutation;
    split32 variantSplit;
    variantSplit.fullHex = Botnet::variant;

    if (Botnet::pendingMessageComplete())
    {
        Botnet::sendCrossInfectAck();
    }
    
    IrSender.sendNEC(badgeIdSplit.splitHex[0], 0x01, 2);
    delay(50);
    IrSender.sendNEC(badgeIdSplit.splitHex[1], 0x02, 2);
    delay(50);
    IrSender.sendNEC(mutationSplit.splitHex[0], 0x03, 2);
    delay(50);
    IrSender.sendNEC(mutationSplit.splitHex[1], 0x04, 2);
    delay(50);
    IrSender.sendNEC(variantSplit.splitHex[0], 0x05, 2);
    delay(50);
    IrSender.sendNEC(variantSplit.splitHex[1], 0x06, 2);
    delay(50);
    IrSender.sendNEC(badgeType, 0x07, 1);
    delay(50);

    //IrReceiver.enableIRIn();
    

    Botnet::crossInfectSendAttempts++;
    Botnet::crossInfectRetryQueued = false;

    Botnet::crossInfectState = CISTATE_LISTEN;
}

/** Retry sending a cross-infect message
 *
 * Retry sending a cross-infect message because the
 * previous message has not been acknowledged.
 * 
 */
void Botnet::crossInfectQueueRetry()
{
    if (Botnet::crossInfectMessageAcked == false)
    {
        if (Botnet::crossInfectRetryQueued == false)
        {
            DEBUG_PRINTLN("Queueing new greeting");
            Botnet::crossInfectRetryQueued = true;
            unsigned int delay = random(1000) + random(5000);
            sendCrossInfectMessageTask.restartDelayed(delay);
        }
    }
    else {
        if (Botnet::pendingMessageComplete())
        {
            Botnet::crossInfectState = CISTATE_MESSAGE_AND_ACK_RECEIVED;
        }
        else
        {
            Botnet::crossInfectState = CISTATE_ACK_RECEIVED_NO_MESSAGE;
        }
    }
}

/**
 * Transition state to send a new message
 * 
 */
void Botnet::sendQueuedMessage()
{
    Botnet::crossInfectState = CISTATE_SEND;
}

/** Check if full cross-infect message has been received
 *
 * Returns true if a full cross-infect message has been received.
 * 
 */
bool Botnet::pendingMessageComplete()
{
    bool isComplete = true;
    for (int i = 0; i < CI_MAX_MESSAGE_SIZE; i++)
    {
        if (Botnet::crossInfectPendingMessageParts[i] != 1)
        {
            isComplete = false;
        }
    }

    return isComplete;
}

/** Receive and handle cross-infect messages via infrared
 *
 * Listen for messages via infrared and if they match cross-infect
 * message format expectations, process and save the parts.
 * 
 */
void Botnet::listenCrossInfect()
{
    if (IrReceiver.decode())
    {
        if (IrReceiver.decodedIRData.protocol == NEC)
        {
            DEBUG_PRINTLN("Received potential message via IR");
            if (IrReceiver.decodedIRData.command < CI_MAX_MESSAGE_SIZE + 1)
            {
                DEBUG_PRINTLN("Received a valid message");
                DEBUG_PRINT(IrReceiver.decodedIRData.command + " - ");
                DEBUG_PRINTLN_HEX(IrReceiver.decodedIRData.address);
                Botnet::crossInfectPendingMessageStarted = true;
                Botnet::crossInfectPendingMessage[IrReceiver.decodedIRData.command - 1] = IrReceiver.decodedIRData.address;
                Botnet::crossInfectPendingMessageParts[IrReceiver.decodedIRData.command - 1] = 1;

                MenuSystem::setReceivedInfectMessageParts(Botnet::crossInfectPendingMessageParts, Botnet::crossInfectMessageAcked);

                if (Botnet::pendingMessageComplete())
                {
                    DEBUG_PRINTLN("Received a full message");
                    // Queue up an acknowledgement message

                    if (Botnet::crossInfectMessageAcked)
                    {
                        DEBUG_PRINTLN("Full message received and our message acked");
                        // Both message and ack completed, move to complete processing
                        Botnet::crossInfectState = CISTATE_MESSAGE_AND_ACK_RECEIVED;
                    }
                    else
                    {
                        DEBUG_PRINTLN("Still waiting ACK for our message");
                        // Still waiting for our message to be acknowledged
                        Botnet::crossInfectState = CISTATE_MESSAGE_RECEIVED_NO_ACK;
                    }
                }
            }
            else if (IrReceiver.decodedIRData.command == 0x08)
            {
                DEBUG_PRINTLN("Received potential ACK message");
                split32 badgeIDSplit;
                badgeIDSplit.fullHex = badgeID;

                if (IrReceiver.decodedIRData.address == badgeIDSplit.splitHex[1])
                {
                    DEBUG_PRINTLN("Received an acknowledgement for our message");
                    Botnet::crossInfectMessageAcked = true;
                    sendCrossInfectMessageTask.disable();

                    if (Botnet::pendingMessageComplete())
                    {
                        // Both message and ack completed, move to complete processing
                        Botnet::crossInfectState = CISTATE_MESSAGE_AND_ACK_RECEIVED;
                    }
                    else
                    {
                        // Still waiting for the complete message
                        Botnet::crossInfectState = CISTATE_MESSAGE_RECEIVED_NO_ACK;
                    }
                }
                else
                {
                    DEBUG_PRINTLN("Received an acknowledgement for someone else's message");
                    Botnet::crossInfectQueueRetry();
                }
            }
            else
            {
                DEBUG_PRINTLN("IR message has the wrong command");
            }
        }
        else {
            Botnet::crossInfectQueueRetry();
        }
        IrReceiver.resume();
    }
    else {
        Botnet::crossInfectQueueRetry();
    }
}

/** Process the completed cross-infect message
 *
 * Parse the cross-infect message parts and check for any potential
 * mutations or variants to unlock.
 * 
 */
void Botnet::processCompleteMessage()
{
    DEBUG_PRINTLN("Processing completed message");
    split32 senderMutationSplit;
    split32 senderVariantSplit;
    split32 senderBadgeIdSplit;

    senderMutationSplit.splitHex[0] = Botnet::crossInfectPendingMessage[2];
    senderMutationSplit.splitHex[1] = Botnet::crossInfectPendingMessage[3];
    senderVariantSplit.splitHex[0] = Botnet::crossInfectPendingMessage[4];
    senderVariantSplit.splitHex[1] = Botnet::crossInfectPendingMessage[5];
    senderBadgeIdSplit.splitHex[0] = Botnet::crossInfectPendingMessage[0];
    senderBadgeIdSplit.splitHex[1] = Botnet::crossInfectPendingMessage[1];
    
    unsigned long senderMutation = senderMutationSplit.fullHex;
    unsigned long senderVariant = senderVariantSplit.fullHex;
    unsigned int senderBadgeType = Botnet::crossInfectPendingMessage[6];
    unsigned int senderBadgeId = senderBadgeIdSplit.fullHex;

    String key = "b_x_" + String(senderBadgeId,HEX);

    unsigned int lastInfected = preferences.getUInt(key.c_str(), 0);

    if (lastInfected == 0 || (botnetIterations - lastInfected) > INFECTION_TIMEOUT)
    {
        // Never cross infected with this badge or its been longer than the timeout
        preferences.putUInt(key.c_str(), botnetIterations);
        if (senderBadgeType == 2 && getVariant(27) == false)
        {
            // Sender is a WG employee and this badge doesn't have green eyes yet
            Botnet::saveMutation(MUTATION_TYPE_V, 27);

            if (Botnet::baseEyesComplete)
            {
                // Badge has a full set of eyes, unlock rainbow eyes
                Botnet::saveMutation(MUTATION_TYPE_V, 28);
                fullEyesComplete = true;
                preferences.putBool("full_eyes", true);
            }

            Botnet::crossInfectNewMutationFound = false;
            Botnet::crossInfectNewVariantFound = true;
            Botnet::crossInfectNewVariant = 27;
        }
        else if (Botnet::baseBodyComplete == false)
        {
            unsigned int unset[13];
            unsigned int ctr = 0;
            unsigned int senderSet[13];
            unsigned int senderCtr = 0;

            for (int i = 0; i < 13; i++)
            {
                if (bitRead(mutation, i) == 0)
                {
                    unset[ctr] = i;
                    ctr++;
                }
            }

            for (int i = 0; i < ctr; i++)
            {
                if (bitRead(senderMutation, unset[i]) == 1)
                {
                    senderSet[senderCtr] = unset[i];
                    senderCtr++;
                }
            }

            if (senderCtr == 0)
            {
                // Sender did not have any unknown mutations
                Botnet::crossInfectNewMutationFound = false;
                Botnet::crossInfectNewVariantFound = false;

                if (random(4) == 1)
                {
                    // They won the lottery, give them a mutation anyway
                    int m = Botnet::mutate();
                    if (m >= 0)
                    {
                        Botnet::crossInfectNewMutationFound = true;
                        Botnet::crossInfectNewMutation = m;
                    }
                }
            }
            else
            {

                int toSet = senderSet[random((senderCtr))];
                Botnet::saveMutation(MUTATION_TYPE_M, toSet);
                
                DEBUG_PRINTLN("Learned new mutation from cross infect: " + String(toSet));

                if (ctr == 1)
                {
                    // That was the last one this badge needed for the base body
                    Botnet::baseBodyComplete = true;
                    preferences.putBool("base_body", true);
                }

                Botnet::crossInfectNewMutationFound = true;
                Botnet::crossInfectNewVariantFound = false;
                Botnet::crossInfectNewMutation = toSet;
            }
        }
        else if (Botnet::fullBodyComplete  == false)
        {
            unsigned int unset[27];
            unsigned int ctr = 0;
            unsigned int senderSet[13];
            unsigned int senderCtr = 0;

            for (int i = 0; i < 27; i++)
            {
                if (bitRead(mutation, i) == 0)
                {
                    unset[ctr] = i;
                    ctr++;
                }
            }

            for (int i = 0; i < ctr; i++)
            {
                if (bitRead(senderMutation, unset[i]) == 1)
                {
                    senderSet[senderCtr] = unset[i];
                    senderCtr++;
                }
            }

            if (senderCtr == 0)
            {
                // Sender did not have any unknown mutations
                Botnet::crossInfectNewMutationFound = false;
                Botnet::crossInfectNewVariantFound = false;
            }
            else
            {
                int toSet = senderSet[random((senderCtr))];
                Botnet::saveMutation(MUTATION_TYPE_M, toSet);

                DEBUG_PRINTLN("Learned new mutation from cross infect: " + String(toSet));

                if (ctr == 1)
                {
                    // That was the last one this badge needed for the base body
                    Botnet::fullBodyComplete = true;
                    preferences.putBool("full_body", true);
                }

                Botnet::crossInfectNewMutationFound = true;
                Botnet::crossInfectNewVariantFound = false;
                Botnet::crossInfectNewMutation = toSet;
            }

        }
        else if (Botnet::baseEyesComplete == false && senderBadgeType != BADGE_TYPE_WG)
        {
            DEBUG_PRINTLN("Need eyes from this badge!");
            unsigned int unset[3];
            unsigned int ctr = 0;
            unsigned int senderSet[3];
            unsigned int senderCtr = 0;

            unsigned int offset = 0;

            if (senderBadgeType == BADGE_TYPE_PANDA)
            {
                DEBUG_PRINTLN("Sender is a panda");
                offset = 3;
            }

            for (int i = 0; i  < 3; i++)
            {
                if (bitRead(variant, i + offset) == 0)
                {
                    unset[ctr] = i + offset;
                    ctr++;
                }
            }
            DEBUG_PRINTLN(ctr);

            for (int i = 0; i < ctr; i++)
            {
                if (bitRead(senderVariant, unset[i]) == 1)
                {
                    senderSet[senderCtr] = unset[i];
                    senderCtr++;
                }
            }

            if (senderCtr == 0)
            {
                // Sender did not have any unknown variants
                Botnet::crossInfectNewMutationFound = false;
                Botnet::crossInfectNewVariantFound = false;
            }
            else
            {
                int toSet = senderSet[random((senderCtr))];
                Botnet::saveMutation(MUTATION_TYPE_V, toSet);

                DEBUG_PRINTLN("Learned new variant from cross infect: " + String(toSet));


                if (toSet < 3)
                {
                    if (bitRead(variant, 0) == 1 && bitRead(variant, 1) == 1 && bitRead(variant, 2) == 1)
                    {
                        // Completed red eyes
                        if (badgeType == BADGE_TYPE_LION)
                        {
                            Botnet::badgeEyesComplete = true;
                            preferences.putBool("badge_eyes", true);
                        }
                        BlingSystem::unlockNewEyeColor(EYES_RED);
                    }
                }
                else if (toSet > 2 && toSet < 6)
                {
                    if (bitRead(variant, 3) == 1 && bitRead(variant, 4) == 1 && bitRead(variant, 5) == 1)
                    {
                        // Completed blue eyes
                        if (badgeType == BADGE_TYPE_PANDA)
                        {
                            Botnet::badgeEyesComplete = true;
                            preferences.putBool("badge_eyes", true);
                        }
                        BlingSystem::unlockNewEyeColor(EYES_BLUE);
                    }
                }

                if (ctr == 1)
                {
                    // That was the last one this badge needed for the base body
                    Botnet::baseEyesComplete = true;
                    Botnet::fullBodyAndBaseEyesComplete = true;
                    preferences.putBool("base_eyes", true);
                    preferences.putBool("full_body_eyes", true);

                    if (bitRead(variant, 27) == 1)
                    {
                        // Badge has a full set of eyes, unlock rainbow eyes
                        Botnet::saveMutation(MUTATION_TYPE_V, 28);
                        fullEyesComplete = true;
                        preferences.putBool("full_eyes", true);
                    }

                }
                Botnet::crossInfectNewMutationFound = false;
                Botnet::crossInfectNewVariantFound = true;
                Botnet::crossInfectNewVariant = toSet;
            }


        }
        else if (Botnet::fullBodyAndBaseEyesComplete && Botnet::allBlingComplete == false)
        {
            // Completed the base body and eyes and has not completed the extra stuff
            unsigned int unset[27];
            unsigned int ctr = 0;
            unsigned int senderSet[27];
            unsigned int senderCtr = 0;

            for (int i = 0; i < 27; i++)
            {
                if (bitRead(variant, i) == 0)
                {
                    unset[ctr] = i;
                    ctr++;
                }
            }

            for (int i = 0; i < ctr; i++)
            {
                if (bitRead(senderVariant, unset[i]) == 1)
                {
                    senderSet[senderCtr] = unset[i];
                    senderCtr++;
                }
            }

            if (senderCtr == 0)
            {
                // Sender did not have any unknown variants
                DEBUG_PRINTLN("Nothing to learn from other badge");
                Botnet::crossInfectNewMutationFound = false;
                Botnet::crossInfectNewVariantFound = false;
            }

            else
            {
                int toSet = senderSet[random((senderCtr))];
                Botnet::saveMutation(MUTATION_TYPE_V, toSet);

                DEBUG_PRINTLN("Learned new variant from cross infect: " + String(toSet));

                if (ctr == 1)
                {
                    // That was the last one this badge needed for bling
                    Botnet::allBlingComplete = true;
                    preferences.putBool("all_bling", true);
                }

                Botnet::crossInfectNewMutationFound = false;
                Botnet::crossInfectNewVariantFound = true;
                Botnet::crossInfectNewVariant = toSet;
            }
        }
        else
        {
            // Nothing left to learn!
            DEBUG_PRINTLN("Nothing left to learn!");
            Botnet::crossInfectNewMutationFound = false;
            Botnet::crossInfectNewVariantFound = false;
        }

        DEBUG_PRINTLN("Done processing received message");
        numInfectedBadges++;
        preferences.putUInt("num_infected", numInfectedBadges);
        timeoutCrossInfectTask.disable();
        Botnet::crossInfectCompleted = true;
        Botnet::crossInfectState = CISTATE_COMPLETED;
        MenuSystem::setInfectionStatus(BOT_INFECT_COMPLETE);
        //MenuSystem::refreshMenu();
    }
    else
    {
        // Too soon since last cross infection with badge
        DEBUG_PRINTLN("To soon after last infection with this badge");
        timeoutCrossInfectTask.disable();
        Botnet::crossInfectCompleted = true;
        Botnet::crossInfectState = CISTATE_COMPLETED;
        MenuSystem::setInfectionStatus(BOT_INFECT_TOO_SOON);
    }

    
}