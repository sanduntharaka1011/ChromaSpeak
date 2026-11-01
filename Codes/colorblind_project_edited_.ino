/*
 * ChromaSpeak — Full Firmware (Cold Boot Optimized)
 * Fixes the battery power boot-up race condition by adding a 4-second delay
 * to let the YN-T6455-C boost converter and DFPlayer Mini stabilize.
 */

#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ===== TCS3200 Pin Assignments =====
#define SO 4
#define S1 5
#define S2 6
#define S3 7
#define OUT 8

// ===== DFPlayer Mini Pin Assignments =====
SoftwareSerial dfSerial(10, 11); // RX, TX
DFRobotDFPlayerMini player;

// ===== Push Button Pin Assignment =====
#define BTN 2

// ===== CALIBRATION BLOCK =====
// Remember to replace these default numbers with your recorded calibration values!
int rWhite = 70,  rBlack = 700;
int gWhite = 90,  gBlack = 720;
int bWhite = 80,  bBlack = 700;

// ===== COLOUR PALETTE LOOKUP TABLE =====
struct NamedColour { 
  const char* name; 
  uint8_t r, g, b; 
  uint8_t track; 
};

NamedColour palette[] = {
  {"red",        247,213,224,  1},  // Track 0001.mp3
  {"green",      230,246,226,  2},  // Track 0002.mp3
  {"blue",       207,215,232, 3},  // Track 0003.mp3
  {"yellow",     255,255,242,  4},  // Track 0004.mp3
  {"orange",     255,232,228,  5},  // Track 0005.mp3
  {"purple",     226,212,228, 6},  // Track 0006.mp3
  {"white",      255,255,255, 7},  // Track 0007.mp3
  {"black",      196,197,210,  8},  // Track 0008.mp3
 // {"brown",      120, 75,  45,  9},  // Track 0009.mp3
 // {"pink",       235, 150, 175, 10}, // Track 0010.mp3
 // {"grey",       140, 140, 140, 11}, // Track 0011.mp3
//{"cyan",       35,  190, 210, 12}, // Track 0012.mp3
 // {"magenta",    210, 50,  180, 13}, // Track 0013.mp3
 // {"lime green", 120, 220, 50,  14}, // Track 0014.mp3
 // {"gold",       220, 180, 40,  15}  // Track 0015.mp3
};
const int NCOLOURS = sizeof(palette) / sizeof(palette[0]);

// Reads raw frequency pulse width from a specific color filter channel
int readChannel(bool s2, bool s3) {
  digitalWrite(S2, s2);
  digitalWrite(S3, s3);
  delay(50);                 // Let the photodiode filter settle
  return pulseIn(OUT, LOW);  // Return pulse width in microseconds
}

// Converts raw frequencies into standard 0-255 RGB values
int toRGB(int raw, int white, int black) {
  int v = map(raw, white, black, 255, 0); // white -> 255, black -> 0
  return constrain(v, 0, 255);            // Keep output bounds strict
}

void setup() {
 
  delay(4000); 
  
  // Initialize sensor pins
  pinMode(SO, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(OUT, INPUT);
  
  // Activate frequency scaling to 20%
  digitalWrite(SO, HIGH); 
  digitalWrite(S1, LOW);
  
  // Setup button input pin
  pinMode(BTN, INPUT_PULLUP);

  // Start Serial communication lines
  Serial.begin(9600);
  dfSerial.begin(9600);
  
  Serial.println(F("Initializing DFPlayer Mini..."));
  
  // Connect to the player using the bypass check
  if (!player.begin(dfSerial)) {
    Serial.println(F("DFPLAYER ERROR: Not connecting, but booting anyway!"));
  } else {
    Serial.println(F("DFPlayer connected successfully!"));
    player.volume(80); // Set playback volume (0 to 30)
  }
  
  Serial.println(F("ChromaSpeak System booted. Ready for button press."));
}

void loop() {
  // If the button is pressed (INPUT_PULLUP drives it LOW on press)
  if (digitalRead(BTN) == LOW) {
    delay(20); // Debounce delay
    if (digitalRead(BTN) != LOW) return; 
    
    // Initialize color sampling accumulators
    long rs = 0, gs = 0, bs = 0;
    const int N = 5; // Take 5 samples for an accurate average
    
    for (int i = 0; i < N; i++) {
      rs += readChannel(LOW, LOW);     // Red Channel
      gs += readChannel(HIGH, HIGH);   // Green Channel
      bs += readChannel(LOW, HIGH);    // Blue Channel
    }
    
    // Average samples and map them to standard RGB
    int R = toRGB(rs / N, rWhite, rBlack);
    int G = toRGB(gs / N, gWhite, gBlack);
    int B = toRGB(bs / N, bWhite, bBlack);
    
    // Match to nearest palette color using Euclidean Distance 
    long best = 2147483647L;
    int idx = 0;
    
    for (int i = 0; i < NCOLOURS; i++) {
      long dr = R - palette[i].r;
      long dg = G - palette[i].g;
      long db = B - palette[i].b;
      long dist = dr*dr + dg*dg + db*db;
      
      if (dist < best) { 
        best = dist; 
        idx = i; 
      }
    }
    
    // Output match data to Serial Monitor
    Serial.print(F("RGB=")); Serial.print(R); Serial.print(',');
    Serial.print(G); Serial.print(','); Serial.print(B);
    Serial.print(F(" -> "));
    Serial.println(palette[idx].name);
    
    // Play the mapped audio tracking number via DFPlayer
    player.playMp3Folder(palette[idx].track); 
    delay(1500); // Allow track timeline to finish playing
    
    // Trap running loop safely until button is fully released
    while (digitalRead(BTN) == LOW) delay(10);
  }
}