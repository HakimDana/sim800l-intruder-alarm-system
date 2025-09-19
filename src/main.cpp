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
const unsigned long authTimeOut = 900000;

void updateSerial();
void handlecommand();
void sendsms(String sms, String phonenumber);
void readWhitelist();
void storePassword(String password);
void handleAll();

String checkstats();
String cutstring(String text, int starti, int endi);

bool CompareFirstofString(String inputString, String stringtoCompare);
bool checkMessenger(char *phonenumber);
bool writeNumberToEeprom(String number);
bool deleteNumberInEeprom(char *number);
bool verifyPassword(char *password);

unsigned char calcChecksum(String inputString);

struct storedNumber
{
        char phoneNumber[13];
        unsigned char checksum = 0;
};

class authNumList
{
private:
        uint8_t length = 0;

        struct authenticatedNumber
        {
                char number[13];
                unsigned long time = 0;
        };

        authenticatedNumber list[5];

public:
        void add(char *number)
        {
                if (length == 10)
                {
                        Serial.println("authenticated number list is full");
                        return;
                }

                for (size_t i = 0; i < length; i++)
                {
                        if (strcmp(number, list[i].number) == 0)
                        {
                                list[i].time = millis();
                                Serial.println("updated the time");
                                return;
                        }
                }

                strncpy(list[length].number, number, 13);
                list[length].time = millis();
                length++;
        }

        void deleteNumber(unsigned char index)
        {
                for (size_t i = index; i < length; i++)
                {
                        // list[i].number = list[i + 1].number;
                        strncpy(list[i].number, list[i + 1].number, 13);
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
#define bufferLength 128

        char data[bufferLength];
        uint8_t length = 0;

        void add(char character)
        {
                if (length < bufferLength - 1)
                {
                        data[length] = character;
                        data[length + 1] = '\0';
                        length++;
                }
        }
        void empty()
        {
                length = 0;
                data[0] = '\0';
        }
        void print()
        {
                for (size_t i = 0; i < strlen(data); i++)
                {
                        Serial.print(data[i]);
                }
        }
};

bool extractSMS(buffer *input, char *output, uint8_t outputLength);
void extractNumber(buffer *input, char *output);
bool executed = false;
bool enable = true;
unsigned long dialstart = 0;
String NumberWhiteList[20];

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
                Serial.println();
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
        writeNumberToEeprom("989029026240");
        storePassword("1234");
        readWhitelist();
        printwholeeeprom();
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

        // writeNumberToEeprom("989029026240");

        // Serial.println("aga in sim800l_buffer khedmat shoma:");
        // sim800l_buffer.print();

        updateSerial();
        handlecommand();
        siren.handle();
        sim800l_buffer.print();
        authenticatedNumList.handle();

        delay(200);
}

void updateSerial()
{
        sim800l_buffer.empty();
        char data;
        while (Serial.available())
        {
                mySerial.write(Serial.read()); // Forward what Serial received to Software Serial Port
        }

        // while (millis() - last < timeout)
        // {
        //         while (mySerial.available())
        //         {
        //                 data = mySerial.read();
        //                 // Serial.write(data); // Forward what Software Serial received to Serial Port
        //                 sim800l_buffer.add(data);
        //                 last = millis();
        //         }
        // }

        while (mySerial.available())
        {
                data = mySerial.read();
                // Serial.write(data); // Forward what Software Serial received to Serial Port
                sim800l_buffer.add(data);
        }
}

void handlecommand()
{

        char sms[32];

        if (extractSMS(&sim800l_buffer, sms, 32))
        {
                char senderPhoneNumber[13];
                extractNumber(&sim800l_buffer, senderPhoneNumber);

                Serial.print("sms recieved,there is your sms: ");
                Serial.println(sms);
                // for (int i = 0; i < 32; i++)
                // {
                //         Serial.print(sms[i], HEX);
                //         Serial.print(":");
                //         Serial.print(sms[i]);
                //         Serial.print(" ");
                // }

                Serial.print("the sender phone number is: ");
                Serial.println(senderPhoneNumber);
                // for (int i = 0; i < 13; i++)
                // {
                //         Serial.print(senderPhoneNumber[i], HEX);
                //         Serial.print(":");
                //         Serial.print(senderPhoneNumber[i]);
                //         Serial.print(" ");
                // }

                char *tokenp = strtok(sms, " ");

                // auth command can be used even if the phone number is not is the whitelist

                if (strstr(tokenp, "auth"))
                {
                        Serial.println("auth command detected");
                        tokenp = strtok(NULL, " ");

                        Serial.println(tokenp);
                        Serial.println(verifyPassword(tokenp));

                        for (int i = 0; i < strlen(tokenp); i++)
                        {
                                Serial.print(tokenp[i], HEX);
                                Serial.print(": ");
                                Serial.print(tokenp[i]);
                                Serial.print("  ");
                        }

                        if (verifyPassword(tokenp))
                        {
                                authenticatedNumList.add(senderPhoneNumber);
                                authenticatedNumList.show();
                        }
                }

                if (checkMessenger(senderPhoneNumber))
                {

                        Serial.println("number matched");
                        Serial.print("there is your sms:");
                        Serial.println(sms);
                        Serial.println();

                        if (strstr(tokenp, "enable"))
                        {
                                Serial.println("enable command detected");
                                enable = true;
                        }
                        else if (strstr(tokenp, "disable"))
                        {
                                Serial.println("disable command detected");
                                enable = false;
                        }
                        else if (strstr(tokenp, "stats"))
                        {
                                Serial.println("stats command detected");
                                Serial.println(checkstats());
                        }
                        else if (strstr(tokenp, "alarm off"))
                        {
                                Serial.println("alarm off command detected");
                        }
                }

                if (authenticatedNumList.check(senderPhoneNumber))
                {
                        if (strstr(tokenp, "add"))
                        {
                                Serial.println("add command detected");

                                tokenp = strtok(NULL, " ");

                                Serial.println(tokenp);

                                if (!writeNumberToEeprom(tokenp))
                                {
                                        Serial.print("succesfully wrote ");
                                        Serial.println(tokenp);
                                }
                        }
                        else if (strstr(tokenp, "del"))
                        {
                                Serial.println("del command detected");
                                tokenp = strtok(NULL, " ");
                                Serial.println(tokenp);
                                deleteNumberInEeprom(tokenp);
                        }
                        else if (strstr(tokenp, "setpass"))
                        {
                                Serial.println("setpass command detected");

                                tokenp = strtok(NULL, " ");
                                Serial.println(tokenp);
                                storePassword(tokenp);
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

void ccutstring(const char *source, int start, int end, char *dest)
{
        int i;
        for (i = 0; i < strlen(source) && i < end; i++)
        {
                dest[i] = source[i];
        }
        dest[i] = '\0';
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
bool checkMessenger(char *phonenumber)
{
        char tempChar[13];
        for (int i = 0; i < numberWhiteListLength; i++)
        {
                NumberWhiteList[i].toCharArray(tempChar, 13);
                if (strcmp(tempChar, phonenumber) == 0 || authenticatedNumList.check(phonenumber))
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

bool deleteNumberInEeprom(char *number)
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
                        if (strcmp(number, tempReadNumber.phoneNumber) == 0)
                        {
                                for (int i = 0; i < STRUCT_SIZE; i++)
                                {
                                        EEPROM.write(i + index, 255);
                                }
                                return false;
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

bool verifyPassword(char *password)
{
        // unsigned char *hash = calcpasswordHash(password);

        unsigned char *hash = MD5::make_hash(password);

        for (int i = 0; i < eepromStartIndex; i++)
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

bool extractSMS(buffer *input, char *output, uint8_t outputLength)
{
        output[0] = '\0';

        if (strstr(input->data, "+CMT"))
        {

                input->data[bufferLength - 1] = '\0';

                strtok(input->data, "\n\r");
                char *token = strtok(NULL, "\n\r");

                if (token != NULL)
                {
                        strncpy(output, token, outputLength);
                        output[outputLength - 1] = '\0';

                        return true;
                }
        }

        return false;
}

void extractNumber(buffer *input, char *output)
{

        if (strstr(input->data, "+CMT"))
        {
                // bool foundBegining = false;
                output[0] = '\0';
                for (int i = 0; i < input->length; i++)
                {

                        if (input->data[i] == '"')
                        {
                                i += 2; // skip the + and "
                                for (int ii = 0; ii < 12; ii++)
                                {
                                        output[ii] = input->data[i + ii];
                                }
                                output[12] = '\0';
                                return;
                        }
                }
        }
}