#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>
#include <SPI.h>
#include <LoRa.h>

// Arduino Nano connections
#define LORA_MISO 12
#define LORA_MOSI 11
#define LORA_SCK  13
#define LORA_NSS  10
#define LORA_RST  9
#define LORA_DIO0 A0

#define LORA_SEND_DELAY 2000

#define GPS_RX 3
#define GPS_TX 2
#define GPS_BAUD 9600
#define SRL_BAUD 9600


TinyGPSPlus gps{};
SoftwareSerial gps_serial{GPS_TX, GPS_RX};

typedef struct {
  float lat, lng;
  uint32_t sat_c, time;
} gps_data_t;


static void init_lora() {
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);

  Serial.println("=> LoRa Sender");

  if (!LoRa.begin(868E6)) {
    Serial.println("LoRa: Init failed!");
    while (1);
  }

  Serial.println("LoRa: Initialized");
}

static void init_gps() {
  gps_serial.begin(GPS_BAUD);
  Serial.println("GPS: Initialized");
}

void setup() {
  Serial.begin(SRL_BAUD);
  while (!Serial);

  init_lora();
  init_gps();
}


static bool feed_the_beast(unsigned long ms) {
  bool new_data = false;
  unsigned long start = millis();
  do { // Can NEVER rest, MUST ALWAYS POLL!!!
    while (gps_serial.available() > 0) {
      uint8_t data = gps_serial.read();
      // uncomment to see the gps string
      // Serial.print(data); 
      if (gps.encode(data)) {
        new_data = true;
      }
    };
  } while (millis() - start < ms);
  return new_data;
}

static void send_data(gps_data_t* data) {
  uint8_t buffer[sizeof(gps_data_t)];
  size_t sz = sizeof(gps_data_t);
  memcpy(buffer, data, sz);
  
  LoRa.beginPacket();
  LoRa.write(buffer, sz);
  LoRa.endPacket(true);
  
  Serial.println("LoRa: Packet sent");
}

void loop() {
  if (!feed_the_beast(LORA_SEND_DELAY)) {
    return;
  }

  // If the beast has been well fed
  Serial.println("GPS: New data");
  gps_data_t data {
    .lat = gps.location.lat(),
    .lng = gps.location.lng(),
    .sat_c = gps.satellites.value(),
    .time = gps.time.value(),
  };
  send_data(&data);
}
