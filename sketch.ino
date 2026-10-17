#include <DHT.h>

#define DHTPIN 21       // dht pin
#define DHTTYPE DHT22   // dht 22 sensor
#define MQ2_PIN 18      // gas sensor pin (digital out)

DHT dht(DHTPIN, DHTTYPE);

// timing for duty cycle / sampling
unsigned long sampleInterval = 4000; 
unsigned long lastTime = 0;

// sliding window arrays for stats
const int WIN_SIZE = 10;
float temps[WIN_SIZE];
float hums[WIN_SIZE];
int head = 0;
bool full = false;

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("ESP32 starting up... env anomaly test");
  
  dht.begin();
  pinMode(MQ2_PIN, INPUT);

  // zero out arrays
  for(int i=0; i<WIN_SIZE; i++) {
    temps[i] = 0;
    hums[i] = 0;
  }
  
  Serial.println("ready. collecting baseline data...");
}

void loop() {
  unsigned long now = millis();

  // duty cycle check - dont read too fast
  if (now - lastTime >= sampleInterval) {
    lastTime = now;

    // read sensors
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    int gas = digitalRead(MQ2_PIN); 

    if (isnan(h) || isnan(t)) {
      Serial.println("Error reading dht sensor!!");
      return;
    }

    // store in window buffer
    temps[head] = t;
    hums[head] = h;
    
    head = (head + 1) % WIN_SIZE;
    if (head == 0) {
      full = true;
    }

    // calc mean
    float tSum = 0, hSum = 0;
    int n = full ? WIN_SIZE : head;

    for(int i=0; i<n; i++) {
      tSum += temps[i];
      hSum += hums[i];
    }
    
    float tMean = tSum / n;
    float hMean = hSum / n;

    // calc std dev
    float tVar = 0, hVar = 0;
    for(int i=0; i<n; i++) {
      tVar += pow(temps[i] - tMean, 2);
      hVar += pow(hums[i] - hMean, 2);
    }
    
    float tStd = sqrt(tVar / n);
    float hStd = sqrt(hVar / n);

    // anomaly check based on stats
    bool anomaly = false;
    String reason = "";

    if (full) {
      // thresholds
      float tThresh = (tStd < 0.2) ? 0.5 : (2.0 * tStd);
      float hThresh = (hStd < 0.5) ? 1.5 : (2.0 * hStd);

      if (abs(t - tMean) > tThresh) {
        anomaly = true;
        reason += "temp spike! ";
      }
      if (abs(h - hMean) > hThresh) {
        anomaly = true;
        reason += "humidity drop/spike! ";
      }
    }

    // mq2 gas check (low means triggered on this module)
    if (gas == LOW) {
      anomaly = true;
      reason += "gas detected! ";
    }

    // print output to serial monitor
    Serial.println("------------------------------------");
    Serial.print("Temp: "); Serial.print(t); 
    Serial.print(" C (avg: "); Serial.print(tMean); Serial.print(" dev: "); Serial.print(tStd); Serial.print(") | ");
    Serial.print("Hum: "); Serial.print(h); 
    Serial.print(" % (avg: "); Serial.print(hMean); Serial.print(") | ");
    Serial.print("Gas status: "); Serial.println(gas == LOW ? "ALERT" : "OK");

    if (anomaly) {
      Serial.print("-> ANOMALY FOUND: ");
      Serial.println(reason);
    } else {
      Serial.println("-> status normal, all good.");
    }

    Serial.println("going to sleep / waiting next cycle...");
  }
}