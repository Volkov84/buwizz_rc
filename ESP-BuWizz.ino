/*
 * Program to Connect BuWizz to any PPM output RC trasmitter
 * In my setup I'm using a Flysky GT3C - PPM output mod + ESP32 dev board
 * 
 */

#include <BLEDevice.h>

#define RECEIVE_PIN 33 //PPM receiver pin
#define CHANNEL_AMOUNT 3 //Number of RC channels
#define DETECTION_SPACE 2500
#define METHOD RISING
int ch[CHANNEL_AMOUNT + 1];

static BLEUUID serviceUUID("4e050000-74fb-4481-88b3-9919b1676e93"); //Service UUID of BuWizz 
static BLEUUID charUUID(BLEUUID((uint16_t)0x92d1)); //Characteristic  UUID of BuWizz control
//String My_BLE_Address = "50:fa:ab:f5:43:55"; //MAC of the BuWizz
static BLERemoteCharacteristic* pRemoteCharacteristic;

BLEScan* pBLEScan; //Name the scanning device as pBLEScan
BLEScanResults foundDevices;

static BLEAddress *Server_BLE_Address;
String Scaned_BLE_Address;

boolean paired = false; //boolean variable to toggel pairing
boolean DEBUGGING = false; //Debuging Serial output. Slows down the operation.. Used for dev only. Basic serial will still on, only the main loop is disabled.
boolean notfound = true;

int8_t ch1; //Temp variable for CH1
int8_t ch2; //Temp variable for CH2

uint8_t valueselect[] = {0x10,0x00,0x00,0x00,0x00,0x00}; //BuWizz "payload" to control the outputs.
uint8_t modeselect[] = {0x11,0x02}; //BuWizz mode select. 0x01: slow, 0x02: normal, 0x03: fast, 0x04: ludicous

boolean mode_fast = false;


bool connectToserver (BLEAddress pAddress){
    BLEClient*  pClient  = BLEDevice::createClient();
    boolean connection_ok = false;
    while (!connection_ok){
      Serial.println("Connecting to BuWizz...");
      pClient->connect(BLEAddress(pAddress));
      if (pClient->isConnected()){
        connection_ok = true;
        Serial.println("Connected to BuWizz");
      }
    }
    
    
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);     // Obtain a reference to the service we are after in the remote BLE server.
    if (pRemoteService != nullptr)
    {
      Serial.println(" - Found the service");
    }
    else
    return false;

    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);     // Obtain a reference to the characteristic in the service of the remote BLE server.
    if (pRemoteCharacteristic != nullptr)
    {
      Serial.println(" - Found the characteristic");
      return true;
    }
    else
    return false;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks 
{
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      Serial.printf("Scan Result: %s \n", advertisedDevice.toString().c_str());
        if (advertisedDevice.getName() == "BuWizz"){
          Serial.println("BuWizz found! MAC address:");
          advertisedDevice.getScan()->stop();
          notfound=false;
          Server_BLE_Address = new BLEAddress(advertisedDevice.getAddress());
          Scaned_BLE_Address = Server_BLE_Address->toString().c_str();
          Serial.println(Scaned_BLE_Address);
          }
    }
};

void setup() {
    Serial.begin(115200); //Start serial monitor 
    Serial.println("ESP32 BLE BuWizz Interface on PPM"); //Intro message
    pinMode(RECEIVE_PIN, INPUT_PULLUP); //PPM receiver pullup
    attachInterrupt(digitalPinToInterrupt(RECEIVE_PIN), ppm_interrupt, METHOD); //PPM interupt

    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan(); //create new scan
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks()); //Call the class that is defined above 
    pBLEScan->setActiveScan(false); //active scan uses more power, but get results faster

    while (notfound == true){ //Searching for BLE device names "BuWizz"
      pBLEScan->start(3); 
      }
    while (connectToserver(*Server_BLE_Address) != true){  
      Serial.println("Tikk-Takk.. waiting..");
    }
}

void loop() {
  if (DEBUGGING){ ppm_write();}
  if (paired == true)
    {
      if (ch[1]<1470) ch1 = map(ch[1],940,1480,-127,0);
      else{
        if (ch[1]>1500) ch1 = map(ch[1],1501,2045,0,127);
        else ch1=0;
      }
      valueselect[2]=ch1;    //Read ch1 output from the interupts, range is from  about 900 to 2000, so remap and reformat into 7 bit signed number.
      
      if (ch[2]<1460) ch2 = map(ch[2],970,1460,-127,0);
      else{
        if (ch[2]>1500) ch2 = map(ch[2],1501,2005,0,127);
       else ch2=0;
      }
      valueselect[3]=ch2;   //Same as above, but for ch2
      
     if (ch[3] > 1500)
     {  
          if (mode_fast == false)
          {
            mode_fast = true;
            modeselect[1] = 0x04;
            pRemoteCharacteristic->writeValue((uint8_t*)modeselect, 2, true);
          }    
     }
     else
     {
      if (mode_fast == true)
        {
          mode_fast = false;
          modeselect[1] = 0x02;
          pRemoteCharacteristic->writeValue((uint8_t*)modeselect, 2, true);
        }
     }        //Mode select. On my GT3, I have a 2 position switch. 1st pos: normal mode, 2nd pos: ludicorus mode.
     
      if (DEBUGGING){
           Serial.print("Steering: ");
           Serial.print(ch[1]);
           Serial.print("\t");
           Serial.print(valueselect[2], HEX);
           Serial.print("\n");
           Serial.print("Throttle: ");
           Serial.print(ch[2]);
           Serial.print("\t");
           Serial.print(valueselect[3], HEX);
           Serial.print("\n");
           Serial.print("CH3: ");
           Serial.print(ch[3]);
           Serial.print("\t");
           Serial.print(modeselect[1], HEX);
           Serial.print("\n");
      }
       pRemoteCharacteristic->writeValue((uint8_t*)valueselect, 6, true);     // send motor values to BuWizz
    }    
}

void ppm_write()
{
  static unsigned long int t;
  if (millis() - t < 100)
  return;
  for (byte i = 0; i < CHANNEL_AMOUNT + 1; i++)
  {
    Serial.print(ch[i]);
    Serial.print("\t");
  }
  Serial.print("\n");
  t = millis();
}

void ppm_interrupt()
{
  static byte i;
  static unsigned long int t_old;
  unsigned long int t = micros(); //store time value a when pin value falling/rising
  unsigned long int dt = t - t_old; //calculating time inbetween two peaks
  t_old = t;
  
  if ((dt > DETECTION_SPACE) || (i > CHANNEL_AMOUNT))
  {
  i = 0;
  }
  ch[i++] = dt;
}
