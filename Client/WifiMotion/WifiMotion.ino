/**
 * FreeIMU library serial communication protocol
*/

#include <ADXL345.h>
#include <bma180.h>
#include <HMC58X3.h>
#include <ITG3200.h>
#include <MS561101BA.h>
#include <I2Cdev.h>
#include <MPU60X0.h>
#include <EEPROM.h>

//#define DEBUG
#include "DebugUtils.h"
#include "CommunicationUtils.h"
#include "FreeIMU_kudos.h"
#include "sensor_data.h"
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <SD.h>

/* Type definition */

#define MAX_WIFI_PACKET_SIZE  80
#define MAX_NUM_SAMPLES       30
#define MAX_CONNECTION_NUM    5
#define STATUS_LED_PIN        13

#define RECORD_SD_CARD        1   // 0 send directly using wifi

/* local type definition */

/* variables */

// SD card
const int chipSelect = 4;
char dataLogname[256] = "datalog.txt";
unsigned long wifi_sd_period_ms = 10000; 
unsigned long previousSDMillis;


char ssid[] = "Tufor";     //  your network SSID (name)
char pass[] = "njqxrkqr";    // your network password
int status = WL_IDLE_STATUS;     // the Wifi radio's status
WiFiClient client; // = server.available();
IPAddress ip_device(192, 168, 0, 20);
IPAddress ip_server(192, 168, 0, 6);
char ip_server_host[16] = "192.168.0.6";

// This data buffer is going to be a pipe with two pointers
// showing the current index for recording (current_data_idx)
// and the index for sending (sent_data_idx)
data_header_t header_buffer[MAX_NUM_SAMPLES];
data_sensor_t data_buffer[MAX_NUM_SAMPLES];
data_calc_t calc_buffer[MAX_NUM_SAMPLES];

// Strings to be sent via wifi
char header_str[256];
char data_str[512];
char calc_data_str[256];

int num_samples = 0, current_data_idx, sent_data_idx;
unsigned long previousMillis;

// Set the FreeIMU object
FreeIMU my3IMU = FreeIMU();
//The command from the PC
char cmd;
unsigned long sampling_period_ms = 300;   // minimum sampling period. Won't sample with higher frequency
unsigned long millis_cycle = 0;

void setup() {
  int i;
  Serial.begin(9600);
  //while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  //}
  
  // Initialize SPI? for sensor comms
  Wire.begin();
  my3IMU.init(true);
  
  // Sync device to hub
  if (RECORD_SD_CARD)
  {
    if (!SD.begin(chipSelect))
    {
      Serial.println("Card failed or not present");
     return; 
    }
    Serial.println("Card was initialized");
    previousSDMillis = millis();
  }
  else
  {
    sync_device();
  }
  
  // initialize variables related to sampling
  init_measurements();
  Serial.println(F("Measurement module initialized"));
    
  // LED
  pinMode(STATUS_LED_PIN, OUTPUT);
  blink_led(5,100);
}


void loop() {
  
  // Take measurements each period 
  if ((unsigned long)(millis() - previousMillis) >= sampling_period_ms &&
      num_samples < MAX_NUM_SAMPLES)
  {
    record_measurement();
  } 
  
  
  // Take measurements each period 
  /*
  if ((unsigned long)(millis() - previousMillis) < sampling_period_ms/2)
  {
    // record on sd card
    send_measurement(1);
  }
  */
  
  // millis is a 4 byte number so it cycles every ~49 days since arduino started
  // we count this cycle to recorded to the return string as the best timestamp
  // the server will take care of interpreting the timestamp with respect to the received time
  if (millis() < previousMillis)
    millis_cycle++;
  
  // Send data via wifi when it is time for it
  if (num_samples >= MAX_NUM_SAMPLES)   // or some other criteria like cat at rest
  {
    blink_led(2,250);
    digitalWrite(STATUS_LED_PIN, HIGH);
    
    if (RECORD_SD_CARD)
    {
      Serial.println(F("Saving measurements"));
      save_measurements(0);
    }
    else
    {
      Serial.println(F("Sending measurements"));
      send_measurements(0);
    }
    blink_led(2,250);
    my3IMU.init(true);
    Serial.println(F("    Done"));
  }     
  
  if (RECORD_SD_CARD)
  {
    if ((unsigned long)(millis() - previousSDMillis) > wifi_sd_period_ms)
    {
      // record on sd card
      Serial.println("Sending SD card data via Wifi");
      send_SDdata2WIFI();
      previousSDMillis = millis();
    }
    
  }
}

// Save measurements in SD card
void save_measurements(int send_num_samples) {
  int i, max_i = send_num_samples;
  char data_aux[32]; // the max number is 4,294,967,295 which has 10 characters + 1 for the termination character
  int keepconnection = 1;
  char filename[256] = "datalog.txt";
  
  // Exit if nothing to send
  if (num_samples == 0)
  {
    return;
  }
  
  File dataFile = SD.open(dataLogname, FILE_WRITE);
  
  if (!dataFile)
  {
    Serial.println("Error opening");  // %s",dataLogname);
    return;
  }
  
  if (num_samples < send_num_samples || send_num_samples <= 0)
    max_i = num_samples;
    
  for (i=0; i<max_i; i++)
  {
    if (i == max_i-1)
      keepconnection = 0;
    //sprintf(data_aux,"sample %d, remaining %d",i+1, num_samples);
    //Serial.println(data_aux);
    create_data_wifi_str(&data_buffer[sent_data_idx]);
    
    if (header_str != NULL)
      dataFile.print(header_str);
    dataFile.print(data_str);  
    if (calc_data_str != NULL)
      dataFile.print(calc_data_str);
    dataFile.println();
    
    increase_data_ptr(&sent_data_idx);
    num_samples--;
  }
  
  dataFile.close();
}

// Send the data of the SD card through wifi
void send_SDdata2WIFI() { 
  char sd_str[512];
  int len = 0;
  File dataFile = SD.open(dataLogname, FILE_READ);
  
  if (!dataFile)
  {
    Serial.println("Error opening");  // %s",dataLogname);
    return;
  }
  
  // Connect to server
  if (!client.connected())
  {
    //Serial.println("reconnecting to server");
    len = MAX_CONNECTION_NUM;
    while (client.connect(ip_server, 80) == 0 && len>0) {
      len--;
    }
  } 
  if (!client.connected())
  {
    Serial.println("Unable to connect to server");
    return;
  }
  
  // read from the file until there's nothing else in it:
  while (dataFile.available()) {
    // Read SD card
    sd_str = dataFile.read();
    
    // Send lines through wifi
    Serial.write(sd_str);
      
    len = strlen(sd_str);
    
    // Create script line from input
    sprintf(script_line,"POST %s HTTP/1.1", script);
    sprintf(host_line,"Host: %s", ip_server_host);
    
    // Make a HTTP request:
    client.flush();
    client.println(script_line);
    client.println(host_line);
    client.println("User-Agent: ArduinoWifi/1.1");
    client.println("Content-Type: application/x-www-form-urlencoded");  
    if (keepConnection)
    {
      client.println("Connection: Keep-Alive");
      client.println("Keep-Alive: timeout=10, max=5");
    }
    else
    {
      client.println("Connection: close");
    }
    client.print("Content-Length: "); 
    client.println(len); 
    client.println();
    sendContent(sd_str);
    client.println();
    
    while (!waitResponse("HTTP/1.1 200 OK", len) && len>0)
    {
      len--;
    }
  }
  
  if (!keepConnection)
  {
    client.flush();
    if (!client.connected())
    {
      client.stop();
      client.flush();
      //Serial.println("Closed connection");
    }
  }
  
  // Close SD card file
  dataFile.close();
  
  // Remove the existing data
  SD.remove(dataLogname);
}

// Sync the shield to the server. Currently just checking the shield and connection status
// in the future it should look for the spected device with some sync steps from the user
void sync_device() {
  int i;
  
  WiFi.config(ip_device);  
  
  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present"); 
    // don't continue:
    while(true);
  } 
  
  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) { 
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:    
    status = WiFi.begin(ssid, pass);
  } 
  Serial.println("Connected to wifi");
  printWifiStatus();
    
  i = MAX_CONNECTION_NUM;
  while (client.connect(ip_server, 80) == 0 && i>0) {
    i--;
  }
    
  if (!client.connected())
  {
    Serial.println("Could not connect to server");
  } else {
    Serial.println("Server is up. Disconnecting from server.");
    client.flush();
    client.stop();
  }
}

// Initialize variables related to measurements
void init_measurements() {
  previousMillis = millis();
  sampling_period_ms = 10;
  millis_cycle = 0;
  num_samples = 0;
  current_data_idx = 0; 
  sent_data_idx = 0;
}

// Take measurements and add them to the buffer
void record_measurement() {
  float q[7];
  int raw_values[9];
  float ypr[3]; // yaw pitch roll
  float angles[3];
  long rssi;
  unsigned long millis_start, millis_start_cycle;
  unsigned long millis_end, millis_end_cycle;

  // Check time before starting sampling
  millis_start = millis();
  if (millis_start < previousMillis)
    millis_cycle++;
  millis_start_cycle = millis_cycle;
  
  rssi = WiFi.RSSI();
  my3IMU.getRawValues(raw_values);
  my3IMU.getQ(q);
  my3IMU.getYawPitchRoll(ypr);
  my3IMU.getEuler(angles);
  
  // Check time after sampling
  millis_end = millis();
  if (millis_end < previousMillis)
    millis_cycle++;
  millis_end_cycle = millis_cycle;
  previousMillis = millis_end;
  
  // Record header info with timestamps
  header_buffer[current_data_idx].tc1 = millis_start;
  header_buffer[current_data_idx].tm1 = millis_start_cycle;
  header_buffer[current_data_idx].tc2 = millis_end;
  header_buffer[current_data_idx].tm2 = millis_end_cycle;
  
  // Record raw data
  data_buffer[current_data_idx].acc1 = raw_values[0];
  data_buffer[current_data_idx].acc2 - raw_values[1];
  data_buffer[current_data_idx].acc3 = raw_values[1];
  data_buffer[current_data_idx].gyro1 = raw_values[3];
  data_buffer[current_data_idx].gyro2 = raw_values[4];
  data_buffer[current_data_idx].gyro3 = raw_values[5];
  data_buffer[current_data_idx].magn1 = raw_values[6];
  data_buffer[current_data_idx].magn2 = raw_values[7];
  data_buffer[current_data_idx].magn3 = raw_values[8];
  data_buffer[current_data_idx].temp1 = raw_values[9];
  data_buffer[current_data_idx].press1 = raw_values[10];
  data_buffer[current_data_idx].dBm = rssi;
  
  // Record calculations
  calc_buffer[current_data_idx].quat1 = (long)(1000*q[0]);
  calc_buffer[current_data_idx].quat2 = (long)(1000*q[1]);
  calc_buffer[current_data_idx].quat3 = (long)(1000*q[2]);
  calc_buffer[current_data_idx].quat4 = (long)(1000*q[3]);
  calc_buffer[current_data_idx].quat5 = (long)(1000*q[4]);
  calc_buffer[current_data_idx].quat6 = (long)(1000*q[5]);
  calc_buffer[current_data_idx].quat7 = (long)(1000*q[6]);
  calc_buffer[current_data_idx].eurl1 = (long)(1000*angles[0]);
  calc_buffer[current_data_idx].eurl2 = (long)(1000*angles[1]);
  calc_buffer[current_data_idx].eurl3 = (long)(1000*angles[2]);
  calc_buffer[current_data_idx].yaw1 = (long)(1000*ypr[0]);
  calc_buffer[current_data_idx].yaw2 = (long)(1000*ypr[1]);
  calc_buffer[current_data_idx].yaw3 = (long)(1000*ypr[2]);
  
  increase_data_ptr(&current_data_idx);
  num_samples++;   
  
  // If overrun the number of samples, then we lose the oldest data
  // if everything was correctly implemented, current_data_idx is about sent_data_idx
  while (num_samples > MAX_NUM_SAMPLES)  
  {
    increase_data_ptr(&sent_data_idx);
    num_samples--;
  }    
}

// Creates the string from the input data sensor type
// Assumes the buffer is large enough
void create_data_wifi_str(data_sensor_t *data_sensor)
{
  
  //Check array size
  //sprintf(data, "tc1=%ul&tm1=%ul&tc2=%ul&tm2=%ul&acc1=%d&acc2=%d&acc3=%d&gyro1=%d&gyro2=%d&gyro3=%d&magn1=%d&magn2=%d&magn3=%d&temp1=%d&press1=%d&quat1=%ld&quat2=%ld&quat3=%ld&quat4=%ld&quat5=%ld&quat6=%ld&quat7=%ld&eurl1=%ld&eurl2=%ld&eurl3=%ld&yaw1=%ld&yaw2=%ld&yaw3=%ld&SSID=%s&dBm=%ld", 
  
  sprintf(header_str, "tc1=%lu&tm1=%lu&tc2=%lu&tm2=%lu&",
    header_buffer->tc1, header_buffer->tm1, header_buffer->tc2, header_buffer->tm2);
  
  sprintf(data_str, "acc1=%d&acc2=%d&acc3=%d&gyro1=%d&gyro2=%d&gyro3=%d&magn1=%d&magn2=%d&magn3=%d&temp1=%d&press1=%d&dBm=%ld&SSID=",
    data_sensor->acc1,data_sensor->acc2,data_sensor->acc3,
    data_sensor->gyro1,data_sensor->gyro2,data_sensor->gyro3,
    data_sensor->magn1,data_sensor->magn2,data_sensor->magn3,
    data_sensor->temp1,data_sensor->press1,data_sensor->dBm);
  strcat(data_str,ssid);
  //sprintf(data_str,"567890123456789012345678901234567899012345678901234567890123456789012345678901234567890"); 
  
  sprintf(calc_data_str, "&quat1=%ld&quat2=%ld&quat3=%ld&quat4=%ld&quat5=%ld&quat6=%ld&quat7=%ld&eurl1=%ld&eurl2=%ld&eurl3=%ld&yaw1=%ld&yaw2=%ld&yaw3=%ld",
    calc_buffer->quat1, calc_buffer->quat2, calc_buffer->quat3, calc_buffer->quat4,
    calc_buffer->quat5, calc_buffer->quat6, calc_buffer->quat7,
    calc_buffer->eurl1, calc_buffer->eurl2, calc_buffer->eurl3,
    calc_buffer->yaw1,  calc_buffer->yaw2,  calc_buffer->yaw3);
}


  // As in this post, I found a limitation on the length of the string
  // So I separate the string in segments if too long http://forum.arduino.cc/index.php?topic=123824.0
  // Apparently is the size define in Arduino library for SPI data transfer
  // #define  _BUFFERSIZE 100
int sendContent(char *data)
{
  int sentbytes = 0, len, charcount=0;
  char data_sub[MAX_WIFI_PACKET_SIZE+1];
  
  len = strlen(data);
  
  while (len > MAX_WIFI_PACKET_SIZE)
  {
    strncpy(data_sub,&data[charcount],MAX_WIFI_PACKET_SIZE);
    data_sub[MAX_WIFI_PACKET_SIZE] = '\0';
    //Serial.print("Sending content: ");
    //Serial.println(data_sub);
    sentbytes += client.print(data_sub);
    //Serial.print("    sent bytes:");
    //Serial.println(sentbytes);
    charcount += MAX_WIFI_PACKET_SIZE; 
    len -= MAX_WIFI_PACKET_SIZE;
  }
  
  strncpy(data_sub,&data[charcount],len);
  data_sub[len] = '\0';
  //Serial.print("Sending final content: ");
  //Serial.println(data_sub);
  sentbytes += client.println(data_sub); 
  //Serial.print("    sent bytes:");
  //Serial.println(sentbytes);
  //client.println();
  
  //Serial.print("Sent ");
  //Serial.print(sentbytes);
  //Serial.println(" bytes");
        
  return sentbytes;
}

// Send the data of the specified strings in the inputs
void sendData2Wifi(char *script, int keepConnection)
{
  int len_data = strlen(data_str), len_header = 0, len_calc = 0, len = 0;
  unsigned long sent_millis;
  char script_line[256];
  char host_line[25];
  
  // Connect to server
  if (!client.connected())
  {
    //Serial.println("reconnecting to server");
    len = MAX_CONNECTION_NUM;
    while (client.connect(ip_server, 80) == 0 && len>0) {
      len--;
    }
  } 
  if (!client.connected())
  {
    Serial.println("Unable to connect to server");
    return;
  }
  //Serial.println("connected to server");
  
  if (header_str != NULL)
    len_header = strlen(header_str);
  if (calc_data_str != NULL)
    len_calc = strlen(calc_data_str);
  len = len_data + len_header + len_calc;
  
  // Create script line from input
  sprintf(script_line,"POST %s HTTP/1.1", script);
  sprintf(host_line,"Host: %s", ip_server_host);
  
  // Make a HTTP request:
  client.flush();
  client.println(script_line);
  client.println(host_line);
  client.println("User-Agent: ArduinoWifi/1.1");
  client.println("Content-Type: application/x-www-form-urlencoded");  
  if (keepConnection)
  {
    client.println("Connection: Keep-Alive");
    client.println("Keep-Alive: timeout=10, max=5");
  }
  else
  {
    client.println("Connection: close");
  }
  client.print("Content-Length: "); 
  client.println(len); 
  client.println();
  
  if (header_str != NULL)
    sendContent(header_str);
  sendContent(data_str);  // This is b/c the max size with client.print is around 90
  if (calc_data_str != NULL)
    sendContent(calc_data_str);
  client.println();
  
  while (!waitResponse("HTTP/1.1 200 OK", len) && len>0)
  {
    len--;
  }
  //Serial.println("Completed sample");
  //delay(3000);
  
  if (!keepConnection)
  {
    client.flush();
    if (!client.connected())
    {
      client.stop();
      client.flush();
      //Serial.println("Closed connection");
    }
  }
}


// Send a number of measurements from the existing buffer
// 0 or negative indicates to send all remaining measurements
void send_measurements(int send_num_samples) {
  int i, max_i = send_num_samples;
  char data_aux[32]; // the max number is 4,294,967,295 which has 10 characters + 1 for the termination character
  int keepconnection = 1;
  
  // Exit if nothing to send
  if (num_samples == 0)
  {
    return;
  }
  
  if (num_samples < send_num_samples || send_num_samples <= 0)
    max_i = num_samples;
  
  // Connect to server
  if (!client.connected())
  {
    i = MAX_CONNECTION_NUM;
    while (client.connect(ip_server, 80) == 0 && i>0) {
      i--;
    }
  }
  
  // Send header for timestamp
  //sprintf(data_aux,"timestamp=%ul",millis());
  //sendData2Wifi("/synct.php", NULL, data_aux, NULL, 1);
  
  // Send data
  //sprintf(data,"Sending %d samples",max_i);  
  //Serial.println(data);
  
  for (i=0; i<max_i; i++)
  {
    if (i == max_i-1)
      keepconnection = 0;
    //sprintf(data_aux,"sample %d, remaining %d",i+1, num_samples);
    //Serial.println(data_aux);
    create_data_wifi_str(&data_buffer[sent_data_idx]);
    sendData2Wifi("/test1.php", 0);
    
    increase_data_ptr(&sent_data_idx);
    num_samples--;
    
  }
  
  // Send termination packet. Another timestamp to fine tune the previous value if needed
  //sprintf(data_aux,"timestamp=%ul",millis());
  //sendData2Wifi("/synct.php", NULL, data_aux, NULL, 1);
  
  
  // if server is disconnected and then flush and stop the client:
  if (client.connected())
  {
    Serial.println();
    Serial.println("disconnecting from server.");
    client.flush();
    client.stop();
  }
}


// Wait for a number of iterations for response from server
int waitResponse(char response_str[], int response_size)
{
  int result = 0, i = 0;
  char c;
  
   while (client.available() && i< response_size) {
     c = client.read();
     if (i==0)
     {
       if (c == response_str[0])
         i=1;
         result = 1;
     } else
     {
       if (c != response_str[i]) result = 0;
       i++;
     }
     //Serial.write(c);
   }
   //Serial.println();
   
   if (result)
     Serial.println("Found server response");
   //else
     //Serial.println("Could not find server response");
   
   return (i>0);
}

// Print the properties of the connection: SSID, IP shield and strength in dBm
void printWifiStatus() {
  // Print the SSID of the network you're attached to
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // Print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // Print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

// Blinks the led a number of times with specified intervals in milliseconds
void blink_led(int num_times, int dtime_ms) {
  int i;
  
  for (i=0;i<num_times; i++)
  {
    digitalWrite(STATUS_LED_PIN, HIGH);   // sets the LED on
    delay(dtime_ms);                  // waits for X milli second
    digitalWrite(STATUS_LED_PIN, LOW);   // sets the LED on
    delay(dtime_ms);                  // waits for X milli second
  }
}

// Increases the data pointer of the sample buffer. Set to 0 if it reached the maximum
void increase_data_ptr(int *idx) {
    (*idx)++;
    if (*idx >= MAX_NUM_SAMPLES)
      *idx = 0; 
}

// Decreases the data pointer of the sample buffer. Set to end if it reached the minimum
void decrease_data_ptr(int *idx) {
    (*idx)--;
    if (*idx < 0)
      *idx = MAX_NUM_SAMPLES-1; 
}

