#include <Arduino.h>
#include <SoftwareSerial.h>

#define switchpin 4
#define redledpin 5
#define greenledpin 6
#define alarmpin 7

void updateSerial();
void handlealarm(bool *alarmstateptr);
void handlecommand();

String checkstats();

void sendsms(String sms, String phonenumber);
String cutstring(String text, int starti, int endi);
bool CompareFirstofString(String inputString, String stringtoCompare);
bool checkMessenger(String *phonenumber);

// Create software serial object to communicate with SIM800L
SoftwareSerial mySerial(3, 2); // SIM800L Tx & Rx is connected to Arduino #3 & #2

bool alarmstate = false;
bool executed = false;
bool newMes = false;
bool enable = true;
unsigned long dialstart = 0;
unsigned long alarmStart = 0;
String sim800lbuff = "";
String NumberWhiteList[] = {"989029026240"};

String *sim800lbuffptr = &sim800lbuff;
bool *newMesptr = &newMes;
bool *enableptr = &enable;

void setup()
{
  pinMode(13,OUTPUT);
  digitalWrite(13,HIGH);
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

  //sendsms("hello world", "989029026240");
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

      /*mySerial.println("ATD+ +989029026240;");
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
      updateSerial();*/
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

  if (checkMessenger(&Senderphonenumber) == true)
  {
    Serial.println("number matched");
  }

  String sms = "";
  if (sim800lbuff[2] == '+' && sim800lbuff[3] == 'C' && sim800lbuff[4] == 'M' && sim800lbuff[5] == 'T')
  {

    for (unsigned int i = 10; i < sim800lbuff.length() - 1; i++)
    {
      if (sim800lbuff[i] == '\n')
      {
        sms = cutstring(sim800lbuff, i + 2, sim800lbuff.length());
        Serial.print(sms[0], DEC);
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

        continue;
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
bool  checkMessenger(String *phonenumber)
{
  //String phonenumber = cutstring(sim800lbuff, 11, 21);

  for (int i = 0; i < sizeof(NumberWhiteList) / sizeof(NumberWhiteList[0]); i++)
  {
    if (*phonenumber == NumberWhiteList[i])
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
