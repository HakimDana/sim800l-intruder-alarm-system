#include <Arduino.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <MD5.h>

#define switchpin 4
#define redledpin 5
#define greenledpin 6
#define alarmpin 7

const unsigned char eepromStartIndex = 16;
const int alarmDuration = 60000;
const unsigned long authTimeOut = 3000;

void updateSerial();
void handlecommand();
void sendsms(String sms, String phonenumber);
void readWhitelist();
void storePassword(String password);
void handleAll();

String checkstats();
String cutstring(String text, int starti, int endi);

bool CompareFirstofString(String inputString, String stringtoCompare);
bool checkMessenger(String phonenumber);
bool writeNumberToEeprom(String number);
bool deleteNumberInEeprom(String number);
bool verifyPassword(String password);


unsigned char calcChecksum(String inputString);

struct storedNumber
{
        char phoneNumber[13];
        unsigned char checksum = 0;
};

class authNumList
{
        private:
        unsigned char length = 0;
        
        struct authenticatedNumber
        {
                String number;
                unsigned long time;
        };
        
        authenticatedNumber list[10];
        
        public:
        void add(String number)
        {
                if (length == 10)
                {
                        Serial.println("authenticated number list is full");
                        return;
                }
                
                for (size_t i = 0; i < length; i++)
                {
                        if (list[i].number == number)
                        {
                                list[i].time = millis();
                                Serial.println("updated the time");
                                return;
                        }
                }
                
                list[length].number = number;
                list[length].time = millis();
                length++;
        }
        
        void deleteNumber(unsigned char index)
        {
                for (size_t i = index; i < length; i++)
                {
                        list[i].number = list[i + 1].number;
                        list[i].time = list[i + 1].time;
                }
                length--;
        }
        
        void handle()
        {
                for (size_t i = 0; i < length; i++)
                {
                        if (millis() - list[i].time >= authTimeOut)
                        {
                                deleteNumber(i);
                                i--;
                        }
                }
        }
        
        void show()
        {
                Serial.println("authenticated number white list: ");
                for (size_t i = 0; i < length; i++)
                {
                        Serial.print(list[i].number);
                        Serial.print(" ");
                        Serial.print(list[i].time);
                        Serial.print(" | ");
                }
        }
        
        bool check(String number)
        {
                handle();
                for (size_t i = 0; i < length; i++)
                {
                        if (number == list[i].number)
                        {
                                return true;
                        }
                }
                return false;
        }
};

class alarm
{
        private:
        bool State = false;
        int pin;
        unsigned long activationTime;
        
        public:
        alarm(int alarmPin);
        void arm()
        {
                digitalWrite(pin, HIGH);
                State = true;
                activationTime = millis();
        }
        
        void handle()
        {
                if (millis() - activationTime > alarmDuration)
                {
                        digitalWrite(pin, LOW);
                        State = false;
                }
        }
        
        bool getState()
        {
                return State;
        }
};


alarm::alarm(int alarmPin)
{
        pin = alarmPin;
}
struct buffer
{
        char data[128];
        char length = 0;
        void add(char character)
        {
                if (length < 127)
                {
                        data[length] = character;
                        length++;
                }
        }
        void print()
        {
                for (size_t i = 0; i < length; i++)
                {
                        Serial.print(data[i]);
                }
                
        }
};

bool extractSMS(buffer *input, buffer *output);
bool executed = false;
bool enable = true;
unsigned long dialstart = 0;
String sim800lbuff = "";
String NumberWhiteList[70];

unsigned char numberWhiteListLength = 0;

void printwholeeeprom()
{
        for (int i = 0; i < eepromStartIndex; i++)
        {
                Serial.print(EEPROM[i], HEX);
                Serial.print(" ");
        }
        Serial.println("");

        for (int i = eepromStartIndex; i < EEPROM.length(); i += 14)
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

// Create software serial object to communicate with SIM800L
SoftwareSerial mySerial(3, 2); // SIM800L Tx & Rx is connected to Arduino #3 & #2
alarm siren(alarmpin);
authNumList authenticatedNumList;
buffer sim800l_buffer;
buffer test;

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
        //   EEPROM.update(i, 255);
        // }

        readWhitelist();
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
                        siren.arm();

                        for (int index = 0; index < numberWhiteListLength; index++)
                        {
                                mySerial.println("ATD+ +" + NumberWhiteList[index] + ";");
                                updateSerial();

                                dialstart = millis();
                                delay(10);

                                while (millis() - dialstart <= 10000)
                                {
                                        updateSerial();
                                        handlecommand();
                                        siren.handle();

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

        // Serial.println("aga in sim800l_buffer khedmat shoma:");
        // sim800l_buffer.print();
        Serial.print("aga inam javab estekhraj sms: ");
        Serial.println(extractSMS(&sim800l_buffer,&test),DEC);
        Serial.print("aga inam buffer output: ");
        test.print();
        test.length = 0;
        //Serial.println();

        
        updateSerial();
        handlecommand();
        siren.handle();

        delay(100);
}

void updateSerial()
{
        sim800lbuff = "";
        sim800l_buffer.length = 0;
        char data;
        delay(500);
        while (Serial.available())
        {
                mySerial.write(Serial.read()); // Forward what Serial received to Software Serial Port
        }
        while (mySerial.available())
        {
                data = mySerial.read();
                // Serial.write(data); // Forward what Software Serial received to Serial Port
                sim800lbuff += data;
                sim800l_buffer.add(data);
        }
}

void handlecommand()
{
        String Senderphonenumber = sim800lbuff.substring(10, 22);
        Serial.println(sim800lbuff.substring(10, 22));
        if (checkMessenger(Senderphonenumber))
        {
                Serial.println("number matched");

                String sms = "";
                if (sim800lbuff.substring(2, 6) == "+CMT")
                {

                        for (unsigned int i = 10; i < sim800lbuff.length() - 1; i++)
                        {
                                if (sim800lbuff[i] == '\n')
                                {
                                        sms = cutstring(sim800lbuff, i + 2, sim800lbuff.length());
                                        Serial.print("there is your sms:");
                                        Serial.println(sms);

                                        if (CompareFirstofString(sms, "enable"))
                                        {
                                                Serial.println("enable command detected");
                                                enable = true;
                                        }
                                        else if (CompareFirstofString(sms, "disable"))
                                        {
                                                Serial.println("disable command detected");
                                                enable = false;
                                        }
                                        else if (CompareFirstofString(sms, "stats"))
                                        {
                                                Serial.println("stats command detected");
                                                Serial.println(checkstats());
                                        }
                                        else if (CompareFirstofString(sms, "alarm off"))
                                        {
                                                Serial.println("alarm off command detected");
                                        }
                                        else if (CompareFirstofString(sms, "auth"))
                                        {
                                                Serial.println("auth number command detected");
                                                String password = cutstring(sms, 4, sms.length() - 1);
                                                Serial.println(password);
                                                Serial.println(verifyPassword(password));
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
        if (!siren.getState())
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

        for (int index = eepromStartIndex; index < EEPROM.length(); index += STRUCT_SIZE)
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

        for (int index = eepromStartIndex; index < EEPROM.length(); index += STRUCT_SIZE)
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

        for (int index = eepromStartIndex; index < EEPROM.length(); index += STRUCT_SIZE)
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

void storePassword(String password)
{

        char charPassword[password.length() + 1];
        password.toCharArray(charPassword, password.length() + 1);
        unsigned char *hash = MD5::make_hash(charPassword);

        for (size_t i = 0; i < eepromStartIndex; i++)
        {
                EEPROM.update(i, hash[i]);
        }

        char *md5str = MD5::make_digest(hash, 16);
        free(hash);
        // print it on our serial monitor
        Serial.println(md5str);
        // Give the Memory back to the System if you run the md5 Hash generation in a loop
        free(md5str);
}

bool verifyPassword(String password)
{
        // unsigned char *hash = calcpasswordHash(password);

        char charPassword[password.length() + 1];
        password.toCharArray(charPassword, password.length() + 1);
        unsigned char *hash = MD5::make_hash(charPassword);

        for (size_t i = 0; i < eepromStartIndex; i++)
        {
                if (hash[i] != EEPROM[i])
                {
                        free(hash);
                        return false;
                }
        }
        free(hash);
        return true;
}

void handleAll()
{
        updateSerial();
        handlecommand();
        siren.handle();
}

bool extractSMS(buffer *input, buffer *output)
{
        output->length = 0;

        if (strstr(input->data, "+CMT"))
        {
                bool newLineFound = false;
                for (size_t i = 3; i < input->length; i++)
                {
                        if (input->data[i] == '\n')
                        {
                                newLineFound = !newLineFound;
                                i++;//
                        }
                        
                        if(newLineFound){
                                output->add(input->data[i]);
                        }
                }

                if (newLineFound)
                {
                        return true;
                }
                
        }

        return false;
}