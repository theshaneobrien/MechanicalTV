//the pins are configured for ESP32 S3
//!!!! Arduino and Events need to run on core 0. core 1 is used realtime for driver timings. 
//!!!! Please set Menu > Tools accordingly

#include <WiFi.h>
#include <WebSocketsServer.h>
#include "esp_task_wdt.h"
#include "credentials.h"

//pin configuration
const int STEP = 3;
const int DIR = 2;
const int OPTO = 4;
const int EN = 1;
const int LED = 21;

const int yres = 25;
const int xres = 32;

const int halfStepsPerRev = 400; //common nema17

volatile int loadingFrame = 0;//1;
volatile int shownFrame = 0;
volatile int loadedFrame = 0;//2;
volatile bool frameReady = false;
volatile uint8_t frame[3][xres][yres];

const float fpsStep = 0.2f;
float currentFps = 1.0f;
float targetFps = 1.f;
uint32_t ticksPerFrame = 1;
uint32_t ticksPerPixel = 1;
uint32_t ticksPerHalfStep = 1000000;
uint32_t ticksPerSecond = 240000000;
uint32_t ticksPerMilli = 240000;

void initWifi()
{
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  //WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void updateSteps()
{
  ticksPerMilli = ESP.getCpuFreqMHz() * 1000L;
  ticksPerSecond = ticksPerMilli * 1000L;
  ticksPerFrame = uint32_t(ticksPerSecond / currentFps);
  ticksPerPixel = ticksPerFrame / (xres * yres);
  ticksPerHalfStep = ticksPerFrame / halfStepsPerRev;
}

void setTargetFps(float fps)
{
  targetFps = fps;
  updateSteps();
}

void rampFps()
{
  if(targetFps > currentFps)
  {
    if(currentFps + fpsStep > targetFps)
      currentFps = targetFps;
    else
      currentFps += fpsStep;
    updateSteps();
  }
  if(targetFps < currentFps)
  {
    if(currentFps - fpsStep < targetFps)
      currentFps = targetFps;
    else
      currentFps -= fpsStep;
    updateSteps();
  }
}

void step(boolean dir, int steps, int delay)
{
  digitalWrite(DIR, dir);
  //delay(50);
  for (int i = 0; i < steps; i++) {
    digitalWrite(STEP, HIGH);
    delayMicroseconds(delay);  
    digitalWrite(STEP, LOW);
    delayMicroseconds(delay);  
  }
}

#include "websocket.h"

void wifiTask(void *param)
{
  while(true)
  {
    loopWebsocket();
    delay(20);
  }
}

void stepperTask(void *param)
{
  uint32_t t = ESP.getCycleCount();
  uint32_t t0 = t;
  uint32_t dtHalfStep = 0;
  uint32_t dtOpto = 0;
  uint64_t time = 0;
  int stepState = 0;
  bool frameStart = false;

  while(true)
//    Serial.println(digitalRead(OPTO)); //test of disk
  {
    t = ESP.getCycleCount();
    uint32_t dt = t - t0;
    t0 = t;
    time += (uint64_t)dt;
    dtOpto += dt;
    dtHalfStep += dt;

    if(time >= ticksPerSecond)
    {
      //Serial.println(time / ticksPerMilli);
      //time -= ticksPerSecond;
    }
    if(digitalRead(OPTO))
    {
      dtOpto = 0;
      frameStart = true;
    }
    else
    {
      if(frameStart)
      {
        frameStart = false;
        rampFps();
        if(frameReady)
        {
          int s = shownFrame;
          shownFrame = loadedFrame;
          loadedFrame = s;
          frameReady = false;
        }
        //Serial.println(time / ticksPerMilli);
      }
    }

    if(dtHalfStep >= ticksPerHalfStep)
    {
      digitalWrite(STEP, stepState);
      stepState ^= 1;
      dtHalfStep -= ticksPerHalfStep;
    }
    uint32_t pixelIndex = (dtOpto / ticksPerPixel);
    if(pixelIndex < xres * yres)
    {
      uint32_t subPixel = dtOpto % ticksPerPixel;
      uint32_t x = xres - 1 - (pixelIndex / yres);
      uint32_t y = yres - 1 - (pixelIndex % yres);
      if(frame[shownFrame][x][y] >= 255 - (subPixel * 255) / ticksPerPixel)
        digitalWrite(LED, 1);
      else
        digitalWrite(LED, 0);
    }
  }
}

void initFrame()
{
  for(int y = 0; y < yres; y++)
  {
    for(int x = 0; x < xres; x++)
    {
      frame[shownFrame][x][y] = 256 * x / xres;
    }
  }
}

void setup()
{
  esp_task_wdt_deinit();
  Serial.begin(115200);
  initWifi();
  setupWebsocket();
  pinMode(DIR, OUTPUT); 
  pinMode(STEP, OUTPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(DIR, 0);
  pinMode(OPTO, INPUT_PULLDOWN);
  pinMode(EN, OUTPUT); 
  initFrame();
  setTargetFps(10);
  TaskHandle_t xHandle = NULL;
  xTaskCreatePinnedToCore(stepperTask, "stepper", 2000, 0,  ( 2 | portPRIVILEGE_BIT ), &xHandle, 1);
  xTaskCreatePinnedToCore(wifiTask, "wifiStuff", 20000, 0,  ( 2 | portPRIVILEGE_BIT ), &xHandle, 0);
}

void loop()
{
}
  