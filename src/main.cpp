/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com
*********/

// Endereço do sensor 1: 28B07B75D0013CED
// Endereço do sensor 2: 287DA475D0013C09

#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <WiFi.h>

String apiKey = "SUA_API_KEY_AQUI";     // SUA_API_KEY_AQUI
const char *server = "api.thingspeak.com";
WiFiClient client;

// Data wire is plugged TO GPIO 4
#define ONE_WIRE_BUS 15

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// Number of temperature devices found
int numberOfDevices;

// We'll use this variable to store a found device address
DeviceAddress tempDeviceAddress;

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (deviceAddress[i] < 16)
            Serial.print("0");
        Serial.print(deviceAddress[i], HEX);
    }
}

String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++)
    {
        if (data.charAt(i) == separator || i == maxIndex)
        {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i + 1 : i;
        }
    }

    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

uint16_t crc16(uint8_t *data_p, uint8_t length)
{
    unsigned char x;
    unsigned short crc = 0xFFFF;

    while (length--)
    {
        x = crc >> 8 ^ *data_p++;
        x ^= x >> 4;
        crc = (crc << 8) ^ ((unsigned short)(x << 12)) ^ ((unsigned short)(x << 5)) ^ ((unsigned short)x);
    }
    return crc;
}

String Montar_Checksum_CRC16(String trama)
{
    String StringCRC;
    unsigned short CRC;
    uint8_t TramaConvertidaArray[trama.length()];
    int i;
    char c = ' ';
    int lastVirg = 0;

    for (i = 0; c != '\0'; i++)
    {
        c = trama[i];
        TramaConvertidaArray[i] = c;
        if (c == ',')
        {
            lastVirg = i;
        }
    }

    CRC = crc16(TramaConvertidaArray, (lastVirg + 1));

    return trama + String(CRC) + "\r\n";
}

boolean Valida_Checksum(String trama)
{
    char String_CKRecebido[10]; // coloquei pra mais...
    char c = ' ';
    int QT_campos = 0, i;
    String TRAMA_SEM_CK = "";
    uint8_t TramaConvertidaParaArray[trama.length()];
    int LastVirg = 0;

    for (i = 0; c != '\0'; i++)
    {
        c = trama[i];
        TramaConvertidaParaArray[i] = c;
        if (c == ',')
        {
            QT_campos++; // Aqui teremos a quantidade de virgulas!
            LastVirg = i;
        }
    }
    TramaConvertidaParaArray[i] = '\0';

    getValue(trama, ',', QT_campos).toCharArray(String_CKRecebido, 10); // <<<<<< aqui temos o Checksum que recebemos do PC
    unsigned short CK_Recebido = atoi(String_CKRecebido);

    unsigned short CKCalculado = crc16(TramaConvertidaParaArray, (LastVirg + 1));

    if (CK_Recebido == CKCalculado)
    {
        return true;
    }
    else
    {
        // SerialPC.println("CK Recebido|Calculado: " + String(CK_Recebido) + "|" + String(CKCalculado));
        return false;
    }
}

void QuebrarTrama(String trama)
{
    String SSID, Senha;

    int Identificador = getValue(trama, ',', 0).toInt();

    switch (Identificador)
    {
    case 0: // SSID,Senha
        SSID = getValue(trama, ',', 1);
        Senha = getValue(trama, ',', 2);

        EEPROM.writeString(0, SSID);
        EEPROM.commit();

        EEPROM.writeString(50, Senha);
        EEPROM.commit();

        break;

    default:
        break;
    }
}

void Task_Recebe_Serial(void *pvParameters)
{
    String TRAMA_PC = "";

    long timealive = 0;

    while (true)
    {
        if (Serial.available())
        {
            TRAMA_PC = "";
            TRAMA_PC = Serial.readStringUntil('\0');

            if (Valida_Checksum(TRAMA_PC))
            {
                QuebrarTrama(TRAMA_PC);
            }
        }

        // Enviando alive para aplicação
        if (millis() - timealive > 1000)
        {
            timealive = millis();
            Serial.println(Montar_Checksum_CRC16("2,1," + String(WiFi.status() == WL_CONNECTED ? "1" : "0") + ","));
        }

        vTaskDelay(50);
    }
}

void configWifi(void)
{
    WiFi.disconnect();

    String _SSID = EEPROM.readString(0);
    String _PASSWORD = EEPROM.readString(50);

    Serial.println(Montar_Checksum_CRC16("0," + _SSID + "," + _PASSWORD + ","));

    WiFi.mode(WIFI_STA);
    WiFi.begin(_SSID, _PASSWORD);

    uint8_t time500milisX = 0;
    do
    {
        vTaskDelay(500);
        Serial.print(".wf .");
        time500milisX++;
    } while ((WiFi.status() != WL_CONNECTED) && (time500milisX < 10));

    if ((WiFi.status() != WL_CONNECTED))
        Serial.println("erro wifi");

    IPAddress IP = WiFi.localIP();
    Serial.printf("\nWiFi IP address: %s\n", IP.toString());
}

void setup()
{
    // start serial port
    Serial.begin(9600);

    xTaskCreatePinnedToCore(
        Task_Recebe_Serial,
        "Task_Recebe_Serial",
        10000,
        NULL,
        1,
        NULL,
        1);

    EEPROM.begin(100);

    Serial.println(Montar_Checksum_CRC16("0," + EEPROM.readString(0) + "," + EEPROM.readString(50) + ","));

    // Start up the library
    sensors.begin();

    // Grab a count of devices on the wire
    numberOfDevices = sensors.getDeviceCount();

    // locate devices on the bus
    Serial.print("Locating devices...");
    Serial.print("Found ");
    Serial.print(numberOfDevices, DEC);
    Serial.println(" devices.");

    // Loop through each device, print out address
    for (int i = 0; i < numberOfDevices; i++)
    {
        // Search the wire for address
        if (sensors.getAddress(tempDeviceAddress, i))
        {
            Serial.print("Found device ");
            Serial.print(i, DEC);
            Serial.print(" with address: ");
            printAddress(tempDeviceAddress);
            Serial.println();
        }
        else
        {
            Serial.print("Found ghost device at ");
            Serial.print(i, DEC);
            Serial.print(" but could not detect address. Check power and cabling");
        }
    }
}

void loop()
{
    sensors.requestTemperatures(); // Send the command to get temperatures

    // Loop through each device, print out temperature data
    /*for(int i=0;i<numberOfDevices; i++){
      // Search the wire for address
      if(sensors.getAddress(tempDeviceAddress, i)){
        // Output the device ID
        Serial.print("Temperature for device: ");
        Serial.println(i,DEC);
        // Print the data
        float tempC = sensors.getTempC(tempDeviceAddress);
        Serial.print("Temp C: ");
        Serial.print(tempC);
        Serial.println("------------");
      }
    }*/

    sensors.getAddress(tempDeviceAddress, 0);
    float Temp1 = sensors.getTempC(tempDeviceAddress);

    sensors.getAddress(tempDeviceAddress, 1);
    float Temp2 = sensors.getTempC(tempDeviceAddress);

    Serial.println(Montar_Checksum_CRC16("1," + String(Temp1) + "," + String(Temp2) + ","));

    if (WiFi.status() == WL_CONNECTED)
    {
        if (client.connect(server, 80))
        {
            Serial.println("CLIENT CONECTED");

            String postStr = apiKey;

            postStr += "&field1=";
            postStr += String(Temp1);
            postStr += "&field2=";
            postStr += String(Temp2);

            postStr += "\r\n\r\n";
            client.print("POST /update HTTP/1.1\n");
            vTaskDelay(100);
            client.print("Host: api.thingspeak.com\n");
            vTaskDelay(100);
            client.print("Connection: close\n");
            vTaskDelay(100);
            client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
            vTaskDelay(100);
            client.print("Content-Type: application/x-www-form-urlencoded\n");
            vTaskDelay(100);
            client.print("Content-Length: ");
            vTaskDelay(100);
            client.print(postStr.length());
            vTaskDelay(100);
            client.print("\n\n");
            vTaskDelay(100);
            client.print(postStr);
            vTaskDelay(100);
        }

        client.stop();
    }
    else
        configWifi();

    delay(1000);
}