#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "settings.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

/***********************************
            Main Setup
 ***********************************/
void setup()
{
    // Initialize pins
    //pinMode(LED_BUILTIN, OUTPUT);
    //digitalWrite(LED_BUILTIN, LOW);
    Serial.begin(BAUD_RATE);
    Serial2.begin(BAUD_RATE, SERIAL_8N1, RXD2, TXD2, true);

#ifdef DEBUG
    Serial.println("Booting - DEBUG mode on");
    blinkLed(2, 500);
    delay(500);
    blinkLed(2, 2000);
    // Blinking 2 times fast and two times slower to indicate DEBUG mode
#endif
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
#ifdef DEBUG
        Serial.println("Connection Failed! Rebooting...");
#endif
        delay(5000);
        ESP.restart();
    }
    delay(3000);
    setupDataReadout();
    setupOTA();

    mqttClient.setServer(MQTT_HOST, atoi(MQTT_PORT));
    blinkLed(5, 500); // Blink 5 times to indicate end of setup
#ifdef DEBUG
    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
#endif
}

/***********************************
            Main Loop
 ***********************************/
void loop()
{
    long now = millis();
    if (WiFi.status() != WL_CONNECTED)
    {
        blinkLed(20, 50); // Blink fast to indicate failed WiFi connection
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        while (WiFi.waitForConnectResult() != WL_CONNECTED)
        {
#ifdef DEBUG
            Serial.println("Connection Failed! Rebooting...");
#endif
            delay(5000);
            ESP.restart();
        }
    }

    ArduinoOTA.handle();

    if (!mqttClient.connected())
    {
        if (now - LAST_RECONNECT_ATTEMPT > 5000)
        {
            LAST_RECONNECT_ATTEMPT = now;

            if (!mqttReconnect())
            {
#ifdef DEBUG
                Serial.println("Connection to MQTT Failed! Rebooting...");
#endif
                delay(5000);
                ESP.restart();
            }
            else
            {
                LAST_RECONNECT_ATTEMPT = 0;
            }
        }
    }
    else
    {
        mqttClient.loop();
    }

    // Check if we want a full update of all the data including the unchanged data.
    if (true)
    {
        for (int i = 0; i < NUMBER_OF_READOUTS; i++)
        {
            telegramObjects[i].sendData = true;
            LAST_FULL_UPDATE_SENT = millis();
        }
    }

    if (now - LAST_UPDATE_SENT > UPDATE_INTERVAL)
    {
        if (readP1Serial())
        {
            LAST_UPDATE_SENT = millis();
            sendDataToBroker();
        }
    }
}

/***********************************
            Setup Methods
 ***********************************/

/**
   setupDataReadout()

   This method can be used to create more data readout to mqtt topic.
   Use the name for the mqtt topic.
   The code for finding this in the telegram see
    https://www.netbeheernederland.nl/_upload/Files/Slimme_meter_15_a727fce1f1.pdf for the dutch codes pag. 19 -23
   Use startChar and endChar for setting the boundies where the value is in between.
   Default startChar and endChar is '(' and ')'
   Note: Make sure when you add or remove telegramObject to update the NUMBER_OF_READOUTS accordingly.
*/
void setupDataReadout()
{
    // 1-0:1.8.1(000992.992*kWh)
    // 1-0:1.8.1 = Elektra verbruik laag tarief (DSMR v5.0)
    telegramObjects[0].name = "consumption_low_tarif";
    strcpy(telegramObjects[0].code, "1-0:1.8.1");
    telegramObjects[0].endChar = '*';

    // 1-0:1.8.2(000560.157*kWh)
    // 1-0:1.8.2 = Elektra verbruik hoog tarief (DSMR v5.0)
    telegramObjects[1].name = "consumption_high_tarif";
    strcpy(telegramObjects[1].code, "1-0:1.8.2");
    telegramObjects[1].endChar = '*';

    // 1-0:1.7.0(00.424*kW) Actueel verbruik
    // 1-0:1.7.x = Electricity consumption actual usage (DSMR v5.0)
    telegramObjects[2].name = "actual_consumption";
    strcpy(telegramObjects[2].code, "1-0:1.7.0");
    telegramObjects[2].endChar = '*';

    // 1-0:2.7.0(00.000*kW) Actuele teruglevering (-P) in 1 Watt resolution
    telegramObjects[3].name = "actual_returndelivery";
    strcpy(telegramObjects[3].code, "1-0:2.7");
    telegramObjects[3].endChar = '*';

    // 1-0:21.7.0(00.378*kW)
    // 1-0:21.7.0 = Instantaan vermogen Elektriciteit levering L1
    telegramObjects[4].name = "l1_instant_power_usage";
    strcpy(telegramObjects[4].code, "1-0:21.7.0");
    telegramObjects[4].endChar = '*';

    // 1-0:41.7.0(00.378*kW)
    // 1-0:41.7.0 = Instantaan vermogen Elektriciteit levering L2
    telegramObjects[5].name = "l2_instant_power_usage";
    strcpy(telegramObjects[5].code, "1-0:41.7.0");
    telegramObjects[5].endChar = '*';

    // 1-0:61.7.0(00.378*kW)
    // 1-0:61.7.0 = Instantaan vermogen Elektriciteit levering L3
    telegramObjects[6].name = "l3_instant_power_usage";
    strcpy(telegramObjects[6].code, "1-0:61.7.0");
    telegramObjects[6].endChar = '*';

    // 1-0:31.7.0(002*A)
    // 1-0:31.7.0 = Instantane stroom Elektriciteit L1
    telegramObjects[7].name = "l1_instant_power_current";
    strcpy(telegramObjects[7].code, "1-0:31.7.0");
    telegramObjects[7].endChar = '*';

    // 1-0:51.7.0(002*A)
    // 1-0:51.7.0 = Instantane stroom Elektriciteit L2
    telegramObjects[8].name = "l2_instant_power_current";
    strcpy(telegramObjects[8].code, "1-0:51.7.0");
    telegramObjects[8].endChar = '*';

    // 1-0:71.7.0(002*A)
    // 1-0:71.7.0 = Instantane stroom Elektriciteit L3
    telegramObjects[9].name = "l3_instant_power_current";
    strcpy(telegramObjects[9].code, "1-0:71.7.0");
    telegramObjects[9].endChar = '*';

    // 1-0:32.7.0(232.0*V)
    // 1-0:32.7.0 = Voltage L1
    telegramObjects[10].name = "l1_voltage";
    strcpy(telegramObjects[10].code, "1-0:32.7.0");
    telegramObjects[10].endChar = '*';

    // 1-0:52.7.0(232.0*V)
    // 1-0:52.7.0 = Voltage L2
    telegramObjects[11].name = "l2_voltage";
    strcpy(telegramObjects[11].code, "1-0:52.7.0");
    telegramObjects[11].endChar = '*';

    // 1-0:72.7.0(232.0*V)
    // 1-0:72.7.0 = Voltage L3
    telegramObjects[12].name = "l3_voltage";
    strcpy(telegramObjects[12].code, "1-0:72.7.0");
    telegramObjects[12].endChar = '*';

    // 0-0:96.14.0(0001)
    // 0-0:96.14.0 = Actual Tarif
    telegramObjects[13].name = "actual_tarif_group";
    strcpy(telegramObjects[13].code, "0-0:96.14.0");

    // 0-0:96.7.21(00003)
    // 0-0:96.7.21 = Aantal onderbrekingen Elektriciteit
    telegramObjects[14].name = "short_power_outages";
    strcpy(telegramObjects[14].code, "0-0:96.7.21");

    // 0-0:96.7.9(00001)
    // 0-0:96.7.9 = Aantal lange onderbrekingen Elektriciteit
    telegramObjects[15].name = "long_power_outages";
    strcpy(telegramObjects[15].code, "0-0:96.7.9");

    // 1-0:32.32.0(00000)
    // 1-0:32.32.0 = Aantal korte spanningsdalingen Elektriciteit in fase 1
    telegramObjects[16].name = "short_power_drops";
    strcpy(telegramObjects[16].code, "1-0:32.32.0");

    // 1-0:32.36.0(00000)
    // 1-0:32.36.0 = Aantal korte spanningsstijgingen Elektriciteit in fase 1
    telegramObjects[17].name = "short_power_peaks";
    strcpy(telegramObjects[17].code, "1-0:32.36.0");

    // 0-1:24.2.1(150531200000S)(00811.923*m3)
    // 0-1:24.2.1 = Gas (DSMR v4.0) on Kaifa MA105 meter
    telegramObjects[18].name = "gas_meter_m3";
    strcpy(telegramObjects[18].code, "0-1:24.2.1");
    telegramObjects[18].endChar = '*';

    // 1-0:2.8.1(000596.853*kWh)
    // 1-0:2.8.1 = Elektra teruglevering laag tarief (DSMR v4.0)
    telegramObjects[19].name = "returndelivery_low_tarif";
    strcpy(telegramObjects[19].code, "1-0:2.8.1");
    telegramObjects[19].endChar = '*';
    

    // 1-0:2.8.2(001441.564*kWh)
    // 1-0:2.8.2 = Elektra teruglevering hoog tarief (DSMR v4.0)
    telegramObjects[20].name = "returndelivery_high_tarif";
    strcpy(telegramObjects[20].code, "1-0:2.8.2");
    telegramObjects[20].endChar = '*';

#ifdef DEBUG
    Serial.println("MQTT Topics initialized:");
    for (int i = 0; i < NUMBER_OF_READOUTS; i++)
    {
        Serial.println(String(MQTT_ROOT_TOPIC) + "/" + telegramObjects[i].name);
    }
#endif
}

/**
   Over the Air update setup
*/
void setupOTA()
{
    ArduinoOTA
        .onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
                type = "sketch";
            else // U_SPIFFS
                type = "filesystem";

            // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
            Serial.println("Start updating " + type);
        })
        .onEnd([]() {
            Serial.println("\nEnd");
        })
        .onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        })
        .onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR)
                Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR)
                Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR)
                Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR)
                Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR)
                Serial.println("End Failed");
        });

    ArduinoOTA.begin();
}
