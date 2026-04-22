#include "web_ui.h"
#include "config.h"
#include "sensors.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <stdarg.h>

//
// Internal state
//

static AsyncWebServer server(WEB_UI_PORT);

static constexpr int 	LOG_LINES	= 30;
static constexpr int 	LOG_LINE_LEN	= 128;
static char		logBuf[LOG_LINES][LOG_LINE_LEN];
static int		logHead		= 0;	// next write index (circular)
static int 		logCount	= 0;

static SensorData	latestData	= {};
static bool		calibPending	= false;
static bool 		calibConfirmed	= false;

//
// LED
//

void ledBlink(int times, int onMs, int offMs) {
	for (int i = 0; i < times; i ++) {
		digitalWrite(LED_PIN, HIGH);	delay(onMs);
		digitalWrite(LED_PIN, LOW);	delay(offMs);
	}
}

//
// Rolling log
//

void logMsg(const char* tag, const char* msg) {
	char line[LOG_LINE_LEN];
	unsigned long s = millis() / 1000;
	snprintf(line, sizeof(line), "[%4lus][%s] %s", s, tag, msg);
	Serial.println(line);
	strncpy(logBuf[logHead], line, LOG_LINE_LEN - 1);
	logHead = (logHead + 1) % LOG_LINES;
	if (logCount < LOG_LINES) logCount++;
}

void logMsgf(const char* tag, const char* fmt, ...) {
	char msg[LOG_LINE_LEN];
	va_list args;
	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);
	logMsg(tag, msg);
}

static String buildLogHtml() {
	String out;
	out.reserve(logCount * (LOG_LINE_LEN + 10));
	// print in chronological order
	int start = (logCount < LOG_LINES) ? 0 : logHead;
	for (int i = 0; i < logCount; i++) {
		int idx = (start + i) % LOG_LINES;
		out += logBuf[idx];
		out += "<br>\n";
	}
	return out;
}

//
// Shared HTML chrome
//

static const char* CSS = R"(
 body{font-family:sans-serif;max-width:640px;margin:24px auto;padding:0 16px;background:#f5f5f5}
 h1{color:#333;font-size:1.4rem}
 nav a{margin-right:12px;color:#0077cc;text-decoration:none;font-weight:bold}
 .card{background:#fff;border-radius:8px;padding:16px;margin:12px 0;box-shadow:0 1px 4px #0002}
 table{width:100%;border-collapse:collapse}
 td,th{padding:6px 10px;border-bottom:1px solid #eee;text-align:left}
 th{color:#555;font-weight:600}
 .badge{display:inline-block;padding:2px 8px;border-radius:12px;font-size:.8rem}
 .ok{background:#d4edda;color:#155724}
 .warn{background:#fff3cd;color:#856404}
 .err{background:#f8d7da;color:#721c24}
 button,input[type=submit]{background:#0077cc;color#fff;border:none;padding:10px 20px;
  border-radius:6px;cursor:pointer;font-size:1rem;margin-top:8px}
 button:hover,input[type=submit]:hover{background:#005fa3}
 .danger{background:#dc3545}
 .danger:hover{background:#b02a37}
 pre{background:#222;color:#0f0;padding:12px;border-radius:6px;font-size:.75rem;
  overflow-x:auto;max-height:400px;overflow-y:auto}
)";

static String pageWrap(const String& title, const String& body) {
	String html;
	html.reserve(2048);
	html += "<!DOCTYPE html><html<head><meta charset='utf-8'>";
	html += "<meta name='viewport' content='widht=device-width,initial-scale=1'>";
	html += "<title>ESP32 - "; html += title; html += "</title>";
	html += "<style>"; html += CSS; html += "</style></head><body>";
	html += "<h1>ESP32 Sensor Node</h1>";
	html += "<nav><a href='/'>Dashboard</a><a href='/calibrate'>Calibrate</a><a href='/logs'>Logs</a></nav>";
	html += "<hr>";
	html += body;
	html += "</body></html>";
	return html;
}

//
// Dashboard page (GET /)
//

static String buildDashboard() {
	const SensorData &d = latestData;

	String b;
	b.reserve(1500);

	// Auto-refresh every 3s
	b += "<meta http-equiv='refresh' content='3'>";

	// WiFi / MQTT status bar
	b += "<div class='card'><table>";
	b += "<tr><th>WiFi</th><td><span class='badge ok'>";
	b += WiFi.localIP().toString();
	b += "</span></td></tr>";
	b += "<<tr><th>Calibrated</th><td><span class'badge ";
	b += loadCellIsCalibrated() ? "ok'>Yes" : "warn'>No - <a hrefs='/calibrate now</a>";
	b += "</span></td></tr></table></div>";

	//Sensor readings
	b += "<div class='card'><table><tr><th>Sensor</th><th>Value</th></tr>";

	//DHT
	if (d.dht.valid) {
		b += "<tr>><td>Temperature</td><td"; b += d.dht.temp; b += " C</td></tr>";
		b += "<tr><td>Humidity</td><td";     b += d.dht.hum;  b += " %</td></tr>";
	} else {
		b += "<tr><td>DHT11</td><td><span class='badge err'>Read failed</span></td></tr>";
	}

	// Load cell
	b += "<tr><td>Weight</td><td>";
	if (d.loadCell.valid) 	{ b += d.loadCell.weightKg; b += " Kg"; }
	else 			{ b += "<span class='badge err'>Invalid / overload</span"; }
	b += "</td></tr>";

	// Current
	b += "<tr><td>Current (ACS712-1)</td><td>"; b += d.current1.ampsRms; b += " A  ("; b += d.current1.watt; b += " W)</td></tr>";

	//Ultrasonic
	b += "<tr><td>Distance 1</td><td>"; b+= d.ultrasonic.dist1; b += " cm</td></<tr>";
	b += "<tr><td>Distance 2</td><td>"; b+= d.ultrasonic.dist2; b += " cm</td></<tr>";

	//LDR
	b += "<tr><td>LDR block time</td><td>"; b+= d.ldr.blockTimeSec; b += " s</td></<tr>";

	// Vibration
	b += "<tr><td>Vibration</td><td><span class='badge ";
	b += d.vibration.vibrating ? "warn'>Detected" : "ok'>None";
	b += "</span></td></tr>";

	b += "</table></div>";
	return b;
}

// 
// Calibration wizard (GET /calibrate)
//

static String buildCalibratePage(const String& message = "") {
	String b;
	b.reserve(800);

	if (!message.isEmpty()) {
		b += "<div class='card'><p><strong>"; b += message; b += "</strong></p></div";
	}
	
	if (!calibPending) {
		// Step 1: start 
		b += "<div class='card'>";
		b += "<h2>Load Cell Calibration</h2>";
		b += "<p>1. Remove <strong>all weight</strong> from the scale.</p>";
		b += "<p>2. Clienk <em>Start Tare</em> - the scale will zero itself.</p>";
		b += "<p>3. You will then be asked to place your known weight (";
		b += String(LOADCELL_CALIB_KG, 2);
		b += " Kg).</p>";
		b += "<form method='POST' action='/calibrate/tare'>";
		b += "<input type='submit' value='Start Tare'>";
		b += "</form></div>";
	} else {
		// Step 2: Confirm weight placed
		b += "<div class='card'>";
		b += "<h2>Step 2 - Place Weight</h2>";
		b += "<p>Place exaclty <strong>";
		b += String(LOADCELL_CALIB_KG, 2);
		b += " Kg</strong> on the scale, then press <em>Confirm</em>.</p>";
		b += "<form method='POST' action='/calibrate/confirm'>";
		b += "<input type='submit' value='1 Confirm - weight is on the scale'>";
		b += "</form>";
		b += "<p><small>Changed your mind? <a href='/calibrate/cancel'>Cancel</a></small></p>";
		b += "</div>";
	}

	return b;
}

//
// /api/status - JSON for the Raspberry Pi
//

static void serveApiStatus(AsyncWebServerRequest* req) {
	JsonDocument doc;
	const SensorData &d = latestData;

	doc["uptime_s"]		= millis() / 1000;
	doc["wifi_ip"]		= WiFi.localIP().toString();
	doc["calibrated"]	= loadCellIsCalibrated();

	auto dht 	= doc["dht"].to<JsonObject>();
	dht["valid"]	= d.dht.valid;
	dht["temp"]	= d.dht.temp;
	dht["hum"]	= d.dht.hum;

	auto lc 	= doc["loadcell"].to<JsonObject>();
	lc["valid"]	= d.loadCell.valid;
	lc["weightKg"] 	= d.loadCell.weightKg;

	auto cur 	= doc["current1"].to<JsonObject>();
	cur["voltage"]	= d.current1.voltage;
	cur["vrms"]	= d.current1.vrms;
	cur["ampsRms"] 	= d.current1.ampsRms;
	cur["watt"]	= d.current1.watt;

	auto us		= doc["Ultrasonic"].to<JsonObject>();
	us["dist1"]	= d.ultrasonic.dist1;
	us["dist2"]	= d.ultrasonic.dist2;

	doc["ldr_blockTimeSec"]	= d.ldr.blockTimeSec;
	doc["vibration"]	= d.vibration.vibrating;

	String out;
	serializeJson(doc, out);
	req->send(200, "application/json", out);
}

//
// Route registration
//

void webUiInit() {
	// Dashboard
	server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
			req->send(200, "text/html", pageWrap("Dashboard", buildDashboard()));
	});

	// Calibration - show wizard
	server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest* req) {
			req->send(200, "text/html", pageWrap("Calibrate", buildCalibratePage()));
	});
	
	// Calibration - step 1: Tare
	server.on("/calibrate/tare", HTTP_GET, [](AsyncWebServerRequest* req) {
			calibPending 	= false;
			calibConfirmed	= false;
			webUiSetCalibrationPending(true);
			logMsg("WebUI", "Calibration tare reqested via browser.");
			req->send(200, "text/html", pageWrap("Calibrate", 
				buildCalibratePage("Taring... place your weight when the LED stops blinking.")));
	});

	// Calibration - step 2: user confirms weight is placed
	server.on("/calibrate/confirm", HTTP_POST, [](AsyncWebServerRequest* req) {
			if (calibPending) {
				calibConfirmed = true;
				logMsg("WebUI", "User confirmed weight place.");
			}
			req->send(200, "text/html", pageWrap("Calibrate",
				buildCalibratePage("Measuring... check the dashboard in a few seconds.")));
	});

	// Calibration - cancel
	server.on("/calibrate/cancel", HTTP_GET, [](AsyncWebServerRequest* req) {
			calibPending	= false;
			calibConfirmed	= false;
			req->send(200, "text/html", pageWrap("calibrate", 
				buildCalibratePage("Calibration cancelled.")));
	});

	// Log viewer
	server.on("/logs", HTTP_GET, [](AsyncWebServerRequest* req) {
			String body = "<div class='card'><pre>" + buildLogHtml() + "</pre></div>";
			req->send(200, "text/html", pageWrap("Logs", body));
	});

	// JSON API for Raspberry Pi
	server.on("/api/status", HTTP_GET, serveApiStatus);

	server.begin();
	logMsg("WebUI", "Server started.");
}

void webUiLoop() {
	// AsyncWebServer is event-driven; nothing to poll.
	// This function exists as a hook for future user (e.g. SSE heartbeat).
}

//
// Public state setters
//

void webUiUpdateSensors(const SensorData &d) { latestData = d; }

void webUiSetCalibrationPending(bool pending) { 
	calibPending 	= pending;
	calibConfirmed 	= false;
}

bool webUiCalibrationConfirmed() { return calibConfirmed; }
void webUiClearCalibrationConfirm() { calibConfirmed = false; }
