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

//#define GPS_DEBUG
#define SERIAL_NOTIFY_UPDATE

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
  Serial.print("GPS: Trying to lock on satellite...");
}

void setup() {
  Serial.begin(SRL_BAUD);
  while (!Serial);

  init_lora();
  init_gps();
}


static bool locked_on = false;

static bool feed_the_beast(unsigned long ms) {
  bool new_data = false;
  unsigned long start = millis();
  do {
    while (gps_serial.available() > 0) {
      uint8_t data = gps_serial.read();
#ifdef GPS_DEBUG
      Serial.print(static_cast<char>(data));
#endif
      if (gps.encode(data)) {
        if (!locked_on && gps.location.isValid()) {
          Serial.println("\nGPS: Satellite locked on!!!");
          locked_on = true;
        }
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
#ifdef SERIAL_NOTIFY_UPDATE
  Serial.println("LoRa: Packet sent");
#endif
}

void loop() {
  if (!feed_the_beast(LORA_SEND_DELAY)) {
    return;
  }
  
  if (!locked_on) {
    Serial.print(".");
    return;
  }

#ifdef SERIAL_NOTIFY_UPDATE
  Serial.print("GPS: Data updated => ");
  Serial.print("sat: ");
  Serial.print(gps.satellites.value());
  Serial.print(" lat: ");
  Serial.print(gps.location.lat(), 6);
  Serial.print(" lng: ");
  Serial.println(gps.location.lng(), 6);
#endif

  gps_data_t data {
    .lat = gps.location.lat(),
    .lng = gps.location.lng(),
    .sat_c = gps.satellites.value(),
    .time = gps.time.value()
  };
  send_data(&data);
}
