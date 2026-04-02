// Smart Traffic Light Controller — manages two intersections with vehicle detection

// --- Pin assignments for Intersection 1 ---
#define RED1_PIN    13
#define YELLOW1_PIN 12
#define GREEN1_PIN  11
#define BUTTON1_PIN 7

// --- Pin assignments for Intersection 2 ---
#define RED2_PIN    8
#define YELLOW2_PIN 9
#define GREEN2_PIN  10
#define BUTTON2_PIN 6

// Timing values in milliseconds
#define GREEN_BASE    10000
#define GREEN_MEDIUM  12000
#define GREEN_HIGH    15000
#define YELLOW_TIME    2000
#define RED_BASE      10000
#define DEBOUNCE_MS    200

// Enum must come before structs and prototypes so the compiler knows the type
enum LightState {
  STATE_RED,
  STATE_YELLOW,
  STATE_GREEN
};

// Struct holds all data for one intersection — pins, state, timing, and counters
struct Intersection {
  int           id;
  int           redPin;
  int           yellowPin;
  int           greenPin;
  int           buttonPin;
  LightState    currentState;
  unsigned long stateStartTime;
  unsigned long greenDuration;
  unsigned long yellowDuration;
  unsigned long redDuration;
  int           vehicleCount;           // vehicles counted in the current green phase
  int           totalVehiclesProcessed; // running total since power-on
  unsigned long lastButtonPress;        // used to debounce the button without delay()
  bool          emergencyOverride;
  bool          manualMode;
};

// Struct holds system-wide statistics logged to Serial
struct TrafficStats {
  int           totalVehicles;
  unsigned long systemStartTime;
  int           manualOverrides;
  int           emergencyEvents;
  int           cyclesCompleted;
};

// Global pointers — allocated on the heap with new so dynamic memory is demonstrated
Intersection* intersection1    = nullptr;
Intersection* intersection2    = nullptr;
TrafficStats* stats            = nullptr;
unsigned long lastSerialUpdate = 0;
bool          systemActive     = true;

// Function prototypes — declared here so they can be called before their definitions
void setupPins(Intersection* inter);
void allOff(Intersection* inter);
void setLightState(Intersection* inter, LightState newState);
void updateIntersection(Intersection* inter);
void checkVehiclePresence(Intersection* inter);
void handleSerialCommands();
void printSystemStatus();
void printDetailedStats();
void printMenu();
void printManualMenu(int id);
void printState(LightState state);
void emergencyStop();
void resetSystem();

void setup() {
  Serial.begin(9600);
  Serial.println(F("Smart Traffic Light Controller"));
  Serial.println(F("Initializing..."));

  // Allocate intersection and stats objects on the heap
  intersection1 = new Intersection;
  intersection2 = new Intersection;
  stats         = new TrafficStats;

  // Intersection 1 starts on GREEN so the two intersections run opposite phases
  intersection1->id                     = 1;
  intersection1->redPin                 = RED1_PIN;
  intersection1->yellowPin              = YELLOW1_PIN;
  intersection1->greenPin               = GREEN1_PIN;
  intersection1->buttonPin              = BUTTON1_PIN;
  intersection1->currentState           = STATE_GREEN;
  intersection1->stateStartTime         = millis();
  intersection1->greenDuration          = GREEN_BASE;
  intersection1->yellowDuration         = YELLOW_TIME;
  intersection1->redDuration            = RED_BASE;
  intersection1->vehicleCount           = 0;
  intersection1->totalVehiclesProcessed = 0;
  intersection1->lastButtonPress        = 0;
  intersection1->emergencyOverride      = false;
  intersection1->manualMode             = false;

  // Intersection 2 starts on RED — opposite to intersection 1
  intersection2->id                     = 2;
  intersection2->redPin                 = RED2_PIN;
  intersection2->yellowPin              = YELLOW2_PIN;
  intersection2->greenPin               = GREEN2_PIN;
  intersection2->buttonPin              = BUTTON2_PIN;
  intersection2->currentState           = STATE_RED;
  intersection2->stateStartTime         = millis();
  intersection2->greenDuration          = GREEN_BASE;
  intersection2->yellowDuration         = YELLOW_TIME;
  intersection2->redDuration            = RED_BASE;
  intersection2->vehicleCount           = 0;
  intersection2->totalVehiclesProcessed = 0;
  intersection2->lastButtonPress        = 0;
  intersection2->emergencyOverride      = false;
  intersection2->manualMode             = false;

  // Zero out all statistics counters
  stats->totalVehicles   = 0;
  stats->systemStartTime = millis();
  stats->manualOverrides = 0;
  stats->emergencyEvents = 0;
  stats->cyclesCompleted = 0;

  setupPins(intersection1);
  setupPins(intersection2);
  setLightState(intersection1, STATE_GREEN);
  setLightState(intersection2, STATE_RED);

  Serial.println(F("System Ready!"));
  printMenu();
}

void loop() {
  if (systemActive) {
    // Run the state machine for each intersection every loop tick
    updateIntersection(intersection1);
    updateIntersection(intersection2);

    // Check if either button has been pressed to log a vehicle
    checkVehiclePresence(intersection1);
    checkVehiclePresence(intersection2);

    // Print a short status line every 2 seconds without blocking
    if (millis() - lastSerialUpdate >= 2000) {
      printSystemStatus();
      lastSerialUpdate = millis();
    }
  }

  // Serial commands are always checked, even during emergency pause
  handleSerialCommands();
}

void setupPins(Intersection* inter) {
  pinMode(inter->redPin,    OUTPUT);
  pinMode(inter->yellowPin, OUTPUT);
  pinMode(inter->greenPin,  OUTPUT);
  pinMode(inter->buttonPin, INPUT_PULLUP); // INPUT_PULLUP means no external resistor needed
}

// Turn all three LEDs off before switching — prevents two lights being on at once
void allOff(Intersection* inter) {
  digitalWrite(inter->redPin,    LOW);
  digitalWrite(inter->yellowPin, LOW);
  digitalWrite(inter->greenPin,  LOW);
}

void setLightState(Intersection* inter, LightState newState) {
  allOff(inter); // always clear first for safety

  switch (newState) {
    case STATE_RED:    digitalWrite(inter->redPin,    HIGH); break;
    case STATE_YELLOW: digitalWrite(inter->yellowPin, HIGH); break;
    case STATE_GREEN:  digitalWrite(inter->greenPin,  HIGH); break;
  }

  inter->currentState   = newState;
  inter->stateStartTime = millis();
}

// Non-blocking state machine — uses millis() so the loop never stalls
void updateIntersection(Intersection* inter) {
  if (inter->manualMode) return; // manual mode disables auto cycling

  unsigned long elapsed = millis() - inter->stateStartTime;

  switch (inter->currentState) {
    case STATE_GREEN:
      if (elapsed >= inter->greenDuration)
        setLightState(inter, STATE_YELLOW);
      break;

    case STATE_YELLOW:
      if (elapsed >= inter->yellowDuration)
        setLightState(inter, STATE_RED);
      break;

    case STATE_RED:
      if (elapsed >= inter->redDuration) {
        inter->vehicleCount = 0; // reset count for the new green cycle
        stats->cyclesCompleted++;
        setLightState(inter, STATE_GREEN);
      }
      break;
  }
}

// Each button press counts as one vehicle — only registered during green phase
void checkVehiclePresence(Intersection* inter) {
  if (inter->currentState != STATE_GREEN) return;

  if (digitalRead(inter->buttonPin) == LOW) {
    unsigned long now = millis();

    // Ignore bounces within the debounce window — no delay() needed
    if (now - inter->lastButtonPress < DEBOUNCE_MS) return;
    inter->lastButtonPress = now;

    inter->vehicleCount++;
    inter->totalVehiclesProcessed++;
    stats->totalVehicles++;

    Serial.print(F("Vehicle at Intersection "));
    Serial.print(inter->id);
    Serial.print(F(" | This cycle: "));
    Serial.print(inter->vehicleCount);
    Serial.print(F(" | Lifetime total: "));
    Serial.println(inter->totalVehiclesProcessed);

    // Extend green time automatically based on how busy the intersection is
    if (inter->vehicleCount > 8) {
      inter->greenDuration = GREEN_HIGH;
      Serial.println(F("  -> Heavy traffic: green extended to 15s"));
    } else if (inter->vehicleCount > 3) {
      inter->greenDuration = GREEN_MEDIUM;
      Serial.println(F("  -> Medium traffic: green extended to 12s"));
    } else {
      inter->greenDuration = GREEN_BASE;
    }
  }
}

void handleSerialCommands() {
  if (Serial.available() == 0) return;

  char command = Serial.read();
  while (Serial.available()) Serial.read(); // flush remaining characters

  switch (command) {
    case '1':
      intersection1->manualMode = !intersection1->manualMode;
      stats->manualOverrides++;
      Serial.print(F("Intersection 1 manual mode: "));
      Serial.println(intersection1->manualMode ? F("ON") : F("OFF"));
      if (intersection1->manualMode) printManualMenu(1);
      break;

    case '2':
      intersection2->manualMode = !intersection2->manualMode;
      stats->manualOverrides++;
      Serial.print(F("Intersection 2 manual mode: "));
      Serial.println(intersection2->manualMode ? F("ON") : F("OFF"));
      if (intersection2->manualMode) printManualMenu(2);
      break;

    case 'R': case 'r':
      if (intersection1->manualMode) { setLightState(intersection1, STATE_RED);    Serial.println(F("Int1 -> RED"));    }
      if (intersection2->manualMode) { setLightState(intersection2, STATE_RED);    Serial.println(F("Int2 -> RED"));    }
      break;

    case 'G': case 'g':
      if (intersection1->manualMode) { setLightState(intersection1, STATE_GREEN);  Serial.println(F("Int1 -> GREEN"));  }
      if (intersection2->manualMode) { setLightState(intersection2, STATE_GREEN);  Serial.println(F("Int2 -> GREEN"));  }
      break;

    case 'Y': case 'y':
      if (intersection1->manualMode) { setLightState(intersection1, STATE_YELLOW); Serial.println(F("Int1 -> YELLOW")); }
      if (intersection2->manualMode) { setLightState(intersection2, STATE_YELLOW); Serial.println(F("Int2 -> YELLOW")); }
      break;

    case 'E': case 'e': emergencyStop();     break;
    case 'S': case 's': resetSystem();       break;
    case 'M': case 'm': printMenu();         break;
    case 'V': case 'v': printDetailedStats(); break;

    default:
      Serial.println(F("Unknown command. Send M for menu."));
      break;
  }
}

// Puts all lights red and pauses the system — operator must send S to resume
void emergencyStop() {
  Serial.println(F("\n!!! EMERGENCY STOP ACTIVATED !!!"));
  stats->emergencyEvents++;
  setLightState(intersection1, STATE_RED);
  setLightState(intersection2, STATE_RED);
  systemActive = false;
  Serial.println(F("All lights RED. System paused. Send S to resume."));
}

void resetSystem() {
  Serial.println(F("\nResetting system..."));
  systemActive = true;

  intersection1->vehicleCount  = 0;
  intersection1->manualMode    = false;
  intersection1->greenDuration = GREEN_BASE;

  intersection2->vehicleCount  = 0;
  intersection2->manualMode    = false;
  intersection2->greenDuration = GREEN_BASE;

  // Reset stats but keep emergency event count for audit purposes
  stats->totalVehicles   = 0;
  stats->systemStartTime = millis();
  stats->cyclesCompleted = 0;

  setLightState(intersection1, STATE_GREEN);
  setLightState(intersection2, STATE_RED);

  Serial.println(F("Reset complete!"));
  printMenu();
}

void printSystemStatus() {
  Serial.println(F("\n--- Status ---"));

  Serial.print(F("Int1 ["));
  printState(intersection1->currentState);
  Serial.print(F("] Vehicles: "));
  Serial.print(intersection1->vehicleCount);
  Serial.print(F(" Total: "));
  Serial.print(intersection1->totalVehiclesProcessed);
  Serial.print(F(" Green: "));
  Serial.print(intersection1->greenDuration / 1000);
  Serial.print(F("s"));
  Serial.println(intersection1->manualMode ? F(" [MANUAL]") : F(" [AUTO]"));

  Serial.print(F("Int2 ["));
  printState(intersection2->currentState);
  Serial.print(F("] Vehicles: "));
  Serial.print(intersection2->vehicleCount);
  Serial.print(F(" Total: "));
  Serial.print(intersection2->totalVehiclesProcessed);
  Serial.print(F(" Green: "));
  Serial.print(intersection2->greenDuration / 1000);
  Serial.print(F("s"));
  Serial.println(intersection2->manualMode ? F(" [MANUAL]") : F(" [AUTO]"));

  Serial.print(F("All-time vehicles: "));
  Serial.print(stats->totalVehicles);
  Serial.print(F(" | Cycles: "));
  Serial.println(stats->cyclesCompleted);
}

void printDetailedStats() {
  unsigned long uptime = (millis() - stats->systemStartTime) / 1000;
  Serial.println(F("\n=== Detailed Statistics ==="));
  Serial.print(F("Uptime: "));
  Serial.print(uptime / 60);
  Serial.print(F(" min "));
  Serial.print(uptime % 60);
  Serial.println(F(" sec"));
  Serial.print(F("Total vehicles processed : ")); Serial.println(stats->totalVehicles);
  Serial.print(F("Total cycles completed   : ")); Serial.println(stats->cyclesCompleted);
  Serial.print(F("Manual overrides issued  : ")); Serial.println(stats->manualOverrides);
  Serial.print(F("Emergency events         : ")); Serial.println(stats->emergencyEvents);
  Serial.println(F("\nIntersection 1:"));
  Serial.print(F("  Vehicles this cycle : ")); Serial.println(intersection1->vehicleCount);
  Serial.print(F("  Lifetime vehicles   : ")); Serial.println(intersection1->totalVehiclesProcessed);
  Serial.print(F("  Current green time  : ")); Serial.print(intersection1->greenDuration / 1000); Serial.println(F("s"));
  Serial.println(F("Intersection 2:"));
  Serial.print(F("  Vehicles this cycle : ")); Serial.println(intersection2->vehicleCount);
  Serial.print(F("  Lifetime vehicles   : ")); Serial.println(intersection2->totalVehiclesProcessed);
  Serial.print(F("  Current green time  : ")); Serial.print(intersection2->greenDuration / 1000); Serial.println(F("s"));
}

void printMenu() {
  Serial.println(F("\n=== Smart Traffic Controller ==="));
  Serial.println(F("1   Toggle manual mode — Intersection 1"));
  Serial.println(F("2   Toggle manual mode — Intersection 2"));
  Serial.println(F("R   Set RED    (manual intersections only)"));
  Serial.println(F("G   Set GREEN  (manual intersections only)"));
  Serial.println(F("Y   Set YELLOW (manual intersections only)"));
  Serial.println(F("E   Emergency stop — all RED"));
  Serial.println(F("S   Reset system"));
  Serial.println(F("V   View detailed statistics"));
  Serial.println(F("M   Show this menu"));
  Serial.println(F("================================\n"));
}

void printManualMenu(int id) {
  Serial.print(F("Manual mode ON — Intersection "));
  Serial.println(id);
  Serial.println(F("Commands: R=Red  G=Green  Y=Yellow"));
}

void printState(LightState state) {
  switch (state) {
    case STATE_RED:    Serial.print(F("RED")); break;
    case STATE_YELLOW: Serial.print(F("YEL")); break;
    case STATE_GREEN:  Serial.print(F("GRN")); break;
  }
}

