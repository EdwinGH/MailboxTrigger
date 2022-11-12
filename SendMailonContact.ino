// Program to send mail when woken up
// Then go back to deep sleep (so very low power)
// v1.0 Basic operation
// v1.1 Optimized way of working (LED flashes, timeouts, error handling)
// v1.2 Added Vcc monitoring

#include <ESP8266WiFi.h>
#include <ESP_Mail_Client.h>

ADC_MODE(ADC_VCC);

#define MY_LED_BUILTIN 2
#define LED_ON LOW
#define LED_OFF HIGH

const char* ssid     = "<WIFI SSID>"
const char* password = "<WIFI PASSWORD>"

#define SMTP_HOST "<SMTP SERVER>"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "espbox@<DOMAIN>"
#define RECIPIENT_EMAIL "<EMAIL>"

SMTPSession smtp;

// Flash the LED OFF a amount of times for period ms
void flashLED(int amount = 1, int period = 250) {
  for (int i = 0; i < amount; i++) {
    digitalWrite(MY_LED_BUILTIN, LED_OFF);
    delay(period);
    digitalWrite(MY_LED_BUILTIN, LED_ON);
    delay(period);
  }
}

// Read the Vcc and put in string to return
char * readVcc() {
  static char buf[10];
  uint16_t v = ESP.getVcc();
  float_t v_cal = ((float)v/1024.0f);
  dtostrf(v_cal, 5, 3, buf);
  return buf;
}

// Establish a Wi-Fi connection with the router
// timeout in seconds, trying 4 per second
bool connectWiFi(int timeout = 10) {
  Serial.print("Connecting to: ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);

  timeout = timeout * 4; // trying 4x per second
  while(WiFi.status() != WL_CONNECTED  && (timeout-- > 0)) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("");
  if(WiFi.status() != WL_CONNECTED) {
     Serial.println("FAILED to connect");
     return false;
  }
  Serial.print("WiFi connected in: ");
  Serial.print(millis());
  Serial.print("ms, IP address: ");
  Serial.println(WiFi.localIP());
  return true;
}

// Sends mail to SMTP server
// Synchronous, routine ends when mail sent successfully (or not)
bool sendMail() {
  // Set debugging on (1) or off (0)
  smtp.debug(0);

  // Set the callback function to get the sending results
  smtp.callback(smtpCallback);

  /* Declare the session config data */
  ESP_Mail_Session session;

  /* Set the session config */
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.user_domain = "zuidema.org";

  /* Declare the message class */
  SMTP_Message message;

  /* Set the message headers */
  message.sender.name = "Mailbox (ESP)";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "You've got (snail) mail!";
  message.addRecipient("Root", RECIPIENT_EMAIL);

  // Send raw text message
  String textMsg  = "ESP Mailbox detected that the lid opened, so you have mail!\n\n";
  textMsg += "(Board voltage =  " + String(readVcc()) + "V)";

  message.text.content = textMsg.c_str();
  message.text.charSet = "us-ascii";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  /* Connect to server with the session config */
  if (!smtp.connect(&session)) {
    Serial.println("Error connecting, " + smtp.errorReason());
    return false;
  }
  /* Start sending Email and close the session */
  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Error sending Email, " + smtp.errorReason());
    return false;
  }
  Serial.println("SendMail done.");
  return true;
}

// Callback function to get the Email sending status
// Called multiple times during session
void smtpCallback(SMTP_Status status){
  /* Print the current status */
  Serial.print("smtpCallback: ");
  Serial.println(status.info());

  // Whend done, print the sending result
  if (status.success()){
    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failed:  %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++){
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %d-%d-%d %d:%d:%d (UTC)\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients);
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject);
      Serial.println("----------------");
    }
  }
  // Free memory every callback
  smtp.sendingResult.clear();
}

// the setup function runs once when you power the board or press reset
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(MY_LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  Serial.println("setup: Test print");
  Serial.println("------------------");

  Serial.println("LED ON while active");
  digitalWrite(MY_LED_BUILTIN, LED_ON);

  Serial.print("setup: Vcc = ");
  Serial.println(readVcc());

  if(connectWiFi()) {
    Serial.println("WiFi initialized (flashing LED 1x)");
    // Show success by flashing the LED
    flashLED(1);
    delay(1000);

    // Send mail as we were woken up by external event
    if(sendMail()) {
      Serial.println("Mail sent (flashing LED 2x)");
      // Show success by flashing the LED 2x
      flashLED(2);
    } else {
      Serial.println("Mail sending FAILED (flashing LED 5x)");
      flashLED(5);
    }
  } else {
    Serial.println("Connecting WiFi FAILED (flashing LED 10x");
    flashLED(10);
  }

  Serial.print("setup: Vcc = ");
  Serial.println(readVcc());

  // Enter Deep sleep mode until RESET pin is connected to a LOW signal (reed contact closes)
  // LED should switch off as well
  Serial.println("Entering Deep Sleep mode (LED will go off)");
  delay(1000);
  ESP.deepSleep(0);
}

// the loop function runs over and over again forever, after completing setup
// As setup never completes in this case, loop should not be called
void loop() {
  Serial.println("loop, should not get here, as calling setup and going to deep sleep from there");
}
