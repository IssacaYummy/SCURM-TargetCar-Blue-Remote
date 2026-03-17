#include <WiFi.h>
#include <WebSocketsServer.h>

// ============================================================
// 配置区 —— 只需修改这里
// ============================================================
#define WIFI_SSID       "Target_Car"
#define WIFI_PASSWORD   "12345678"
#define WS_PORT         81

// ===== ① 新增：心跳配置 =====
#define HEARTBEAT_CHAR      'h'   // 心跳字符，与前端原配置一致
#define HEARTBEAT_INTERVAL  200   // 心跳间隔 ms
// ============================

WebSocketsServer webSocket(WS_PORT);

bool deviceConnected    = false;
bool oldDeviceConnected = false;
uint8_t connectedClient = 0xFF;

// ===== ② 新增：心跳定时器声明与回调 =====
esp_timer_handle_t heartbeatTimer;

void IRAM_ATTR onHeartbeatTimer(void* arg) {
    if (deviceConnected) {
        Serial.print(HEARTBEAT_CHAR); // 向 STM32 发送心跳
    }
}

void startHeartbeat() {
    esp_timer_create_args_t timerArgs = {
        .callback = onHeartbeatTimer,
        .arg      = nullptr,
        .name     = "hb_timer"
    };
    esp_timer_create(&timerArgs, &heartbeatTimer);
    esp_timer_start_periodic(heartbeatTimer,
                             (uint64_t)HEARTBEAT_INTERVAL * 1000ULL); // ms → us
}
// =========================================

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {

        case WStype_DISCONNECTED:
            deviceConnected = false;
            connectedClient = 0xFF;
            Serial.println("Device disconnected");
            break;

        case WStype_CONNECTED: {
            deviceConnected = true;
            connectedClient = num;
            Serial.println("Device connected");
            break;
        }

        case WStype_TEXT:
            if (length > 0) {
                // ===== ③ 新增：过滤前端发来的心跳包，避免重复发送 =====
                if (length == 1 && payload[0] == HEARTBEAT_CHAR) break;
                // ========================================================
                for (size_t i = 0; i < length; i++) {
                    Serial.print((char)payload[i]);
                }
            }
            break;

        case WStype_BIN:
            if (length > 0) {
                for (size_t i = 0; i < length; i++) {
                    Serial.print((char)payload[i]);
                }
            }
            break;

        default:
            break;
    }
}

void setup() {
    Serial.setRxBufferSize(1024);
    Serial.begin(115200);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
    delay(100);

    Serial.println("Waiting a client connection to notify...");

    webSocket.begin();
    webSocket.onEvent(onWebSocketEvent);
    webSocket.enableHeartbeat(200, 100, 2);

    startHeartbeat(); // ===== ③ 新增：启动心跳定时器 =====
}

void loop() {
    webSocket.loop();

    if (deviceConnected) {
        int avail = Serial.available();
        if (avail > 0) {
            size_t len = (avail > 20) ? 20 : avail;
            uint8_t buf[20];
            Serial.readBytes(buf, len);
            webSocket.sendTXT(connectedClient, buf, len);
            delay(2);
        }
    }

    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }
}
