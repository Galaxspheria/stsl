#include <WiFi.h>
#include <Adafruit_APDS9960.h>
#include <esp32-hal-ledc.h>

const char * ssid = "RJ_TRAINII_THOMAS";
const char * password = "robojackets";
IPAddress huzzah_ip(10,10,10,1);
IPAddress network_mask(255,255,255,0);
uint16_t port = 8008;

int led = 13;
int led_state = HIGH;
unsigned long led_time;

int line_center_pin = A2;
int line_offset_pin = A3;

// NOTE: These will change when the new board design comes in
int us_trigger_pin = 18;
int us_echo_pin = 19;

int left_a_pin = A1;
int left_b_pin = A10;
int right_a_pin = A9;
int right_b_pin = A8;
int lift_a_pin = A7;
int lift_b_pin = A6;

int left_a_channel = 1;
int left_b_channel = 2;
int right_a_channel = 3;
int right_b_channel = 4;
int lift_a_channel = 5;
int lift_b_channel = 6;

int battery_pin = A13;
double low_battery_voltage = 3.6;

WiFiServer server(port);

Adafruit_APDS9960 apds;
bool apds_ready = false;

double bitsToVolts(int bits) {
  return (bits / 4096.0) * 7.4;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Setting up...");
  
  pinMode(led, OUTPUT);

  pinMode(line_center_pin, INPUT);
  pinMode(line_offset_pin, INPUT);
  
  pinMode(us_trigger_pin, OUTPUT);
  pinMode(us_echo_pin, INPUT);

  pinMode(battery_pin, INPUT);

  Serial.println("Pins ready.");

  double battery_voltage = bitsToVolts(analogRead(battery_pin));
  Serial.print("Battery: ");
  Serial.print(battery_voltage);
  Serial.println("v");
  if(battery_voltage <= 3.6) {
    while(true) {
      // ERROR FLASH!
      delay(100);
      digitalWrite(led, HIGH);
      delay(100);
      digitalWrite(led, LOW);
    }
  }

  Serial.println("Setting up ADPS 9960");
  if(apds.begin()) {
    apds.enableProximity(true);
    apds.enableGesture(true);
    apds.enableColor(true);
    Serial.println("ADPS 9960 Ready.");
    apds_ready = true;
  } else {
    Serial.println("ADPS 9960 Failed to initialize.");
    for(int flash = 0; flash < 3; flash++) {
      // ERROR FLASH!
      delay(100);
      digitalWrite(led, HIGH);
      delay(100);
      digitalWrite(led, LOW);
    }
  }

  Serial.println("Setting up motors");
  ledcSetup(left_a_channel,50,8);
  ledcSetup(left_b_channel,50,8);
  ledcSetup(right_a_channel,50,8);
  ledcSetup(right_b_channel,50,8);
  ledcSetup(lift_a_channel,50,8);
  ledcSetup(lift_b_channel,50,8);
  ledcAttachPin(left_a_pin, left_a_channel);
  ledcAttachPin(left_b_pin, left_b_channel);
  ledcAttachPin(right_a_pin, right_a_channel);
  ledcAttachPin(right_b_pin, right_b_channel);
  ledcAttachPin(lift_a_pin, lift_a_channel);
  ledcAttachPin(lift_b_pin, lift_b_channel);
  stopMotors();

  Serial.println("Enabling AP.");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.println("Wait 100 ms for AP_START...");
  delay(100);
  Serial.println("Configurnig AP.");
  WiFi.softAPConfig(huzzah_ip, huzzah_ip, network_mask);

  Serial.println("WiFi ready.");

  server.begin();
  Serial.println("Server ready.");
  
  Serial.print("Waiting for connections at ");
  Serial.print(WiFi.softAPIP());
  Serial.print(":");
  Serial.println(port);

  digitalWrite(led, HIGH);
  led_time = millis();
}

String readLine(WiFiClient &client) {
  String line = "";
  while(client.connected()) {
    if(client.available()) {
      char c = client.read();
      if(c == '\n') {
        return line;
      } else {
        line += c;
      }
    }
  }
  /* If the client disconnects before we get a newline character,
   * just return whatever we've got so far.
   */
  return line;
}

void writeString(WiFiClient &client, const String &str) {
  size_t totalBytesSent = 0;
  const char* buf = str.c_str();
  size_t bufSize = str.length();
  while(totalBytesSent < str.length()) {
    size_t bytesSent = client.write(buf, bufSize);
    buf += bytesSent;
    bufSize -= bytesSent;
    totalBytesSent += bytesSent;
  }
}

double getUltrasonicDistance() {
  digitalWrite(us_trigger_pin, LOW);
  delayMicroseconds(2);
  // Send the ping
  digitalWrite(us_trigger_pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(us_trigger_pin, LOW);
  // Measure how long the echo pin was held high (pulse width)
  return pulseIn(us_echo_pin, HIGH) / 58.0;
}

void stopMotors() {
  ledcWrite(left_a_channel, 0);
  ledcWrite(left_b_channel, 0);
  ledcWrite(right_a_channel, 0);
  ledcWrite(right_b_channel, 0);
  ledcWrite(lift_a_channel, 0);
  ledcWrite(lift_b_channel, 0);
}

void setMotorPower(int channel_a, int channel_b, int power) {
  if(power > 0) {
    ledcWrite(channel_a, power);
    ledcWrite(channel_b, 0);
  } else {
    ledcWrite(channel_a, 0);
    ledcWrite(channel_b, -1*power);
  }
}

void loop() {
  WiFiClient client = server.available();
  if(client) {
    Serial.println("Client connected");
    digitalWrite(led, HIGH);
    while(client.connected()) {
      String command = readLine(client);
      if(command == "GetGesture") {
        uint8_t gesture = 0;
        if(apds_ready) {
          gesture = apds.readGesture();
        }
        String response = "";
        if(gesture == APDS9960_DOWN) response = "DOWN\n";
        else if(gesture == APDS9960_UP) response = "UP\n";
        else if(gesture == APDS9960_LEFT) response = "LEFT\n";
        else if(gesture == APDS9960_RIGHT) response = "RIGHT\n";
        else response = "NONE\n";
        writeString(client, response);
      }
      else if(command == "GetColor") {
        uint16_t r = 0;
        uint16_t g = 0;
        uint16_t b = 0;
        uint16_t c = 0;
        if(apds_ready) {
          for(int attempt = 0; attempt < 10; attempt++) {
            if(apds.colorDataReady()) {
              break;
            }
            delay(5);
          }
          apds.getColorData(&r, &g, &b, &c);
        }
        String response = String(r) + " " + String(g) + " " + String(b) + " " + String(c) + "\n";
        writeString(client, response);
        
      }
      else if(command == "GetProximity") {
        if(apds_ready) {
          // TODO scale proximity readings to real world units
          writeString(client, String(apds.readProximity()) + "\n");
        } else {
          writeString(client, "0\n");
        }
      }
      else if(command == "GetLineCenter") {
        writeString(client, String(analogRead(line_center_pin)) + "\n");
      }
      else if(command == "GetLineOffset") {
        writeString(client, String(analogRead(line_offset_pin)) + "\n");
      }
      else if(command == "GetUltrasonic") {
        writeString(client, String(getUltrasonicDistance()) + "\n");
      }
      else if(command == "StopMotors") {
        stopMotors();
      }
      else if(command.substring(0,7) == "SetLift") {
        int power = command.substring(7).toInt();
        setMotorPower(lift_a_channel, lift_b_channel, power);
      }
      else if(command.substring(0,8) == "SetDrive") {
        int delimIndex = command.indexOf('|');
        int leftPower = command.substring(8,delimIndex).toInt();
        int rightPower = command.substring(delimIndex+1).toInt();
        setMotorPower(left_a_channel, left_b_channel, leftPower);
        setMotorPower(right_a_channel, right_b_channel, rightPower);
      }
    }
    stopMotors();
  } else {
    unsigned long now = millis();
    if((now - led_time) > 500) {
      led_state = !led_state;
      digitalWrite(led, led_state);
      led_time = now;
    }
  }
}
