#define fsrpin A0
#define ecgpin A1
#define LO_POS 10  // AD8232 LO+ pin
#define LO_NEG 11  // AD8232 LO- pin
#define LED_PIN 13 // LED indicator for compression rate

// Variables for sensors
int fsrReading;    // Variable for FSR pressure sensor
int ecgReading;    // Variable for ECG sensor

// Variables for compression rate calculation
unsigned long lastCompressionTime = 0;
unsigned long compressionTimes[10];
int compressionIndex = 0;
bool wasCompressed = false;
int compressionThreshold = 300; // Adjust based on your FSR sensitivity

// Variables for ADC adjustment
int ecgGain = 1; // 1 = normal, 2 = reduced sensitivity during compressions

void setup() {
  // Begin serial communication at a baudrate of 9600:
  Serial.begin(9600);
  pinMode(LO_POS, INPUT); // Configuration for LO+ detection
  pinMode(LO_NEG, INPUT); // Configuration for LO- detection
  pinMode(LED_PIN, OUTPUT); // LED output for compression rate feedback
}

void loop() {
  // Read ECG sensor
  if((digitalRead(LO_POS) == 1) || (digitalRead(LO_NEG) == 1)) {
    // If leads are off, send an indicator
    Serial.println("!ECG_LEADS_OFF!");
  } else {
    // Read ECG with dynamic gain adjustment
    ecgReading = analogRead(ecgpin) / ecgGain;
    Serial.print("ECG:");
    Serial.println(ecgReading);
  }
  
  // Read FSR pressure sensor
  fsrReading = analogRead(fsrpin);
  Serial.print("FSR:");
  Serial.println(fsrReading);
  
  // Detect compressions and calculate rate
  detectCompressions();
  
  // Adjust ECG gain if compression is detected
  if (fsrReading > compressionThreshold) {
    ecgGain = 2; // Reduce sensitivity during compressions
  } else {
    ecgGain = 1; // Normal sensitivity
  }
  
  // Display FSR reading classification
  classifyPressure();
  
  delay(20); // Small delay to avoid serial saturation
}

void detectCompressions() {
  // Detect a compression
  if (fsrReading > compressionThreshold && !wasCompressed) {
    wasCompressed = true;
    
    // Record compression time
    unsigned long currentTime = millis();
    if (lastCompressionTime > 0) {
      unsigned long interval = currentTime - lastCompressionTime;
      
      // Record the interval in the circular buffer
      compressionTimes[compressionIndex] = interval;
      compressionIndex = (compressionIndex + 1) % 10;
      
      // Calculate compression rate
      int compressionRate = calculateCompressionRate();
      
      // Provide feedback if outside 100-120 range
      if (compressionRate < 100 || compressionRate > 120) {
        digitalWrite(LED_PIN, HIGH);
      } else {
        digitalWrite(LED_PIN, LOW);
      }
      
      // Send compression rate
      Serial.print("RATE:");
      Serial.println(compressionRate);
    }
    
    lastCompressionTime = currentTime;
  }
  else if (fsrReading <= compressionThreshold) {
    wasCompressed = false;
  }
}

int calculateCompressionRate() {
  // Calculate average time between compressions
  unsigned long totalTime = 0;
  int validCompressions = 0;
  
  for (int i = 0; i < 10; i++) {
    if (compressionTimes[i] > 0) {
      totalTime += compressionTimes[i];
      validCompressions++;
    }
  }
  
  if (validCompressions < 2) {
    return 0; // Not enough data
  }
  
  // Calculate BPM (60000 ms ÷ average interval)
  unsigned long avgInterval = totalTime / validCompressions;
  return 60000 / avgInterval;
}

void classifyPressure() {
  // Display pressure classification
  if (fsrReading < 10) {
    Serial.println("PRESSURE:None");
  } else if (fsrReading < 200) {
    Serial.println("PRESSURE:Light");
  } else if (fsrReading < 500) {
    Serial.println("PRESSURE:Medium");
  } else if (fsrReading < 800) {
    Serial.println("PRESSURE:Strong");
  } else {
    Serial.println("PRESSURE:VeryStrong");
  }
}
