# smsforwarder-esp32
a sms forwarder based on esp32 and Air780E, coding and compiled in arduino

### supported hardware

**esp32 board**: Lolin32 lite

**4g board**: Air780X

### features

The device can function both as a Soft AP and maintain a concurrent Wi-Fi connection. Users have the ability to connect to the AP and utilize a web browser to navigate to http://192.168.4.1, where they can configure Wi-Fi settings such as SSID and password, along with other system preferences.

Upon establishing a Wi-Fi connection and ensuring the LTE network is active, the device is designed to relay incoming SMS messages directly to your Bark application on iOS (with potential future support for additional targets) via Wi-Fi.