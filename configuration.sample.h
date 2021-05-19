 
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


#define BAD_STATUS_COUNTDOWN 60
#define CHECK_PING_COUNTDOWN 30
#define PING_COUNT 4

#define NOPRINT_DEBUG



#define SPEAKER_PIN D3
