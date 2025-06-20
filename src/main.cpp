#include <Arduino.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

#define switchpin 4
#define redledpin 5
#define greenledpin 6
#define alarmpin 7

void updateSerial();
void handlealarm(bool *alarmstateptr);
void handlecommand();
void sendsms(String sms, String phonenumber);
void readWhitelist();

String checkstats();
String cutstring(String text, int starti, int endi);

bool CompareFirstofString(String inputString, String stringtoCompare);
bool checkMessenger(String phonenumber);
bool writeNumberToEeprom(String number);
bool deleteNumberInEeprom(String number);

unsigned char calcChecksum(String inputString);

struct storedNumber
{
  char phoneNumber[13];
  unsigned char checksum = 0;
};

// Create software serial object to communicate with SIM800L
SoftwareSerial mySerial(3, 2); // SIM800L Tx & Rx is connected to Arduino #3 & #2

bool alarmstate = false;
bool executed = false;
bool newMes = false;
bool enable = true;
unsigned long dialstart = 0;
unsigned long alarmStart = 0;
String sim800lbuff = "";
String NumberWhiteList[70];

unsigned char numberWhiteListLength = 0;

String *sim800lbuffptr = &sim800lbuff;
bool *newMesptr = &newMes;
bool *enableptr = &enable;

void printwholeeeprom()
{
  for (int i = 0; i < EEPROM.length(); i += 14)
  {
    for (int index = 0; index < 14 && i + index < EEPROM.length(); index++)
    {
      Serial.print((char)EEPROM[i + index]);
      Serial.print(" ");
    }
    Serial.println("");
  }
}

void printwholewhitelist()
{
  for (int i = 0; i < numberWhiteListLength; i++)
  {

    Serial.print(NumberWhiteList[i]);
    Serial.print(" ");
  }
  Serial.println(" aga white listo printidam");
}

void setup()
{
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  // Begin serial communication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(9600);

  // Begin serial communication with Arduino and SIM800L
  mySerial.begin(9600);

  pinMode(switchpin, INPUT_PULLUP);
  pinMode(redledpin, OUTPUT);
  pinMode(greenledpin, OUTPUT);
  pinMode(alarmpin, OUTPUT);

  Serial.println("Initializing...");
  delay(1000);

  mySerial.println("AT"); // Once the handshake test is successful, i t will back to OK
  updateSerial();
  mySerial.println("AT+CMGF=1"); // Change to text mode
  updateSerial();
  mySerial.println("AT+CNMI=1,2,0,0,0");
  updateSerial();
  // erase eeprom:
  // for (int i = 0; i < EEPROM.length(); i++)
  //{
  //  EEPROM.update(i, 255);
  //}

  // Serial.println(writeNumberToEeprom("985138442196"));
  // readWhitelist();
  // printwholewhitelist();
  // Serial.println(writeNumberToEeprom("985138442196"));
  // readWhitelist();
  deleteNumberInEeprom("985138442196");
  writeNumberToEeprom("989029026240");
  printwholeeeprom();
  readWhitelist();
  printwholewhitelist();
  Serial.print("numberwhitelistlength is: ");
  Serial.println(numberWhiteListLength);
}
void loop()
{

  if (digitalRead(switchpin) == HIGH)
  {
    if (enable == false)
    {
      digitalWrite(greenledpin, LOW);
      digitalWrite(redledpin, LOW);
    }
    else
    {
      digitalWrite(greenledpin, LOW);
      digitalWrite(redledpin, HIGH);
    }

    if (enable == false)
    {
      executed = true;
    }

    if (executed == false)
    {
      digitalWrite(alarmpin, HIGH);
      alarmstate = true;

      alarmStart = millis();

      for (int index = 0; index < sizeof(NumberWhiteList) / sizeof(NumberWhiteList[0]); index++)
      {
        mySerial.println("ATD+ +" + NumberWhiteList[index] + ";");
        updateSerial();

        dialstart = millis();
        delay(10);

        while (millis() - dialstart <= 10000)
        {
          updateSerial();
          handlecommand();
          handlealarm(&alarmstate);

          if (enable == false)
          {
            digitalWrite(greenledpin, LOW);
            digitalWrite(redledpin, LOW);
          }
          else if (digitalRead(switchpin) == LOW)
          {
            digitalWrite(greenledpin, HIGH);
            digitalWrite(redledpin, LOW);
          }
        }

        mySerial.println("ATH");
        updateSerial();
      }
    }

    executed = true;
  }
  else
  {
    if (enable == false)
    {
      digitalWrite(greenledpin, LOW);
      digitalWrite(redledpin, LOW);
    }
    else
    {
      digitalWrite(redledpin, LOW);
      digitalWrite(greenledpin, HIGH);
    }
    executed = false;
  }

  updateSerial();
  handlecommand();
  handlealarm(&alarmstate);

  delay(100);
}

void updateSerial()
{
  *sim800lbuffptr = "";
  char data;
  delay(500);
  while (Serial.available())
  {
    mySerial.write(Serial.read()); // Forward what Serial received to Software Serial Port
  }
  while (mySerial.available())
  {
    data = mySerial.read();
    Serial.write(data); // Forward what Software Serial received to Serial Port
    sim800lbuff += data;
  }
}

void handlealarm(bool *alarmStateptr)
{
  if (millis() - alarmStart > 600000)
  {
    digitalWrite(alarmpin, LOW);
    *alarmStateptr = false;
  }
}

void handlecommand()
{
  String Senderphonenumber = cutstring(sim800lbuff, 11, 21);

  if (checkMessenger(Senderphonenumber) == true)
  {
    Serial.println("number matched");

    String sms = "";
    if (sim800lbuff[2] == '+' && sim800lbuff[3] == 'C' && sim800lbuff[4] == 'M' && sim800lbuff[5] == 'T')
    {

      for (unsigned int i = 10; i < sim800lbuff.length() - 1; i++)
      {
        if (sim800lbuff[i] == '\n')
        {
          sms = cutstring(sim800lbuff, i + 2, sim800lbuff.length());
          Serial.print("there is your sms:");
          Serial.println(sms);

          if (CompareFirstofString(sms, "enable") == true)
          {
            Serial.println("enable command detected");
            *enableptr = true;
          }
          else if (CompareFirstofString(sms, "disable") == true)
          {
            Serial.println("disable command detected");
            *enableptr = false;
          }
          else if (CompareFirstofString(sms, "stats") == true)
          {
            Serial.println("stats command detected");
            Serial.println(checkstats());
          }
          else if (CompareFirstofString(sms, "alarm off") == true)
          {
            Serial.println("alarm off command detected");
          }
          else if (CompareFirstofString(sms, "add number"))
          {
            Serial.println("add number command detected");
          }
          continue;
        }
      }
    }
  }
}
// returns a string starting and ending from the given index numbers in the given string
String cutstring(String text, int starti, int endi)
{

  String outputstring = "";
  for (int i = starti - 1; i <= endi; i++)
  {
    outputstring += text[i];
  }

  return outputstring;
}
// compares the first characters of input string to the second one
bool CompareFirstofString(String inputString, String stringtoCompare)
{
  for (unsigned int i = 0; i < stringtoCompare.length(); i++)
  {
    if (inputString[i] != stringtoCompare[i])
    {
      return false;
    }
  }
  return true;
}
// compares the senders phone number to the given number
bool checkMessenger(String phonenumber)
{
  // String phonenumber = cutstring(sim800lbuff, 11, 21);

  for (int i = 0; i < numberWhiteListLength; i++)
  {
    if (phonenumber == NumberWhiteList[i])
    {
      return true;
    }
  }
  return false;
}

void sendsms(String sms, String phonenumber)
{
  mySerial.println("AT+CMGS=\"+" + phonenumber + "\""); // change ZZ with country code and xxxxxxxxxxx with phone number to sms
  updateSerial();
  mySerial.print(sms); // text content
  updateSerial();
  mySerial.write(26);
}

String checkstats()
{
  String message = "Door state: ";
  if (digitalRead(switchpin) == LOW)
  {
    message += "closed";
  }
  else
  {
    message += "open";
  }

  message += "\nAlarm state: ";
  if (!alarmstate)
  {
    message += "inactive";
  }
  else
  {
    message += "active";
  }

  return message;
}

unsigned char calcChecksum(String inputString)
{
  unsigned char checksum = 0;

  for (int i = 0; i < inputString.length(); i++)
  {
    checksum += inputString[i];
  }

  return checksum;
}

void readWhitelist()
{
  numberWhiteListLength = 0;
  storedNumber tempReadNumber;

  const int STRUCT_SIZE = sizeof(tempReadNumber);

  for (int index = 0; index < EEPROM.length(); index += STRUCT_SIZE)
  {
    bool isEmpty = false;
    for (int slotIndex = 0; slotIndex < STRUCT_SIZE; slotIndex++)
    {
      if (EEPROM[slotIndex + index] != 0xff)
      {
        isEmpty = false;
        break;
      }
      else
      {
        isEmpty = true;
      }
    }
    if (isEmpty == false)
    {
      EEPROM.get(index, tempReadNumber);
      if (tempReadNumber.checksum == calcChecksum(tempReadNumber.phoneNumber))
      {
        NumberWhiteList[numberWhiteListLength] = tempReadNumber.phoneNumber;
        numberWhiteListLength++;
      }
    };
  }
  Serial.print("number white list inside the function: ");
  printwholewhitelist();
}

bool writeNumberToEeprom(String number)
{
  if (number.length() != 12)
  {
    Serial.println("bad format :(");
    return true;
  }

  readWhitelist();

  for (int i = 0; i < numberWhiteListLength; i++)
  {
    Serial.println("man to in for e am");
    if (number == NumberWhiteList[i])
    {
      Serial.println("number is already stored!!");
      return true;
    }
  }

  storedNumber tempStoredNumber;
  number.toCharArray(tempStoredNumber.phoneNumber, 13);
  tempStoredNumber.checksum = calcChecksum(tempStoredNumber.phoneNumber);

  Serial.println(tempStoredNumber.phoneNumber);
  Serial.println(tempStoredNumber.checksum);

  const int STRUCT_SIZE = sizeof(tempStoredNumber);

  for (int index = 0; index < EEPROM.length(); index += STRUCT_SIZE)
  {

    bool isEmpty = true;
    for (int slotIndex = 0; slotIndex < STRUCT_SIZE; slotIndex++)
    {

      if (EEPROM[slotIndex + index] != 0xff)
      {
        isEmpty = false;
      }
      else
      {
        isEmpty = true;
        break;
      }
    }
    if (isEmpty)
    {
      EEPROM.put(index, tempStoredNumber);
      return false;
    }
  }
  Serial.println("not enough space on eeprom!!");
  return true;
}

bool deleteNumberInEeprom(String number)
{

  numberWhiteListLength = 0;
  storedNumber tempReadNumber;

  const int STRUCT_SIZE = sizeof(tempReadNumber);

  for (int index = 0; index < EEPROM.length(); index += STRUCT_SIZE)
  {
    bool isEmpty = false;
    for (int slotIndex = 0; slotIndex < STRUCT_SIZE; slotIndex++)
    {
      if (EEPROM[slotIndex + index] != 0xff)
      {
        isEmpty = false;
        break;
      }
      else
      {
        isEmpty = true;
      }
    }
    if (isEmpty == false)
    {
      EEPROM.get(index, tempReadNumber);
      if (number == tempReadNumber.phoneNumber)
      {
        for (int i = 0; i < STRUCT_SIZE; i++)
        {
          EEPROM.write(i + index, 255);
        }
        return 0;
      }
    }
  }
  Serial.println("shomare ro piyda nakardom");
  return true;
}
