#include <fpp-pch.h>

#include <string>
#include <algorithm>
#include <cstring>
#include <functional>

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

class SyncMultiSyncPlugin : public MultiSyncPlugin  {
public:
    
    SyncMultiSyncPlugin() {}
    virtual ~SyncMultiSyncPlugin() {}

    bool Init() {
        LogInfo(VB_SYNC, "Started web sync plugin\n");
        return true;
    }

    void callback(CURL* curl) {
        // Check response code
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code != 200) {
            LogWarn(VB_SYNC, "CURL callback: response code %ld\n", response_code);
            return;
        }

        // Estimate time between our send and server receive (includes all of APPCONNECT and half of TOTAL_TIME after that)
        double connect_time = 0.0;
        curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME, &connect_time);
        double total_time = 0.0;
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);
        double server_delay = (total_time + connect_time) / 2.0;
        if (server_delay <= 0.0) {
            LogWarn(VB_SYNC, "CURL callback: server delay %f\n", server_delay);
            return;
        }
        LogDebug(VB_SYNC, "Estimated CURL latency: %f\n", server_delay);

        // Update latest latencies
        serverLatency[2] = serverLatency[1];
        serverLatency[1] = serverLatency[0];
        serverLatency[0] = server_delay;
    }

    void send(char *buf, CURL* curl = nullptr) {
        if (!curl) curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, "192.168.168.230:9000");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, buf);
            CurlManager::INSTANCE.addCURL(curl, std::bind(&SyncMultiSyncPlugin::callback, this, std::placeholders::_1));
        } else {
            LogErr(VB_SYNC, "CURL init failed\n");
        }
    }
    
    void SendSync(float seconds) {
        LogDebug(VB_SYNC, "SendSync: %f\n", seconds);
        float diffT = seconds - lastSentTime;
        if (diffT > 0.5 || diffT < 0.0) {
            // It's been 0.5 seconds, send a sync request
            char buf[128];
            sprintf(buf, "type=sync&id=%d&time=%f&l1=%f&l2=%f&l3=%f", lastMediaId, seconds, serverLatency[0], serverLatency[1], serverLatency[2]);
            send(buf);
            lastSentTime = seconds;
        }
    }
    
    virtual void SendMediaOpenPacket(const std::string &filename) override {
        CURL *curl;
        curl = curl_easy_init();
        if (!curl) {
            LogErr(VB_SYNC, "CURL init failed\n");
        }
        char* filename_enc = curl_easy_escape(curl, filename.c_str(), filename.length());
        char buf[1024];
        sprintf(buf, "type=media_start&id=%d&filename=%s", lastMediaId, filename_enc);
        curl_free(filename_enc);
        send(buf);
        lastMedia = filename;
        lastMediaId++;
        lastSentTime = -1.0f;
    }
    virtual void SendMediaSyncStartPacket(const std::string &filename) override {
        if (filename != lastMedia) {
            SendSeqOpenPacket(filename);
        }
        lastSentTime = -1.0f;
        SendSync(0.0f);
    }
    virtual void SendMediaSyncStopPacket(const std::string &filename) override {
        char buf[64];
        sprintf(buf, "type=media_stop&id=%d", lastMediaId);
        send(buf);
        lastMedia = "";
        lastMediaId++;
        lastSentTime = -1.0f;
    }
    virtual void SendMediaSyncPacket(const std::string &filename, float seconds) override {
        if (filename != lastMedia) {
            SendMediaOpenPacket(filename);
        }
        SendSync(seconds);
    }

    bool loadSettings() {
        bool enabled = false;
        if (FileExists(FPP_DIR_CONFIG("/plugin.fpp-Sync"))) {
            std::ifstream infile(FPP_DIR_CONFIG("/plugin.fpp-Sync"));
            std::string line;
            while (std::getline(infile, line)) {
                std::istringstream iss(line);
                std::string a, b, c;
                if (!(iss >> a >> b >> c)) { break; } // error
                
                c.erase(std::remove( c.begin(), c.end(), '\"' ), c.end());
                if (a == "SyncEnable") {
                    enabled = (c == "1");
                }
            }
        }
        return enabled;
    }

    std::string lastMedia;
    int lastMediaId = 0;
    float lastSentTime = -1.0f;
    double serverLatency[3] = {0, 0, 0};
};


class SyncFPPPlugin : public FPPPlugin {
public:
    SyncMultiSyncPlugin *plugin = new SyncMultiSyncPlugin();
    bool enabled = false;
    
    SyncFPPPlugin() : FPPPlugin("Sync") {
        enabled = plugin->loadSettings();
    }
    virtual ~SyncFPPPlugin() {}
    
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
        return new SyncFPPPlugin();
    }
}
