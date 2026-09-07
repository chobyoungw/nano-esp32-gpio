/*
  Nano ESP32 - WiFi GPIO Control (자체 웹서버 버전)
  D2 ~ D13 디지털 핀 + 보드 내장 LED를 같은 Wi-Fi 안에서 웹페이지로 제어합니다.
  Bluefy 앱이나 별도 호스팅 없이, 보드 자체가 웹서버 역할을 합니다.

  보드 설정: Tools > Board > "Arduino Nano ESP32"

  사용법:
   1) 아래 ssid, password를 본인의 2.4GHz 공유기 정보로 수정
   2) 업로드 후 시리얼 모니터(115200)에서 IP 주소 확인
   3) 아이폰을 같은 Wi-Fi에 연결한 뒤, Safari에서 http://IP주소 로 접속
*/

#include <WiFi.h>
#include <WebServer.h>

// ===== 여기를 본인 공유기 정보로 수정하세요 (2.4GHz만 지원) =====
const char* ssid = "여기에_와이파이_이름";
const char* password = "여기에_와이파이_비밀번호";
// ================================================================

WebServer server(80);

const int pins[] = { D2, D3, D4, D5, D6, D7, D8, D9, D10, D11, D12, D13 };
const int pinCount = sizeof(pins) / sizeof(pins[0]);
bool pinState[12] = { false };
bool ledState = false;

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="ko">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-title" content="GPIO 제어">
<title>Nano ESP32 GPIO 제어 (WiFi)</title>
<style>
  :root{--ink:#1E2420;--paper:#F3F1E9;--line:#D9D5C7;--moss:#4B5D45;--moss-dim:#8A9784;--live:#3E7C4A;--card:#fff;}
  *{box-sizing:border-box;}
  html,body{margin:0;background:var(--paper);color:var(--ink);font-family:-apple-system,"SF Pro Text","Apple SD Gothic Neo",sans-serif;-webkit-font-smoothing:antialiased;}
  body{display:flex;justify-content:center;padding:env(safe-area-inset-top,24px) 16px calc(env(safe-area-inset-bottom,16px) + 24px);min-height:100vh;}
  .sheet{width:100%;max-width:440px;padding-top:28px;}
  .eyebrow{font-size:13px;color:var(--moss);margin:0 0 6px;}
  h1{font-size:26px;font-weight:650;margin:0 0 16px;letter-spacing:-0.01em;}
  .status-card{display:flex;align-items:center;gap:10px;background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px 16px;margin-bottom:20px;}
  .dot{width:10px;height:10px;border-radius:50%;background:var(--live);box-shadow:0 0 0 4px rgba(62,124,74,0.15);}
  .status-text{font-size:14px;}
  .status-sub{font-size:12px;color:var(--moss-dim);margin-top:1px;}
  .pin-grid{display:flex;flex-direction:column;gap:1px;background:var(--line);border-radius:14px;overflow:hidden;border:1px solid var(--line);margin-bottom:18px;}
  .pin-row{display:flex;align-items:center;justify-content:space-between;background:var(--card);padding:14px 16px;}
  .pin-name{font-size:16px;font-weight:600;}
  .pin-sub{font-size:12px;color:var(--moss-dim);margin-top:2px;}
  .switch{position:relative;width:50px;height:30px;flex-shrink:0;}
  .switch input{opacity:0;width:0;height:0;}
  .slider{position:absolute;inset:0;background:var(--line);border-radius:30px;cursor:pointer;transition:background 0.2s ease;}
  .slider::before{content:"";position:absolute;width:24px;height:24px;left:3px;top:3px;background:#fff;border-radius:50%;transition:transform 0.2s ease;box-shadow:0 1px 2px rgba(0,0,0,0.2);}
  input:checked + .slider{background:var(--moss);}
  input:checked + .slider::before{transform:translateX(20px);}
</style>
</head>
<body>
  <div class="sheet">
    <p class="eyebrow">Nano ESP32 · Wi-Fi</p>
    <h1>GPIO 제어</h1>
    <div class="status-card">
      <div class="dot"></div>
      <div>
        <div class="status-text">Wi-Fi로 연결됨</div>
        <div class="status-sub">같은 네트워크에서 바로 제어 가능</div>
      </div>
    </div>

    <div class="pin-grid" id="ledGrid"></div>
    <div class="pin-grid" id="pinGrid"></div>
  </div>

<script>
  const pinNumbers = [2,3,4,5,6,7,8,9,10,11,12,13];
  const pinGrid = document.getElementById('pinGrid');
  const ledGrid = document.getElementById('ledGrid');

  function createRow(key, name, sub) {
    const row = document.createElement('div');
    row.className = 'pin-row';
    row.innerHTML = `<div><div class="pin-name">${name}</div><div class="pin-sub">${sub}</div></div>`;

    const switchWrap = document.createElement('label');
    switchWrap.className = 'switch';

    const input = document.createElement('input');
    input.type = 'checkbox';
    input.id = 'sw-' + key;
    input.addEventListener('change', () => setState(key, input.checked));

    const slider = document.createElement('span');
    slider.className = 'slider';

    switchWrap.appendChild(input);
    switchWrap.appendChild(slider);
    row.appendChild(switchWrap);
    return row;
  }

  ledGrid.appendChild(createRow('LED', '보드 LED', '온보드 내장 LED'));
  pinNumbers.forEach(num => {
    pinGrid.appendChild(createRow(String(num), 'D' + num, 'GPIO 디지털 출력'));
  });

  async function loadState() {
    const res = await fetch('/state');
    const data = await res.json();
    Object.keys(data).forEach(key => {
      const el = document.getElementById('sw-' + key);
      if (el) el.checked = data[key] === 1;
    });
  }

  async function setState(key, isOn) {
    try {
      await fetch(`/set?pin=${key}&state=${isOn ? 1 : 0}`);
    } catch (e) {
      alert('전송 실패: ' + e.message);
    }
  }

  loadState();
</script>
</body>
</html>
)HTML";

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

String buildStateJson() {
  String s = "{";
  for (int i = 0; i < pinCount; i++) {
    s += "\"" + String(i + 2) + "\":" + String(pinState[i] ? 1 : 0) + ",";
  }
  s += "\"LED\":" + String(ledState ? 1 : 0);
  s += "}";
  return s;
}

void handleState() {
  server.send(200, "application/json", buildStateJson());
}

void handleSet() {
  if (!server.hasArg("pin") || !server.hasArg("state")) {
    server.send(400, "text/plain", "missing args");
    return;
  }

  String pin = server.arg("pin");
  int state = server.arg("state").toInt();

  if (pin == "LED") {
    ledState = (state == 1);
    digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);  // 반전 로직
    Serial.println(ledState ? "보드 LED -> ON" : "보드 LED -> OFF");
  } else {
    int pinNum = pin.toInt();
    int index = pinNum - 2;
    if (index < 0 || index >= pinCount) {
      server.send(400, "text/plain", "invalid pin");
      return;
    }
    pinState[index] = (state == 1);
    digitalWrite(pins[index], pinState[index] ? HIGH : LOW);
    Serial.print("D");
    Serial.print(pinNum);
    Serial.println(pinState[index] ? " -> HIGH" : " -> LOW");
  }

  server.send(200, "application/json", buildStateJson());
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < pinCount; i++) {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
  }
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // 꺼진 상태로 시작

  WiFi.begin(ssid, password);
  Serial.print("Wi-Fi 연결 중");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("연결됨! 이 주소로 접속하세요: http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/state", handleState);
  server.on("/set", handleSet);
  server.begin();

  Serial.println("웹서버 시작됨");
}

void loop() {
  server.handleClient();
}
