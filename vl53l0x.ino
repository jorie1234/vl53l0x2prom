#include "Adafruit_VL53L0X.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "arduino_secrets.h"


void setup_wifi();
void setup_http_server();
void handle_http_home_client();
void handle_http_metrics_client();

enum LogLevel {
  DEBUG,
  INFO,
  ERROR,
};
// Debug mode is enabled if not zero
#define DEBUG_MODE 1
#define BOARD_NAME "ESP8266"
// Wi-Fi SSID (required)
#define WIFI_SSID SECRET_SSID
// Wi-Fi password (required)
#define WIFI_PASSWORD SECRET_PASS
// HTTP server port
#define HTTP_SERVER_PORT 80
// HTTP metrics endpoint
#define HTTP_METRICS_ENDPOINT "/metrics"
// Prometheus namespace, aka metric prefix
#define PROM_NAMESPACE "iot"

ESP8266WebServer http_server(HTTP_SERVER_PORT);

Adafruit_VL53L0X lox = Adafruit_VL53L0X();

void setup() {
  Serial.begin(115200);

  setup_wifi();
  setup_http_server();

  // wait until serial port opens for native USB devices
  while (! Serial) {
    delay(1);
  }

  Serial.println("Adafruit VL53L0X test");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    while (1);
  }
  // power
  Serial.println(F("VL53L0X API Simple Ranging example\n\n"));
}


void loop() {
  http_server.handleClient();
}

void log_request() {
  char message[128];
  char method_name[16];
  get_http_method_name(method_name, 16, http_server.method());
  snprintf(message, 128, "Request: client=%s:%u method=%s path=%s",
           http_server.client().remoteIP().toString().c_str(), http_server.client().remotePort(), method_name, http_server.uri().c_str());
  log1(message, LogLevel::INFO);
}

void log1(char const *message, LogLevel level) {
  if (DEBUG_MODE == 0 && level == LogLevel::DEBUG) {
    return;
  }
  // Will overflow after a while
  float seconds = millis() / 1000.0;
  char str_level[10];
  switch (level) {
    case DEBUG:
      strcpy(str_level, "DEBUG");
      break;
    case INFO:
      strcpy(str_level, "INFO");
      break;
    case ERROR:
      strcpy(str_level, "ERROR");
      break;
    default:
      break;
  }
  char record[150];
  snprintf(record, 150, "[%10.3f] [%-5s] %s", seconds, str_level, message);
  Serial.println(record);
}
void setup_wifi() {
  char message[128];
  log1("Setting up Wi-Fi", LogLevel::DEBUG);
  snprintf(message, 128, "Wi-Fi SSID: %s", WIFI_SSID);
  log1(message, LogLevel::DEBUG);
  snprintf(message, 128, "MAC address: %s", WiFi.macAddress().c_str());
  log1(message, LogLevel::DEBUG);
  snprintf(message, 128, "Initial hostname: %s", WiFi.hostname().c_str());
  log1(message, LogLevel::DEBUG);

  WiFi.mode(WIFI_STA);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    log1("Wi-Fi connection not ready, waiting", LogLevel::DEBUG);
    delay(500);
  }

  log1("Wi-Fi connected.", LogLevel::DEBUG);
  snprintf(message, 128, "SSID: %s", WiFi.SSID().c_str());
  log1(message, LogLevel::DEBUG);
  snprintf(message, 128, "BSSID: %s", WiFi.BSSIDstr().c_str());
  log1(message, LogLevel::DEBUG);
  snprintf(message, 128, "Hostname: %s", WiFi.hostname().c_str());
  log1(message, LogLevel::DEBUG);
  snprintf(message, 128, "MAC address: %s", WiFi.macAddress().c_str());
  log1(message, LogLevel::DEBUG);
  snprintf(message, 128, "IPv4 address: %s", WiFi.localIP().toString().c_str());
  log1(message, LogLevel::DEBUG);
  snprintf(message, 128, "IPv4 subnet mask: %s", WiFi.subnetMask().toString().c_str());
  log1(message, LogLevel::DEBUG);
  snprintf(message, 128, "IPv4 gateway: %s", WiFi.gatewayIP().toString().c_str());
  log1(message, LogLevel::DEBUG);
  snprintf(message, 128, "Primary DNS server: %s", WiFi.dnsIP(0).toString().c_str());
  log1(message, LogLevel::DEBUG);
  snprintf(message, 128, "Secondary DNS server: %s", WiFi.dnsIP(1).toString().c_str());
  log1(message, LogLevel::DEBUG);
}
void setup_http_server() {
  char message[128];
  log1("Setting up HTTP server", LogLevel::DEBUG);
  http_server.on("/", HTTPMethod::HTTP_GET, handle_http_root);
  http_server.on(HTTP_METRICS_ENDPOINT, HTTPMethod::HTTP_GET, handle_http_metrics);
  http_server.onNotFound(handle_http_not_found);
  http_server.begin();
  log1("HTTP server started", LogLevel::DEBUG);
  snprintf(message, 128, "Metrics endpoint: %s", HTTP_METRICS_ENDPOINT);
  log1(message, LogLevel::DEBUG );
}


void handle_http_root() {
  log_request();
  static size_t const BUFSIZE = 256;
  static char const *response_template =
    "Prometheus ESP8266 VL53L0X Exporter \n"
    "\n"
    "Project: https://github.com/jorie123/vl53l0x2prom\n"
    "\n"
    "Usage: %s\n";
  char response[BUFSIZE];
  snprintf(response, BUFSIZE, response_template, HTTP_METRICS_ENDPOINT);
  http_server.send(200, "text/plain; charset=utf-8", response);
}

void handle_http_metrics() {
  log_request();
  static size_t const BUFSIZE = 1024;
  static char const *response_template =
    "# HELP " PROM_NAMESPACE "_desk_hight_mm Desk Hight in mm.\n"
    "# TYPE " PROM_NAMESPACE "_desk_hight_mm gauge\n"
    "# UNIT " PROM_NAMESPACE "_desk_hight_mm %%\n"
    PROM_NAMESPACE "_desk_hight_mm %i\n";

  VL53L0X_RangingMeasurementData_t measure;

  Serial.print("Reading a measurement... ");
  lox.rangingTest(&measure, false); // pass in 'true' to get debug data printout!
  int hightInMM;
  if (measure.RangeStatus != 4) {  // phase failures have incorrect data
    Serial.print("Distance (mm): "); Serial.println(measure.RangeMilliMeter);
    hightInMM = measure.RangeMilliMeter;
  } else {
    Serial.println(" out of range ");
    http_server.send(500, "text/plain; charset=utf-8", "Sensor error.");
    return;
  }


  char response[BUFSIZE];
  snprintf(response, BUFSIZE, response_template, hightInMM);
  http_server.send(200, "text/plain; charset=utf-8", response);
}

void handle_http_not_found() {
  log_request();
  http_server.send(404, "text/plain; charset=utf-8", "Not found.");
}

void get_http_method_name(char *name, size_t name_length, HTTPMethod method) {
  switch (method) {
    case HTTP_GET:
      snprintf(name, name_length, "GET");
      break;
    case HTTP_HEAD:
      snprintf(name, name_length, "HEAD");
      break;
    case HTTP_POST:
      snprintf(name, name_length, "POST");
      break;
    case HTTP_PUT:
      snprintf(name, name_length, "PUT");
      break;
    case HTTP_PATCH:
      snprintf(name, name_length, "PATCH");
      break;
    case HTTP_DELETE:
      snprintf(name, name_length, "DELETE");
      break;
    case HTTP_OPTIONS:
      snprintf(name, name_length, "OPTIONS");
      break;
    default:
      snprintf(name, name_length, "UNKNOWN");
      break;
  }
}
