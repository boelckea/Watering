/*
 "C:\Program Files\Git\mingw64\bin\curl.exe" -F image=@C:\Data\sloeber-workspace\Esp32Watering\Release\Esp32Watering.bin esp32watering/update
 "C:\Program Files\Git\mingw64\bin\curl.exe" -F image=@C:\Data\sloeber-workspace\Esp32Watering\Release\Esp32Watering.bin 192.168.11.44/update
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
static int pauseTimeMin = 20;
static int wateringTimeSec = 10;
static int moistureMinLevel = 500;
static unsigned long interval = 60L * 1000 * pauseTimeMin;
static unsigned long previousMillis = interval;

const char* updatehost = "esp32watering";
WebServer server(80);

#define FORMAT_SPIFFS_IF_FAILED true

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
	String serverpage = String("v7<br>") +
			"<form id='f1' method='POST' action='/update' enctype='multipart/form-data'>" +
			"<input type='file' name='update'><input type='submit' value='Update'>" +
			"</form><br>" +
			"<form method='POST' action='/settings'>" +
			"Pause time in minutes:<br>" +
			"<input type='text' name='pausetime' value='" + String(pauseTimeMin) + "'><br>" +
			"Watering time in seconds:<br>" +
			"<input type='text' name='wateringtime' value='" + String(wateringTimeSec) + "'><br>" +
			"Minimum moisture level 0-1000:<br>" +
			"<input type='text' name='moistureminlevel' value='" + String(moistureMinLevel) + "'><br>" +
			"<input type='submit' value='Save Settings'>" +
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
		server.on("/", HTTP_GET, []() {
			server.sendHeader("Connection", "close");
			String page = getServerPage();
			server.send(200, "text/html", page);
		});
		server.on("/settings", HTTP_POST,
				[]() {
					String pausetime = server.arg("pausetime");
					String wateringtime = server.arg("wateringtime");
					String moistureminlevel = server.arg("moistureminlevel");

					writeFile(SPIFFS, "/settings.txt",
							(String("pausetime=")+pausetime+";wateringtime="+wateringtime+";moistureminlevel="+moistureminlevel+";").c_str());

					pauseTimeMin = getParam(SPIFFS, "/settings.txt", "pausetime", pauseTimeMin);
					wateringTimeSec = getParam(SPIFFS, "/settings.txt", "wateringtime", wateringTimeSec);
					moistureMinLevel = getParam(SPIFFS, "/settings.txt", "moistureminlevel", moistureMinLevel);
					interval = 60L * 1000 * pauseTimeMin;
					Serial.println("pauseTimeMin: " + String(pauseTimeMin));
					Serial.println("wateringTimeSec: " + String(wateringTimeSec));
					Serial.println("moistureMinLevel: " + String(moistureMinLevel));

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

	pauseTimeMin = getParam(SPIFFS, "/settings.txt", "pausetime", pauseTimeMin);
	Serial.println("pauseTimeMin: " + String(pauseTimeMin));
	wateringTimeSec = getParam(SPIFFS, "/settings.txt", "wateringtime", wateringTimeSec);
	Serial.println("wateringTimeSec: " + String(wateringTimeSec));

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
	wasserstand = map(wasserstand, 1300, 2290, 80, 0);
	Serial.printf("Wasserstand: %d \n", wasserstand); //print Low 4bytes.
	digitalWrite(ledRed, wasserstand < 10 ? HIGH : LOW);
	return wasserstand;
}

void SendValues(String values) {
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

//	// Read all the lines of the reply from server and print them to Serial
//	while (client.available()) {
//		String line = client.readStringUntil('\r');
//		Serial.print(line);
//	}

	Serial.println();
	Serial.println("closing send connection. Send Values done.");
}

unsigned long previousLedMillis = millis();
unsigned long ledInterval = 5000;

void loop() {
	unsigned long currentMillis = millis();
	if (currentMillis - previousMillis >= interval) {
		previousMillis = currentMillis;

		String values = String("");
		for (int plantId = 0; plantId < 8; plantId++) {
			int feuchte = getFeuchte(plantId);
			if (feuchte < moistureMinLevel) {
				waessern(plantId, wateringTimeSec * 1000);
			}
			values += String(feuchte * (feuchte < moistureMinLevel ? -1 : 1)) + ",";
		}

		int wasserstand = getWasserstand();
		values += String(wasserstand);

		Serial.println(String("Now sending values: ") + values);
		SendValues(values);
	}

	if (currentMillis - previousLedMillis >= ledInterval) {
		digitalWrite(ledBuiltIn, HIGH);
	}
	if (currentMillis - previousLedMillis >= ledInterval + 100) {
		digitalWrite(ledBuiltIn, LOW);
		previousLedMillis = currentMillis;
	}

	server.handleClient();
	delay(10);
}

