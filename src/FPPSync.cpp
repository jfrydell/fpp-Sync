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

    void send(Json::Value &req) {
        CURL* curl = curl_easy_init();
        if (curl) {
            std::string body = SaveJsonToString(req, std::string());
            LogDebug(VB_SYNC, "Sending: %s\n", body.c_str());
            curl_easy_setopt(curl, CURLOPT_URL, "fpp.fletchrydell.com");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            struct curl_slist *hs = NULL;
            hs = curl_slist_append(hs, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.length());
            curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, body.c_str());
            CurlManager::INSTANCE.addCURL(std::string("fpp.fletchrydell.com"), curl, std::bind(&SyncMultiSyncPlugin::callback, this, std::placeholders::_1), true);
        } else {
            LogErr(VB_SYNC, "CURL init failed\n");
        }
    }
    
    void SendSync(float seconds) {
        LogDebug(VB_SYNC, "SendSync: %f\n", seconds);
        float diffT = seconds - lastSentTime;
        if (diffT > 0.5 || diffT < 0.0) {
            // It's been 0.5 seconds, send a sync request
            Json::Value req;
            req["type"] = "sync";
            req["id"] = lastMediaId;
            req["time"] = seconds;
            Json::Value &latencies = req["latencies"];
            latencies.resize(3);
            latencies[0] = serverLatency[0];
            latencies[1] = serverLatency[1];
            latencies[2] = serverLatency[2];
            send(req);
            lastSentTime = seconds;
        }
    }
    
    virtual void SendMediaOpenPacket(const std::string &filename) override {
        Json::Value req;
        req["type"] = "media_start";
        req["id"] = ++lastMediaId;
        req["filename"] = filename;
        send(req);
        lastMedia = filename;
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
        Json::Value req;
        req["type"] = "media_stop";
        req["id"] = ++lastMediaId;
        send(req);
        lastMedia = "";
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
