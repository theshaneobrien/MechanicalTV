WebSocketsServer webSocket = WebSocketsServer(81);

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        // send message to client
        webSocket.sendTXT(num, "Connected");
            }
            break;
        case WStype_TEXT:
            /*if(length > 0)
              lastText = (const char*)payload;
            else
              lastText = "";*/
            break;
        case WStype_BIN:
            if(length == 1)
            {
              setTargetFps(payload[0] * 0.1f);
              //only needed if image is chopped up
              //line = payload[0];
            }
            else if(length > 1)
            {
              //Serial.println(length);
              //8bit
              unsigned char *pix = (unsigned char*)payload;
              int i = 0;
              for(int y = 0; y < yres; y++)
              {
                for(int x = 0; x < xres; x+= 2)
                {
                  frame[loadingFrame][x][y] = (pix[i] & 0xf) * 0x11;
                  frame[loadingFrame][x + 1][y] = (pix[i++] >> 4) * 0x11;
                }
              }
              int s = loadedFrame;
              loadedFrame = loadingFrame;
              loadingFrame = s;
              frameReady = true;
            }
            break;
    case WStype_ERROR:      
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
    }
}

void setupWebsocket()
{
  //Serial.println(WiFi.localIP());
	webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}
void loopWebsocket()
{
	webSocket.loop();
}