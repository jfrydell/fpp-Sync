#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cstring>

#include <unistd.h>
#include <termios.h>
#include <chrono>
#include <thread>

#include <curl/curl.h>

#include "CurlManager.h"

#include "common.h"
#include "settings.h"
#include "MultiSync.h"
#include "Plugin.h"
#include "Plugins.h"
#include "Sequence.h"
#include "log.h"


enum {
    SET_SEQUENCE_NAME = 1,
    SET_MEDIA_NAME    = 2,

    START_SEQUENCE    = 3,
    START_MEDIA       = 4,
    STOP_SEQUENCE     = 5,
    STOP_MEDIA        = 6,
    SYNC              = 7,
    
    BLANK             = 9
};

void callback(CURL* curl) {
    LogWarn(VB_SYNC, "CURL callback!");
}

class LoRaMultiSyncPlugin : public MultiSyncPlugin  {
public:
    
    LoRaMultiSyncPlugin() {}
    virtual ~LoRaMultiSyncPlugin() {}

    bool Init() {
        LogWarn(VB_SYNC, "Started thing!");
        return true;
    }

    void send(char *buf, int len) {
        LogWarn(VB_SYNC, "Sending data WOW!");
        CURL *curl;
        curl = curl_easy_init();
        if(curl) {
            curl_easy_setopt(curl, CURLOPT_URL, "192.168.168.230:9000");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, len);
            curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, buf);
            CurlManager::INSTANCE.addCURL(curl, callback);
        }
    }
    
    void SendSync(uint32_t frames, float seconds) {
        int diff = frames - lastSentFrame;
        float diffT = seconds - lastSentTime;
        bool sendSync = false;
        if (diffT > 0.5) {
            sendSync = true;
        } else if (!frames) {
            // no need to send the 0 frame
        } else if (frames < 32) {
            //every 8 at the start
            if (frames % 8 == 0) {
                sendSync = true;
            }
        } else if (diff == 16) {
            sendSync = true;
        }
        
        if (sendSync) {
            char buf[120];
            buf[0] = SYNC;
            memcpy(&buf[1], &frames, 4);
            memcpy(&buf[5], &seconds, 4);
            send(buf, 9);

            lastSentFrame = frames;
            lastSentTime = seconds;
        }
        lastFrame = frames;
    }

    virtual void SendSeqOpenPacket(const std::string &filename) override {
        char buf[256];
        strcpy(&buf[1], filename.c_str());
        buf[0] = SET_SEQUENCE_NAME;
        send(buf, filename.length() + 2);
        lastSequence = filename;
        lastFrame = -1;
        lastSentTime = -1.0f;
        lastSentFrame = -1;
    }
    virtual void SendSeqSyncStartPacket(const std::string &filename) override {
        if (filename != lastSequence) {
            SendSeqOpenPacket(filename);
        }
        char buf[2];
        buf[0] = START_SEQUENCE;
        send(buf, 1);
        lastFrame = -1;
        lastSentTime = -1.0f;
        lastSentFrame = -1;
    }
    virtual void SendSeqSyncStopPacket(const std::string &filename) override {
        char buf[2];
        buf[0] = STOP_SEQUENCE;
        send(buf, 1);
        lastSequence = "";
        lastFrame = -1;
        lastSentTime = -1.0f;
        lastSentFrame = -1;
    }
    virtual void SendSeqSyncPacket(const std::string &filename, int frames, float seconds) override {
        if (filename != lastSequence) {
            SendSeqSyncStartPacket(filename);
        }
        SendSync(frames, seconds);
    }
    
    virtual void SendMediaOpenPacket(const std::string &filename) override {
        if (sendMediaSync) {
            char buf[256];
            strcpy(&buf[1], filename.c_str());
            buf[0] = SET_MEDIA_NAME;
            send(buf, filename.length() + 2);
            lastMedia = filename;
            lastFrame = -1;
            lastSentTime = -1.0f;
            lastSentFrame = -1;
        }
    }
    virtual void SendMediaSyncStartPacket(const std::string &filename) override {
        if (sendMediaSync) {
            if (filename != lastMedia) {
                SendSeqOpenPacket(filename);
            }
            char buf[2];
            buf[0] = START_MEDIA;
            send(buf, 1);
            lastFrame = -1;
            lastSentTime = -1.0f;
            lastSentFrame = -1;
        }
    }
    virtual void SendMediaSyncStopPacket(const std::string &filename) override {
        if (sendMediaSync) {
            char buf[2];
            buf[0] = STOP_MEDIA;
            send(buf, 1);
            lastMedia = "";
            lastFrame = -1;
            lastSentTime = -1.0f;
            lastSentFrame = -1;
        }
    }
    virtual void SendMediaSyncPacket(const std::string &filename, float seconds) override {
        if (sendMediaSync) {
            if (filename != lastMedia) {
                SendMediaSyncStartPacket(filename);
            }
            SendSync(lastFrame > 0 ? lastFrame : 0, seconds);
        }
    }
    
    virtual void SendBlankingDataPacket(void) override {
        char buf[2];
        buf[0] = BLANK;
        send(buf, 1);
    }
    
    bool fullCommandRead(int &commandSize) {
        if (curPosition == 0) {
            return false;
        }
        switch (readBuffer[0]) {
        case SET_SEQUENCE_NAME:
        case SET_MEDIA_NAME:
            //need null terminated string
            for (commandSize = 0; commandSize < curPosition; commandSize++) {
                if (readBuffer[commandSize] == 0) {
                    commandSize++;
                    return true;
                }
            }
            return false;
        case SYNC:
            commandSize = 9;
            return curPosition >= 9;
        case START_SEQUENCE:
        case START_MEDIA:
        case STOP_SEQUENCE:
        case STOP_MEDIA:
        case BLANK:
            commandSize = 1;
            break;
        default:
            commandSize = 1;
            return false;
        }
        return true;
    }

    bool loadSettings() {
        bool enabled = false;
        if (FileExists(FPP_DIR_CONFIG("/plugin.fpp-LoRa"))) {
            std::ifstream infile(FPP_DIR_CONFIG("/plugin.fpp-LoRa"));
            std::string line;
            while (std::getline(infile, line)) {
                std::istringstream iss(line);
                std::string a, b, c;
                if (!(iss >> a >> b >> c)) { break; } // error
                
                c.erase(std::remove( c.begin(), c.end(), '\"' ), c.end());
                if (a == "LoRaEnable") {
                    enabled = (c == "1");
                }
            }
        }
        return enabled;
    }

    std::string lastSequence;
    std::string lastMedia;
    bool sendMediaSync = true;
    int lastFrame = -1;
    
    float lastSentTime = -1.0f;
    int lastSentFrame = -1;
    
    char readBuffer[256];
    int curPosition = 0;

};


class LoRaFPPPlugin : public FPPPlugin {
public:
    LoRaMultiSyncPlugin *plugin = new LoRaMultiSyncPlugin();
    bool enabled = false;
    
    LoRaFPPPlugin() : FPPPlugin("LoRa") {
        enabled = plugin->loadSettings();
    }
    virtual ~LoRaFPPPlugin() {}
    
    void registerApis(httpserver::webserver *m_ws) {
        //at this point, most of FPP is up and running, we can register our MultiSync plugin
        if (enabled && plugin->Init()) {
            //only register the sender for master mode
            multiSync->addMultiSyncPlugin(plugin);
        } else {
            enabled = false;
        } 
    }
};


extern "C" {
    FPPPlugin *createPlugin() {
        return new LoRaFPPPlugin();
    }
}
