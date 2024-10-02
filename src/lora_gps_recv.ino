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

#define WIFI_DEBUG

const char* ssid = "my_funny_ssid";
const char* pswd = "my_funny_pswd";

IPAddress local_ip{192, 168, 0, 53};
IPAddress gateway{192, 168, 0, 1};
IPAddress subnet{255, 255, 255, 0};


typedef struct {
  float lat{0.f}, lng{0.f};
  uint32_t sat_c{0}, time{0};
} gps_data_t;


gps_data_t gps_cache;
unsigned long last_update = 0;

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
static void init_server(const char* path, Fun&& f) {
  server.on(path, f);
  server.begin();

  Serial.print("Server: Initialized -> ");
  Serial.println(path);
}

static String json_encode(const gps_data_t& data) {
  String out = "{";
  out += "\"last_update\":";
  out += String{last_update, 5};
  out += ",\"time\":";
  out += String{data.time/100};
  out += ",\"sat_count\":";
  out += String{data.sat_c, 5};
  out += ",\"lat\":";
  out += String{data.lat, 6};
  out += ",\"lng\":";
  out += String{data.lng, 6};
  out += "}";
  return out;
}

void setup() {
  Serial.begin(SRL_BAUD);
  while (!Serial);
  
  init_wifi();
  init_lora();
  init_server("/", []() {
    String response = json_encode(gps_cache);
    server.send(200, "text/json", response);
    Serial.print("Server: GET response -> ");
    Serial.println(response);
  });

  pinMode(LED_BUILTIN, OUTPUT);
}


static bool lora_poll(gps_data_t* data) {
  int packet_size = LoRa.parsePacket();
  if (!packet_size) {
    return false;
  }
  
  if ((size_t)packet_size != sizeof(gps_data_t)) {
    Serial.println("Invalid packet received!");
    return false;
  }

  // No other checks allowed!!
  uint8_t buff[sizeof(gps_data_t)];
  size_t sz = sizeof(gps_data_t);
  for (size_t i = 0; i < sz; ++i) {
    buff[i] = (uint8_t)LoRa.read();
  }
  memcpy(data, buff, sz);

  Serial.print("LoRa: Received packet with RSSI ");
  Serial.println(LoRa.packetRssi());
  return true;
}

void loop() {
  if (lora_poll(&gps_cache)) {
    last_update = millis();
  }
  server.handleClient();
}
