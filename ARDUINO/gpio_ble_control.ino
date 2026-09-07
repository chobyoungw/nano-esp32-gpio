/*
  Nano ESP32 - BLE GPIO Control
  D2 ~ D13 디지털 핀을 블루투스(BLE)로 ON/OFF 제어합니다.

  보드 설정: Arduino IDE > 보드 매니저에서 "Arduino ESP32 Boards" 설치
            보드 선택: "Arduino Nano ESP32"

  통신 방식:
   - CONTROL 특성(Write): "핀번호:상태" 형식의 문자열을 받음 (예: "5:1" -> D5 HIGH)
   - STATE 특성(Read/Notify): 전체 핀 상태를 "2:0,3:1,4:0,...,13:0" 형식으로 전달
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID       "12345678-1234-5678-1234-56789abcdef0"
#define CHAR_CONTROL_UUID  "12345678-1234-5678-1234-56789abcdef1"
#define CHAR_STATE_UUID    "12345678-1234-5678-1234-56789abcdef2"

const int pins[] = { D2, D3, D4, D5, D6, D7, D8, D9, D10, D11, D12, D13 };
const int pinCount = sizeof(pins) / sizeof(pins[0]);
bool pinState[12] = { false };

// 보드 내장 LED (Nano ESP32 온보드 RGB LED, 공통 애노드 방식이라 반전 로직 사용)
bool ledState = false;

BLECharacteristic *pStateChar;
BLEServer *pServer;
bool deviceConnected = false;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) {
    deviceConnected = true;
    Serial.println("클라이언트 연결됨");
  }
  void onDisconnect(BLEServer *server) {
    deviceConnected = false;
    Serial.println("연결 끊김, 재광고 시작");
    BLEDevice::startAdvertising();
  }
};

String buildStateString() {
  String s = "";
  for (int i = 0; i < pinCount; i++) {
    s += String(i + 2) + ":" + String(pinState[i] ? 1 : 0);
    s += ",";
  }
  s += "LED:" + String(ledState ? 1 : 0);
  return s;
}

class ControlCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    String value = String(characteristic->getValue().c_str());
    int sep = value.indexOf(':');
    if (sep == -1) return;

    String key = value.substring(0, sep);
    int state = value.substring(sep + 1).toInt();

    if (key == "LED") {
      ledState = (state == 1);
      // 온보드 RGB LED는 공통 애노드라 LOW일 때 켜짐 (반전 로직)
      digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);
      Serial.println(ledState ? "보드 LED -> ON" : "보드 LED -> OFF");
    } else {
      int pinNum = key.toInt();
      int index = pinNum - 2;
      if (index < 0 || index >= pinCount) return;

      pinState[index] = (state == 1);
      digitalWrite(pins[index], pinState[index] ? HIGH : LOW);

      Serial.print("D");
      Serial.print(pinNum);
      Serial.println(pinState[index] ? " -> HIGH" : " -> LOW");
    }

    String stateStr = buildStateString();
    pStateChar->setValue(stateStr.c_str());
    pStateChar->notify();
  }
};

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < pinCount; i++) {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
  }

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // 꺼진 상태로 시작 (반전 로직)

  BLEDevice::init("Nano-ESP32-GPIO");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  BLECharacteristic *pControlChar = pService->createCharacteristic(
    CHAR_CONTROL_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pControlChar->setCallbacks(new ControlCallbacks());

  pStateChar = pService->createCharacteristic(
    CHAR_STATE_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pStateChar->addDescriptor(new BLE2902());
  pStateChar->setValue(buildStateString().c_str());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("BLE 광고 시작됨: Nano-ESP32-GPIO");
}

void loop() {
  delay(20);
}
