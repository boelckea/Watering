/*
 "C:\Program Files\Git\mingw64\bin\curl.exe" -F image=@C:\Data\sloeber-workspace\Esp32Watering\Release\Esp32Logging.bin espLogging/update
 "C:\Program Files\Git\mingw64\bin\curl.exe" -F image=@C:\Data\sloeber-workspace\Esp32Watering\Release\Esp32Logging.bin 192.168.11.49/update
 http://esp32watering/
 */

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

const char* ssid = "ROSID";
const char* password = "ROSI29ROSI!";
const char* loghost = "www.web675.server21.eu";
const char* path = "/espwrite.php/?info=";

const char* updatehost = "EspLogging";
WebServer server(80);

int ledBuiltIn = GPIO_NUM_2;
int donePin = GPIO_NUM_32;
int meassurePin = A3;

String getServerPage() {
	String serverpage = String("Version 2<br>") +
			"<form id='f1' method='POST' action='/update' enctype='multipart/form-data'>" +
			"<input type='file' name='update'><input type='submit' value='Update'>" +
			"</form><br/>" +
			"<a target='_blank' href='http://" + String(loghost) + "/espreport.php'>Esp Report</a>";
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

	Serial.println();
	Serial.println("closing send connection. Send Values done.");
}

void setup() {
	pinMode(meassurePin, INPUT);
	pinMode(ledBuiltIn, OUTPUT);
	pinMode(donePin, OUTPUT);

	digitalWrite(ledBuiltIn, LOW);
	digitalWrite(donePin, LOW);

	Serial.begin(115200);
	delay(10);

	startUpdateServer();

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

	// Read and send values
	int log1 = analogRead(meassurePin);
	log1 = map(log1, 0, 4096, 0, 1000);
	SendValues(String(log1));
	digitalWrite(ledBuiltIn, HIGH);
	delay(100);
	digitalWrite(donePin, HIGH);
}


void loop() {
	server.handleClient();
	delay(10);
}
