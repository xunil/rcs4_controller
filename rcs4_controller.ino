#include <Bounce.h>
#include <Encoder.h>
#include <EEPROM.h>

// Push button on the encoder
const int buttonPin = 4;
Bounce pushbutton = Bounce(buttonPin, 5);  
                                      // 5 ms debounce
// Rotary encoder
Encoder knob(2, 3);
long pos = -999;

// LEDs
const int ledBase = 5;                // pin at which LED bar graph starts
const int ledCount = 4;               // how many LEDs are connected
byte portSelected = 0;

// Other variables
#define MAX_TITLE_LEN 40
#define NUM_PORTS 4
#define SELECTED_PORT_ADDR 192        // EEPROM address for the most recently selected port
int i, j;
char portTitles[NUM_PORTS][MAX_TITLE_LEN];
char c;
char serialBuffer[MAX_TITLE_LEN+1];
int bufPos = 0;
byte editMode = false;
byte editPort = 0;
const int K1 = 10;                    // Relay port: K1
const int K2 = 12;                    // Relay port: K2
byte relayStates[NUM_PORTS][2] = {
  {LOW, LOW},
  {LOW, HIGH},
  {HIGH, LOW},
  {HIGH, HIGH},
};
const int K3 = 23;                     // KPA500 keying relay port
byte amp_keyed = false;

void readPortTitles() {
  // Read port titles from EEPROM
  for (i = 0; i < NUM_PORTS; i++) {
    for (j = 0; j < MAX_TITLE_LEN; j++) {
      c = EEPROM.read((i*MAX_TITLE_LEN)+j);
      if (c == 255)
        break;
      portTitles[i][j] = c;
    }
  }
}

void writePortTitles() {
  // Write port titles to EEPROM
  int titleLen;
  for (i = 0; i < NUM_PORTS; i++) {
    titleLen = strlen(portTitles[i]);
    for (j = 0; j < MAX_TITLE_LEN; j++) {
      if (j > titleLen) {
        c = 255;
      } else {
        c = portTitles[i][j];
      }
      EEPROM.write((i*MAX_TITLE_LEN)+j, c);
    }
  }
}

void displayPortTitles() {
  Serial.println();
  Serial.println("RCS-4 Antenna/Rig Switch");
  Serial.println("----------------------------------------");
  for (i = 0; i < NUM_PORTS; i++) {
    if (portSelected == i) {
      Serial.print("*");
    } else {
      Serial.print(" ");
    }
    Serial.print(i+1);
    Serial.print(": ");
    Serial.print(portTitles[i]);
    Serial.println();
  }
  Serial.println();
  Serial.print("Amp status: ");
  if (amp_keyed) {
    Serial.println("KEYED");
  } else {
    Serial.println("not keyed");
  }
  Serial.println();
}

void displayMenu() {
  displayPortTitles();
  Serial.println();
  Serial.println("* selected");
  Serial.println("'En' to edit port n's title (e.g. E1). Number to switch to that port.");
  Serial.println("'A' to toggle amp key-down state.");
}

// This will soon also trigger relays (or BJTs) to feed bias to remote switchbox
void updateLEDs() {
  for (i = 0; i < ledCount; i++) {
    digitalWrite(ledBase+i, (i == portSelected ? HIGH : LOW));
  }
}

void selectPort() {
  digitalWrite(K1, relayStates[portSelected][0]);
  digitalWrite(K2, relayStates[portSelected][1]);
  EEPROM.write(SELECTED_PORT_ADDR, portSelected);
  updateLEDs();
}

void dumpEEPROM() {
  char e;
  char buf[16];
  char text[16];
  Serial.println("EEPROM dump of first 128 bytes");
  Serial.println("----------------------------------------");
  for (i = 0; i < 128; i += 16) {
    sprintf(buf, "%04X", i);
    Serial.print(buf);
    Serial.print(": ");
    for (j = i; j < (i+16); j++) {
      e = EEPROM.read(j);
      text[j-i] = e;
      sprintf(buf, "%02X ", e);
      Serial.print(buf);
    }
    Serial.print("  ");
    for (j = 0; j < 16; j++) {
      Serial.print(text[j] > 31 && text[j] < 127 ? text[j] : '.');
    }
    Serial.println();
  }
}

void toggleAmpKeyDown() {
  if (amp_keyed) {
    amp_keyed = false;
    digitalWrite(K3, LOW);
  } else {
    amp_keyed = true;
    digitalWrite(K3, HIGH);  // Key the relay coil to sink KPA500 keying to ground
  }  
}

void handleInput() {
  if (editMode == true) {
    // User has just entered a string, stored in serialBuffer, of length bufPos.
    memset(portTitles[editPort], 0, MAX_TITLE_LEN);
    strcpy(portTitles[editPort], serialBuffer);
    Serial.print("Port ");
    Serial.print(editPort+1);
    Serial.print(" title updated to '");
    Serial.print(portTitles[editPort]);
    Serial.println("'.");
    Serial.println("Saving titles to EEPROM, do not remove power.");
    writePortTitles();
    Serial.println("Done.");
    editMode = false;
    displayMenu();
  } else {
    c = serialBuffer[0];
    if (c > '0' && c <= ('0' + NUM_PORTS)) {  // ASCII number values 1-4
      portSelected = (c - '0') - 1;
      selectPort();
      displayMenu();
    } else if (c == 'e' || c == 'E') {
      // Edit mode
      editPort = (serialBuffer[1] - '0') - 1;
      if (editPort >= NUM_PORTS) {
        Serial.print("Invalid port number ");
        Serial.println(editPort+1);
        displayMenu();
      } else {
        Serial.print("Editing title for port ");
        Serial.print(editPort+1);
        Serial.println(". Enter up to 40 characters.");
        Serial.print("> ");
        Serial.flush();
        editMode = true;
      }
    } else if (c == 'd' || c == 'D') {
      dumpEEPROM();
    } else if (c == 'a' || c == 'A') {
      toggleAmpKeyDown();
    } else {
      displayMenu();
    }
  }
  
  memset(serialBuffer, 0, MAX_TITLE_LEN+1);
  bufPos = 0;
}

void setup() {
  Serial.begin(9600);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(K1, OUTPUT);
  digitalWrite(K1, LOW);
  pinMode(K2, OUTPUT);
  digitalWrite(K2, LOW);
  pinMode(K3, OUTPUT);
  digitalWrite(K3, LOW);
  for (i = 0; i < ledCount; i++) {
    pinMode(ledBase+i, OUTPUT);
    digitalWrite(ledBase+i, LOW);
  }
  pos = knob.read();               // Avoid needlessly incrementing selected port
  portSelected = EEPROM.read(SELECTED_PORT_ADDR);
  selectPort();
  readPortTitles();
}

byte previousState = HIGH;         // what state was the button last time
unsigned int count = 0;            // how many times has it changed to low
unsigned long countAt = 0;         // when count changed
unsigned int countPrinted = 0;     // last count printed

void loop() {
  long newPos;
  newPos = knob.read();
  if (newPos != pos) {
    if (newPos > pos) { // Rotated CW
      portSelected = ((portSelected + 1) >= ledCount ? (0) : (portSelected + 1));
    } else { // Rotated CCW
      portSelected = ((portSelected - 1) < 0 ? (ledCount - 1) : (portSelected - 1));
    }
    pos = newPos;
    selectPort();
  }
  
  if (Serial.available() > 0) {
    c = Serial.read();
    if ((c == 13 || c == 10) || bufPos >= MAX_TITLE_LEN) {
      serialBuffer[bufPos++] = 0;
      Serial.println();
      handleInput();
    } else if (c == 8 || c == 127) {
      serialBuffer[--bufPos] = 0;
      Serial.write(8);                // Print a backspace
      Serial.write(32);               // Print a space to erase the previously-typed char
      Serial.write(8);                // Print a backspace
      Serial.flush();
    } else {
      serialBuffer[bufPos++] = c;
      Serial.print(c);
    }
  }
  /*
  if (pushbutton.update()) {
    if (pushbutton.fallingEdge()) {
      count = count + 1;
      countAt = millis();
    }
  } else {
    if (count != countPrinted) {
      unsigned long nowMillis = millis();
      if (nowMillis - countAt > 100) {
        Serial.print("count: ");
        Serial.println(count);
        countPrinted = count;
      }
    }
  }
  */
}
