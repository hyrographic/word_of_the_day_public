#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "settings_assets.h"
#include "sleep_manager.h"
#include "led_controller.h"

const char settings_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
	<head>
		<meta charset="UTF-8">
		<meta name="viewport" content="width=device-width, initial-scale=1.0">
		<title>Word Box WiFi Setup</title>
		<style>
			* { box-sizing: border-box; font-family: Arial;}
			body { font-family: 'Jersey 10', monospace; background-color: white; padding: 10px; margin: 0; }
			.container { max-width: 800px; margin: 0 auto; width: 100%; }
			.header { background-color: white; color: white; padding-top: 10px; display: flex; align-items: center; gap: 15px; margin-bottom: 0px; font-size: 24px; letter-spacing: 2px; }
			.header-img { max-width: 100%; height: auto; object-fit: contain; display: block}
			.section-img { max-width: 50%; margin-left: 5px;}
			.section-img-follower { margin-left: 5px; }
            .section { padding: 5px 0px 0px 0px; }
            .form-group { display: flex; flex-wrap: wrap; align-items: stretch; margin-top: 10px; padding-left: 15px; padding-bottom: 5px; gap: 10px; }
            .form-label-img { width: auto; flex-shrink: 0; align-self: center;}
            .form-input { flex: 1 1 150px; min-width: 0; width: 100%; border: 3px solid black; font-family: "Arial"; align-self: center; }
            #recallOptions { display: flex; flex-direction: column; align-items: flex-start; width: 100%; margin-top: 20px; margin-left: 15px; gap: 10px; }
            #downloadButton { max-width: 600px; aspect-ratio: initial; height: auto;}
            #deleteButton { max-width: 600px; aspect-ratio: initial; height: auto;}
            .save-section { display: flex; justify-content: flex-end; margin-top: 10px; }
            #saveButton {width: 25%;}
            #scanNetwork {margin-left: 15px; margin-top: 2px ; width: 25%;}
            .status-message { display: none; padding: 10px; margin-bottom: 10px; border-radius: 4px; text-align: center; }
            .status-info { background-color: #e3f2fd; color: #1565c0; }
            .status-success { background-color: #e8f5e9; color: #2e7d32; }
            .status-error { background-color: #ffebee; color: #c62828; }
            .wifi-list { display: none; margin: 10px 15px; max-height: 200px; overflow-y: auto; border: 2px solid #ccc; border-radius: 4px; }
            .wifi-item { padding: 10px; cursor: pointer; display: flex; justify-content: space-between; border-bottom: 1px solid #eee; }
            .wifi-item:hover { background-color: #f5f5f5; }
            .wifi-item.selected { background-color: #e3f2fd; }
		</style>
	</head>
	<body>
		<div class="container">
			<img id="headerImg" class="header-img" src="svg/header_image.svg">
			<div id="statusMessage" class="status-message"></div>
			<div class="section">
					<img class="header-img section-img" id="wifiSetup" src="svg/wifi_setup.svg">
					<img src="svg/scan_network.svg" id="scanNetwork" onclick="scanWiFi()" style="cursor: pointer;">
				<div class="form-group">
					<img class="form-label-img" src="svg/Network.svg">
					<input type="text" id="ssid" class="form-input" placeholder="Enter WiFi SSID" value="">
				</div>
				<div class="form-group">
					<img class="form-label-img" src="svg/Password.svg">
					<input type="password" id="password" class="form-input" placeholder="Enter WiFi Password" value="">
				</div>
				<!-- <button class="btn btn-scan" onclick="scanWiFi()">Scan for Networks</button> -->
				<div id="wifiList" class="wifi-list"></div>
				<div class="save-section">
					<img class="section-img" id="saveButton" src="svg/save.svg" onclick="saveSettings()" style="cursor: pointer;">
				</div>
			</div>
            <div class="section">
                <img class="header-img section-img-follower" id="recallHistory" src="svg/recall_history.svg">
                <div id="recallOptions">
                <img id="downloadButton" src="svg/download_recall_history.svg" onclick="downloadHistory()" style="cursor: pointer;">
                <img id="deleteButton" src="svg/delete_recall_history.svg" onclick="deleteHistory()" style="cursor: pointer;">
                </div>
            </div>
		</div>
		<script>
			function getSignalBars(rssi) {
			    if (rssi > -50) return '████';
			    if (rssi > -60) return '███░';
			    if (rssi > -70) return '██░░';
			    return '█░░░';
			}

			function syncImageHeights() {
			    const headerImg = document.getElementById('headerImg');
			    if (!headerImg) return;
			    const headerHeight = headerImg.offsetHeight;
			    document.querySelectorAll('.section-img').forEach(img => {
			        img.style.height = headerHeight + 'px';
			    });
			    document.querySelectorAll('.form-label-img').forEach(img => {
			        img.style.height = (headerHeight / 2) + 'px';
			    });
			    document.querySelectorAll('.form-input').forEach(input => {
			        input.style.height = (headerHeight / 3) + 15 + 'px';
			    });
			    const recallHeight = headerHeight / 1.5;
			    document.getElementById('downloadButton').style.height = recallHeight + 'px';
			    document.getElementById('deleteButton').style.height = recallHeight + 'px';
			    document.getElementById('recallHistory').style.height = recallHeight*0.85 + 'px';

			}
			window.addEventListener('load', () => {
			    loadSettings();
			    // Wait for images to fully render before syncing heights
			    const images = document.querySelectorAll('img');
			    let loadedCount = 0;
			    const checkAllLoaded = () => {
			        loadedCount++;
			        if (loadedCount >= images.length) {
			            syncImageHeights();
			        }
			    };
			    images.forEach(img => {
			        if (img.complete) {
			            checkAllLoaded();
			        } else {
			            img.addEventListener('load', checkAllLoaded);
			        }
			    });
			});
			window.addEventListener('resize', syncImageHeights);

			async function loadSettings() {
			    try {
			        const response = await fetch('/api/settings');
			        if (response.ok) {
			            const settings = await response.json();
			            if (settings.ssid) document.getElementById('ssid').value = settings.ssid;
			            if (settings.password) document.getElementById('password').value = settings.password;
			        }
			    } catch (error) {
			        console.error('Failed to load settings:', error);
			    }
			}

			let statusTimeout = null;
			function showStatus(msg, type) {
			    const el = document.getElementById('statusMessage');
			    el.textContent = msg;
			    el.className = 'status-message status-' + type;
			    el.style.display = 'block';
			    if (statusTimeout) clearTimeout(statusTimeout);
			    statusTimeout = setTimeout(() => { el.style.display = 'none'; }, 5000);
			}
			async function scanWiFi() {
			    showStatus('Scanning for WiFi networks...', 'info');
			    try {
			        const response = await fetch('/api/scan');
			        if (response.ok) {
			            const networks = await response.json();
			            displayWiFiNetworks(networks);
			            showStatus('Found ' + networks.length + ' networks', 'success');
			        } else {
			            showStatus('Failed to scan networks', 'error');
			        }
			    } catch (error) {
			        showStatus('Error scanning networks', 'error');
			    }
			}
			function displayWiFiNetworks(networks) {
			    const ssidInput = document.getElementById('ssid');
			    const currentValue = ssidInput.value;

			    // Create select dropdown
			    const select = document.createElement('select');
			    select.id = 'ssid';
			    select.className = 'form-input';

			    // Add empty option
			    const emptyOption = document.createElement('option');
			    emptyOption.value = '';
			    emptyOption.textContent = 'Select a network...';
			    select.appendChild(emptyOption);

			    // Add network options
			    networks.forEach(network => {
			        const option = document.createElement('option');
			        option.value = network.ssid;
			        option.textContent = getSignalBars(network.rssi) + ' | ' + network.ssid;
			        if (network.ssid === currentValue) option.selected = true;
			        select.appendChild(option);
			    });

			    // Focus password on selection of encrypted network
			    select.onchange = function() {
			        const selectedNetwork = networks.find(n => n.ssid === select.value);
			        if (selectedNetwork && selectedNetwork.encrypted) {
			            document.getElementById('password').focus();
			        }
			    };

			    // Replace input with select
			    ssidInput.replaceWith(select);
			    syncImageHeights();
			}
			async function saveSettings() {
			    const ssid = document.getElementById('ssid').value;
			    const password = document.getElementById('password').value;
			    if (!ssid) {
			        showStatus('Please enter a WiFi SSID', 'error');
			        return;
			    }
			    showStatus('Saving settings...', 'info');
			    try {
			        const response = await fetch('/api/save', {
			            method: 'POST',
			            headers: { 'Content-Type': 'application/json' },
			            body: JSON.stringify({ ssid: ssid, password: password })
			        });
			        if (response.ok) {
			            showStatus('Settings saved! Restarting...', 'success');
			        } else {
			            showStatus('Failed to save settings', 'error');
			        }
			    } catch (error) {
			        showStatus('Error saving settings', 'error');
			    }
			}

			function downloadHistory() {
			    window.location.href = '/download';
			}

			async function deleteHistory() {
			    const confirmed = confirm('WARNING: This will delete all recall history and reset your streak to 0. This cannot be undone.\n\nAre you sure you want to continue?');
			    if (!confirmed) return;

			    showStatus('Deleting recall history...', 'info');
			    try {
			        const response = await fetch('/api/reset', { method: 'POST' });
			        if (response.ok) {
			            showStatus('Recall history deleted and streak reset.', 'success');
			        } else {
			            showStatus('Failed to delete history', 'error');
			        }
			    } catch (error) {
			        showStatus('Error deleting history', 'error');
			    }
			}
		</script>
	</body>
</html>)rawliteral";

class SettingsServer {
public:
    static void setup(WebServer& server);
    static void loop(WebServer& server, int buttonPin, LEDController& led);

private:
    static void registerApiRoutes(WebServer& server);
    static void registerHistoryRoutes(WebServer& server);
};

void SettingsServer::setup(WebServer& server) {
    server.on("/", HTTP_GET, [&server]() {
        server.send(200, "text/html", settings_html);
    });

    registerApiRoutes(server);
    registerHistoryRoutes(server);
    registerSvgRoutes(server);
    server.begin();
}

void SettingsServer::loop(WebServer& server, int buttonPin, LEDController& led) {
    while (true) {
        server.handleClient();
        if (digitalRead(buttonPin) == LOW) {
            delay(50);  // Debounce
            if (digitalRead(buttonPin) == LOW) {
                Serial.println("Button pressed - restarting");
				led.stop();
				delay(3000);
                ESP.restart();
            }
        }
        delay(10);
    }
}

void SettingsServer::registerApiRoutes(WebServer& server) {
    server.on("/api/settings", HTTP_GET, [&server]() {
        Preferences prefs;
        prefs.begin("wifi", true);
        String ssid = prefs.getString("ssid", "");
        String password = prefs.getString("password", "");
        prefs.end();

        String json = "{\"ssid\":\"" + ssid + "\",\"password\":\"" + password + "\"}";
        server.send(200, "application/json", json);
    });

    server.on("/api/scan", HTTP_GET, [&server]() {
        Serial.println("Scanning WiFi networks...");
        int n = WiFi.scanNetworks();

        String json = "[";
        for (int i = 0; i < n; i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
            json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
            json += "\"encrypted\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
            json += "}";
        }
        json += "]";

        WiFi.scanDelete();
        server.send(200, "application/json", json);
    });

    server.on("/api/save", HTTP_POST, [&server]() {
        if (server.hasArg("plain")) {
            String body = server.arg("plain");

            int ssidStart = body.indexOf("\"ssid\":\"") + 8;
            int ssidEnd = body.indexOf("\"", ssidStart);
            String newSSID = body.substring(ssidStart, ssidEnd);

            int passStart = body.indexOf("\"password\":\"") + 12;
            int passEnd = body.indexOf("\"", passStart);
            String newPassword = body.substring(passStart, passEnd);

            Serial.printf("Saving WiFi: %s\n", newSSID.c_str());

            Preferences prefs;
            prefs.begin("wifi", false);
            prefs.putString("ssid", newSSID);
            prefs.putString("password", newPassword);
            prefs.putInt("fails", 0);
            prefs.end();

            server.send(200, "application/json", "{\"status\":\"ok\"}");

            delay(1000);
            ESP.restart();
        } else {
            server.send(400, "application/json", "{\"error\":\"No data\"}");
        }
    });
}

void SettingsServer::registerHistoryRoutes(WebServer& server) {
    server.on("/download", HTTP_GET, [&server]() {
        if (!LittleFS.exists("/recall_history.jsonl")) {
            server.send(404, "text/plain", "No history file found");
            return;
        }

        File file = LittleFS.open("/recall_history.jsonl", "r");
        if (!file) {
            server.send(500, "text/plain", "Failed to open file");
            return;
        }

        server.sendHeader("Content-Disposition", "attachment; filename=recall_history.jsonl");
        server.streamFile(file, "application/octet-stream");
        file.close();
    });

    server.on("/api/reset", HTTP_POST, [&server]() {
        if (LittleFS.exists("/recall_history.jsonl")) {
            LittleFS.remove("/recall_history.jsonl");
        }

        Preferences prefs;
        prefs.begin("recall", false);
        prefs.clear();
        prefs.end();

        rtc_currentStreak = 0;
        rtc_lastRecallDay = 0;
        rtc_recallDoneToday = false;

        Serial.println("Recall history reset");
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });
}

#endif
