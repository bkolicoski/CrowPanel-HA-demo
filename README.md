# CrowPanel ESP32 1.28" Display Demo Project

This example demonstrates how you can use the CrowPanel 1.28" round display with Home Assistant via MQTT to display a gauge with temperature and humidity display comming in from Home Assitant. 

- [Watch the Video](https://www.youtube.com/watch?v=t8tVZexVKgg)
- [Website Article](https://www.tastethecode.com/exploring-the-potential-of-the-crowpanel-esp32-128-display-for-diy-projects)

The secrets.h file should have the following content:

```
#define WIFI_SSID "Your WIFI"
#define WIFI_PASSWORD "your_password"
#define mqtt_username "your_mqtt_username"
#define mqtt_password "your_mqtt_password"
```


## Troubleshooting

The example was built and working with ESP32 board version 2.0.10
Please note that at this moment, I was unable to use any newer version and make it work. 

Libraries versions: 
LovyanGFX - 1.1.5
LVGL - 8.3.0-dev

If you encounter issues:
- Ensure your environment uses compatible library versions, as newer releases may not yet support all features.
- Verify MQTT and WiFi settings are correctly configured and the display is subscribed to the appropriate topics.
