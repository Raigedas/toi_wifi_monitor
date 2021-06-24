
#include "pitches.h"

#include "melody_player.h"

#include <ESP8266WiFi.h>
#include <Pinger.h>
#include <ArduinoOTA.h>
#include <IPAddress.h>

#include <ESP8266WebServer.h>

#include "Switch.h"

#include "melody_player.h"

#include <StreamString.h>


MelodyPart noWifiMelody[] = {
  // police fast
  {635, 912, 1000, 0},
  {912, 635, 1000, 0}
};

MelodyPart noInternetMelody[] = {
  // police slow
  {635, 912, 5000, 0},
  {912, 635, 5000, 0}
};

MelodyPart groupDownMelody[] = {
  {NOTE_A6, -1, 70, 70},
  {NOTE_A6, -1, 70, 70},
  {NOTE_A6, -1, 70, 3000},
};

MelodyPart noPowerMelody[] = {
  {NOTE_C6, -1, 100, 100},
  {NOTE_A6, -1, 100, 800}
};


#define CHECK_STATUS_OK -1
#define CHECK_STATUS_BAD_TIMEOUT 0

struct WifiNetworkConfiguration {
  char ssid[100];
  char password[100];
};

struct WifiMonitor {
  IPAddress ip;
  bool me;
  long checkPingCountdown;
  bool online;
};

struct CheckResult {
  int wifi = CHECK_STATUS_OK;
  int internet = CHECK_STATUS_OK;
  int group = CHECK_STATUS_OK;
  int power = CHECK_STATUS_OK;
};


#define IP_ADDRESS_LENGTH 16
#define HOSTNAME_LENGTH 50


#include "configuration.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <CircularBuffer.h>


WifiMonitor* wifiMonitors;
bool meDefined;
int pingWifiMonitorIndex;
bool pinging;
int pingCountdown;


#define DEVICE_CLASS "wifi-monitor"

Pinger pinger;

byte macRaw[6];
char mac[12+1];

WiFiClient wiFiClient;
int wifiStatus = WL_DISCONNECTED;

int wifiNetworkConfigurationCount = 0;
int wifiNetworkConfigurationIndex = 0;

CheckResult checkResult;

WiFiEventHandler onWiFiConnectedHandler;
WiFiEventHandler onWiFiDisconnectedHandler;
WiFiEventHandler onWiFiGotIpHandler;

WifiMonitor *me = NULL;

bool otaInitialized = false;

unsigned long lastTimeIncrement = 0;

#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

float scannerAngle = 0;

ESP8266WebServer server(80);

Switch button = Switch(BUTTON_PIN);

MelodyPlayer melodyPlayer = MelodyPlayer(SPEAKER_PIN);

bool ignoreGroupDown = false;

CircularBuffer<int,5> batteryVoltagesBuffer;
float batteryVoltageAverage = 0;


void playMelody() {
  int notes[] = {
    NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
  };
  
  int noteDurations[] = {
    8, 16, 16, 8, 8, 8, 8, 8
  };
  
  for (int i = 0; i < 8; i++) {
    int noteDuration = 1000 / noteDurations[i];
    tone(SPEAKER_PIN, notes[i], noteDuration);
    int pauseBetweenNotes = noteDuration * 1.10;
    delay(pauseBetweenNotes);
    noTone(SPEAKER_PIN);
  }
}

void readMac() {
  WiFi.macAddress(macRaw);
  Serial.print("mac: ");
  sprintf(mac, "%02X%02X%02X%02X%02X%02X", macRaw[0], macRaw[1], macRaw[2], macRaw[3], macRaw[4], macRaw[5]);
  Serial.print(mac);
  Serial.println();
}

void setupOta() {
  Serial.println("Setup OTA");
  
  // Port defaults to 8266
  ArduinoOTA.setPort(9366);

  // Hostname defaults to esp8266-[ChipID]
  //ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  //ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("ArduinoOTA Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nArduinoOTA End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("ArduinoOTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  otaInitialized = true;
}

int getDefinedMonitorCount() {
  return sizeof(monitorIps) / IP_ADDRESS_LENGTH;
}

int getAllocatedMonitorCount() {
  return getDefinedMonitorCount() +1;
}

void readWifiNetworkConfigurations() {
  wifiNetworkConfigurationCount = sizeof(wifiNetworkConfigurations) / sizeof(WifiNetworkConfiguration);
  wifiNetworkConfigurationIndex = 0;
}

void readMonitorConfigurations() {
#ifdef PRINT_DEBUG
    StreamString ss;
#endif
  wifiMonitors = (WifiMonitor*) malloc(sizeof(WifiMonitor) *getAllocatedMonitorCount());
  me = &wifiMonitors[0];
  me->me = true;
  me->checkPingCountdown = 0;
  me->online = false;
  for (byte i = 0; i < getDefinedMonitorCount(); i++) {
    wifiMonitors[i +1].checkPingCountdown = 0;
    wifiMonitors[i +1].online = false;
    wifiMonitors[i +1].me = false;
    wifiMonitors[i +1].ip.fromString(monitorIps[i]);
#ifdef PRINT_DEBUG
    Serial.print("readMonitorConfiguration ");
//    serialPrint(&wifiMonitors[i +1].ip);
    String a = wifiMonitors[i +1].ip.toString();
    ss.reserve(40);
    wifiMonitors[i +1].ip.printTo(ss);
#endif
  }
  meDefined = false;
#ifdef PRINT_DEBUG
  Serial.println(ss);
#endif
}


bool isWifiConnected() {
  return (wifiStatus == WL_CONNECTED);
}

boolean connectWifi(WifiNetworkConfiguration* configuration) {
  Serial.printf("Connecting to a WiFi network '%s'\n", configuration->ssid);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setOutputPower(0);
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.begin(configuration->ssid, configuration->password);
  wifiStatus = WL_IDLE_STATUS;

  return (wifiStatus == WL_CONNECTED);
}

boolean connectWifi() {
  if (wifiStatus == WL_IDLE_STATUS) {
    return false;
  }
  if (!isWifiConnected()) {
    Serial.printf("Wifi disconnected, will try to connect\n");
  }
  bool r = connectWifi(&wifiNetworkConfigurations[wifiNetworkConfigurationIndex]);
  wifiNetworkConfigurationIndex++;
  wifiNetworkConfigurationIndex %= wifiNetworkConfigurationCount;
  return r;
}

boolean ensureWifiConnected() {
  if (!isWifiConnected()) {
    return connectWifi();
  }
  return true;
}

bool onEndPinger(const PingerResponse &response) {
  Serial.printf("onEndPinger %s pings from %s\n", response.TotalReceivedResponses ? "Received some" : "Missed", response.DestIPAddress.toString().c_str());
  if(response.TotalReceivedResponses) {
    setOnline(response.DestIPAddress);
  }
  increasePingWifiMonitorIndex();
  pinging = false;
  return true;
}

bool onOnePingResultPinger(const PingerResponse &response) {
  if (pingCountdown) {
    pingCountdown--;
  }
//  Serial.printf("pingCountdown %d\n", pingCountdown);

  Serial.printf("%s ping #%d from %s\n", 
    response.ReceivedResponse ? "Received" : "Missed", 
    PING_COUNT -pingCountdown,
    response.DestHostname.length() ? response.DestHostname.c_str() : response.DestIPAddress.toString().c_str()
  );

  if (response.ReceivedResponse) {
    WifiMonitor *monitor = findMonitorOrMe(response.DestIPAddress);
    if (monitor) {
      monitor->online = true;
      monitor->checkPingCountdown = CHECK_PING_COUNTDOWN;
    }
    pinging = false;
    return false;
  }
  
  return !!pingCountdown;
}


void handleRoot() {
  String out = "<html><head><meta charset=\"UTF-8\"/><meta http-equiv=\"refresh\" content=\"";
  out += WEBPAGE_RELOAD_INTERVAL_SECONDS;
  out += "\"><title>WiFi Monitor ";
  out += mac;
  out += "</title></head>";
  out += "<body style='background-color:transparent'>";
  
  out += "<p>";
  out += "Check Status: ";
  if (checkResult.wifi == CHECK_STATUS_BAD_TIMEOUT) {
    out += "No WiFi";
  } else if (checkResult.internet == CHECK_STATUS_BAD_TIMEOUT) {
    out += "No Internet";
  } else if (checkResult.group == CHECK_STATUS_BAD_TIMEOUT) {
    out += "Group down";
  } else if (checkResult.power == CHECK_STATUS_BAD_TIMEOUT) {
    out += "No Power";
  } else {
    out += "OK. <br/>Scanner angle: ";
    out += String((int)scannerAngle);
    out += "°";
  }
  out += "<br/>";
  out += "</p>";
  
  out += "<p>Battery Voltage: ";
  out += String(batteryVoltageAverage);
  out += " V</p>";
  
  out += "<p>MAC: ";
  out += mac;
  out += "</p>";
  
  out += "</body>";
  out += "</html>";
  server.send(200, "text/html", out);
}

void setupServer() {
  server.on("/", handleRoot);
  server.on("/", []() {handleRoot();} );
  server.on("/h", []() {
    Serial.println("handicap?"); 
//    handicapSignVisible = !handicapSignVisible;
    handleRoot(); 
   } );
  server.on("/d", []() {
    Serial.println("dark");  
//    displayShowTime = 20;
    handleRoot(); 
  } );
  server.on("/b", []() {
    Serial.println("dark"); 
//    displayShowTime = 60;
    handleRoot(); 
  } );
  server.begin();
  Serial.println("HTTP server started");
}


void setup() {
  Serial.begin(115200);
  Serial.println();
  
  playMelody();
  pinMode(SPEAKER_PIN, OUTPUT);

  display.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDRESS);
  display.clearDisplay();

  //beepAll();
//  for (int i = 0; i < 5; i++) {
//    beep(2000 +(i*500), 1000);
//    delay(10);
//  }

  readMac();

  readWifiNetworkConfigurations();
  
  onWiFiConnectedHandler = WiFi.onStationModeConnected(&onWiFiConnected);
  onWiFiDisconnectedHandler = WiFi.onStationModeDisconnected(&onWiFiDisconnected);
  onWiFiGotIpHandler = WiFi.onStationModeGotIP(&onWiFiGotIp);

  readMonitorConfigurations();
  
  pinger.OnEnd(onEndPinger);
  pinger.OnReceive(onOnePingResultPinger);
  
  setBadResultStatus(&checkResult.wifi);
  setBadResultStatus(&checkResult.internet);
  setBadResultStatus(&checkResult.group);
  setBadResultStatus(&checkResult.power);
  
  setupServer();
}

void loop() {
  delay(1); // must delay to avoid WiFi disconnects

  uint32_t t = millis();

  uint32_t secondsIncreased = 0;

  uint32_t incrementInterval = (t >= lastTimeIncrement ? t -lastTimeIncrement : lastTimeIncrement -t);
  if (incrementInterval > 1000) {
    uint32_t secondsInterval = incrementInterval /1000;
    lastTimeIncrement += secondsInterval *1000; 

    secondsIncreased = incrementInterval /1000;
    //Serial.printf("secondsIncreased a %d\n", secondsIncreased);
  }

  melodyPlayer.handle();
  
  button.poll();

  if (isWifiConnected()) {
    if (!otaInitialized) {
      setupOta();
    }
    
    ArduinoOTA.handle();
  }
  
  
  float sensorValue = analogRead(ADC_BATTERY_VCC_PIN);
//  Serial.print("sensor = ");
//  Serial.println(sensorValue);
  batteryVoltagesBuffer.push(sensorValue);
  
  float average = 0.0;
  for (unsigned char i = 0; i < batteryVoltagesBuffer.size(); i++) {
    average += batteryVoltagesBuffer[i];
  }
  average /= (float)batteryVoltagesBuffer.size();
  average = calculateVoltage(average);
  
  batteryVoltageAverage = average;
//  Serial.print("avg ");
//  Serial.println(batteryVoltageAverage);



  if (secondsIncreased) {
    //Serial.printf("secondsIncreased b %d\n", secondsIncreased);
    decreaseCheckPingCountdownAll(secondsIncreased);
  
    for (byte i = getMonitorStartIndex(); i < getAllocatedMonitorCount(); i++) {
      if (!wifiMonitors[i].checkPingCountdown) {
        wifiMonitors[i].online = false;
      }
    }

    for (byte i = getMonitorStartIndex(); i < getAllocatedMonitorCount(); i++) {
      if (!pinging) {
        if (pingWifiMonitorIndex == i) {
          if (!wifiMonitors[i].checkPingCountdown) {
            wifiMonitors[i].online = false;
            if (ensureWifiConnected()) {
              pingForMonitor(&wifiMonitors[i]);
            }
          } else {
            increasePingWifiMonitorIndex();
          }
        }
      }
    }
    
    
    for (byte i = getMonitorStartIndex(); i < getAllocatedMonitorCount(); i++) {
      if (wifiMonitors[i].online) {
        if (wifiMonitors[i].me) {
          setGoodResultStatus(&checkResult.internet);
        }
      } else {
        if (wifiMonitors[i].me) {
          setBadResultStatus(&checkResult.internet);
        } else {
          setBadResultStatus(&checkResult.group);
        }
        Serial.print("monitor offline ");
        serialPrint(&wifiMonitors[i].ip);
        Serial.println();
      }
    }
    if (isGroupOnline()) {
      setGoodResultStatus(&checkResult.group);
    }

    if (batteryVoltageAverage < BATTERY_VOLTAGE_MINIMUM) {
      setBadResultStatus(&checkResult.power);
    } else {
      setGoodResultStatus(&checkResult.power);
    }

    decreaseResultStatus(secondsIncreased);
    
    if (checkResult.wifi == CHECK_STATUS_BAD_TIMEOUT) {
      Serial.println("   ALARM: no wifi");
    } else if (checkResult.internet == CHECK_STATUS_BAD_TIMEOUT) {
      Serial.println("   ALARM: no internet");
    } else if (checkResult.group == CHECK_STATUS_BAD_TIMEOUT) {
      Serial.println("   ALARM: group down");
    } else if (checkResult.power == CHECK_STATUS_BAD_TIMEOUT) {
      Serial.println("   ALARM: no power");
    } 
  
  }

  if (button.released()) {
    if (checkResult.group == CHECK_STATUS_BAD_TIMEOUT) {
      Serial.println("will ignore group down");
      ignoreGroupDown = true;
      melodyPlayer.stop(groupDownMelody);
    }
  }

  display.clearDisplay();
  drawCheckResult();
  display.display();
  scannerAngle += 19;
  while (scannerAngle >= 360) {
    scannerAngle -= 360;
  }

  server.handleClient();
}

bool pingForMonitor(WifiMonitor *monitor) {
  Serial.printf("pingForMonitor ");
  serialPrint(&monitor->ip);
  Serial.println();
  if (!isWifiConnected()) {
    return connectWifi();
  }
  return monitor->me ? pingWebsites() : pingMonitor(monitor);
}

bool pingWebsites() {
  for (byte i = 0; i < (sizeof(websites) / HOSTNAME_LENGTH); i++) {
    Serial.printf("ping Website '%s'\n", websites[i]);
    if (pinger.Ping(websites[i])) {
      pinging = true;
      pingCountdown = PING_COUNT;
      return true;
    }
  }
  Serial.println("can not ping any website!");
  increasePingWifiMonitorIndex();
  pinging = false;
  return false;
}

bool pingMonitor(WifiMonitor *monitor) {
  Serial.printf("ping Monitor ");
  serialPrint(&monitor->ip);
  Serial.println();
  pinging = true;
  pingCountdown = PING_COUNT;
  bool r = pinger.Ping(monitor->ip);
  if (!r) {
    Serial.printf("can not ping Monitor ");
    serialPrint(&monitor->ip);
    Serial.println();
    increasePingWifiMonitorIndex();
    pinging = false;
  }
  return r;
}

void decreaseCheckPingCountdownAll(uint32_t secondsIncreased) {
  for (byte i = getMonitorStartIndex(); i < getAllocatedMonitorCount(); i++) {
    if (wifiMonitors[i].checkPingCountdown) {
      wifiMonitors[i].checkPingCountdown -= secondsIncreased;
      if (wifiMonitors[i].checkPingCountdown < 0) {
        wifiMonitors[i].checkPingCountdown = 0;
      }
      if (!wifiMonitors[i].checkPingCountdown) {
        wifiMonitors[i].online = false;
      }
    }
  }
}

byte getMonitorStartIndex() {
  return (meDefined ? 1 : 0);
}

byte getMonitorCountEffective() {
  return getAllocatedMonitorCount() -getMonitorStartIndex();
}

void increasePingWifiMonitorIndex() {
  pingWifiMonitorIndex++;
  pingWifiMonitorIndex %= getAllocatedMonitorCount();
  if (meDefined && !pingWifiMonitorIndex) {
    pingWifiMonitorIndex++;
  }
  
  //Serial.printf("increasePingWifiMonitorIndex %d\n", pingWifiMonitorIndex);
}

void setAlarm() {
  if (checkResult.wifi == CHECK_STATUS_BAD_TIMEOUT) {
    melodyPlayer.play(4, noWifiMelody, sizeof(noWifiMelody)/sizeof(MelodyPart));
  } else if (checkResult.internet == CHECK_STATUS_BAD_TIMEOUT) {
    melodyPlayer.play(3, noInternetMelody, sizeof(noInternetMelody)/sizeof(MelodyPart));
  } else if (checkResult.group == CHECK_STATUS_BAD_TIMEOUT) {
    if (!ignoreGroupDown) {
      melodyPlayer.play(2, groupDownMelody, sizeof(groupDownMelody)/sizeof(MelodyPart));
    } else {
      melodyPlayer.stop();
    }
  } else if (checkResult.power == CHECK_STATUS_BAD_TIMEOUT) {
    melodyPlayer.play(1, noPowerMelody, sizeof(noPowerMelody)/sizeof(MelodyPart));
  }
}

void setGoodResultStatus(int* status) {
  if (*status != CHECK_STATUS_BAD_TIMEOUT) {
    *status = CHECK_STATUS_OK;
  }
//  Serial.print("setGoodResultStatus new status: ");
//  Serial.println(*status);
}

void setBadResultStatus(int* status) {
  if (*status == CHECK_STATUS_OK) {
    *status = BAD_STATUS_COUNTDOWN;
  }
//  Serial.print("setBadResultStatus new status: ");
//  Serial.println(*status);
}

void decreaseResultStatus(int* status, uint32_t secondsIncreased) {
  bool needSetAlarm = false;
  if (*status != CHECK_STATUS_OK) {
    if (*status == 1) {
      needSetAlarm = true;
    }
    *status -= secondsIncreased;
    if (*status < 0) {
      *status = 0;
    }
  }
  if (needSetAlarm) {
    setAlarm();
  }
}

void decreaseResultStatus(uint32_t secondsIncreased) {
  decreaseResultStatus(&checkResult.wifi, secondsIncreased);
  decreaseResultStatus(&checkResult.internet, secondsIncreased);
  decreaseResultStatus(&checkResult.group, secondsIncreased);
  decreaseResultStatus(&checkResult.power, secondsIncreased);
}


void serialPrint(IPAddress *ip) {
  Serial.print((*ip)[0]);
  Serial.print(".");
  Serial.print((*ip)[1]);
  Serial.print(".");
  Serial.print((*ip)[2]);
  Serial.print(".");
  Serial.print((*ip)[3]);
}


WifiMonitor* findMonitor(IPAddress ip) {
  for (byte i = getMonitorStartIndex(); i < getAllocatedMonitorCount(); i++) {
    if (wifiMonitors[i].ip == ip) {
      return &wifiMonitors[i];
    }
  }
  return NULL;
}

WifiMonitor* findMonitorOrMe(IPAddress ip) {
  WifiMonitor* r = findMonitor(ip);
  if (!r) {
    r = me;
  }
  if (!r) {
    Serial.printf("ERROR: can not detect any monitor for ");
    serialPrint(&ip);
    Serial.println();
  }
  return r;
}

void setMe(IPAddress ip) {
  meDefined = false;
  for (byte i = 1; i < getAllocatedMonitorCount(); i++) {
    wifiMonitors[i].me = (wifiMonitors[i].ip == ip);
    if (wifiMonitors[i].me) {
      Serial.printf("found me defined\n");
      me = &wifiMonitors[i];
      meDefined = true;
    }
  }
  if (!meDefined) {
    Serial.printf("me undefined\n");
    me = &wifiMonitors[0];
    me->ip = ip;
  }
}

void setOnline(IPAddress ip) {
  Serial.printf("resetOfflineCountdown ");
  serialPrint(&ip);
  Serial.println();
  for (byte i = getMonitorStartIndex(); i < getAllocatedMonitorCount(); i++) {
    if (wifiMonitors[i].ip == ip) {
      wifiMonitors[i].online = true;
    }
  }
}

bool isGroupOnline() {
  for (byte i = getMonitorStartIndex(); i < getAllocatedMonitorCount(); i++) {
    if (wifiMonitors[i].me) {
      continue;
    }
    if (!wifiMonitors[i].online) {
      return false;
    }
  }
  return true;
}

float calculateVoltage(float adcValue) {
  return adcValue *VOLTAGE_DIVIDER /1023.0;
}

void beep(int frequency) {
  pinMode(SPEAKER_PIN, OUTPUT);
  analogWriteFreq(frequency);
  analogWrite(SPEAKER_PIN, frequency /2);
}

void beep(int frequency, int period) {
  pinMode(SPEAKER_PIN, OUTPUT);
  analogWriteFreq(frequency);
  analogWrite(SPEAKER_PIN, 511);
  delay(period);
  analogWrite(SPEAKER_PIN, 0);
  pinMode(SPEAKER_PIN, INPUT);
}

void beepAll() {
  for (int i = 0; i < 80; i++) {
    beep(0 +(i*100), 200);
    delay(10);
  }
}

void drawScannerLine(float angle) {
  while (angle < 0) {
    angle += 360;
  }
  int xCenter = display.width() /2;
  int yCenter = display.height() /2;
  int length = xCenter < yCenter ? xCenter : yCenter;
  drawLineAtAngle(xCenter, yCenter, angle, length);
}

void drawScannerPixel(float angle) {
  while (angle < 0) {
    angle += 360;
  }
  int xCenter = display.width() /2;
  int yCenter = display.height() /2;
  int length = xCenter < yCenter ? xCenter : yCenter;
  drawPixelAtAngle(xCenter, yCenter, angle, length);
}

void drawScanner() {
  drawScannerLine(scannerAngle);
  drawScannerLine(scannerAngle -2);
  drawScannerLine(scannerAngle -4);
  drawScannerLine(scannerAngle -6);

  for (int i = 0; i < 6; i++) {
    drawScannerPixel(scannerAngle -6 -(i +1)*5);
  }
}

void drawWarningText(char *text) {
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(text);
}

void drawMyIp(const char *text) {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(15, 5);
  display.println(text);
}

void drawBatteryVoltage(const char *text) {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(15, display.height()-10);
  display.println(text);
}

char lcdText[50];

void drawMyIp() {
  if (WiFi.status() == WL_CONNECTED) {
    lcdText[0] = '.';
    itoa(WiFi.localIP()[3], &lcdText[1], 10);
    drawMyIp(lcdText);
  }
}

void drawBatteryVoltage() {
    lcdText[0] = '.';
    sprintf(lcdText, "%.2f V", batteryVoltageAverage);
    drawBatteryVoltage(lcdText);
}

void drawCheckResult() {
  if (checkResult.wifi == CHECK_STATUS_BAD_TIMEOUT) {
    drawWarningText("No\nWiFi");
  } else if (checkResult.internet == CHECK_STATUS_BAD_TIMEOUT) {
    drawWarningText("No\nInternet");
  } else if (checkResult.group == CHECK_STATUS_BAD_TIMEOUT) {
    drawWarningText("Group\ndown");
    if (ignoreGroupDown) {
      drawScanner();
    }
  } else if (checkResult.power == CHECK_STATUS_BAD_TIMEOUT) {
    drawWarningText("No\nPower");
    drawBatteryVoltage();
  } else {
    drawScanner();
    drawMyIp();
    drawBatteryVoltage();
  }
}


void onWiFiConnected(const WiFiEventStationModeConnected& event) {
  Serial.print("[WiFi Event] Connected to AP: ");
  Serial.println(event.ssid);
  WiFi.hostname(String("ToI-") + String(DEVICE_CLASS) + String("-") + String(mac));
  wifiStatus = WL_CONNECTED;
  setGoodResultStatus(&checkResult.wifi);
}
 
void onWiFiDisconnected(const WiFiEventStationModeDisconnected& event) {
  wifiStatus = WL_DISCONNECTED;
  Serial.print("[WiFi Event] Disconnect from AP: ");
  Serial.println(event.ssid);
  otaInitialized = false;
  setBadResultStatus(&checkResult.wifi);
  setMe(NULL);
  pingWifiMonitorIndex = getMonitorStartIndex();
}
 
void onWiFiGotIp(const WiFiEventStationModeGotIP& event) {
  Serial.print("[WiFi Event] Got IP: ");
  Serial.println(event.ip);
  setMe(event.ip);

  if (getMonitorCountEffective() == 1) { // we are the only one in the group
    Serial.println("we are the only one in the group");
    if (checkResult.group != CHECK_STATUS_BAD_TIMEOUT) {
      checkResult.group = CHECK_STATUS_OK;
    }
  }
  
  pingWifiMonitorIndex = getMonitorStartIndex();
}

//-------


void drawLineAtAngle(int xStart, int yStart, float angle, int length) {
  float angleRadians = angle *DEG_TO_RAD;
  int xEnd = xStart + length *sin(angleRadians);
  int yEnd = yStart - length *cos(angleRadians);
  display.drawLine(xStart, yStart, xEnd, yEnd, WHITE);
}

void drawPixelAtAngle(int xStart, int yStart, float angle, int length) {
  float angleRadians = angle *DEG_TO_RAD;
  int xEnd = xStart + length *sin(angleRadians);
  int yEnd = yStart - length *cos(angleRadians);
  display.drawPixel(xEnd, yEnd, WHITE);
}
