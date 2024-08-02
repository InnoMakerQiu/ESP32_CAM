#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include "WiFi.h"
#include <PubSubClient.h>
#include <stdbool.h>
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include <base64.h>

//指定指令最大的长度不超过300
#define MAX_COMMAND_LENGTH 300

//
#define MAX_TIMES 20000

#define SLEEP_DELAY 6000  // Delay for 60 Sec, 0 sec - PIR sensor mode

// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

//指定控制LED亮暗的IO引脚
const int LED_BUILTIN = 4;

//下面是确定订阅主题和发布主题
const char* commandTopic  = "/command/send/esp32cam_1";
const char* statusTopic = "/status/upload/esp32cam_1";
const char* resultTopic = "/executionResult/upload/esp32cam_1";
const char* pictureTopic = "/picture/upload/esp32cam_1";


//下面是声明wifiClient，mqttClient
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// create four servo objects 
Servo servo1;
Servo servo2;

const char* wifi_ssid = "zxic";
const char* wifi_password = "agpj3401";

// Replace with your network credentials
// const char* wifi_ssid = "404_2.4g";
// const char* wifi_password = "@404@404";

// const char* wifi_ssid = "ACCOMPANY_206";
// const char* wifi_password = "206206206";

// Configure MQTT Broker connection
const char* mqtt_server = "frp.vhelpyou.eu.org";
const int mqtt_port = 1883;
const char* mqtt_user = "esp32cam_1";
const char* mqtt_password = "xiaocaiji";

//下面是设置舵机控制的标志位
int rotationFlag = 0;

//下面是设置指令解析的标志位
int parseCommandFlag = 0;


//这里用来控制舵机转动的范围
const int minUs = 0;
const int maxUs = 2000;

//这里我们指定舵机的引脚
const int servo1Pin = 15;
const int servo2Pin = 14;

//这里我们指定舵机当前的位置
int servo1CurPos = 0;      // position in degrees
int servo2CurPos = 0;

//这里我们指定舵机的目标位置
int servo1DesPos = 0;
int servo2DesPos = 0;

//下面用于设置计数延时的时间
//int times = 0;

//设置指令存放的缓冲区
char commandTopicString[40];
char commandString[MAX_COMMAND_LENGTH];
char controlDeviceId[20];


//上传指令解析的状态
void upLoadFeedBack(const char* controlDeviceId,const char* status,const char* feedback);
// 发布信息
void pubMQTTMsg(const char* publishTopic,const char* publishMsg);




void setup_camera() {
  // Turn off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // OV2640 camera module
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

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;  // FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
  }
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }
}

void publishTelemetry(String data) {
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  //得到图片大小数据
  Serial.print("The size of the imageData: ");
  Serial.println(String(data.length()));

  boolean Status = mqttClient.publish_P(pictureTopic, (const uint8_t*) data.c_str(), data.length(), true);
  Serial.println(String(Status ? "Successfully" : "Error") );
}

void capturePhoto(void) {
  // Retrieve camera framebuffer
  camera_fb_t* fb = NULL;
  uint8_t* _jpg_buf = NULL;
  esp_err_t res = ESP_OK;
  size_t frame_size = 0;
  Serial.print("Capturing Image...");

  digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on
  delay(1000);                      // wait for a second
  fb = esp_camera_fb_get();
  digitalWrite(LED_BUILTIN, LOW);  // turn the LED off
  delay(1000);                     // wait for a second
  if (!fb) {
    Serial.println("Camera capture failed");
    res = ESP_FAIL;
  } else {
    Serial.println("Done!");
    Serial.println(String("Size of the image...") + String(fb->len));
    if (fb->format != PIXFORMAT_JPEG) {
      Serial.println("Compressing");
      bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &frame_size);
      esp_camera_fb_return(fb);
      fb = NULL;
      if (!jpeg_converted) {
        Serial.println("JPEG compression failed");
        res = ESP_FAIL;
      }
    } else {
      //这里进行堆栈测试
      Serial.print("Heap Size : ");
      Serial.println(ESP.getFreeHeap());
      //指明长度信息
      Serial.print("fb len : ");
      Serial.println(fb->len);
      //显示base64编码得到的图片信息
      String base64image = base64::encode(fb->buf, fb->len);
      Serial.print("Publish photo...");
      publishTelemetry(base64image);
      Serial.println("Done!");
      esp_camera_fb_return(fb);
    }
  }
  if (res != ESP_OK) {
    return;
  }
}

//这里设置字符转为整形数字的函数
int stringToInteger(const char* str) {
    int result = 0;
    int sign = 1; // 符号位，默认为正数

    // 检查是否为负数
    if (*str == '-') {
        sign = -1;
        str++; // 跳过负号字符
    }

    // 遍历字符串每个字符，计算对应的数字
    while (*str != '\0') {
        if (*str >= '0' && *str <= '9') {
            result = result * 10 + (*str - '0');
        } else {
            // 如果遇到非数字字符，返回0
            return 0;
        }
        str++; // 移动到下一个字符
    }

    return result * sign; // 返回最终结果，包括符号位
}

// 发布信息
void pubMQTTMsg(const char* publishTopic,const char* publishMsg){
  // 实现ESP8266向主题发布信息
  if(mqttClient.publish(publishTopic, publishMsg)){
    Serial.println("Publish Topic:");Serial.println(publishTopic);
    Serial.println("Publish message:");Serial.println(publishMsg);    
  } else {
    Serial.println("Message Publish Failed."); 
  }
}

void receiveCallback(char* topic, byte* payload, unsigned int length) {
    //下面查看信息发送是否无误
  Serial.print("Message Received [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("");
  Serial.print("Message Length(Bytes) ");
  Serial.println(length);
  // Handle non-retained messages as before
  strcpy(commandTopicString,(char*)topic);
  strcpy(commandString,(char*)payload);
  parseCommandFlag=1;
}

void controlRotation(){
  while(servo1DesPos<servo1CurPos){
    servo1CurPos--;
    servo1.write(servo1CurPos);
  }
  while(servo1DesPos>servo1CurPos){
    servo1CurPos++;
    servo1.write(servo1CurPos);
  }
  while(servo2DesPos<servo2CurPos){ 
    servo2CurPos--;
    servo2.write(servo2CurPos);
  }
  while(servo2DesPos>servo2CurPos){
    servo2CurPos++;
    servo2.write(servo2CurPos);
  }
}

//上传指令解析的状态
void upLoadFeedBack(const char* controlDeviceId,const char* status,const char* feedback){
    // 创建 JSON 对象
  StaticJsonDocument<200> doc;
  
  // 添加键值对到 JSON 对象
  doc["controlDeviceId"] = controlDeviceId;
  doc["status"] = status;
  if(feedback==NULL){
      doc["feedback"] = "";
  }else{
      doc["feedback"] = feedback;
  }

  // 序列化 JSON 对象为字符串
  String jsonString;
  serializeJson(doc, jsonString);

  // // 输出序列化后的 JSON 字符串
  // Serial.println(jsonString);
  pubMQTTMsg(resultTopic,jsonString.c_str());
}

//解析下发指令
boolean parseCommand(){
  int value;//这个是转动角度的值
  StaticJsonDocument<1024> doc; //这里是存储解析指令json对象
  DeserializationError error = deserializeJson(doc, commandString);

  // 检查解析是否成功
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return false;
  }

  //首先我们需要解析出controlDeviceId数据
  // 从JSON数据中提取控制设备ID
  strcpy(controlDeviceId,doc["command"]["controlDeviceId"]);
  Serial.print("Control Device ID: ");
  Serial.println(controlDeviceId);
  if(controlDeviceId==NULL){
    return false;
  }

  // 从JSON数据中提取控制设备类型
  const char* controlDeviceType = doc["command"]["controlDeviceType"];
  Serial.print("Control Device Type: ");
  Serial.println(controlDeviceType);

  // 从JSON数据中提取参数
  const char* type = doc["command"]["parameters"]["type"];
  const char* valueString = doc["command"]["parameters"]["value"];
  Serial.print("Type: ");
  Serial.println(type);
  Serial.print("Value: ");
  Serial.println(valueString);

  if(valueString==NULL){
    return false;
  }

  if(!strcmp(type,"clockwise")){
    value = stringToInteger(valueString);
  }
  else if(!strcmp(type,"anticlockwise")){
    value = 0-stringToInteger(valueString);
  }
  else{
    Serial.println("Type is not exist");
    return false;
  }

  if(!strcmp(controlDeviceId,"servo_1")){
    servo1DesPos += value;
    if(servo1DesPos>=180){
      servo1DesPos=180;
    }
    if(servo1DesPos<=0){
      servo1DesPos = 0;
    }
  }
  else if(!strcmp(controlDeviceId,"servo_2")){
    servo2DesPos += value;
    if(servo2DesPos>=180){
      servo2DesPos=180;
    }
    if(servo2DesPos<=0){
      servo2DesPos = 0;
    }
  }else{
    Serial.println("The control device is not existing"); 
    return false;
  }
  rotationFlag = 1;
  return true;
}

//上传控制类设备状态
boolean upLoadStatus(int servoNum){
    // 创建 JSON 对象
  StaticJsonDocument<200> doc;
  char value[5];
  char servoString[8];

  if(servoNum==1){
    strcpy(servoString,"servo_1");
    sprintf(value,"%d",servo1CurPos);
  }
  else if(servoNum==2){
    strcpy(servoString,"servo_2");
    sprintf(value,"%d",servo2CurPos);
  }
  else{
    Serial.println("ServoNum Error");
    return false;
  }

  // 添加键值对到 JSON 对象
  doc["controlDeviceId"] = servoString;
  doc["controlDeviceType"] = "servo";
  doc["action"] = "execute";
  
  JsonObject parameters = doc.createNestedObject("parameters");
  parameters["type"] = "location";
  parameters["value"] = value;
  parameters["unit"] = "";


  // 序列化 JSON 对象为字符串
  String jsonString;
  serializeJson(doc, jsonString);

  // 输出序列化后的 JSON 字符串
  Serial.println(jsonString);
  pubMQTTMsg(statusTopic,jsonString.c_str());
  return true;
}



void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      // 通过串口监视器输出是否成功订阅主题以及订阅的主题名称
      if(mqttClient.subscribe(commandTopic)){
        Serial.println("Subscrib Topic:");
        Serial.println(commandTopic);
      } else {
        Serial.print("Subscribe Fail...");
      }  
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address : ");
  Serial.println(WiFi.localIP());
}

void setup_pwm(){
  // Allow allocation of all timers
  //这里告诉ESP32PWM库，我们需要使用那些定时器
	ESP32PWM::allocateTimer(0); 
	ESP32PWM::allocateTimer(1);

	servo1.setPeriodHertz(50);      // 这里设置pwm的输出周期为50Hz，单位是Hz
	servo2.setPeriodHertz(50);      // Standard 50hz servo

  //对舵机进行初始化
  servo1.attach(servo1Pin, minUs, maxUs); //这里是将舵机1配置到指定引脚，并且指定最小和最大脉冲宽度，单位是us
	servo2.attach(servo2Pin, minUs, maxUs);
}

void setup() {
  Serial.begin(115200); //进行串口初始化操作
  setup_pwm(); //初始化舵机
  setup_wifi(); //初始化wifi
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(receiveCallback); // 设置MQTT订阅回调函数
  reconnect(); //连接mqtt服务器，同时订阅指定主题
  setup_camera();

  //初始化变量值
//  times = 0;
  //下面是设置舵机控制的标志位
  rotationFlag = 0;
  //下面是设置指令解析的标志位
  parseCommandFlag = 0;
  //这里我们指定舵机当前的位置
  servo1CurPos = 0;      // position in degrees
  servo2CurPos = 0;

  //这里我们指定舵机的目标位置
  servo1DesPos = 0;
  servo2DesPos = 0;

  Serial.println(rotationFlag);
}


void loop() {
  static int times = 0;
  reconnect();

  // if(times%100==0){
  //   Serial.print("parseCommandFlag");
  //   Serial.println(parseCommandFlag);
  //   Serial.print("rotationFlag");
  //   Serial.println(rotationFlag);
  // }
    // 检查并执行舵机控制操作
  if (rotationFlag) {
      Serial.println("control servo");
      controlRotation();
      rotationFlag = 0; // 清零标志位
      upLoadFeedBack(controlDeviceId,"OK",NULL);
  }

          // 检查并执行指令解析
  if (parseCommandFlag) {
      if(!parseCommand()){
        upLoadFeedBack(controlDeviceId,"FAIL","parse command error");
      }
      parseCommandFlag = 0; // 清零标志位
  }

  mqttClient.loop();
  if(times%10000==0){
    // 通过串口监视器输出是否成功订阅主题以及订阅的主题名称
    if(mqttClient.subscribe(commandTopic)){
      Serial.println("Subscrib Topic:");
      Serial.println(commandTopic);
    } else {
      Serial.print("Subscribe Fail...");
    }  
  }

  if(times%4000==0){
    Serial.println("upLoadStatus");
    upLoadStatus(1);
    upLoadStatus(2);
    capturePhoto();
  }

  // 延时10ms
  delay(10);
  // 更新定时标志
  times++;
  if (times >= MAX_TIMES) {
      times = 0; // 清零重新计数
  }
}

