/*
 "C:\Program Files\Git\mingw64\bin\curl.exe" -F image=@C:\Data\sloeber-workspace\Esp32Watering\Release\Esp32Watering.bin esp32watering/update
 "C:\Program Files\Git\mingw64\bin\curl.exe" -F image=@C:\Data\sloeber-workspace\Esp32Watering\Release\Esp32Watering.bin 192.168.11.47/update
 http://esp32watering/
 */

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#include "FS.h"
#include "SPIFFS.h"

const char* ssid = "ROSID";
const char* password = "ROSI29ROSI!";
const char* loghost = "www.web675.server21.eu";
const char* path = "/loginfo.php/?info=";
static int pauseTimeMin = 30;
static int wateringTimeSec[8] = { 5, 5, 5, 5, 5, 5, 5, 5 };
static int moistureMinLevel[8] = { 100, 100, 100, 100, 100, 100, 100, 100 };
static unsigned long interval = 60L * 1000 * pauseTimeMin;
static unsigned long previousMillis = interval;

const char* updatehost = "esp32watering";
WebServer server(80);

#define FORMAT_SPIFFS_IF_FAILED true

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
uint8_t temprature_sens_read();

int ledBuiltIn = GPIO_NUM_2;
int ledRed = GPIO_NUM_14;
int ledGreen = GPIO_NUM_26;

int pumpe = GPIO_NUM_32;

int feuchtePulse = GPIO_NUM_4;
int pulseMultiplex1 = GPIO_NUM_22;
int pulseMultiplex2 = GPIO_NUM_15;
int pulseMultiplex3 = GPIO_NUM_21;
int feuchteReadPin = A0;

int dataPin = GPIO_NUM_23;
int latchPin = GPIO_NUM_16;
int clockPin = GPIO_NUM_18;

int wasserstandPin = A3;

String readFile(fs::FS &fs, const char * path) {
	Serial.printf("Reading file: %s\r\n", path);

	File file = fs.open(path);
	if (!file || file.isDirectory()) {
		Serial.println("- failed to open file for reading");
		return "";
	}

	String content = file.readString();
	return content;
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
	Serial.printf("Writing file: %s\r\n", path);

	File file = fs.open(path, FILE_WRITE);
	if (!file) {
		Serial.println("- failed to open file for writing");
		return;
	}
	if (file.print(message)) {
		Serial.println("- file written");
	} else {
		Serial.println("- frite failed");
	}
}

int getParam(fs::FS &fs, const char * path, const char * pnameFind, int defaultValue) {
	String content = readFile(SPIFFS, path);
	Serial.println("content: " + String(content));

	int index = content.indexOf(";");
	while (index > 0) {
		String part = content.substring(0, index);
		int index2 = part.indexOf("=");
		String pname = part.substring(0, index2);
		if (pname.equals(pnameFind)) {
			String value = part.substring(index2 + 1);
			int vint = value.toInt();
			return vint;
		}
		content = content.substring(index + 1);
		index = content.indexOf(";");
	}
	return defaultValue;
}

String getServerPage() {
	String serverpage = String("{DATE:2019-07-25 16:55}<br>") +
			"<form id='f1' method='POST' action='/update' enctype='multipart/form-data'>" +
			"<input type='file' name='update'><input type='submit' value='Update'>" +
			"</form><br>" +
			"<form method='POST' action='/settings'>" +
			"Pause time in minutes: " +
			"<input type='text' name='pausetime' value='" + String(pauseTimeMin) + "'><br><br>" +
			"Watering time in seconds | Minimum moisture level 0-1000<br>";
	for (int plant = 0; plant < 8; plant++) {
		String ps = String(plant + 1);
		serverpage += String("P") + ps + " <input type='text' name='wateringtime" + ps + "' value='" + String(wateringTimeSec[plant]) + "'>"
				+ "<input type='text' name='moistureminlevel" + ps + "' value='" + String(moistureMinLevel[plant]) + "'><br>";
	}
	serverpage += String("<br><input type='submit' value='Save Settings'>") +
			"</form><br>" +
			"<a target='_blank' href='http://" + String(loghost) + "/plantinfo.php'>Plantinfo</a>";
	return serverpage;
}

void startUpdateServer() {
	Serial.println();
	Serial.println("Booting Sketch...");
	WiFi.mode(WIFI_AP_STA);
	WiFi.begin(ssid, password);
	if (WiFi.waitForConnectResult() == WL_CONNECTED) {
		MDNS.begin(updatehost);
		server.on("/settings", HTTP_GET, []() {
			server.sendHeader("Connection", "close");
			String page = getServerPage();
			server.send(200, "text/html", page);
		});
		server.on("/settings", HTTP_POST,
				[]() {
					String settings = String("pausetime=")+server.arg("pausetime")+";";
					for (int plant = 0; plant < 8; plant++) {
						String p = String(plant+1);
						settings += String("wateringtime"+p+"=")+server.arg("wateringtime"+p)+
						";moistureminlevel"+p+"="+server.arg("moistureminlevel"+p)+";";
					}
					writeFile(SPIFFS, "/settings.txt", settings.c_str());

					pauseTimeMin = getParam(SPIFFS, "/settings.txt", "pausetime", pauseTimeMin);
					for (int plant = 0; plant < 8; plant++) {
						wateringTimeSec[plant] = getParam(SPIFFS, "/settings.txt", (String("wateringtime")+String(plant+1)).c_str(), wateringTimeSec[plant]);
						moistureMinLevel[plant] = getParam(SPIFFS, "/settings.txt", (String("moistureminlevel")+String(plant+1)).c_str(), moistureMinLevel[plant]);
					}
					interval = 60L * 1000 * pauseTimeMin;

					server.sendHeader("Connection", "close");
					String page = getServerPage();
					server.send(200, "text/html", page);
				});
		server.on("/update", HTTP_POST,
				[]() {
					server.sendHeader("Connection", "close");
					server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
					ESP.restart();
				},
				[]() {
					HTTPUpload& upload = server.upload();
					if (upload.status == UPLOAD_FILE_START) {
						Serial.setDebugOutput(true);
						Serial.printf("Update: %s\n", upload.filename.c_str());
						if (!Update.begin()) { //start with max available size
					Update.printError(Serial);
				}
			} else if (upload.status == UPLOAD_FILE_WRITE) {
				if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
					Update.printError(Serial);
				}
			} else if (upload.status == UPLOAD_FILE_END) {
				if (Update.end(true)) { //true to set the size to the current progress
					Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
				} else {
					Update.printError(Serial);
				}
				Serial.setDebugOutput(false);
			}
		});
		server.begin();
		MDNS.addService("http", "tcp", 80);

		Serial.printf("Ready! Open http://%s.local in your browser\n", updatehost);
	} else {
		Serial.println("WiFi Failed");
	}
}

void setup() {
	Serial.begin(115200);
	delay(10);

	startUpdateServer();

	pinMode(feuchtePulse, INPUT);
	pinMode(pulseMultiplex1, OUTPUT);
	pinMode(pulseMultiplex2, OUTPUT);
	pinMode(pulseMultiplex3, OUTPUT);

	pinMode(ledBuiltIn, OUTPUT);
	pinMode(ledRed, OUTPUT);
	pinMode(ledGreen, OUTPUT);
	pinMode(wasserstandPin, INPUT);

	pinMode(dataPin, OUTPUT);
	pinMode(latchPin, OUTPUT);
	pinMode(clockPin, OUTPUT);

	pinMode(feuchteReadPin, INPUT);

	pinMode(pumpe, OUTPUT);

	pumpen(LOW);
	closeValves();

	// Wifi connect
	Serial.print("Connecting to ");
	Serial.println(ssid);

	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	Serial.println("");
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());

	if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
		Serial.println("SPIFFS Mount Failed");
		return;
	}

	// Read settings
	pauseTimeMin = getParam(SPIFFS, "/settings.txt", "pausetime", pauseTimeMin);
	for (int plant = 0; plant < 8; plant++) {
		wateringTimeSec[plant] = getParam(SPIFFS, "/settings.txt",
				(String("wateringtime") + String(plant + 1)).c_str(),
				wateringTimeSec[plant]);
		moistureMinLevel[plant] = getParam(SPIFFS, "/settings.txt",
				(String("moistureminlevel") + String(plant + 1)).c_str(),
				moistureMinLevel[plant]);
	}
	interval = 60L * 1000 * pauseTimeMin;
	previousMillis = interval;
}

int getFeuchte(int plantId) {
	digitalWrite(pulseMultiplex1, (7 - plantId) >> 0 & 1);
	digitalWrite(pulseMultiplex2, (7 - plantId) >> 1 & 1);
	digitalWrite(pulseMultiplex3, (7 - plantId) >> 2 & 1);
	delay(10);

	pinMode(feuchtePulse, OUTPUT);
	for (int i = 0; i < 100; i++) {
		digitalWrite(feuchtePulse, HIGH);
		delay(2);
		digitalWrite(feuchtePulse, LOW);
		delay(2);
	}
	pinMode(feuchtePulse, INPUT);

	int pflanze = plantId + 1;
	int feuchte = analogRead(feuchteReadPin);
	feuchte = map(feuchte, 1500, 3200, 0, 1000);
	Serial.printf("Feuchte Pflanze %d: %d \n", pflanze, feuchte); //print Low 4bytes.

	return feuchte;
}

void openValve(int plantId) {
	digitalWrite(latchPin, LOW);
	delay(10);
	shiftOut(dataPin, clockPin, MSBFIRST, 255 ^ (1 << (7 - plantId)));
	delay(10);
	digitalWrite(latchPin, HIGH);
}
void closeValves() {
	digitalWrite(latchPin, LOW);
	delay(10);
	shiftOut(dataPin, clockPin, MSBFIRST, 255);
	delay(10);
	digitalWrite(latchPin, HIGH);
}

void pumpen(int highLow) {
	digitalWrite(pumpe, highLow);
	digitalWrite(ledGreen, highLow);
}

void waessern(int plantId, int time) {
	openValve(plantId);
	pumpen(HIGH);
	delay(time);
	pumpen(LOW);
	closeValves();
}

int getWasserstand() {
	int wasserstand = analogRead(wasserstandPin);
	wasserstand = map(wasserstand, 2700, 3200, 800, 0);
	wasserstand = max(wasserstand, 0);
	//wasserstand = wasserstand/10;
	Serial.printf("Wasserstand: %d \n", wasserstand); //print Low 4bytes.
	digitalWrite(ledRed, wasserstand < 100 ? HIGH : LOW);
	return wasserstand;
}

void SendValues(String values) {
	// Check Wifi connection
	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("WiFi not connected. Restarting");
		delay(60000); // To slow reboot loop down
		ESP.restart();
	}

	// Use WiFiClient class to create TCP connections
	WiFiClient client;
	const int httpPort = 80;
	if (!client.connect(loghost, httpPort)) {
		Serial.println("connection failed");
		return;
	}

	// We now create a URI for the request
	String url = String(path) + values;
	Serial.print("Requesting URL: ");
	Serial.println(url);

	// This will send the request to the server
	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
			"Host: " + loghost + "\r\n" +
			"Connection: close\r\n\r\n");
	unsigned long timeout = millis();
	while (client.available() == 0) {
		if (millis() - timeout > 5000) {
			Serial.println(">>> Client Timeout !");
			client.stop();
			return;
		}
	}

	Serial.println();
	Serial.println("closing send connection. Send Values done.");
}

unsigned long previousLedMillis = millis();
unsigned long ledInterval = 10000;
unsigned long lastDuration = 0;

void loop() {
	unsigned long currentMillis = millis();
	if (currentMillis - previousMillis >= interval) {
		previousMillis = currentMillis;

		int wasserstand = getWasserstand();

		String values = String("");
		for (int plantId = 0; plantId < 8; plantId++) {
			int feuchte = getFeuchte(plantId);
			if (feuchte < moistureMinLevel[plantId]) {
				waessern(plantId, wateringTimeSec[plantId] * 1000);
			}
			values += String(feuchte * (feuchte < moistureMinLevel[plantId] ? -1 : 1)) + ",";
		}

		uint8_t tempertaure = (temprature_sens_read() - 32) / 1.8;

		values += String(wasserstand)
				+ "," + String(currentMillis)
				+ "," + String(lastDuration)
				+ "," + String(tempertaure);

		Serial.println(String("Now sending values: ") + values);
		SendValues(values);
		lastDuration = millis() - currentMillis;
	}

	if (currentMillis - previousLedMillis >= ledInterval) {
		digitalWrite(ledBuiltIn, HIGH);
	}
	if (currentMillis - previousLedMillis >= ledInterval + 30) {
		digitalWrite(ledBuiltIn, LOW);
		previousLedMillis = currentMillis;
	}

	server.handleClient();
	delay(10);
}
