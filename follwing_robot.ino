#include "esp_camera.h"
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <iostream>
#include <sstream>

struct MOTOR_PINS {
  int pinEn;  
  int pinIN1;
  int pinIN2;    
};

std::vector<MOTOR_PINS> motorPins = {
  {12, 13, 15},  // RIGHT_MOTOR Pins (EnA, IN1, IN2)
  {12, 14, 2},   // LEFT_MOTOR  Pins (EnB, IN3, IN4)
};
#define LIGHT_PIN 4

#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4
#define STOP 0
#define LEFT_SLOW 5
#define RIGHT_SLOW 6

#define RIGHT_MOTOR 0
#define LEFT_MOTOR 1

#define FORWARD 1
#define BACKWARD -1

const int PWMFreq = 1000; /* 1 KHz */
const int PWMResolution = 8;
const int PWMSpeedChannel = 2;
const int PWMLightChannel = 3;

// Camera related constants
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

const char* ssid     = "Khanh Lê";      // Thay bằng SSID Wi-Fi của bạn
const char* password = "anna461982";     // Thay bằng mật khẩu Wi-Fi của bạn

AsyncWebServer server(80);
AsyncWebSocket wsCamera("/Camera");
AsyncWebSocket wsCarInput("/CarInput");
uint32_t cameraClientId = 0;

// Biến để theo dõi chế độ
bool isManualMode = false; // false = tự động, true = thủ công

const char* htmlInterface PROGMEM = R"HTMLINTERFACE(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32-CAM Car with Person Tracking</title>
  <script src="https://cdn.jsdelivr.net/npm/@tensorflow/tfjs@latest/dist/tf.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/@tensorflow-models/coco-ssd@latest"></script>
  <style>
    body {
      font-family: Arial, sans-serif;
      background: #181818;
      color: #EFEFEF;
      text-align: center;
    }
    .image-container {
      position: relative;
      margin: 0 auto;
      width: 320px;
      height: 240px;
    }
    #cameraImage {
      width: 320px;
      height: 240px;
      object-fit: cover;
    }
    #canvas {
      position: absolute;
      top: 0;
      left: 0;
      width: 320px;
      height: 240px;
    }
    .arrows {
      font-size: 40px;
      color: rgb(6, 2, 126);
    }
    td.button {
      background-color: rgb(234, 250, 9);
      border-radius: 25%;
      box-shadow: 5px 5px #888888;
      width: 60px;
      height: 60px;
      text-align: center;
      cursor: pointer;
    }
    td.button:active {
      transform: translate(5px, 5px);
      box-shadow: none;
    }
    .noselect {
      -webkit-touch-callout: none;
      -webkit-user-select: none;
      -khtml-user-select: none;
      -moz-user-select: none;
      -ms-user-select: none;
      user-select: none;
    }
    .slidecontainer {
      width: 100%;
    }
    .slider {
      -webkit-appearance: none;
      width: 100%;
      height: 15px;
      border-radius: 5px;
      background: #d3d3d3;
      outline: none;
      opacity: 0.7;
      -webkit-transition: .2s;
      transition: opacity .2s;
    }
    .slider:hover {
      opacity: 1;
    }
    .slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 25px;
      height: 25px;
      border-radius: 50%;
      background: rgb(6, 2, 126);
      cursor: pointer;
    }
    .toggle-container {
      margin: 20px auto;
      width: 200px;
    }
    .toggle-button {
      background-color: #4CAF50;
      color: white;
      padding: 10px 20px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
    }
    .toggle-button.off {
      background-color: #f44336;
    }
  </style>
</head>
<body class="noselect" align="center">
  <h2>ESP32-CAM Car with Person Tracking</h2>
  <div class="image-container">
    <img id="cameraImage" src="" alt="Camera Feed">
    <canvas id="canvas"></canvas>
  </div>
  <div id="result"></div>

  <div class="toggle-container">
    <button id="modeToggle" class="toggle-button" onclick="toggleMode()">Auto Mode: ON</button>
  </div>

  <table id="mainTable" style="width:320px;margin:auto;table-layout:fixed" CELLSPACING=10>
    <tr>
      <td></td>
      <td class="button" onmousedown='startMove("MoveCar","1")' onmouseup='stopMove()' ontouchstart='startMove("MoveCar","1")' ontouchend='stopMove()'><span class="arrows">↑</span></td>
      <td></td>
    </tr>
    <tr>
      <td class="button" onmousedown='startMove("MoveCar","3")' onmouseup='stopMove()' ontouchstart='startMove("MoveCar","3")' ontouchend='stopMove()'><span class="arrows">←</span></td>
      <td class="button"></td>
      <td class="button" onmousedown='startMove("MoveCar","4")' onmouseup='stopMove()' ontouchstart='startMove("MoveCar","4")' ontouchend='stopMove()'><span class="arrows">→</span></td>
    </tr>
    <tr>
      <td></td>
      <td class="button" onmousedown='startMove("MoveCar","2")' onmouseup='stopMove()' ontouchstart='startMove("MoveCar","2")' ontouchend='stopMove()'><span class="arrows">↓</span></td>
      <td></td>
    </tr>
    <tr><td colspan="3"><hr></td></tr>
    <tr>
      <td style="text-align:left"><b>Speed:</b></td>
      <td colspan=2>
        <div class="slidecontainer">
          <input type="range" min="0" max="255" value="150" class="slider" id="Speed" oninput='sendButtonInput("Speed",value)'>
        </div>
      </td>
    </tr>
    <tr>
      <td style="text-align:left"><b>Light:</b></td>
      <td colspan=2>
        <div class="slidecontainer">
          <input type="range" min="0" max="255" value="0" class="slider" id="Light" oninput='sendButtonInput("Light",value)'>
        </div>
      </td>
    </tr>
  </table>

  <script>
    var webSocketCameraUrl = "ws://" + window.location.hostname + "/Camera";
    var webSocketCarInputUrl = "ws://" + window.location.hostname + "/CarInput";
    var websocketCamera;
    var websocketCarInput;
    let model;
    const canvas = document.getElementById('canvas');
    const ctx = canvas.getContext('2d');
    const result = document.getElementById('result');
    let isManualMode = false;
    let moveInterval = null;

    function initCameraWebSocket() {
      websocketCamera = new WebSocket(webSocketCameraUrl);
      websocketCamera.binaryType = 'blob';
      websocketCamera.onopen = function(event) {
        console.log("Camera WebSocket opened");
        startDetection();
      };
      websocketCamera.onclose = function(event) {
        setTimeout(initCameraWebSocket, 2000);
      };
      websocketCamera.onmessage = function(event) {
        var imageId = document.getElementById("cameraImage");
        imageId.src = URL.createObjectURL(event.data);
      };
    }

    function initCarInputWebSocket() {
      websocketCarInput = new WebSocket(webSocketCarInputUrl);
      websocketCarInput.onopen = function(event) {
        console.log("Car Input WebSocket opened");
        var speedButton = document.getElementById("Speed");
        sendButtonInput("Speed", speedButton.value);
        var lightButton = document.getElementById("Light");
        sendButtonInput("Light", lightButton.value);
      };
      websocketCarInput.onclose = function(event) {
        setTimeout(initCarInputWebSocket, 2000);
      };
      websocketCarInput.onmessage = function(event) {
        console.log("Received from ESP32: " + event.data);
      };
    }

    function initWebSocket() {
      initCameraWebSocket();
      initCarInputWebSocket();
    }

    function sendButtonInput(key, value) {
      var data = key + "," + value;
      console.log("Sending: " + data);
      if (websocketCarInput && websocketCarInput.readyState === WebSocket.OPEN) {
        websocketCarInput.send(data);
      }
    }

    function startMove(key, value) {
      if (isManualMode) {
        clearInterval(moveInterval);
        moveInterval = setInterval(() => {
          sendButtonInput(key, value);
        }, 100);
      }
    }

    function stopMove() {
      if (isManualMode) {
        clearInterval(moveInterval);
        sendButtonInput("MoveCar", "0");
      }
    }

    function toggleMode() {
      isManualMode = !isManualMode;
      const toggleButton = document.getElementById("modeToggle");
      if (isManualMode) {
        toggleButton.textContent = "Manual Mode: ON";
        toggleButton.classList.add("off");
        sendButtonInput("Mode", "1");
      } else {
        toggleButton.textContent = "Auto Mode: ON";
        toggleButton.classList.remove("off");
        sendButtonInput("Mode", "0");
      }
    }

    async function startDetection() {
      result.innerHTML = "Loading model...";
      try {
        model = await cocoSsd.load();
        result.innerHTML = "Model loaded. Starting detection...";
        detectLoop();
      } catch (error) {
        result.innerHTML = "Failed to load model: " + error.message;
        console.error(error);
      }
    }

    async function detectLoop() {
      if (!model) return;

      try {
        const img = document.getElementById('cameraImage');
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        const predictions = await model.detect(img);
        drawPredictions(predictions);
        if (!isManualMode) {
          sendToESP(predictions);
        }
        setTimeout(detectLoop, 50);
      } catch (error) {
        result.innerHTML = "Detection error: " + error.message;
        console.error(error);
      }
    }

    function drawPredictions(predictions) {
      const img = document.getElementById('cameraImage');
      ctx.drawImage(img, 0, 0, 320, 240);
      let personDetected = false;
      result.innerHTML = "";

      predictions.forEach((pred, i) => {
        if (pred.class === "person" && pred.score > 0.5) {
          personDetected = true;
          const [x, y, width, height] = pred.bbox;
          ctx.strokeStyle = "#00FFFF";
          ctx.lineWidth = 2;
          ctx.strokeRect(x, y, width, height);
          ctx.fillStyle = "#00FFFF";
          ctx.font = "16px Arial";
          ctx.fillText(`Person (${Math.round(pred.score * 100)}%)`, x, y - 5);
          result.innerHTML += `[${i}] Person: ${Math.round(pred.score * 100)}% at (${Math.round(x)}, ${Math.round(y)}, ${Math.round(width)}, ${Math.round(height)})<br>`;
        }
      });

      if (!personDetected) {
        result.innerHTML = "No person detected.";
      }
    }

    function sendToESP(predictions) {
      let targetFound = false;
      let closestX, closestY, closestWidth, closestHeight;

      for (const pred of predictions) {
        if (pred.class === "person" && pred.score > 0.5) {
          const [x, y, width, height] = pred.bbox;
          closestX = x;
          closestY = y;
          closestWidth = width;
          closestHeight = height;
          targetFound = true;
          console.log("Person detected at: x=" + closestX + ", y=" + closestY + ", w=" + closestWidth + ", h=" + closestHeight);
          break;
        }
      }

      if (targetFound) {
        const message = `Person,${Math.round(closestX)},${Math.round(closestY)},${Math.round(closestWidth)},${Math.round(closestHeight)}`;
        if (websocketCarInput && websocketCarInput.readyState === WebSocket.OPEN) {
          websocketCarInput.send(message);
          console.log("Sent to ESP32: " + message);
        } else {
          console.log("WebSocket not open");
        }
      } else if (websocketCarInput && websocketCarInput.readyState === WebSocket.OPEN) {
        websocketCarInput.send("MoveCar,0"); // Dừng ngay khi không có person
        console.log("Sent to ESP32: MoveCar,0");
      }
    }

    window.onload = initWebSocket;
    document.getElementById("mainTable").addEventListener("touchend", function(event) {
      event.preventDefault();
    });
  </script>
</body>
</html>
)HTMLINTERFACE";

void rotateMotor(int motorNumber, int motorDirection) {
  if (motorDirection == FORWARD) {
    digitalWrite(motorPins[motorNumber].pinIN1, HIGH);
    digitalWrite(motorPins[motorNumber].pinIN2, LOW);    
  } else if (motorDirection == BACKWARD) {
    digitalWrite(motorPins[motorNumber].pinIN1, LOW);
    digitalWrite(motorPins[motorNumber].pinIN2, HIGH);     
  } else {
    digitalWrite(motorPins[motorNumber].pinIN1, LOW);
    digitalWrite(motorPins[motorNumber].pinIN2, LOW);       
  }
}

void moveCar(int inputValue) {
  Serial.printf("Got value as %d\n", inputValue);  
  switch(inputValue) {
    case UP:
      rotateMotor(RIGHT_MOTOR, FORWARD);
      rotateMotor(LEFT_MOTOR, FORWARD);                  
      Serial.println("Moving: UP");
      break;
    case DOWN:
      rotateMotor(RIGHT_MOTOR, BACKWARD);
      rotateMotor(LEFT_MOTOR, BACKWARD);  
      Serial.println("Moving: DOWN");
      break;
    case LEFT:
      rotateMotor(RIGHT_MOTOR, FORWARD);
      rotateMotor(LEFT_MOTOR, BACKWARD);  
      Serial.println("Moving: LEFT");
      break;
    case RIGHT:
      rotateMotor(RIGHT_MOTOR, BACKWARD);
      rotateMotor(LEFT_MOTOR, FORWARD); 
      Serial.println("Moving: RIGHT");
      break;
    case LEFT_SLOW:
      rotateMotor(RIGHT_MOTOR, FORWARD);
      rotateMotor(LEFT_MOTOR, STOP);  
      Serial.println("Moving: LEFT_SLOW");
      break;
    case RIGHT_SLOW:
      rotateMotor(RIGHT_MOTOR, STOP);
      rotateMotor(LEFT_MOTOR, FORWARD); 
      Serial.println("Moving: RIGHT_SLOW");
      break;
    case STOP:
      rotateMotor(RIGHT_MOTOR, STOP);
      rotateMotor(LEFT_MOTOR, STOP);    
      Serial.println("Moving: STOP");
      break;
    default:
      rotateMotor(RIGHT_MOTOR, STOP);
      rotateMotor(LEFT_MOTOR, STOP);    
      Serial.println("Moving: DEFAULT STOP");
      break;
  }
}

void controlCarBasedOnBoundingBox(int x, int y, int width, int height) {
  if (isManualMode) {
    Serial.println("Manual mode active, skipping auto control");
    return;
  }

  const int frameWidth = 320;  
  const int centerX = x + width / 2; 

  const int leftThreshold = frameWidth / 4;      // 0 - 80: Trái
  const int rightThreshold = 3 * frameWidth / 4; // 240 - 320: Phải

  Serial.printf("CenterX: %d, LeftThreshold: %d, RightThreshold: %d\n", 
                centerX, leftThreshold, rightThreshold);

  if (centerX < leftThreshold) {
    moveCar(LEFT_SLOW);
  } 
  else if (centerX > rightThreshold) {
    moveCar(RIGHT_SLOW);
  } 
  else {
    moveCar(UP); // Đi thẳng tới khi person ở giữa
  }
}

void handleRoot(AsyncWebServerRequest *request) {
  request->send(200, "text/html", 
    "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>ESP32-CAM Car</title></head><body><h2>Loading interface...</h2>"
    "<script>window.location.href = '/interface';</script></body></html>");
}

void handleNotFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "File Not Found");
}

void onCarInputWebSocketEvent(AsyncWebSocket *server, 
                              AsyncWebSocketClient *client, 
                              AwsEventType type,
                              void *arg, 
                              uint8_t *data, 
                              size_t len) {                      
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      moveCar(STOP);
      ledcWrite(PWMLightChannel, 0);  
      isManualMode = false;
      break;
    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        std::string myData = "";
        myData.assign((char *)data, len);
        Serial.println("Received: " + String(myData.c_str()));
        std::istringstream ss(myData);
        std::string key, value;
        std::getline(ss, key, ',');
        
        if (key == "Person") {
          std::string xStr, yStr, widthStr, heightStr;
          std::getline(ss, xStr, ',');
          std::getline(ss, yStr, ',');
          std::getline(ss, widthStr, ',');
          std::getline(ss, heightStr, ',');
          int x = atoi(xStr.c_str());
          int y = atoi(yStr.c_str());
          int width = atoi(widthStr.c_str());
          int height = atoi(heightStr.c_str());
          Serial.printf("Bounding Box: x=%d, y=%d, width=%d, height=%d\n", x, y, width, height);
          controlCarBasedOnBoundingBox(x, y, width, height);
        } 
        else if (key == "MoveCar") {
          std::getline(ss, value, ',');
          int valueInt = atoi(value.c_str());
          if (isManualMode) {
            moveCar(valueInt); // Điều khiển thủ công
          } else if (valueInt == STOP && !isManualMode) {
            moveCar(STOP); // Dừng ngay khi không có person ở chế độ tự động
          }
        } 
        else if (key == "Speed") {
          std::getline(ss, value, ',');
          int valueInt = atoi(value.c_str());
          ledcWrite(PWMSpeedChannel, valueInt);
          Serial.println("Speed set to: " + String(valueInt));
        } 
        else if (key == "Light") {
          std::getline(ss, value, ',');
          int valueInt = atoi(value.c_str());
          ledcWrite(PWMLightChannel, valueInt); 
          Serial.println("Light set to: " + String(valueInt));        
        } 
        else if (key == "Mode") {
          std::getline(ss, value, ',');
          isManualMode = (atoi(value.c_str()) == 1);
          if (isManualMode) {
            moveCar(STOP);
            Serial.println("Switched to Manual Mode");
          } else {
            Serial.println("Switched to Auto Mode");
          }
        }
      }
      break;
    }
    case WS_EVT_PONG:
      break;
    case WS_EVT_ERROR:
      break;
    default:
      break;  
  }
}

void onCameraWebSocketEvent(AsyncWebSocket *server, 
                            AsyncWebSocketClient *client, 
                            AwsEventType type,
                            void *arg, 
                            uint8_t *data, 
                            size_t len) {                      
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      cameraClientId = client->id();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      cameraClientId = 0;
      break;
    case WS_EVT_DATA:
      break;
    case WS_EVT_PONG:
      break;
    case WS_EVT_ERROR:
      break;
    default:
      break;  
  }
}

void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA; // 320x240
  config.jpeg_quality = 10;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  Serial.println("Camera initialized");
}

void sendCameraPicture() {
  if (cameraClientId == 0) return;
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Frame buffer could not be acquired");
    return;
  }
  wsCamera.binary(cameraClientId, fb->buf, fb->len);
  esp_camera_fb_return(fb);

  while (true) {
    AsyncWebSocketClient * clientPointer = wsCamera.client(cameraClientId);
    if (!clientPointer || !(clientPointer->queueIsFull())) break;
    delay(1);
  }
}

void setUpPinModes() {
  ledcSetup(PWMSpeedChannel, PWMFreq, PWMResolution);
  ledcSetup(PWMLightChannel, PWMFreq, PWMResolution);
  
  for (int i = 0; i < motorPins.size(); i++) {
    pinMode(motorPins[i].pinEn, OUTPUT);    
    pinMode(motorPins[i].pinIN1, OUTPUT);
    pinMode(motorPins[i].pinIN2, OUTPUT);  
    ledcAttachPin(motorPins[i].pinEn, PWMSpeedChannel);
  }
  moveCar(STOP);

  pinMode(LIGHT_PIN, OUTPUT);    
  ledcAttachPin(LIGHT_PIN, PWMLightChannel);
}

void setup(void) {
  setUpPinModes();
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi. IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/interface", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", htmlInterface);
  });
  server.on("/", HTTP_GET, handleRoot);
  server.onNotFound(handleNotFound);

  wsCamera.onEvent(onCameraWebSocketEvent);
  server.addHandler(&wsCamera);

  wsCarInput.onEvent(onCarInputWebSocketEvent);
  server.addHandler(&wsCarInput);

  server.begin();
  Serial.println("HTTP server started");

  setupCamera();
}

void loop() {
  wsCamera.cleanupClients(); 
  wsCarInput.cleanupClients(); 
  sendCameraPicture();
}