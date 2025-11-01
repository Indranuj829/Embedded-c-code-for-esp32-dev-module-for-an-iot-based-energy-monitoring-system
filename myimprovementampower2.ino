#include <FirebaseClient.h> // The Firebase library
#include <WiFi.h>
#include <PZEM004Tv30.h>
#include <HardwareSerial.h>

// Define the UART2 RX and TX pins on ESP32
#define PZEM_RX_PIN 16 // ESP32 RX2 (Connect to PZEM TX)
#define PZEM_TX_PIN 17 // ESP32 TX2 (Connect to PZEM RX)

// Update with your actual credentials
#define FIREBASE_HOST "carbon-credits-collector-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "LPwTXAReTIF4W0FZxTMZ2JDuNkTUhzE9P8e4MA0c"
#define WIFI_SSID "vivo 1907"
#define WIFI_PASSWORD "HAIVIVO1"

// PZEM configuration
HardwareSerial pzemSerial(1); // Use Serial 1 (pins 9 and 10) for PZEM as per your previous code
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

// Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Global variables for credit calculation
const float WATT_SECONDS_PER_CREDIT = 5.0; // 1W for 5 seconds = 1 credit
const unsigned long DURATION_TO_CALCULATE_CREDITS = 5 * 60 * 1000; // 5 minutes in milliseconds
unsigned long lastReadTime = 0;
unsigned long lastCreditTime = 0;
float cumulativeWattSeconds = 0.0;
long creditsEarned = 0; // The total credits earned

// Timing variables for Firebase update
const unsigned long updateInterval = 60000; // Update every 60 seconds
unsigned long lastFirebaseUpdateTime = 0;

// Function to read data from PZEM and accumulate energy
void readAndProcessData() {
  float power = pzem.power();
  if (isnan(power)) {
    Serial.println("Error reading power from PZEM!");
  } else {
    // Add the power consumption of the last 5 seconds to the cumulative total
    cumulativeWattSeconds += (power * (millis() - lastReadTime) / 1000.0); 
    Serial.print("Current Power: ");
    Serial.print(power);
    Serial.println(" W");
  }
}

// Function to calculate credits and reset cumulative data
void calculateCredits() {
  if (cumulativeWattSeconds > 0) {
    long newCredits = floor(cumulativeWattSeconds / WATT_SECONDS_PER_CREDIT);
    creditsEarned += newCredits; // Add new credits to the total
    cumulativeWattSeconds = 0.0; // Reset cumulative watt-seconds
    
    Serial.println();
    Serial.println("--- CREDIT SUMMARY (LAST 5 MINUTES) ---");
    Serial.print("Credits earned in this interval: ");
    Serial.println(newCredits);
    Serial.print("Total credits: ");
    Serial.println(creditsEarned);
    Serial.println("-------------------------------------");
  } else {
    Serial.println("No power consumed in the last 5 minutes. No new credits earned.");
  }
}

// Function to send total credits to Firebase
void sendCreditsToFirebase() {
  // Path to save data in Firebase
  String path = "/amppower/credit_data";

  // Check if Firebase is ready before sending
  if (Firebase.ready()) {
    // Pushing the total accumulated credits to Firebase Realtime Database
    if (Firebase.setInt(fbdo, path, creditsEarned)) {
      Serial.println("Data pushed successfully!");
    } else {
      Serial.print("Failed to push data: ");
      Serial.println(fbdo.errorReason());
    }
  }
}

void setup() {
  Serial.begin(115200);
  // Initialize the Hardware Serial interface for the PZEM module
  // Baud rate 9600 for PZEM, define RX/TX pins
  Serial2.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);

  Serial.println("PZEM-004T Power Monitor with Firebase Upload");
  Serial.println("----------------------------------------------");
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Configure Firebase with your database details
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true); // Ensure auto-reconnection
  Firebase.setDoubleDigits(5); // Adjust for higher precision if needed

  lastReadTime = millis();
  lastCreditTime = millis();
  lastFirebaseUpdateTime = millis();
}

void loop() {
  // Read PZEM data every 5 seconds, similar to the working code
  if (millis() - lastReadTime > 5000) {
    readAndProcessData();
    lastReadTime = millis();
  }

  // Calculate credits every 5 minutes
  if (millis() - lastCreditTime > DURATION_TO_CALCULATE_CREDITS) {
    calculateCredits();
    lastCreditTime = millis();
  }
  
  // Send total credits to Firebase at a specific interval
  if ((millis() - lastFirebaseUpdateTime) > updateInterval) {
    sendCreditsToFirebase();
    lastFirebaseUpdateTime = millis();
  }
}
