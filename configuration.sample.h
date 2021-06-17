 
WifiNetworkConfiguration wifiNetworkConfigurations[] = {
  {"org", "openwireless"},
  {"doesnotexist", "badcode"}
};

char monitorIps[][IP_ADDRESS_LENGTH] = {
  "192.168.1.45\0", 
  "192.168.1.46\0"
};

char websites[][HOSTNAME_LENGTH] = {
  "www.delfi.lt\0",
  "google.lt\0"
};


#define WEBPAGE_RELOAD_INTERVAL_SECONDS 3


#define BAD_STATUS_COUNTDOWN 60
#define CHECK_PING_COUNTDOWN 30
#define PING_COUNT 4

#define BATTERY_VOLTAGE_MINIMUM 4.00


#define NOPRINT_DEBUG



#define BUTTON_PIN D3


#define SPEAKER_PIN D5

#define VOLTAGE_DIVIDER (5.2)

#define ADC_BATTERY_VCC_PIN A0


#define SSD1306_I2C_ADDRESS 0x3C
#define SSD1306_64_48
#define SSD1306_LCDWIDTH 64
#define SSD1306_LCDHEIGHT 48

#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16
