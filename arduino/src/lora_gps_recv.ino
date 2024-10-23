#include <SPI.h>
#include <LoRa.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// NodeMCU connections
#define LORA_MISO 12 // D6
#define LORA_MOSI 13 // D7
#define LORA_SCK  14 // D5
#define LORA_NSS  4  // D2
#define LORA_RST  5  // D1
#define LORA_DIO0 A0 // A0

#define SRL_BAUD 9600

#define GPS_WAIT_THRESH 60000 // 1 minute

// #define USE_AP
#define WIFI_DEBUG

#ifdef USE_AP
const char* ssid = "WiFi_feo";
const char* pswd = "contrasena_segura";

IPAddress local_ip{192, 168, 62, 53};
IPAddress gateway {192, 168, 62, 1};
IPAddress subnet {255, 255, 255, 0};
#else
const char* ssid = "my_funny_ssid";
const char* pswd = "my_funny_pswd";

IPAddress local_ip{192, 168, 0, 53};
IPAddress gateway {192, 168, 0, 1};
IPAddress subnet {255, 255, 255, 0};
#endif


typedef struct {
  float lat{0.f}, lng{0.f};
  uint32_t sat_c{0}, time{0};
} gps_data_t;

static struct {
  gps_data_t cache{};
  unsigned long last_update{0};
  int rssi{0};
  bool available{false};
} gps;

ESP8266WebServer server{80};

static void init_lora() { 
  Serial.println("=> LoRa Receiver");
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(868E6)) {
    Serial.println("LoRa: Init failed!");
    while (1);
  }

  Serial.println("LoRa: Initialized");
}

static void init_wifi() {
#ifdef WIFI_DEBUG
  WiFi.printDiag(Serial);
#endif

  if (!WiFi.config(local_ip, gateway, subnet)) {
    Serial.println("WiFI: STA failed to configure!");
    while (1);
  }

  WiFi.begin(ssid, pswd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("\nWiFi: Connected -> ");
  Serial.println(WiFi.localIP());
}

template<typename Fun>
void init_server(const char* path, Fun&& f) {
  server.on(path, f);
  server.begin();

  Serial.print("Server: Initialized -> ");
  Serial.println(path);
}

static String json_encode() {
  String out = "{";
  out +="\"available\":";
  out += String{gps.available};
  out += ",\"last_update\":";
  out += String{gps.last_update};
  out += ",\"rssi\":";
  out += String{gps.rssi};
  out += ",\"time\":";
  out += String{gps.cache.time/100};
  out += ",\"sat_count\":";
  out += String{gps.cache.sat_c, 5};
  out += ",\"lat\":";
  out += String{gps.cache.lat, 6};
  out += ",\"lng\":";
  out += String{gps.cache.lng, 6};
  out += "}";
  return out;
}

void setup() {
  Serial.begin(SRL_BAUD);
  while (!Serial);
  
  init_wifi();
  init_lora();
  init_server("/", []() {
    String response = json_encode();
    server.send(200, "text/json", response);
    Serial.print("Server: GET response -> ");
    Serial.println(response);
  });

  pinMode(LED_BUILTIN, OUTPUT);
  
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
}


static bool lora_poll() {
  int packet_size = LoRa.parsePacket();
  if(!packet_size) {
    return false;
  }
  
  if ((size_t)packet_size != sizeof(gps_data_t)) {
    Serial.println("Invalid packet received!");
    return false;
  }

  uint8_t buff[sizeof(gps_data_t)];
  size_t sz = sizeof(gps_data_t);
  
  for (size_t i = 0; i < sz; ++i) {
    buff[i] = (uint8_t)LoRa.read();
  }
  memcpy(&gps.cache, buff, sz);

  
  Serial.print("LoRa: Received packet with RSSI ");
  Serial.println(LoRa.packetRssi());
  gps.rssi = LoRa.packetRssi();
  return true;
}

void loop() {
  if (lora_poll()) {
    if (!gps.available) {
      digitalWrite(LED_BUILTIN, LOW);
      gps.available = true; 
    }
    gps.last_update = millis();
  }
  if (gps.available && millis() - gps.last_update > GPS_WAIT_THRESH) {
    digitalWrite(LED_BUILTIN, HIGH);
    gps.available = false;
  }
  
  server.handleClient();
}
