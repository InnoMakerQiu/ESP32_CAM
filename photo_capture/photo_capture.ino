/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-cam-video-streaming-web-server-camera-home-assistant/
  
  IMPORTANT!!! 
   - Select Board "AI Thinker ESP32-CAM"
   - GPIO 0 must be connected to GND to upload a sketch
   - After connecting GPIO 0 to GND, press the ESP32-CAM on-board RESET button to put your board in flashing mode
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

#include "esp_camera.h" //esp32-cam模块的驱动程序库
#include <WiFi.h> //用于连接WiFi网络，是能够通过网络进行数据传输和通信
#include "esp_timer.h" //用于创建定时任务
#include "img_converters.h" //图像转换的相关函数
#include "Arduino.h" //包含arduino核心库的基本定义和函数
#include "fb_gfx.h" //用于在帧缓冲区进行图形操作和渲染
#include "soc/soc.h" //disable brownout problems，
#include "soc/rtc_cntl_reg.h"  //disable brownout problems
#include "esp_http_server.h" //用于创建和管理http服务，实现web服务器功能

//why to disable brownout problems
//This is essentially you telling the ESP32 to stop checking for insufficient power,
// and work with whatever power it got. 
//Please note that this may not stop other errors like ‘Guru Mediation…’, etc.


//Replace with your network credentials
// const char* ssid = "404_2.4g";
// const char* password = "@404@404"; 

const char* ssid = "ACCOMPANY_206";
const char* password = "206206206"; 

// const char* ssid = "zxic";
// const char* password = "agpj3401";

// const char* ssid = "627";
// const char* password = "2020090921627";

#define PART_BOUNDARY "123456789000000000000987654321"


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

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;

// // 函数名称：stream_handler
// // 功能：处理流式传输的 HTTP 请求，从摄像头获取图像数据并将其发送到客户端
// // 参数：
// //    - req: 指向 HTTP 请求的指针
// // 返回值：一个 esp_err_t 类型的错误码，指示函数执行的结果
// static esp_err_t stream_handler(httpd_req_t *req){
//   // 初始化变量
//   camera_fb_t * fb = NULL;          // 指向摄像头帧缓冲区的指针
//   esp_err_t res = ESP_OK;            // 函数执行结果，默认为成功
//   size_t _jpg_buf_len = 0;           // JPEG 图像缓冲区长度
//   uint8_t * _jpg_buf = NULL;         // 指向 JPEG 图像缓冲区的指针
//   char * part_buf[64];               // 分段缓冲区
//   int count = 0;

//   // 设置 HTTP 响应类型为流式 JPEG 图像
//   res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
//   if(res != ESP_OK){
//     return res;  // 如果设置响应类型失败，则返回错误码
//   }
//   int times = 0;//这里我们标记发送数据块的。
//   // 循环处理图像捕获和发送
//   while(true){
//     // 从摄像头获取图像帧
//     fb = esp_camera_fb_get();
//     if (!fb) {
//       Serial.println("Camera capture failed");  // 摄像头捕获失败
//       res = ESP_FAIL;  // 设置结果为失败
//     } else {
//       // 如果图像宽度大于 400 像素
//       if(fb->width > 400){
//         // 如果图像格式不是 JPEG，则进行 JPEG 压缩转换
//         if(fb->format != PIXFORMAT_JPEG){
//           bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
//           esp_camera_fb_return(fb);  // 释放图像帧缓冲区
//           fb = NULL;
//           if(!jpeg_converted){
//             Serial.println("JPEG compression failed");  // JPEG 压缩失败
//             res = ESP_FAIL;  // 设置结果为失败
//           }
//         } else {
//           _jpg_buf_len = fb->len;
//           _jpg_buf = fb->buf;
//         }
//       }
//     }
//     // 如果处理过程中无错误
//     if(res == ESP_OK){
//       // 发送 JPEG 图像的分段信息
//       size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
//       res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
//     }
//     // 发送 JPEG 图像数据
//     if(res == ESP_OK){
//       res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
//     }
//     // 发送流式传输边界
//     if(res == ESP_OK){
//       res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
//     }
//     //这里我们增加count
//     // count++;
//     // if(count==5){
//     //   Serial.println("transfer finished");
//     //   httpd_resp_send_chunk(req,NULL,0);
//     // }

//     // 释放图像帧缓冲区和 JPEG 缓冲区
//     if(fb){
//       esp_camera_fb_return(fb);
//       fb = NULL;
//       _jpg_buf = NULL;
//     } else if(_jpg_buf){
//       free(_jpg_buf);
//       _jpg_buf = NULL;
//     }

//     return res;
//     // 如果处理过程中发生错误，则退出循环
//     if(res != ESP_OK){
//       break;
//     }
//     // 输出 JPEG 图像大小
//     //Serial.printf("MJPG: %uB\n",(uint32_t)(_jpg_buf_len));
//   }
//   return res;  // 返回函数执行结果
// }

// 功能：处理 HTTP 请求，从摄像头获取一张图像数据并发送到客户端
// 参数：
//    - req: 指向 HTTP 请求的指针
// 返回值：一个 esp_err_t 类型的错误码，指示函数执行的结果
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;  // 指向摄像头帧缓冲区的指针
    esp_err_t res = ESP_OK;   // 函数执行结果，默认为成功

    // 从摄像头获取一张图像帧
    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");  // 摄像头捕获失败
        return ESP_FAIL;
    }

    // 设置 HTTP 响应类型为 JPEG 图像
    res = httpd_resp_set_type(req, "image/jpeg");
    if (res != ESP_OK) {
        esp_camera_fb_return(fb);  // 释放图像帧缓冲区
        Serial.println("Failed to set response type");
        return res;
    }

    // 发送 JPEG 图像数据到客户端
    res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    if (res != ESP_OK) {
        Serial.println("Failed to send image data");  // 发送图像数据失败
    }

    // 释放图像帧缓冲区
    esp_camera_fb_return(fb);

    return res;  // 返回函数执行结果
}


// 函数名称：startCameraServer
// 功能：启动摄像头服务器
void startCameraServer(){
  // 创建一个默认的 HTTPD 配置
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  // 将服务器端口设置为 80
  config.server_port = 80;

  // 创建一个 HTTP URI 结构体，用于处理根目录的 HTTP GET 请求
  httpd_uri_t index_uri = {
    .uri       = "/",              // URI 路径为根目录
    .method    = HTTP_GET,         // 处理 HTTP GET 请求
    .handler   = stream_handler,   // 处理程序为 stream_handler 函数
    .user_ctx  = NULL              // 不需要用户上下文数据
  };
  
  // 尝试启动 HTTPD 服务器，并注册根目录的 URI 处理程序
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &index_uri);
  }
}


void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  Serial.begin(115200);
  Serial.setDebugOutput(false);
  
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
  
  if(psramFound()){
    config.frame_size =  FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size =  FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  // Wi-Fi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  
  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.print(WiFi.localIP());
  
  // Start streaming web server
  startCameraServer();
}

void loop() {
  delay(1);
}

