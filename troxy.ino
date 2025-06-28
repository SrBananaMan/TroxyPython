#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>

namespace Helpers {
  void printPrompt(const String& label) {
    Serial.print(label);
    Serial.flush();
  }

  void readLine(String& output, bool hideInput = false) {
    output = "";
    while (true) {
      if (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') break;
        if (hideInput) continue;
        output += c;
      }
      delay(1);
    }
    output.trim();
  }

  bool isDiscoveryPacket(const uint8_t* buf, size_t len) {
    return len >= 4 && buf[0] == 0xf2 && buf[1] == 0x03 && buf[2] == 0x00 && buf[3] == 0x00;
  }
}

struct Config {
  Preferences prefs;
  const char* namespaceName = "net";
  
  String ssid;
  String password;
  String serverIp;
  uint16_t serverPort;
  IPAddress staticIP;
  bool useStaticIP = false;
  bool darkMode = true;
  
  const uint16_t port8888 = 8888;
  const uint16_t port7777 = 7777;
  const uint32_t BROADCAST_INTERVAL = 5000;
  const uint32_t TCP_TIMEOUT = 30000;
  const uint32_t RECONNECT_DELAY = 1000;
  const uint32_t SETUP_TIMEOUT = 5000;
  const uint32_t SERVER_CHECK_INTERVAL = 10000;
} config;

struct NetworkObjects {
  WiFiUDP udp8888;
  WiFiUDP udp7777;
  WiFiUDP udpOut;
  WiFiServer tcpServer{config.port7777};
  WebServer webServer{80};
  
  const uint8_t responsePacket[41] = {
    0xf2, 0x03, 0x00, 0x00,
    0x61, 0x1e, 0x00, 0x00,
    0x08, 'T','e','r','r','e','r','i','a',
    0x0d, 'T','e','r','r','e','r','i','a','P','r','o','x','y',
    0x68,
    0x10, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x10, 0x00, 0x00
  };

  bool wifiConnected = false;
  bool inAPMode = false;
  bool serverReachable = false;
  String serverStatus = "Unknown";
} net;

// Timing
struct Timing {
  uint32_t lastBroadcast = 0;
  uint32_t lastReconnectAttempt = 0;
  uint32_t lastWebHandle = 0;
  uint32_t lastServerCheck = 0;
} timing;

const char serverConfigPage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <title>Terraria Proxy</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
  <style>
    :root {
      --primary: #6c5ce7;
      --primary-dark: #5d4bdb;
      --success: #00b894;
      --danger: #d63031;
      --text-light: #f8f9fa;
      --text-dark: #2d3436;
      --bg-light: #ffffff;
      --bg-dark: #121212;
      --card-bg-light: #ffffff;
      --card-bg-dark: #1e1e1e;
      --input-bg-light: #ffffff;
      --input-bg-dark: #2d2d2d;
      --border-light: #e0e0e0;
      --border-dark: #333333;
    }
    
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      margin: 0;
      padding: 20px;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      background: var(--bg-dark);
      color: var(--text-light);
      transition: all 0.3s ease;
    }
    
    .card {
      background: var(--card-bg-dark);
      border-radius: 15px;
      box-shadow: 0 15px 35px rgba(0,0,0,0.2);
      width: 100%;
      max-width: 600px;
      overflow: hidden;
      transition: all 0.3s ease;
    }
    
    .card-header {
      background: linear-gradient(135deg, var(--primary) 0%, var(--primary-dark) 100%);
      color: white;
      padding: 25px;
      text-align: center;
    }
    
    .card-body {
      padding: 30px;
    }
    
    .form-group {
      margin-bottom: 25px;
    }
    
    label {
      display: block;
      margin-bottom: 8px;
      font-weight: 600;
      color: var(--text-light);
    }
    
    input {
      width: 100%;
      padding: 14px 20px;
      background: var(--input-bg-dark);
      border: 2px solid var(--border-dark);
      border-radius: 10px;
      font-size: 16px;
      transition: all 0.3s;
      color: var(--text-light);
    }
    
    input:focus {
      border-color: var(--primary);
      box-shadow: 0 0 0 4px rgba(108, 92, 231, 0.2);
      outline: none;
    }
    
    button {
      background: var(--primary);
      color: white;
      border: none;
      padding: 15px;
      width: 100%;
      border-radius: 10px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.3s;
      margin-top: 10px;
    }
    
    button:hover {
      background: var(--primary-dark);
      transform: translateY(-3px);
      box-shadow: 0 10px 20px rgba(0,0,0,0.2);
    }
    
    .status-container {
      margin: 25px 0;
    }
    
    .status {
      padding: 15px 20px;
      border-radius: 10px;
      text-align: center;
      font-weight: 600;
      margin-bottom: 15px;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    
    .status i {
      margin-right: 10px;
    }
    
    .online {
      background-color: rgba(0, 184, 148, 0.1);
      color: var(--success);
      border: 2px solid var(--success);
    }
    
    .offline {
      background-color: rgba(214, 48, 49, 0.1);
      color: var(--danger);
      border: 2px solid var(--danger);
    }
    
    .info-box {
      background-color: rgba(108, 92, 231, 0.1);
      color: var(--primary);
      border: 2px solid var(--primary);
      padding: 15px;
      border-radius: 10px;
      margin-bottom: 25px;
      font-size: 14px;
    }
    
    .connection-details {
      display: flex;
      justify-content: space-between;
      margin-top: 20px;
      font-size: 14px;
    }
    
    .connection-details span {
      background: rgba(255,255,255,0.1);
      padding: 8px 12px;
      border-radius: 20px;
    }
    
    .theme-switch {
      position: absolute;
      top: 20px;
      right: 20px;
    }
    
    .theme-switch input {
      display: none;
    }
    
    .theme-switch label {
      cursor: pointer;
      display: flex;
      align-items: center;
    }
    
    .theme-switch .slider {
      width: 50px;
      height: 24px;
      background: #555;
      border-radius: 12px;
      position: relative;
      margin-left: 10px;
      transition: background 0.3s;
    }
    
    .theme-switch .slider:before {
      content: '';
      position: absolute;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: white;
      top: 2px;
      left: 2px;
      transition: transform 0.3s;
    }
    
    .theme-switch input:checked + .slider {
      background: var(--primary);
    }
    
    .theme-switch input:checked + .slider:before {
      transform: translateX(26px);
    }
    
    body.light-mode {
      background: var(--bg-light);
      color: var(--text-dark);
    }
    
    body.light-mode .card {
      background: var(--card-bg-light);
    }
    
    body.light-mode input {
      background: var(--input-bg-light);
      border-color: var(--border-light);
      color: var(--text-dark);
    }
    
    body.light-mode label {
      color: var(--text-dark);
    }
    
    body.light-mode .connection-details span {
      background: rgba(0,0,0,0.05);
    }
  </style>
</head>
<body class="%THEMEMODE%">
  <div class="theme-switch">
    <input type="checkbox" id="theme-toggle" %THEMECHECKED%>
    <label for="theme-toggle">
      Theme
      <span class="slider"></span>
    </label>
  </div>
  
  <div class="card">
    <div class="card-header">
      <h1>Terraria Proxy</h1>
      <p>Configure your server settings</p>
    </div>
    <div class="card-body">
      <div class="info-box">
        INFO: Enter your Terraria server details below
      </div>
      
      <form action="/saveserver" method="POST">
        <div class="form-group">
          <label for="serverip">Server IP Address</label>
          <input type="text" id="serverip" name="serverip" value="%SERVERIP%" placeholder="192.168.1.100" required>
        </div>
        <div class="form-group">
          <label for="serverport">Server Port</label>
          <input type="number" id="serverport" name="serverport" value="%SERVERPORT%" placeholder="7777" required>
        </div>
        <input type="hidden" name="darkmode" id="darkmode" value="%THEMEMODE%">
        <button type="submit">Save Settings</button>
      </form>
      
      <div class="status-container">
        <div class="status %STATUSCLASS%">
          <i>%STATUSICON%</i>
          %STATUS%
        </div>
        <div class="connection-details">
          <span>Proxy IP: %PROXYIP%</span>
          <span>WiFi: %WIFISTATUS%</span>
        </div>
      </div>
    </div>
  </div>
  
  <script>
    const themeToggle = document.getElementById('theme-toggle');
    const darkModeInput = document.getElementById('darkmode');
    
    themeToggle.addEventListener('change', function() {
      document.body.classList.toggle('light-mode');
      darkModeInput.value = this.checked ? 'light-mode' : '';
    });
    
    document.querySelector('form').addEventListener('submit', function(e) {
      const button = this.querySelector('button');
      button.disabled = true;
      button.innerHTML = 'Saving...';
    });
  </script>
</body>
</html>
)=====";

const char wifiConfigPage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <title>WiFi Configuration</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
  <style>
    :root {
      --primary: #4a6fa5;
      --primary-dark: #3a5a8f;
      --text-light: #f8f9fa;
      --text-dark: #343a40;
      --bg-light: #ffffff;
      --bg-dark: #121212;
      --card-bg-light: #ffffff;
      --card-bg-dark: #1e1e1e;
      --input-bg-light: #ffffff;
      --input-bg-dark: #2d2d2d;
      --border-light: #e0e0e0;
      --border-dark: #333333;
    }
    
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: var(--bg-dark);
      margin: 0;
      padding: 20px;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      color: var(--text-light);
      transition: all 0.3s ease;
    }
    
    .card {
      background: var(--card-bg-dark);
      border-radius: 15px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.2);
      width: 100%;
      max-width: 500px;
      overflow: hidden;
      transition: all 0.3s ease;
    }
    
    .card-header {
      background: var(--primary);
      color: white;
      padding: 25px;
      text-align: center;
    }
    
    .card-body {
      padding: 30px;
    }
    
    .form-group {
      margin-bottom: 20px;
    }
    
    label {
      display: block;
      margin-bottom: 8px;
      font-weight: 600;
      color: var(--text-light);
    }
    
    input {
      width: 100%;
      padding: 12px 15px;
      background: var(--input-bg-dark);
      border: 2px solid var(--border-dark);
      border-radius: 8px;
      font-size: 16px;
      transition: all 0.3s;
      color: var(--text-light);
    }
    
    input:focus {
      border-color: var(--primary);
      box-shadow: 0 0 0 3px rgba(74, 111, 165, 0.2);
      outline: none;
    }
    
    button {
      background: var(--primary);
      color: white;
      border: none;
      padding: 14px;
      width: 100%;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.3s;
    }
    
    button:hover {
      background: var(--primary-dark);
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(0,0,0,0.2);
    }
    
    .theme-switch {
      position: absolute;
      top: 20px;
      right: 20px;
    }
    
    .theme-switch input {
      display: none;
    }
    
    .theme-switch label {
      cursor: pointer;
      display: flex;
      align-items: center;
    }
    
    .theme-switch .slider {
      width: 50px;
      height: 24px;
      background: #555;
      border-radius: 12px;
      position: relative;
      margin-left: 10px;
      transition: background 0.3s;
    }
    
    .theme-switch .slider:before {
      content: '';
      position: absolute;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: white;
      top: 2px;
      left: 2px;
      transition: transform 0.3s;
    }
    
    .theme-switch input:checked + .slider {
      background: var(--primary);
    }
    
    .theme-switch input:checked + .slider:before {
      transform: translateX(26px);
    }
    
    body.light-mode {
      background: var(--bg-light);
      color: var(--text-dark);
    }
    
    body.light-mode .card {
      background: var(--card-bg-light);
    }
    
    body.light-mode input {
      background: var(--input-bg-light);
      border-color: var(--border-light);
      color: var(--text-dark);
    }
    
    body.light-mode label {
      color: var(--text-dark);
    }
  </style>
</head>
<body class="%THEMEMODE%">
  <div class="theme-switch">
    <input type="checkbox" id="theme-toggle" %THEMECHECKED%>
    <label for="theme-toggle">
      Theme
      <span class="slider"></span>
    </label>
  </div>
  
  <div class="card">
    <div class="card-header">
      <h1>WiFi Configuration</h1>
    </div>
    <div class="card-body">
      <form action="/savewifi" method="POST">
        <div class="form-group">
          <label for="ssid">WiFi SSID</label>
          <input type="text" id="ssid" name="ssid" placeholder="Your WiFi Network" required>
        </div>
        <div class="form-group">
          <label for="password">WiFi Password</label>
          <input type="password" id="password" name="password" placeholder="Your WiFi Password">
        </div>
        <div class="form-group">
          <label for="staticip">Static IP (optional)</label>
          <input type="text" id="staticip" name="staticip" placeholder="Leave blank for DHCP">
        </div>
        <input type="hidden" name="darkmode" id="darkmode" value="%THEMEMODE%">
        <button type="submit">Save & Reboot</button>
      </form>
    </div>
  </div>
  
  <script>
    const themeToggle = document.getElementById('theme-toggle');
    const darkModeInput = document.getElementById('darkmode');
    
    themeToggle.addEventListener('change', function() {
      document.body.classList.toggle('light-mode');
      darkModeInput.value = this.checked ? 'light-mode' : '';
    });
  </script>
</body>
</html>
)=====";

namespace ConfigManager {
  void saveSettings() {
    config.prefs.begin(config.namespaceName, false);
    config.prefs.putString("ssid", config.ssid);
    config.prefs.putString("pass", config.password);
    config.prefs.putString("srvip", config.serverIp);
    config.prefs.putUShort("srvport", config.serverPort);
    config.prefs.putBool("use_static", config.useStaticIP);
    config.prefs.putBool("dark_mode", config.darkMode);
    if (config.useStaticIP) {
      config.prefs.putString("static_ip", config.staticIP.toString());
    }
    config.prefs.end();
  }

  void loadSettings() {
    config.prefs.begin(config.namespaceName, true);
    config.ssid = config.prefs.getString("ssid", "");
    config.password = config.prefs.getString("pass", "");
    config.serverIp = config.prefs.getString("srvip", "");
    config.serverPort = config.prefs.getUShort("srvport", 7777);
    config.useStaticIP = config.prefs.getBool("use_static", false);
    config.darkMode = config.prefs.getBool("dark_mode", true);
    if (config.useStaticIP) {
      config.staticIP.fromString(config.prefs.getString("static_ip", ""));
    }
    config.prefs.end();
  }
}

namespace NetOps {
  bool checkServerConnection() {
    WiFiClient client;
  
    IPAddress serverIp;
    if (!serverIp.fromString(config.serverIp)) {
      if (!WiFi.hostByName(config.serverIp.c_str(), serverIp)) {
        Serial.printf("DNS lookup failed for: %s\n", config.serverIp.c_str());
        net.serverReachable = false;
        net.serverStatus = "✗ DNS Failed";
        return false;
      }
      Serial.printf("Resolved %s to %s\n", config.serverIp.c_str(), serverIp.toString().c_str());
    } else {
      serverIp.fromString(config.serverIp);
    }
  
    if (client.connect(serverIp, config.serverPort)) {
      client.stop();
      net.serverReachable = true;
      net.serverStatus = "✓ Server Online";
      return true;
    }
    
    net.serverReachable = false;
    net.serverStatus = "✗ Server Offline";
    return false;
  }


  void updateServerStatus() {
    uint32_t now = millis();
    if (now - timing.lastServerCheck >= config.SERVER_CHECK_INTERVAL) {
      timing.lastServerCheck = now;
      checkServerConnection();
    }
  }

  void sendBroadcast() {
    net.udp8888.beginPacket("255.255.255.255", config.port8888);
    net.udp8888.write(net.responsePacket, sizeof(net.responsePacket));
    net.udp8888.endPacket();
    Serial.println("[+] Sent LAN discovery broadcast.");
  }
  
  void forwardUdp(WiFiUDP& udp, const char* label) {
    int packetSize = udp.parsePacket();
    if (packetSize <= 0) return;
  
    IPAddress clientIp = udp.remoteIP();
    uint16_t clientPort = udp.remotePort();
    uint8_t buf[512];
    int len = udp.read(buf, sizeof(buf));
  
    if (Helpers::isDiscoveryPacket(buf, len)) {
      udp.beginPacket(clientIp, clientPort);
      udp.write(net.responsePacket, sizeof(net.responsePacket));
      udp.endPacket();
      Serial.printf("Discovery from %s:%d\n", clientIp.toString().c_str(), clientPort);
      return;
    }
  
    if (!net.serverReachable) {
      Serial.println("Server unreachable - dropping packet");
      return;
    }
 
    IPAddress targetIp;
    if (!targetIp.fromString(config.serverIp)) {
      if (!WiFi.hostByName(config.serverIp.c_str(), targetIp)) {
        Serial.printf("DNS lookup failed for: %s\n", config.serverIp.c_str());
        return;
      }
    }
    
    net.udpOut.beginPacket(targetIp, config.serverPort);
    net.udpOut.write(buf, len);
    if (!net.udpOut.endPacket()) {
      Serial.println("Failed to send UDP packet to server");
      return;
    }
  
    if (net.udpOut.parsePacket() > 0) {
      int rlen = net.udpOut.read(buf, sizeof(buf));
      udp.beginPacket(clientIp, clientPort);
      udp.write(buf, rlen);
      udp.endPacket();
    }
  }
  
  void forwardTcp() {
    WiFiClient client = net.tcpServer.accept();
    if (!client) return;
  
    if (!net.serverReachable) {
      client.stop();
      return;
    }
  
    IPAddress serverIp;
    if (!serverIp.fromString(config.serverIp)) {
      if (!WiFi.hostByName(config.serverIp.c_str(), serverIp)) {
        Serial.printf("DNS lookup failed for: %s\n", config.serverIp.c_str());
        client.stop();
        net.serverReachable = false;
        net.serverStatus = "✗ DNS Failed";
        return;
      }
    }
  
    WiFiClient server;
    if (!server.connect(serverIp, config.serverPort)) {
      client.stop();
      net.serverReachable = false;
      net.serverStatus = "✗ Server Offline";
      return;
    }
  
    client.setNoDelay(true);
    server.setNoDelay(true);
  
    uint32_t lastActive = millis();
    uint8_t buf[1024];
  
    while (client.connected() && server.connected()) {
      int len = client.available();
      if (len > 0) {
        len = client.read(buf, sizeof(buf));
        if (len > 0) {
          server.write(buf, len);
          lastActive = millis();
        }
      }
  
      len = server.available();
      if (len > 0) {
        len = server.read(buf, sizeof(buf));
        if (len > 0) {
          client.write(buf, len);
          lastActive = millis();
        }
      }
  
      if (millis() - lastActive > config.TCP_TIMEOUT) break;
      delay(1);
    }
  
    client.stop();
    server.stop();
  }


  bool connectWiFi() {
    if (config.useStaticIP) {
      IPAddress gateway(192, 168, 1, 1);
      IPAddress subnet(255, 255, 255, 0);
      WiFi.config(config.staticIP, gateway, subnet);
    }

    WiFi.setSleep(false);
    WiFi.begin(config.ssid.c_str(), config.password.c_str());
    
    Serial.print("Connecting to WiFi");
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < config.SETUP_TIMEOUT) {
      delay(250);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      net.wifiConnected = true;
      Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
      
      if (!MDNS.begin("terrariaproxy")) {
        Serial.println("Error setting up MDNS responder!");
      } else {
        Serial.println("mDNS responder started: terrariaproxy.local");
        MDNS.addService("http", "tcp", 80);  // ← ADD THIS LINE
      }

      
      checkServerConnection();
      Serial.printf("Server status: %s\n", net.serverStatus.c_str());
      return true;
    }

    Serial.println("\n[WiFi] Failed to connect");
    return false;
  }

  void startAPMode() {
    Serial.println("Starting configuration AP");
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP("TerrariaProxyConfig");
    net.inAPMode = true;
    net.wifiConnected = false;
    
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }

  void handleRoot() {
    if (net.inAPMode) {
      String page = FPSTR(wifiConfigPage);
      page.replace("%THEMEMODE%", config.darkMode ? "" : "light-mode");
      page.replace("%THEMECHECKED%", config.darkMode ? "" : "checked");
      net.webServer.send(200, "text/html", page);
    } else {
      String page = FPSTR(serverConfigPage);
      page.replace("%SERVERIP%", config.serverIp);
      page.replace("%SERVERPORT%", String(config.serverPort));
      page.replace("%STATUS%", net.serverStatus);
      page.replace("%STATUSCLASS%", net.serverReachable ? "online" : "offline");
      page.replace("%STATUSICON%", net.serverReachable ? "✓" : "✗");
      page.replace("%PROXYIP%", WiFi.localIP().toString());
      page.replace("%WIFISTATUS%", WiFi.SSID());
      page.replace("%THEMEMODE%", config.darkMode ? "" : "light-mode");
      page.replace("%THEMECHECKED%", config.darkMode ? "" : "checked");
      net.webServer.send(200, "text/html", page);
    }
  }

  void saveWifiSettings() {
    config.ssid = net.webServer.arg("ssid");
    config.password = net.webServer.arg("password");
    String staticIp = net.webServer.arg("staticip");
    config.darkMode = net.webServer.arg("darkmode") != "light-mode";
    
    if (staticIp != "") {
      config.useStaticIP = true;
      config.staticIP.fromString(staticIp);
    } else {
      config.useStaticIP = false;
    }
    
    ConfigManager::saveSettings();
    net.webServer.send(200, "text/plain", "Settings saved. Rebooting...");
    delay(1000);
    ESP.restart();
  }

  void saveServerSettings() {
    String newServerIp = net.webServer.arg("serverip");
    String newServerPort = net.webServer.arg("serverport");
    
    if (newServerIp.length() == 0 || newServerPort.length() == 0) {
      net.webServer.send(400, "text/plain", "Server IP and Port are required");
      return;
    }
    
    config.serverIp = newServerIp;
    config.serverPort = newServerPort.toInt();
    config.darkMode = net.webServer.arg("darkmode") != "light-mode";
    
    ConfigManager::saveSettings();
    net.serverStatus = "Checking...";
    checkServerConnection();
    net.webServer.sendHeader("Location", "/");
    net.webServer.send(303);
  }

  void setupWebHandlers() {
    net.webServer.on("/", HTTP_GET, handleRoot);
    
    if (net.inAPMode) {
      net.webServer.on("/savewifi", HTTP_POST, saveWifiSettings);
    } else {
      net.webServer.on("/saveserver", HTTP_POST, saveServerSettings);
    }
    
    net.webServer.onNotFound([]() {
      net.webServer.send(404, "text/plain", "Not found");
    });
  }

  void setupNetwork() {
    net.udp8888.begin(config.port8888);
    net.udp7777.begin(config.port7777);
    net.udpOut.begin(0);
    net.tcpServer.begin();

    setupWebHandlers();
    net.webServer.begin();

    Serial.printf("UDP listening on %d and %d\n", config.port8888, config.port7777);
    Serial.printf("TCP listening on port %d\n", config.port7777);
    Serial.println("Web interface available at terrariaproxy.local");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[ESP32 Proxy Booting]");

  ConfigManager::loadSettings();

  if (!NetOps::connectWiFi()) {
    NetOps::startAPMode();
  }

  NetOps::setupNetwork();
}

void loop() {

  if (!net.wifiConnected && !net.inAPMode) {
    uint32_t now = millis();
    if (now - timing.lastReconnectAttempt >= config.RECONNECT_DELAY) {
      timing.lastReconnectAttempt = now;
      Serial.println("[WiFi] Attempting to reconnect...");
      if (NetOps::connectWiFi()) {
        net.inAPMode = false;
      } else if (!net.inAPMode) {
        NetOps::startAPMode();
      }
    }
  }

  if (net.wifiConnected) {
    NetOps::updateServerStatus();
    
    NetOps::forwardUdp(net.udp8888, "UDP 8888");
    NetOps::forwardUdp(net.udp7777, "UDP 7777");
    NetOps::forwardTcp();

    uint32_t now = millis();
    if (now - timing.lastBroadcast >= config.BROADCAST_INTERVAL) {
      timing.lastBroadcast = now;
      NetOps::sendBroadcast();
    }
  }

  if (millis() - timing.lastWebHandle > 100) {
    timing.lastWebHandle = millis();
    net.webServer.handleClient();
  }

  delay(1);
}
