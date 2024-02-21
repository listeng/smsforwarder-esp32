#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <HardwareSerial.h>
#include <pdulib.h>
#include <UrlEncode.h>

#define LED 22

const int RXPin = 16;
const int TXPin = 17;
const long baudRate = 115200;

String deviceName = "esp32";

String sta_ssid = "";
String sta_password = "";
String barkKey = "";

const char* ap_ssid = "sms-forward";
const char* ap_password = "123456789";
const char* bark_url = "https://api.day.app";

bool firstConn = true;

Preferences preferences;
HardwareSerial SerialAT(2);
WebServer server(80);
PDU mypdu = PDU(500);

int sendMsg(String msg) {
  if (barkKey == "") {
    Serial.println("Bark key is empty!");
    return -99;
  }

  HTTPClient http;

  char buffer[1000];
  snprintf(buffer, sizeof(buffer), "%s/%s/%s/",
           bark_url, barkKey.c_str(), urlEncode(msg).c_str());

  Serial.println(buffer);
  http.begin(buffer);

  int httpCode = http.GET();

  http.end();

  return httpCode;
}

void handleIp() {
  if (WiFi.status() == WL_CONNECTED) {
    // 当连接到WiFi时，发送本地IP地址
    server.send(200, "text/plain", WiFi.localIP().toString());
  } else {
    // 如果没有连接到WiFi，发送错误消息
    server.send(200, "text/plain", "Not connected to a WiFi network");
  }
}

// 处理/wifi路径的访问
void handleWifi() {
  sta_ssid = server.arg("ssid"); // 从HTTP请求中获取ssid参数
  sta_password = server.arg("pwd"); // 从HTTP请求中获取pwd参数

  // 响应请求者
  String response = "Setting SSID to: " + sta_ssid + " and PASSWORD to: " + sta_password;
  server.send(200, "text/plain", response);

  // 将SSID和密码存储到Preferences
  preferences.begin("wifi", false); // "wifi"是Preferences的命名空间
  preferences.putString("ssid", sta_ssid);
  preferences.putString("password", sta_password);
  preferences.end();

  firstConn = true;

  // 如果SSID和密码都不为空，则尝试连接到WiFi
  if (sta_ssid != "" && sta_password != "") {
    connectToWiFi(sta_ssid.c_str(), sta_password.c_str());
  }
}

void handleTest() {
  String msg = "Hello from ESP32! IP: " + WiFi.localIP().toString();

  int httpCode = sendMsg(msg);

  // 检查请求是否成功
  if (httpCode > 0) {
    // 请求成功，获取响应内容
    // String payload = http.getString();
    // 将结果返回给请求者
    server.send(200, "text/plain", "ok");
  } else {
    // 请求失败，发送错误信息
    server.send(500, "text/plain", "Server Error: " + String(httpCode));
  }

  // 关闭HTTP连接

}

void handleConfig() {
  String bark_key = server.arg("bark_key");

  preferences.begin("config", false);
  preferences.putString("bark_key", bark_key);
  preferences.end();

  barkKey = bark_key;

  server.send(200, "text/plain", "ok");
}

void handlePhoneNumber() {
  String tel = server.arg("tel");
  String msg = server.arg("msg");

  if (tel == "") {
    tel = "2020";
  }

  if (msg == "") {
    msg = "number";
  }

  int len = mypdu.encodePDU(tel.c_str(), msg.c_str());

  Serial.println("Sending message...");

  SerialAT.println("AT+CMGF=0"); // 发送AT命令，设置为pdu模式
  while (SerialAT.available() == 0) {
  }
  String ATData = SerialAT.readString();
  Serial.println(ATData);

  // 发送短信
  SerialAT.printf("AT+CMGS=%d\r", len);
  while (SerialAT.available() == 0) {
  }
  ATData = SerialAT.readString();
  Serial.println(ATData);

  SerialAT.print(mypdu.getSMS());

  Serial.println("Message sent!");

  server.send(200, "text/plain", "ok");
}

void connectToWiFi(const char * ssid, const char * pwd) {
  Serial.println("WiFi connect task started.");
  WiFi.begin(ssid, pwd);
}

void setup() {
  Serial.begin(115200);

  SerialAT.begin(baudRate, SERIAL_8N1, RXPin, TXPin); // 配置硬件串行端口

  pinMode(LED, OUTPUT);
  firstConn = true;

  // 配置并启动WiFi
  WiFi.mode(WIFI_AP_STA); // 同时设置为AP和STA模式

  // 启动AP模式
  Serial.println("Setting up AP...");
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // 定义路由
  server.on("/ip", handleIp);
  server.on("/wifi", handleWifi);
  server.on("/config", handleConfig);
  server.on("/test", handleTest);
  server.on("/phone", handlePhoneNumber);

  // 开始服务器
  server.begin();
  Serial.println("HTTP server started");

  // 从Preferences读取保存的SSID和密码
  preferences.begin("wifi", true);
  sta_ssid = preferences.getString("ssid", ""); // 如果没有值则返回空字符串
  sta_password = preferences.getString("password", "");

  if (sta_ssid != "" && sta_password != "") {
    connectToWiFi(sta_ssid.c_str(), sta_password.c_str());
  }
  preferences.end();

  preferences.begin("config", true);
  barkKey = preferences.getString("bark_key", "");
  deviceName = preferences.getString("device_name", "esp32");
  preferences.end();

  // 短信设置成pdu模式
  SerialAT.println("AT+CMGF=0");
}

void loop() {
  String msgIndex = "";

  // 处理客户端的请求
  server.handleClient();

  // 检查WiFi连接状态
  if (WiFi.status() == WL_CONNECTED) {
    // 连接成功

    if (firstConn) {
      String msg = "Hello from ESP32! IP: " + WiFi.localIP().toString();

      Serial.print("WIFI IP address: ");
      Serial.println(WiFi.localIP());

      sendMsg(msg);
      firstConn = false;
      digitalWrite(LED, LOW);

      SerialAT.println("AT+CGREG?");
    }

  } else {
    digitalWrite(LED, HIGH);
    firstConn = true;
  }

  // 检查SerialAT端口是否有数据
  if (SerialAT.available()) {
    String ATData = SerialAT.readStringUntil('\n');

    if (ATData.startsWith("+CIEV:")) {
      // 收到事件信息
      if (ATData.indexOf("\"MESSAGE\"") > 0) {
        // 收到短信
        SerialAT.readStringUntil('\n');

        String smsInfo = SerialAT.readStringUntil('\n');
        if (smsInfo.startsWith("+CMT:")) {
          // 读取短信
          String smsData = SerialAT.readStringUntil('\n');

          mypdu.decodePDU(smsData.c_str());

          char buffer[500];
          snprintf(buffer, sizeof(buffer), "短信 - %s<br/>来自：%s<br/>时间：%s<br/>设备：%s",
                   mypdu.getText(), mypdu.getSender(), mypdu.getTimeStamp(), deviceName);

          Serial.print("sms data: ");
          Serial.println(buffer);

          sendMsg(buffer);

          String smsMsg = mypdu.getText();
          if (smsMsg.startsWith("Your number is ")) {
            deviceName = smsMsg.substring(15);

            preferences.begin("config", false);
            preferences.putString("device_name", deviceName);
            preferences.end();
          }
        }
      }


    } else if (ATData.startsWith("+CGREG: 0,5") || ATData.startsWith("+CGREG: 0,2")) {
      Serial.println("GSM net ok - " + deviceName);
      sendMsg("GSM net ok - " + deviceName);
    } else {
      Serial.println("GSM module data: " + ATData);
    }

  }
}
