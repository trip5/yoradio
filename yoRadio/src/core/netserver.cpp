#include "netserver.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "ESPFileUpdater/ESPFileUpdater.h"
#include "config.h"
#include "player.h"
#include "telnet.h"
#include "display.h"
#include "options.h"
#include "network.h"
#include "mqtt.h"
#include "controls.h"
#include <Update.h>
#include <ESPmDNS.h>
#ifdef USE_SD
#include "sdmanager.h"
#endif
#ifndef MIN_MALLOC
#define MIN_MALLOC 24112
#endif
#ifndef NSQ_SEND_DELAY
  #define NSQ_SEND_DELAY       (TickType_t)100  //portMAX_DELAY?
#endif

// Global list for radio-browser servers to persist across searches
String g_ipv4_servers[20];

// For the search task
TaskHandle_t g_searchTaskHandle = NULL;
#define FS_REQUIRED_FREE_SPACE 150 // in KB - must be minimum x3 of the limit_per_page in search.js

//#define CORS_DEBUG

NetServer netserver;

AsyncWebServer webserver(80);
AsyncWebSocket websocket("/ws");
AsyncUDP udp;

String processor(const String& var);
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void handleUploadWeb(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void handleUpdate(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void handleHTTPArgs(AsyncWebServerRequest * request);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void vTaskSearchRadioBrowser(void *pvParameters);
void handleSearchPost(AsyncWebServerRequest *request);

bool  shouldReboot  = false;
#ifdef MQTT_ROOT_TOPIC
Ticker mqttplaylistticker;
bool  mqttplaylistblock = false;
void mqttplaylistSend() {
  mqttplaylistblock = true;
  mqttplaylistticker.detach();
  mqttPublishPlaylist();
  mqttplaylistblock = false;
}
#endif

char* updateError() {
  static char ret[140] = {0};
  sprintf(ret, "Update failed with error (%d)<br /> %s", (int)Update.getError(), Update.errorString());
  return ret;
}

void handleSearch(AsyncWebServerRequest *request) {
  // handle search request
  if (request->hasParam("search")) {
    if (g_searchTaskHandle != NULL) {
      request->send(429, "text/plain", "Search task is already running.");
      return;
    }
    String searchQuery = request->getParam("search")->value();
    char* search_str = new (std::nothrow) char[searchQuery.length() + 1];
    if (!search_str) {
      request->send(500, "text/plain", "Failed to allocate memory for search task.");
      return;
    }
    strcpy(search_str, searchQuery.c_str());
    // Create a task to run the search
    xTaskCreatePinnedToCore(vTaskSearchRadioBrowser, "searchRadioBrowser", 8192, (void*)search_str, 0, &g_searchTaskHandle, 0);
    request->send(200, "application/json", "{\"status\":\"searching\"}");
  } else {
    // If no search parameter, assume user wants the search page
    request->send(SPIFFS, "/www/search.html", "text/html", false, processor);
  }
}

void handleSearchPost(AsyncWebServerRequest *request) {
  // handle preview or add to playlist
  bool addtoplaylist = false;
  if (request->hasParam("addtoplaylist", true)) {
    if (request->getParam("addtoplaylist", true)->value() == "true") addtoplaylist = true;
  }
  if (!request->hasParam("url", true) || !request->hasParam("name", true)) {
    request->send(400, "text/plain", "Missing url or name");
    return;
  }
  String sUrl = request->getParam("url", true)->value();
  String sName = request->getParam("name", true)->value();
  sName.trim();
  sUrl.trim();
  if (sName.length() >= sizeof(config.station.name)) sName = sName.substring(0, sizeof(config.station.name) - 1);
  if (sUrl.length() >= sizeof(config.station.url)) sUrl = sUrl.substring(0, sizeof(config.station.url) - 1);
  if (!addtoplaylist) { // This is a preview
    config.loadStation(0); // Load into temporary station slot
    launchPlaybackTask(sUrl, sName);
    netserver.requestOnChange(GETINDEX, 0);
    request->send(200, "text/plain", "PREVIEW");
  } else { // This is add to playlist
    int sOvol = 0;
    // Check for duplicate URL before adding
    bool found = false;
    int foundIdx = 0;
    auto normalizeUrl = [](const String& url) -> String {
        String u = url;
        u.trim();
        if (u.startsWith("http://")) u = u.substring(7);
        else if (u.startsWith("https://")) u = u.substring(8);
        u.trim();
        return u;
        };
    String normNewUrl = normalizeUrl(sUrl);
    for (int i = 1; i <= config.store.countStation; ++i) {
      config.loadStation(i);
      String existingUrl = String(config.station.url);
      String normExistingUrl = normalizeUrl(existingUrl);
      if (normExistingUrl.equalsIgnoreCase(normNewUrl)) {
        found = true;
        foundIdx = i;
        break;
      }
    }
    if (found) { // play the slot if it already exists
      player.sendCommand({PR_PLAY, (uint16_t)foundIdx});
      request->send(200, "text/plain", "DUPLICATE");
    } else { // add it and play it
      File playlistfile = SPIFFS.open(PLAYLIST_PATH, "a");
      if (playlistfile) {
        playlistfile.printf("%s\t%s\t%d\r\n", sName.c_str(), sUrl.c_str(), sOvol);
        playlistfile.close();
        uint16_t newIdx = config.store.countStation + 1;
        config.indexPlaylist();
        config.initPlaylist();
        player.sendCommand({PR_PLAY, newIdx});
        netserver.requestOnChange(PLAYLISTSAVED, 0);
        request->send(200, "text/plain", "ADDED");
      } else {
        request->send(500, "text/plain", "Failed to open playlist file");
      }
    }
  }
}

bool NetServer::begin(bool quiet) {
  if(network.status==SDREADY) return true;
  if(!quiet) Serial.print("##[BOOT]#\tnetserver.begin\t");
  importRequest = IMDONE;
  irRecordEnable = false;
  nsQueue = xQueueCreate( 20, sizeof( nsRequestParams_t ) );
  while(nsQueue==NULL){;}
  if(config.emptyFS){
    webserver.on("/", HTTP_GET, [](AsyncWebServerRequest * request) { request->send_P(200, "text/html", emptyfs_html, processor); });
    webserver.on("/", HTTP_POST, [](AsyncWebServerRequest *request) { 
      if(request->arg("ssid")!="" && request->arg("pass")!=""){
        char buf[BUFLEN];
        memset(buf, 0, BUFLEN);
        snprintf(buf, BUFLEN, "%s\t%s", request->arg("ssid").c_str(), request->arg("pass").c_str());
        request->redirect("/");
        config.saveWifiFromNextion(buf);
        return;
      }
      request->redirect("/"); 
      ESP.restart(); 
    }, handleUploadWeb);
  }else{
    webserver.on("/", HTTP_ANY, handleHTTPArgs);
    webserver.on("/webboard", HTTP_GET, [](AsyncWebServerRequest * request) { request->send_P(200, "text/html", emptyfs_html, processor); });
    webserver.on("/webboard", HTTP_POST, [](AsyncWebServerRequest *request) { request->redirect("/"); }, handleUploadWeb);
  }
  
  webserver.on(PLAYLIST_PATH, HTTP_GET, handleHTTPArgs);
  webserver.on(INDEX_PATH, HTTP_GET, handleHTTPArgs);
  webserver.on(PLAYLIST_SD_PATH, HTTP_GET, handleHTTPArgs);
  webserver.on(INDEX_SD_PATH, HTTP_GET, handleHTTPArgs);
  webserver.on(SSIDS_PATH, HTTP_GET, handleHTTPArgs);
  
  webserver.on("/upload", HTTP_POST, beginUpload, handleUpload);
  webserver.on("/update", HTTP_GET, handleHTTPArgs);
  webserver.on("/update", HTTP_POST, beginUpdate, handleUpdate);
  webserver.on("/settings", HTTP_GET, handleHTTPArgs);
  if (IR_PIN != 255) webserver.on("/ir", HTTP_GET, handleHTTPArgs);
  webserver.on("/search", HTTP_GET, handleSearch);
  webserver.on("/search", HTTP_POST, handleSearchPost);
  webserver.serveStatic("/", SPIFFS, "/www/").setCacheControl("max-age=31536000");
#ifdef CORS_DEBUG
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), F("*"));
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Headers"), F("content-type"));
#endif
  webserver.begin();
  if(strlen(config.store.mdnsname)>0)
    MDNS.begin(config.store.mdnsname);
  websocket.onEvent(onWsEvent);
  webserver.addHandler(&websocket);

  //echo -n "helle?" | socat - udp-datagram:255.255.255.255:44490,broadcast
  if (udp.listen(44490)) {
    udp.onPacket([](AsyncUDPPacket packet) {
      if (strcmp((char*)packet.data(), "helle?") == 0)
        packet.println(WiFi.localIP());
    });
  }
  if(!quiet) Serial.println("done");
  return true;
}

void NetServer::beginUpdate(AsyncWebServerRequest *request) {
  shouldReboot = !Update.hasError();
  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK" : updateError());
  response->addHeader("Connection", "close");
  request->send(response);
}

void handleUpdate(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    int target = (request->getParam("updatetarget", true)->value() == "spiffs") ? U_SPIFFS : U_FLASH;
    Serial.printf("Update Start: %s\n", filename.c_str());
    player.sendCommand({PR_STOP, 0});
    display.putRequest(NEWMODE, UPDATING);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, target)) {
      Update.printError(Serial);
      request->send(200, "text/html", updateError());
    }
  }
  if (!Update.hasError()) {
    if (Update.write(data, len) != len) {
      Update.printError(Serial);
      request->send(200, "text/html", updateError());
    }
  }
  if (final) {
    if (Update.end(true)) {
      Serial.printf("Update Success: %uB\n", index + len);
    } else {
      Update.printError(Serial);
      request->send(200, "text/html", updateError());
    }
  }
}

void NetServer::beginUpload(AsyncWebServerRequest *request) {
  if (request->hasParam("plfile", true, true)) {
    netserver.importRequest = IMPL;
    request->send(200);
  } else if (request->hasParam("wifile", true, true)) {
    netserver.importRequest = IMWIFI;
    request->send(200);
  } else {
    request->send(404);
  }
}

size_t NetServer::chunkedHtmlPageCallback(uint8_t* buffer, size_t maxLen, size_t index){
  File requiredfile;
  bool sdpl = strcmp(netserver.chunkedPathBuffer, PLAYLIST_SD_PATH) == 0;
  if(sdpl){
    requiredfile = config.SDPLFS()->open(netserver.chunkedPathBuffer, "r");
  }else{
    requiredfile = SPIFFS.open(netserver.chunkedPathBuffer, "r");
  }
  if (!requiredfile) return 0;
  size_t filesize = requiredfile.size();
  size_t needread = filesize - index;
  if (!needread) {
    requiredfile.close();
    return 0;
  }
  size_t canread = (needread > maxLen) ? maxLen : needread;
  DBGVB("[%s] seek to %d in %s and read %d bytes with maxLen=%d", __func__, index, netserver.chunkedPathBuffer, canread, maxLen);
  requiredfile.seek(index, SeekSet);
  //vTaskDelay(1);
  requiredfile.read(buffer, canread);
  index += canread;
  if (requiredfile) requiredfile.close();
  return canread;
}

void NetServer::chunkedHtmlPage(const String& contentType, AsyncWebServerRequest *request, const char * path, bool doproc) {
  memset(chunkedPathBuffer, 0, sizeof(chunkedPathBuffer));
  strlcpy(chunkedPathBuffer, path, sizeof(chunkedPathBuffer)-1);
  AsyncWebServerResponse *response;
  if(doproc)
    response = request->beginChunkedResponse(contentType, chunkedHtmlPageCallback, processor);
  else
    response = request->beginChunkedResponse(contentType, chunkedHtmlPageCallback);
  request->send(response);
}

#ifndef DSP_NOT_FLIPPED
  #define DSP_CAN_FLIPPED true
#else
  #define DSP_CAN_FLIPPED false
#endif
#if !defined(HIDE_WEATHER) && (!defined(DUMMYDISPLAY) && !defined(USE_NEXTION))
  #define SHOW_WEATHER  true
#else
  #define SHOW_WEATHER  false
#endif

#ifndef NS_QUEUE_TICKS
  #define NS_QUEUE_TICKS 0
#endif

const char *getFormat(BitrateFormat _format) {
  switch (_format) {
    case BF_MP3:  return "MP3";
    case BF_AAC:  return "AAC";
    case BF_FLAC: return "FLC";
    case BF_OGG:  return "OGG";
    case BF_WAV:  return "WAV";
    case BF_VOR:  return "VOR";
    case BF_OPU:  return "OPU";
    default:      return "bitrate";
  }
}

char wsbuf[BUFLEN * 2];
void NetServer::processQueue(){
  if(nsQueue==NULL) return;
  nsRequestParams_t request;
  if(xQueueReceive(nsQueue, &request, NS_QUEUE_TICKS)){
    memset(wsbuf, 0, BUFLEN * 2);
    uint8_t clientId = request.clientId;
    switch (request.type) {
      case PLAYLIST:        getPlaylist(clientId); break;
      case PLAYLISTSAVED:   {
        #ifdef USE_SD
        if(config.getMode()==PM_SDCARD) {
        //  config.indexSDPlaylist();
          config.initSDPlaylist();
        }
        #endif
        if(config.getMode()==PM_WEB){
          config.indexPlaylist(); 
          config.initPlaylist(); 
        }
        getPlaylist(clientId); break;
      }
      case GETACTIVE: {
          bool dbgact = false, nxtn=false;
          String act = F("\"group_wifi\",");
          if (network.status == CONNECTED) {
                                                                act += F("\"group_system\",");
            if (BRIGHTNESS_PIN != 255 || DSP_CAN_FLIPPED || DSP_MODEL == DSP_NOKIA5110 || dbgact)    act += F("\"group_display\",");
          #ifdef USE_NEXTION
                                                                act += F("\"group_nextion\",");
            if (!SHOW_WEATHER || dbgact)                        act += F("\"group_weather\",");
            nxtn=true;
          #endif
                                                              #if defined(LCD_I2C) || defined(DSP_OLED)
                                                                act += F("\"group_oled\",");
                                                              #endif
                                                              #ifndef HIDE_VU
                                                                act += F("\"group_vu\",");
                                                              #endif
            if (BRIGHTNESS_PIN != 255 || nxtn || dbgact)                act += F("\"group_brightness\",");
            if (DSP_CAN_FLIPPED || dbgact)                      act += F("\"group_tft\",");
            if (TS_MODEL != TS_MODEL_UNDEFINED || dbgact)       act += F("\"group_touch\",");
            if (DSP_MODEL == DSP_NOKIA5110)                     act += F("\"group_nokia\",");
                                                                act += F("\"group_timezone\",");
            if (SHOW_WEATHER || dbgact)                         act += F("\"group_weather\",");
                                                                act += F("\"group_controls\",");
            if (ENC_BTNL != 255 || ENC2_BTNL != 255 || dbgact)  act += F("\"group_encoder\",");
            if (IR_PIN != 255 || dbgact)                        act += F("\"group_ir\",");
          }
                                                                act = act.substring(0, act.length() - 1);
          sprintf (wsbuf, "{\"act\":[%s]}", act.c_str());
          break;
        }
      case GETMODE:       sprintf (wsbuf, "{\"pmode\":\"%s\"}", network.status == CONNECTED ? "player" : "ap"); break;
      case GETINDEX:      {
          requestOnChange(STATION, clientId); 
          requestOnChange(TITLE, clientId); 
          requestOnChange(VOLUME, clientId); 
          requestOnChange(EQUALIZER, clientId); 
          requestOnChange(BALANCE, clientId); 
          requestOnChange(BITRATE, clientId); 
          requestOnChange(MODE, clientId); 
          requestOnChange(SDINIT, clientId);
          requestOnChange(GETPLAYERMODE, clientId); 
          if (config.getMode()==PM_SDCARD) { requestOnChange(SDPOS, clientId); requestOnChange(SDLEN, clientId); requestOnChange(SDSNUFFLE, clientId); } 
          return; 
          break;
        }
      case GETSYSTEM:     sprintf (wsbuf, "{\"sst\":%d,\"aif\":%d,\"vu\":%d,\"softr\":%d,\"vut\":%d,\"mdns\":\"%s\"}", 
                                  config.store.smartstart != 2, 
                                  config.store.audioinfo, 
                                  config.store.vumeter, 
                                  config.store.softapdelay,
                                  config.vuThreshold,
                                  config.store.mdnsname); 
                                  break;
      case GETSCREEN:     sprintf (wsbuf, "{\"flip\":%d,\"inv\":%d,\"nump\":%d,\"tsf\":%d,\"tsd\":%d,\"dspon\":%d,\"br\":%d,\"con\":%d,\"scre\":%d,\"scrt\":%d,\"scrb\":%d,\"scrpe\":%d,\"scrpt\":%d,\"scrpb\":%d}", 
                                  config.store.flipscreen, 
                                  config.store.invertdisplay, 
                                  config.store.numplaylist, 
                                  config.store.fliptouch, 
                                  config.store.dbgtouch, 
                                  config.store.dspon, 
                                  config.store.brightness, 
                                  config.store.contrast,
                                  config.store.screensaverEnabled,
                                  config.store.screensaverTimeout,
                                  config.store.screensaverBlank,
                                  config.store.screensaverPlayingEnabled,
                                  config.store.screensaverPlayingTimeout,
                                  config.store.screensaverPlayingBlank);
                                  break;
      case GETTIMEZONE:   sprintf (wsbuf, "{\"tz_name\":\"%s\",\"tzposix\":\"%s\",\"sntp1\":\"%s\",\"sntp2\":\"%s\"}",
                                  config.store.tz_name, 
                                  config.store.tzposix, 
                                  config.store.sntp1, 
                                  config.store.sntp2); 
                                  break;
      case GETWEATHER:    sprintf (wsbuf, "{\"wen\":%d,\"wlat\":\"%s\",\"wlon\":\"%s\",\"wkey\":\"%s\"}", 
                                  config.store.showweather, 
                                  config.store.weatherlat, 
                                  config.store.weatherlon, 
                                  config.store.weatherkey); 
                                  break;
      case GETCONTROLS:   sprintf (wsbuf, "{\"vols\":%d,\"enca\":%d,\"irtl\":%d,\"skipup\":%d}", 
                                  config.store.volsteps, 
                                  config.store.encacc, 
                                  config.store.irtlp,
                                  config.store.skipPlaylistUpDown); 
                                  break;
      case DSPON:         sprintf (wsbuf, "{\"dspontrue\":%d}", 1); break;
      case STATION:       requestOnChange(STATIONNAME, clientId); requestOnChange(ITEM, clientId); break;
      case STATIONNAME:   sprintf (wsbuf, "{\"nameset\": \"%s\"}", config.station.name); break;
      case ITEM:          sprintf (wsbuf, "{\"current\": %d}", config.lastStation()); break;
      case TITLE:         sprintf (wsbuf, "{\"meta\": \"%s\"}", config.station.title); telnet.printf("##CLI.META#: %s\n> ", config.station.title); break;
      case VOLUME:        sprintf (wsbuf, "{\"vol\": %d}", config.store.volume); telnet.printf("##CLI.VOL#: %d\n", config.store.volume); break;
      case NRSSI:         sprintf (wsbuf, "{\"rssi\": %d}", rssi); /*rssi = 255;*/ break;
      case SDPOS:         sprintf (wsbuf, "{\"sdpos\": %d,\"sdend\": %d,\"sdtpos\": %d,\"sdtend\": %d}", 
                                  player.getFilePos(), 
                                  player.getFileSize(), 
                                  player.getAudioCurrentTime(), 
                                  player.getAudioFileDuration()); 
                                  break;
      case SDLEN:         sprintf (wsbuf, "{\"sdmin\": %d,\"sdmax\": %d}", player.sd_min, player.sd_max); break;
      case SDSNUFFLE:     sprintf (wsbuf, "{\"snuffle\": %d}", config.store.sdsnuffle); break;
      case BITRATE:       sprintf (wsbuf, "{\"bitrate\": %d, \"format\": \"%s\"}", config.station.bitrate, getFormat(config.configFmt)); break;
      case MODE:          sprintf (wsbuf, "{\"mode\": \"%s\"}", player.status() == PLAYING ? "playing" : "stopped"); telnet.info(); break;
      case EQUALIZER:     sprintf (wsbuf, "{\"bass\": %d, \"middle\": %d, \"trebble\": %d}", config.store.bass, config.store.middle, config.store.trebble); break;
      case BALANCE:       sprintf (wsbuf, "{\"balance\": %d}", config.store.balance); break;
      case SDINIT:        sprintf (wsbuf, "{\"sdinit\": %d}", SDC_CS!=255); break;
      case GETPLAYERMODE: sprintf (wsbuf, "{\"playermode\": \"%s\"}", config.getMode()==PM_SDCARD?"modesd":"modeweb"); break;
      case SEARCH_DONE:   sprintf (wsbuf, "{\"search_done\":true}"); break;
      case SEARCH_FAILED: sprintf (wsbuf, "{\"search_failed\":true}"); break;
      #ifdef USE_SD
        case CHANGEMODE:    config.changeMode(newConfigMode); return; break;
      #endif
      default:          break;
    }
    if (strlen(wsbuf) > 0) {
      if (clientId == 0) { websocket.textAll(wsbuf); }else{ websocket.text(clientId, wsbuf); }
  #ifdef MQTT_ROOT_TOPIC
      if (clientId == 0 && (request.type == STATION || request.type == ITEM || request.type == TITLE || request.type == MODE)) mqttPublishStatus();
      if (clientId == 0 && request.type == VOLUME) mqttPublishVolume();
  #endif
    }
  }
}

void NetServer::loop() {
  if(network.status==SDREADY) return;
  if (shouldReboot) {
    Serial.println("Rebooting...");
    delay(100);
    ESP.restart();
  }
  websocket.cleanupClients();
  switch (importRequest) {
    case IMPL:    importPlaylist();  importRequest = IMDONE; break;
    case IMWIFI:  config.saveWifi(); importRequest = IMDONE; break;
    default:      break;
  }
  //if (rssi < 255) requestOnChange(NRSSI, 0);
  processQueue();
}

#if IR_PIN!=255
void NetServer::irToWs(const char* protocol, uint64_t irvalue) {
  char buf[BUFLEN] = { 0 };
  sprintf (buf, "{\"ircode\": %llu, \"protocol\": \"%s\"}", irvalue, protocol);
  websocket.textAll(buf);
}
void NetServer::irValsToWs() {
  if (!irRecordEnable) return;
  char buf[BUFLEN] = { 0 };
  sprintf (buf, "{\"irvals\": [%llu, %llu, %llu]}", config.ircodes.irVals[config.irindex][0], config.ircodes.irVals[config.irindex][1], config.ircodes.irVals[config.irindex][2]);
  websocket.textAll(buf);
}
#endif

void NetServer::onWsMessage(void *arg, uint8_t *data, size_t len, uint8_t clientId) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    char cmd[65], val[65];
    if (config.parseWsCommand((const char*)data, cmd, val, 65)) {
      if (strcmp(cmd, "getmode") == 0     ) { requestOnChange(GETMODE, clientId);     return; }
      if (strcmp(cmd, "getindex") == 0    ) { requestOnChange(GETINDEX, clientId);    return; }
      if (strcmp(cmd, "getsystem") == 0   ) { requestOnChange(GETSYSTEM, clientId);   return; }
      if (strcmp(cmd, "getscreen") == 0   ) { requestOnChange(GETSCREEN, clientId);   return; }
      if (strcmp(cmd, "gettimezone") == 0 ) { requestOnChange(GETTIMEZONE, clientId); return; }
      if (strcmp(cmd, "getcontrols") == 0 ) { requestOnChange(GETCONTROLS, clientId); return; }
      if (strcmp(cmd, "getweather") == 0  ) { requestOnChange(GETWEATHER, clientId);  return; }
      if (strcmp(cmd, "getactive") == 0   ) { requestOnChange(GETACTIVE, clientId);   return; }
      if (strcmp(cmd, "search_done") == 0 ) { websocket.textAll("{\"search_done\":true}"); return; }
      if (strcmp(cmd, "newmode") == 0     ) { newConfigMode = atoi(val); requestOnChange(CHANGEMODE, 0); return; }
      if (strcmp(cmd, "smartstart") == 0) {
        uint8_t valb = atoi(val);
        uint8_t ss = valb == 1 ? 1 : 2;
        if (!player.isRunning() && ss == 1) ss = 0;
        config.setSmartStart(ss);
        return;
      }
      if (strcmp(cmd, "audioinfo") == 0) {
        bool valb = static_cast<bool>(atoi(val));
        config.saveValue(&config.store.audioinfo, valb);
        display.putRequest(AUDIOINFO);
        return;
      }
      if (strcmp(cmd, "vumeter") == 0) {
        bool valb = static_cast<bool>(atoi(val));
        config.saveValue(&config.store.vumeter, valb);
        display.putRequest(SHOWVUMETER);
        return;
      }
      if (strcmp(cmd, "softap") == 0) {
        uint8_t valb = atoi(val);
        config.saveValue(&config.store.softapdelay, valb);
        return;
      }
      if (strcmp(cmd, "mdnsname") == 0) {
        config.saveValue(config.store.mdnsname, val, MDNS_LENGTH);
        return;
      }
      if (strcmp(cmd, "rebootmdns") == 0) {
        char buf[MDNS_LENGTH*2];
        if(strlen(config.store.mdnsname)>0)
          snprintf(buf, MDNS_LENGTH*2, "{\"redirect\": \"http://%s.local\"}", config.store.mdnsname);
        else
          snprintf(buf, MDNS_LENGTH*2, "{\"redirect\": \"http://%s/\"}", WiFi.localIP().toString().c_str());
        websocket.text(clientId, buf);
        delay(500);
        ESP.restart();
        return;
      }
      if (strcmp(cmd, "invertdisplay") == 0) {
        bool valb = static_cast<bool>(atoi(val));
        config.saveValue(&config.store.invertdisplay, valb);
        display.invert();
        return;
      }
      if (strcmp(cmd, "numplaylist") == 0) {
        bool valb = static_cast<bool>(atoi(val));
        config.saveValue(&config.store.numplaylist, valb);
        display.putRequest(NEWMODE, CLEAR); display.putRequest(NEWMODE, PLAYER);
        return;
      }
      if (strcmp(cmd, "fliptouch") == 0) {
        bool valb = static_cast<bool>(atoi(val));
        config.saveValue(&config.store.fliptouch, valb);
        flipTS();
        return;
      }
      if (strcmp(cmd, "dbgtouch") == 0) {
        bool valb = static_cast<bool>(atoi(val));
        config.saveValue(&config.store.dbgtouch, valb);
        return;
      }
      if (strcmp(cmd, "flipscreen") == 0) {
        bool valb = static_cast<bool>(atoi(val));
        config.saveValue(&config.store.flipscreen, valb);
        display.flip();
        display.putRequest(NEWMODE, CLEAR); display.putRequest(NEWMODE, PLAYER);
        return;
      }
      if (strcmp(cmd, "brightness") == 0) {
        uint8_t valb = atoi(val);
        if (!config.store.dspon) requestOnChange(DSPON, 0);
        config.store.brightness = valb;
        config.setBrightness(true);
        return;
      }
      if (strcmp(cmd, "screenon") == 0) {
        bool valb = static_cast<bool>(atoi(val));
        config.setDspOn(valb);
        return;
      }
      if (strcmp(cmd, "contrast") == 0) {
        uint8_t valb = atoi(val);
        config.saveValue(&config.store.contrast, valb);
        display.setContrast();
        return;
      }
      if (strcmp(cmd, "screensaverenabled") == 0) {
        bool valb = static_cast<bool>(atoi(val));
        config.saveValue(&config.store.screensaverEnabled, valb);
        #ifndef DSP_LCD
        display.putRequest(NEWMODE, PLAYER);
        #endif
        return;
      }
      if (strcmp(cmd, "screensavertimeout") == 0) {
        uint16_t valb = atoi(val);
        valb = constrain(valb,5,65520);
        config.saveValue(&config.store.screensaverTimeout, valb);
        #ifndef DSP_LCD
        display.putRequest(NEWMODE, PLAYER);
        #endif
        return;
      }
      if (strcmp(cmd, "screensaverblank") == 0) {
        bool valb = static_cast<bool>(atoi(val));
        config.saveValue(&config.store.screensaverBlank, valb);
        #ifndef DSP_LCD
        display.putRequest(NEWMODE, PLAYER);
        #endif
        return;
      }
      if (strcmp(cmd, "screensaverplayingenabled") == 0) {
        bool valb = static_cast<bool>(atoi(val));
        config.saveValue(&config.store.screensaverPlayingEnabled, valb);
        #ifndef DSP_LCD
        display.putRequest(NEWMODE, PLAYER);
        #endif
        return;
      }
      if (strcmp(cmd, "screensaverplayingtimeout") == 0) {
        uint16_t valb = atoi(val);
        valb = constrain(valb,5,65520);
        config.saveValue(&config.store.screensaverPlayingTimeout, valb);
        #ifndef DSP_LCD
        display.putRequest(NEWMODE, PLAYER);
        #endif
        return;
      }
      if (strcmp(cmd, "screensaverplayingblank") == 0) {
        bool valb = static_cast<bool>(atoi(val));
        config.saveValue(&config.store.screensaverPlayingBlank, valb);
        #ifndef DSP_LCD
        display.putRequest(NEWMODE, PLAYER);
        #endif
        return;
      }
      if (strcmp(cmd, "tz_name") == 0) {
        strlcpy(config.store.tz_name, val, sizeof(config.store.tz_name));
        config.saveValue(config.store.tz_name, config.store.tz_name, sizeof(config.store.tz_name), true, true);
        return;
      }
      if (strcmp(cmd, "tzposix") == 0) {
        strlcpy(config.store.tzposix, val, sizeof(config.store.tzposix));
        config.saveValue(config.store.tzposix, config.store.tzposix, sizeof(config.store.tzposix), true, true);
        network.forceTimeSync = true;
        network.requestTimeSync(true);
        return;
      }
      if (strcmp(cmd, "sntp2") == 0) {
        strlcpy(config.store.sntp2, val, sizeof(config.store.sntp2));
        config.saveValue(config.store.sntp2, config.store.sntp2, sizeof(config.store.sntp2), true, true);
        return;
      }
      if (strcmp(cmd, "sntp1") == 0) {
        strlcpy(config.store.sntp1, val, sizeof(config.store.sntp1));
        config.saveValue(config.store.sntp1, config.store.sntp1, sizeof(config.store.sntp1), true, true);
        network.forceTimeSync = true;
        network.requestTimeSync(true);
        return;
      }
      if (strcmp(cmd, "volsteps") == 0) {
        uint8_t valb = atoi(val);
        config.saveValue(&config.store.volsteps, valb);
        return;
      }
      if (strcmp(cmd, "encacceleration") == 0) {
        uint16_t valb = atoi(val);
        setEncAcceleration(valb);
        config.saveValue(&config.store.encacc, valb);
        return;
      }
      if (strcmp(cmd, "irtlp") == 0) {
        uint8_t valb = atoi(val);
        setIRTolerance(valb);
        return;
      }
      if (strcmp(cmd, "oneclickswitching") == 0) {
        bool valb = static_cast<bool>(atoi(val));
        config.saveValue(&config.store.skipPlaylistUpDown, valb);
        return;
      }
      if (strcmp(cmd, "showweather") == 0) {
        bool valb = static_cast<bool>(atoi(val));
        config.saveValue(&config.store.showweather, valb);
        network.trueWeather=false;
        network.forceWeather = true;
        display.putRequest(SHOWWEATHER);
        return;
      }
      if (strcmp(cmd, "lat") == 0) {
        strlcpy(config.store.weatherlat, val, sizeof(config.store.weatherlat));
        config.saveValue(config.store.weatherlat, config.store.weatherlat, sizeof(config.store.weatherlat), true, true);
        return;
      }
      if (strcmp(cmd, "lon") == 0) {
        strlcpy(config.store.weatherlon, val, sizeof(config.store.weatherlon));
        config.saveValue(config.store.weatherlon, config.store.weatherlon, sizeof(config.store.weatherlon), true, true);
        return;
      }
      if (strcmp(cmd, "key") == 0) {
        strlcpy(config.store.weatherkey, val, sizeof(config.store.weatherkey));
        config.saveValue(config.store.weatherkey, config.store.weatherkey, sizeof(config.store.weatherkey), true, true);
        network.trueWeather=false;
        display.putRequest(NEWMODE, CLEAR); display.putRequest(NEWMODE, PLAYER);
        return;
      }
      /*  RESETS  */
      if (strcmp(cmd, "reset") == 0) {
        if (strcmp(val, "system") == 0) {
          config.saveValue(&config.store.smartstart, (uint8_t)2, false);
          config.saveValue(&config.store.audioinfo, false, false);
          config.saveValue(&config.store.vumeter, false, false);
          config.saveValue(&config.store.softapdelay, (uint8_t)0, false);
          snprintf(config.store.mdnsname, MDNS_LENGTH, "yoradio-%x", config.getChipId());
          config.saveValue(config.store.mdnsname, config.store.mdnsname, MDNS_LENGTH, true, true);
          display.putRequest(NEWMODE, CLEAR); display.putRequest(NEWMODE, PLAYER);
          requestOnChange(GETSYSTEM, clientId);
          return;
        }
        if (strcmp(val, "screen") == 0) {
          config.saveValue(&config.store.flipscreen, false, false);
          display.flip();
          config.saveValue(&config.store.invertdisplay, false, false);
          display.invert();
          config.saveValue(&config.store.dspon, true, false);
          config.store.brightness = 100;
          config.setBrightness(false);
          config.saveValue(&config.store.contrast, (uint8_t)55, false);
          display.setContrast();
          config.saveValue(&config.store.numplaylist, false);
          config.saveValue(&config.store.screensaverEnabled, false);
          config.saveValue(&config.store.screensaverTimeout, (uint16_t)20);
          config.saveValue(&config.store.screensaverBlank, false);
          config.saveValue(&config.store.screensaverPlayingEnabled, false);
          config.saveValue(&config.store.screensaverPlayingTimeout, (uint16_t)20);
          config.saveValue(&config.store.screensaverPlayingBlank, false);
          display.putRequest(NEWMODE, CLEAR); display.putRequest(NEWMODE, PLAYER);
          requestOnChange(GETSCREEN, clientId);
          return;
        }
        if (strcmp(val, "timezone") == 0) {
          config.saveValue(config.store.tz_name, TIMEZONE_NAME, sizeof(config.store.tz_name), false);
          config.saveValue(config.store.tzposix, TIMEZONE_POSIX, sizeof(config.store.tzposix), false);
          config.saveValue(config.store.sntp1, SNTP1, sizeof(config.store.sntp1), false);
          config.saveValue(config.store.sntp2, SNTP2, sizeof(config.store.sntp2), false);
          network.forceTimeSync = true;
          network.requestTimeSync(true);
          requestOnChange(GETTIMEZONE, clientId);
          return;
        }
        if (strcmp(val, "weather") == 0) {
          config.saveValue(&config.store.showweather, false, false);
          config.saveValue(config.store.weatherlat, WEATHERLAT, sizeof(config.store.weatherlat), false);
          config.saveValue(config.store.weatherlon, WEATHERLON, sizeof(config.store.weatherlon), false);
          config.saveValue(config.store.weatherkey, "", WEATHERKEY_LENGTH);
          network.trueWeather=false;
          display.putRequest(NEWMODE, CLEAR); display.putRequest(NEWMODE, PLAYER);
          requestOnChange(GETWEATHER, clientId);
          return;
        }
        if (strcmp(val, "controls") == 0) {
          config.saveValue(&config.store.volsteps, (uint8_t)1, false);
          config.saveValue(&config.store.fliptouch, false, false);
          config.saveValue(&config.store.dbgtouch, false, false);
          config.saveValue(&config.store.skipPlaylistUpDown, false);
          
          setEncAcceleration(200);
          setIRTolerance(40);
          requestOnChange(GETCONTROLS, clientId);
          return;
        }
      } /*  EOF RESETS  */
      if (strcmp(cmd, "volume") == 0) {
        uint8_t v = atoi(val);
        player.setVol(v);
      }
      if (strcmp(cmd, "sdpos") == 0) {
        //return;
        if (config.getMode()==PM_SDCARD){
          config.sdResumePos = 0;
          if(!player.isRunning()){
            player.setResumeFilePos(atoi(val)-player.sd_min);
            player.sendCommand({PR_PLAY, config.store.lastSdStation});
          }else{
            player.setFilePos(atoi(val)-player.sd_min);
          }
        }
        return;
      }
      if (strcmp(cmd, "snuffle") == 0) {
        config.setSnuffle(strcmp(val, "true") == 0);
        return;
      }
      if (strcmp(cmd, "balance") == 0) {
        int8_t valb = atoi(val);
        player.setBalance(valb);
        config.setBalance(valb);
        netserver.requestOnChange(BALANCE, 0);
        return;
      }
      if (strcmp(cmd, "treble") == 0) {
        int8_t valb = atoi(val);
        player.setTone(config.store.bass, config.store.middle, valb);
        config.setTone(config.store.bass, config.store.middle, valb);
        netserver.requestOnChange(EQUALIZER, 0);
        return;
      }
      if (strcmp(cmd, "middle") == 0) {
        int8_t valb = atoi(val);
        player.setTone(config.store.bass, valb, config.store.trebble);
        config.setTone(config.store.bass, valb, config.store.trebble);
        netserver.requestOnChange(EQUALIZER, 0);
        return;
      }
      if (strcmp(cmd, "bass") == 0) {
        int8_t valb = atoi(val);
        player.setTone(valb, config.store.middle, config.store.trebble);
        config.setTone(valb, config.store.middle, config.store.trebble);
        netserver.requestOnChange(EQUALIZER, 0);
        return;
      }
      if (strcmp(cmd, "submitplaylist") == 0) {
        return;
      }
      if (strcmp(cmd, "submitplaylistdone") == 0) {
#ifdef MQTT_ROOT_TOPIC
        //mqttPublishPlaylist();
        mqttplaylistticker.attach(5, mqttplaylistSend);
#endif
        if (player.isRunning()) {
          player.sendCommand({PR_PLAY, -config.lastStation()});
        }
        return;
      }
#if IR_PIN!=255
      if (strcmp(cmd, "irbtn") == 0) {
        config.irindex = atoi(val);
        irRecordEnable = (config.irindex >= 0);
        config.irchck = 0;
        irValsToWs();
        if (config.irindex < 0) config.saveIR();
      }
      if (strcmp(cmd, "chkid") == 0) {
        config.irchck = atoi(val);
      }
      if (strcmp(cmd, "irclr") == 0) {
        uint8_t cl = atoi(val);
        config.ircodes.irVals[config.irindex][cl] = 0;
      }
#endif
    }
  }
}

void NetServer::getPlaylist(uint8_t clientId) {
  char buf[160] = {0};
  sprintf(buf, "{\"file\": \"http://%s%s\"}", WiFi.localIP().toString().c_str(), PLAYLIST_PATH);
  if (clientId == 0) { websocket.textAll(buf); } else { websocket.text(clientId, buf); }
}

uint8_t NetServer::_readPlaylistLine(File &file, char * line, size_t size){
  int bytesRead = file.readBytesUntil('\n', line, size);
  if(bytesRead>0){
    line[bytesRead] = 0;
    if(line[bytesRead-1]=='\r') line[bytesRead-1]=0;
  }
  return bytesRead;
}

bool NetServer::importPlaylist() {
  if(config.getMode()==PM_SDCARD) return false;
  File tempfile = SPIFFS.open(TMP_PATH, "r");
  if (!tempfile) {
    return false;
  }
  char sName[BUFLEN], sUrl[BUFLEN], linePl[BUFLEN*3];
  int sOvol;
  // Read first non-empty line
  String firstLine;
  size_t firstPos = tempfile.position();
  while (tempfile.available()) {
    _readPlaylistLine(tempfile, linePl, sizeof(linePl)-1);
    firstLine = String(linePl); firstLine.trim();
    if (firstLine.length() > 0) break;
    firstPos = tempfile.position();
  }
  tempfile.seek(firstPos); // rewind to first non-empty line
  // Detect minified JSON array (single line, starts with [)
  bool isJsonArray = firstLine.startsWith("[");
  bool foundAny = false;
  File playlistfile = SPIFFS.open(TMP2_PATH, "w");
  if (isJsonArray) {
    // Read the whole file into a String
    String jsonStr;
    tempfile.seek(0);
    while (tempfile.available()) {
      _readPlaylistLine(tempfile, linePl, sizeof(linePl)-1);
      jsonStr += String(linePl);
    }
    jsonStr.trim();
    // Remove leading/trailing brackets if present
    if (jsonStr.startsWith("[")) jsonStr = jsonStr.substring(1);
    if (jsonStr.endsWith("]")) jsonStr = jsonStr.substring(0, jsonStr.length()-1);
    // Robustly extract each {...} object using brace counting
    int len = jsonStr.length();
    int i = 0;
    while (i < len) {
      // Skip whitespace and commas
      while (i < len && (jsonStr[i] == ' ' || jsonStr[i] == '\n' || jsonStr[i] == '\r' || jsonStr[i] == ',')) i++;
      if (i >= len) break;
      if (jsonStr[i] != '{') { i++; continue; }
      int start = i;
      int brace = 1;
      i++;
      while (i < len && brace > 0) {
        if (jsonStr[i] == '{') brace++;
        else if (jsonStr[i] == '}') brace--;
        i++;
      }
      if (brace == 0) {
        String objStr = jsonStr.substring(start, i);
        objStr.trim();
        if (objStr.length() == 0) continue;
        strncpy(linePl, objStr.c_str(), sizeof(linePl)-1);
        if (config.parseJSONnew(linePl, sName, sUrl, sOvol)) {
          snprintf(linePl, sizeof(linePl)-1, "%s\t%s\t%d", sName, sUrl, sOvol);
          playlistfile.print(String(linePl) + "\r\n");
          foundAny = true;
        }
      }
    }
  } else {
    // Not a minified array: process line by line
    tempfile.seek(0);
    while (tempfile.available()) {
      _readPlaylistLine(tempfile, linePl, sizeof(linePl)-1);
      String trimmed = String(linePl); trimmed.trim();
      if (trimmed.length() == 0 || trimmed == "[" || trimmed == "]" || trimmed == ",") continue;
      // Only treat as JSON if line starts with '{' and ends with '}'
      if (trimmed.startsWith("{") && trimmed.endsWith("}")) {
        if (config.parseJSONnew(linePl, sName, sUrl, sOvol)) {
          snprintf(linePl, sizeof(linePl)-1, "%s\t%s\t%d", sName, sUrl, sOvol);
          playlistfile.print(String(linePl) + "\r\n");
          foundAny = true;
        }
      } else {
        // Only treat as CSV if not JSON
        if (config.parseCSVnew(linePl, sName, sUrl, sOvol)) {
          snprintf(linePl, sizeof(linePl)-1, "%s\t%s\t%d", sName, sUrl, sOvol);
          playlistfile.print(String(linePl) + "\r\n");
          foundAny = true;
        }
      }
    }
  }
  playlistfile.flush();
  playlistfile.close();
  tempfile.close();
  if (foundAny) {
    SPIFFS.remove(PLAYLIST_PATH);
    SPIFFS.rename(TMP2_PATH, PLAYLIST_PATH);
    requestOnChange(PLAYLISTSAVED, 0);
    return true;
  }
  SPIFFS.remove(TMP_PATH);
  SPIFFS.remove(TMP2_PATH);
  return false;
}

void NetServer::requestOnChange(requestType_e request, uint8_t clientId) {
  if(nsQueue==NULL) return;
  nsRequestParams_t nsrequest;
  nsrequest.type = request;
  nsrequest.clientId = clientId;
  xQueueSend(nsQueue, &nsrequest, NSQ_SEND_DELAY);
}

void NetServer::resetQueue(){
  if(nsQueue!=NULL) xQueueReset(nsQueue);
}

String processor(const String& var) { // %Templates%
  if (var == "ACTION") return (network.status == CONNECTED && !config.emptyFS)?"webboard":"";
  if (var == "UPLOADWIFI") return (network.status == CONNECTED)?" hidden":"";
  if (var == "VERSION") return YOVERSION;
  return String();
}

int freeSpace;
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    if(filename!="tempwifi.csv"){
      if(SPIFFS.exists(PLAYLIST_PATH)) SPIFFS.remove(PLAYLIST_PATH);
      if(SPIFFS.exists(INDEX_PATH)) SPIFFS.remove(INDEX_PATH);
      if(SPIFFS.exists(PLAYLIST_SD_PATH)) SPIFFS.remove(PLAYLIST_SD_PATH);
      if(SPIFFS.exists(INDEX_SD_PATH)) SPIFFS.remove(INDEX_SD_PATH);
    }
    freeSpace = (float)SPIFFS.totalBytes()/100*68-SPIFFS.usedBytes();
    request->_tempFile = SPIFFS.open(TMP_PATH , "w");
  }
  if (len) {
    if(freeSpace>index+len){
      request->_tempFile.write(data, len);
    }
  }
  if (final) {
    request->_tempFile.close();
  }
}

void handleUploadWeb(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  DBGVB("File: %s, size:%u bytes, index: %u, final: %s\n", filename.c_str(), len, index, final?"true":"false");
  if (!index) {
    String spath = "/www/";
    if(filename=="playlist.csv" || filename=="wifi.csv") spath = "/data/";
    request->_tempFile = SPIFFS.open(spath + filename , "w");
  }
  if (len) {
    request->_tempFile.write(data, len);
  }
  if (final) {
    request->_tempFile.close();
    if(filename=="playlist.csv") config.indexPlaylist();
  }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT: if (config.store.audioinfo) Serial.printf("[WEBSOCKET] client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str()); break;
    case WS_EVT_DISCONNECT: if (config.store.audioinfo) Serial.printf("[WEBSOCKET] client #%u disconnected\n", client->id()); break;
    case WS_EVT_DATA: netserver.onWsMessage(arg, data, len, client->id()); break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// Helper to select and randomize radio-browser servers
void selectRadioBrowserServer() { // No longer takes params, works on global g_ipv4_servers
  size_t arr_size = sizeof(g_ipv4_servers) / sizeof(g_ipv4_servers[0]);
  for (size_t i = 0; i < arr_size; ++i) g_ipv4_servers[i] = "";
  File serversFile = SPIFFS.open("/www/rb_srvrs.json", "r");
  if (!serversFile) {
    Serial.println("[Search] [Error] Failed to open /www/rb_srvrs.json - will try to get IP of all.api.radio-browser.info instead.");
    goto useIP;
  } else {
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, serversFile);
    serversFile.close();
    if (error) {
      Serial.print(F("[Search] [Error] deserializeJson() failed: "));
      Serial.println(error.c_str());
      goto useIP; // get out of the else
    }
    JsonArray servers = doc.as<JsonArray>();
    if (servers.isNull() || servers.size() == 0) {
      Serial.println("[Search] [Error] JSON is not a valid or is an empty array.");
      goto useIP; //get out of the else
    }
    // Collect unique IPv4 server names
    size_t count = 0;
    for (JsonObject server_obj : servers) {
      const char* ip = server_obj["ip"];
      if (ip && strchr(ip, '.')) { // It's an IPv4
        bool duplicate = false;
        for (size_t j = 0; j < count; ++j) {
          if (g_ipv4_servers[j] == ip) {
            duplicate = true;
            break;
          }
        }
        if (!duplicate && count < arr_size) {
          g_ipv4_servers[count++] = ip;
        }
      }
    }
    // Shuffle
    for (size_t i = count - 1; i > 0; --i) {
      size_t j = random(i + 1);
      String temp = g_ipv4_servers[i];
      g_ipv4_servers[i] = g_ipv4_servers[j];
      g_ipv4_servers[j] = temp;
    }

    IPAddress serverIP;
    if (WiFi.hostByName("all.api.radio-browser.info", serverIP)) {
      Serial.printf("Resolved IP: %s\n", serverIP.toString().c_str());
      g_ipv4_servers[0] = serverIP.toString();
    }
  }
  return;
useIP:
  IPAddress serverIP;
  if (WiFi.hostByName("all.api.radio-browser.info", serverIP)) {
    Serial.printf("Resolved IP: %s\n", serverIP.toString().c_str());
    g_ipv4_servers[0] = serverIP.toString();
  }
}

void vTaskSearchRadioBrowser(void *pvParameters) {
  char* search_str = (char*)pvParameters;
  Serial.printf("[Search] Starting radio browser search. Search: %s\n", search_str);
  // Check SPIFFS free space
  size_t freeSpace = SPIFFS.totalBytes() - SPIFFS.usedBytes();
  if (freeSpace < (FS_REQUIRED_FREE_SPACE * 1024)) {
    Serial.printf("[Search] [Error] Not enough free SPIFFS space: %u bytes. Aborting.\n", freeSpace);
    netserver.requestOnChange(SEARCH_FAILED, 0);
    delete[] search_str;
    g_searchTaskHandle = NULL;
    vTaskDelete(NULL);
    return;
  }
  // Count non-empty servers from our global persistent list
  size_t arr_size = sizeof(g_ipv4_servers) / sizeof(g_ipv4_servers[0]);
  int server_count = 0;
  for (size_t i = 0; i < arr_size; ++i) {
    if (g_ipv4_servers[i].length() > 0) server_count++;
  }
  // If the list is empty, it's the first run or all servers failed previously. Let's (re)populate it.
  if (server_count == 0) {
    Serial.println("[Search] Server list is empty, repopulating from file.");
    selectRadioBrowserServer();
    // Recount after filling
    server_count = 0;
    for (size_t i = 0; i < arr_size; ++i) {
      if (g_ipv4_servers[i].length() > 0) server_count++;
    }
  }
  // If still no servers, then the API source is likely down or unreachable.
  if (server_count == 0) {
    Serial.println("[Search] [Error] No IPv4 servers available after attempting to select.");
    netserver.requestOnChange(SEARCH_FAILED, 0);
    delete[] search_str;
    g_searchTaskHandle = NULL;
    vTaskDelete(NULL);
    return;
  }
  ESPFileUpdater searchResultsFetch(SPIFFS);
  const char* localPath = "/www/searchresults.json";
  bool success = false;
  bool server_retried = false;
  bool json_valid = false;
  for (size_t i = 0; i < arr_size; ++i) {
    if (g_ipv4_servers[i].length() == 0) continue;
    String server = g_ipv4_servers[i];
    // Compose the URL using the full search string
    String url = "http://" + server + "/json/stations/search?" + String(search_str);
    Serial.printf("[Search] Attempting to download from: %s\n", url.c_str());
    auto status = searchResultsFetch.checkAndUpdate(localPath, url, ESPFILEUPDATER_VERBOSE);
    if (status == ESPFileUpdater::UPDATED) {
      Serial.printf("[Search] Successfully downloaded from %s\n", server.c_str());
      // Check if the downloaded file ends with ']'
      File jsonFile = SPIFFS.open(localPath, "r");
      if (jsonFile) {
        int fileSize = jsonFile.size();
        char lastChar = 0;
        if (fileSize > 0) {
          for (int pos = fileSize - 1; pos >= 0; --pos) {
            jsonFile.seek(pos, SeekSet);
            char c = jsonFile.read();
            if (!isspace((unsigned char)c)) {
              lastChar = c;
              break;
            }
          }
        }
        jsonFile.close();
        if (lastChar != ']') {
          if (server_retried == true) {
            Serial.printf("[Search] [Warning] searchresults.json is incomplete. Not retrying.\n");
            server_retried = false;
          } else {
            Serial.printf("[Search] [Warning] searchresults.json is incomplete. Retrying same server.\n");
            server_retried = true;
            --i;
          }
          continue;
        } else {
          json_valid = true;
        }
      } else {
        if (server_retried == true) {
          Serial.println("[Search] [Error] Could not open searchresults.json for validation. Not retrying.\n");
          server_retried = false;
        } else {
          Serial.println("[Search] [Error] Could not open searchresults.json for validation. Retrying same server.\n");
          server_retried = true;
          --i;
        }
        continue;
      }
      if (json_valid) {
        // Write /www/search.txt with the actual search string (single line)
        File file = SPIFFS.open("/www/search.txt", "w");
        if (file) {
          file.printf("%s\n", search_str);
          file.close();
        } else {
          Serial.println("[Search] [Error] Failed to open search.txt for writing.");
        }
        success = true;
        break;
      } else {
        Serial.printf("[Search] [Error] Invalid JSON from %s. Removing from list.\n", server.c_str());
        g_ipv4_servers[i] = "";
        server_retried = false;
      }
    } else {
      Serial.printf("[Search] [Error] Failed to download from %s. Removing from persistent list.\n", server.c_str());
      g_ipv4_servers[i] = "";
      server_retried = false;
    }
  }
  if (success) {
    netserver.requestOnChange(SEARCH_DONE, 0);
  } else {
    Serial.println("[Search] [Error] Failed to download from all available servers.");
    netserver.requestOnChange(SEARCH_FAILED, 0);
  }
  delete[] search_str;
  search_str = nullptr;
  g_searchTaskHandle = NULL;
  vTaskDelete(NULL);
}

void launchPlaybackTask(const String& url, const String& name) {
  if (name.length() > 0 && name.length() < sizeof(config.station.name)) {
    strlcpy(config.station.name, name.c_str(), sizeof(config.station.name));
  } else {
    strlcpy(config.station.name, "Playing", sizeof(config.station.name));
  }
  player.sendCommand({PR_STOP, 0}); // Stop any current playback first
  display.putRequest(NEWSTATION, 0);
  Serial.println("[netserver] Creating a dedicated task for playback.");
  // Use a lambda to capture the URL and pass it to the task
  String* url_copy = new String(url);
  if (url_copy) {
    // Use a larger stack for HTTPS, as it requires more memory for SSL/TLS.
    UBaseType_t stackSize = url.startsWith("https://") ? 8192 : 4096;
    xTaskCreate(
        [](void* pvParameters) {
          String* urlToPlay = (String*)pvParameters;
          vTaskDelay(pdMS_TO_TICKS(100)); // A small delay can help the network stack release resources
          Serial.printf("[PlaybackTask] Starting playback for URL: %s. Free heap: %u\n", urlToPlay->c_str(), ESP.getFreeHeap());
          player.playUrl(urlToPlay->c_str());
          delete urlToPlay; // Free the string
          vTaskDelete(NULL);
        },
        "playbackTask",
        stackSize,
        (void*)url_copy,
        1,
        NULL
    );
  } else {
    Serial.println("[netserver] ERROR: Failed to allocate memory for playback task URL.");
  }
}

void handleHTTPArgs(AsyncWebServerRequest * request) {
  if (request->method() == HTTP_GET) {
    DBGVB("[%s] client ip=%s request of %s", __func__, request->client()->remoteIP().toString().c_str(), request->url().c_str());
    if (strcmp(request->url().c_str(), PLAYLIST_PATH) == 0 || 
        strcmp(request->url().c_str(), SSIDS_PATH) == 0 || 
        strcmp(request->url().c_str(), INDEX_PATH) == 0 || 
        strcmp(request->url().c_str(), TMP_PATH) == 0 || 
        strcmp(request->url().c_str(), PLAYLIST_SD_PATH) == 0 || 
        strcmp(request->url().c_str(), INDEX_SD_PATH) == 0) {
#ifdef MQTT_ROOT_TOPIC
      if (strcmp(request->url().c_str(), PLAYLIST_PATH) == 0) while (mqttplaylistblock) vTaskDelay(5);
#endif
      if(strcmp(request->url().c_str(), PLAYLIST_PATH) == 0 && config.getMode()==PM_SDCARD){
         netserver.chunkedHtmlPage("application/octet-stream", request, PLAYLIST_SD_PATH, false);
      }else{
        netserver.chunkedHtmlPage("application/octet-stream", request, request->url().c_str(), false);
      }
      return;
    }
    if (strcmp(request->url().c_str(), "/") == 0 && request->params() == 0) {
      netserver.chunkedHtmlPage(String(), request, network.status == CONNECTED ? "/www/index.html" : "/www/settings.html");
      return;
    }
    if (strcmp(request->url().c_str(), "/update") == 0 || strcmp(request->url().c_str(), "/settings") == 0 || strcmp(request->url().c_str(), "/ir") == 0) {
      char buf[40] = { 0 };
      sprintf(buf, "/www%s.html", request->url().c_str());
      netserver.chunkedHtmlPage(String(), request, buf);
      return;
    }
  }
  if (network.status == CONNECTED) {
    bool commandFound=false;
    if (request->hasArg("start")) { player.sendCommand({PR_PLAY, config.lastStation()}); commandFound=true; }
    if (request->hasArg("stop")) { player.sendCommand({PR_STOP, 0}); commandFound=true; }
    if (request->hasArg("toggle")) { player.toggle(); commandFound=true; }
    if (request->hasArg("prev")) { player.prev(); commandFound=true; }
    if (request->hasArg("next")) { player.next(); commandFound=true; }
    if (request->hasArg("volm")) { player.stepVol(false); commandFound=true; }
    if (request->hasArg("volp")) { player.stepVol(true); commandFound=true; }
    #ifdef USE_SD
    if (request->hasArg("mode")) {
      AsyncWebParameter* p = request->getParam("mode");
      int mm = atoi(p->value().c_str());
      if(mm>2) mm=0;
      if(mm==2)
        config.changeMode();
      else
        config.changeMode(mm);
      commandFound=true;
    }
    #endif
    if (request->hasArg("reset")) { request->redirect("/"); request->send(200); config.reset(); return; }
    if (request->hasArg("trebble") && request->hasArg("middle") && request->hasArg("bass")) {
      AsyncWebParameter* pt = request->getParam("trebble", request->method() == HTTP_POST);
      AsyncWebParameter* pm = request->getParam("middle", request->method() == HTTP_POST);
      AsyncWebParameter* pb = request->getParam("bass", request->method() == HTTP_POST);
      int t = atoi(pt->value().c_str());
      int m = atoi(pm->value().c_str());
      int b = atoi(pb->value().c_str());
      player.setTone(b, m, t);
      config.setTone(b, m, t);
      netserver.requestOnChange(EQUALIZER, 0);
      commandFound=true;
    }
    if (request->hasArg("ballance")) {
      AsyncWebParameter* p = request->getParam("ballance", request->method() == HTTP_POST);
      int b = atoi(p->value().c_str());
      player.setBalance(b);
      config.setBalance(b);
      netserver.requestOnChange(BALANCE, 0);
      commandFound=true;
    }
    if (request->hasArg("playstation") || request->hasArg("play")) {
      AsyncWebParameter* p = request->getParam(request->hasArg("playstation") ? "playstation" : "play", request->method() == HTTP_POST);
      int id = atoi(p->value().c_str());
      if (id < 1) id = 1;
      if (id > config.store.countStation) id = config.store.countStation;
      config.loadStation(id);
      launchPlaybackTask(config.station.url, config.station.name);
      commandFound=true;
      DBGVB("[%s] play=%d", __func__, id);
    }
    if (request->hasArg("vol")) {
      AsyncWebParameter* p = request->getParam("vol", request->method() == HTTP_POST);
      int v = atoi(p->value().c_str());
      if (v < 0) v = 0;
      if (v > 254) v = 254;
      config.store.volume = v;
      player.setVol(v);
      commandFound=true;
      DBGVB("[%s] vol=%d", __func__, v);
    }
    if (request->hasArg("dspon")) {
      AsyncWebParameter* p = request->getParam("dspon", request->method() == HTTP_POST);
      int d = atoi(p->value().c_str());
      config.setDspOn(d!=0);
      commandFound=true;
    }
    if (request->hasArg("dim")) {
      AsyncWebParameter* p = request->getParam("dim", request->method() == HTTP_POST);
      int d = atoi(p->value().c_str());
      if (d < 0) d = 0;
      if (d > 100) d = 100;
      config.store.brightness = (uint8_t)d;
      config.setBrightness(true);
      commandFound=true;
    }
    if (request->hasArg("sleep")) {
      AsyncWebParameter* sfor = request->getParam("sleep", request->method() == HTTP_POST);
      int sford = atoi(sfor->value().c_str());
      int safterd = 0;
      if(request->hasArg("after")){
        AsyncWebParameter* safter = request->getParam("after", request->method() == HTTP_POST);
        safterd = atoi(safter->value().c_str());
      }
      if(sford > 0 && safterd >= 0){
        request->send(200);
        config.sleepForAfter(sford, safterd);
        commandFound=true;
      }
    }
    if (request->hasArg("clearspiffs")) {
      if(config.spiffsCleanup()){
        config.saveValue(&config.store.play_mode, static_cast<uint8_t>(PM_WEB));
        request->redirect("/");
        ESP.restart();
      }else{
        request->send(200);
      }
      return;
    }

    if (request->hasArg("savewifi")) {
      AsyncWebParameter* p = request->getParam("savewifi", request->method() == HTTP_POST);
      config.saveWifiFromNextion(p->value().c_str());
      commandFound=true;
    }
    if(commandFound){
      request->redirect("/");
      request->send(200);
      return;
    }
  }
  request->send(404);
}
