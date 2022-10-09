#include <Arduino.h>
#include <HardwareSerial.h>
#include <HADevice.h>
#include <HAMqtt.h>
#include <ArduinoOTA.h>
#include <wenfilter.h>
#include <ESP8266mDNS.h>
#include "HAFan.h"

const char * mDNSname= "wenfiltercontroller";
const char * deviceId = "wenfilter-controller-1";
const char * entityPrefix = "wfc1";
const char * ssid = "WaitingOnComcast";
const char * pwd = "1594N2640W";
const char * mqtt_host = "tiltpi.equationoftime.tech";
const char * graylog_host = "dockervm";

const int WDT_TIMEOUT = 5;

WiFiClient client;
HADevice device(deviceId);
HAMqtt mqtt(client, device);

const int transmitter433_pin = 5;
WenFilter wenAirFilterDevice (transmitter433_pin);
HAFan * wenAirFilter = new HAFan("wec1_wen_filter", HAFan::SpeedsFeature);

void setup(){

    Serial.begin(115200);

    while (!Serial) { // needed to keep leonardo/micro from starting too fast!
        delay(10);
    }
    WiFi.setAutoReconnect(true);

    //TODO: A webportal to configure wifi stuff would be awesome.
    //TODO: Save logs to internal memory for recovery
    do{
        Serial.println("Attempting Wifi connection");
        WiFi.begin(ssid, pwd);
    } while (WiFi.waitForConnectResult() != WL_CONNECTED);

//    auto address = MDNS.queryHost(graylog_host);
//    if (((uint32_t)address) == 0)
//    {
//        Serial.printf("Query for %s failed\n", graylog_host);
//    }
//    else
//    {
//    }

    device.enableSharedAvailability();
    device.setAvailability(true);

    device.setName("Workshop Controller");
    device.setSoftwareVersion("1.0.0");
    device.enableLastWill();

    //configure air filter radio
    wenAirFilter->setAvailability(true);
    wenAirFilter->setName("WEN Air Filter");
    //I tried to set this to 0, because a 0 speed makes sense to be "off". But the underlying calculation uses a log10() which returns -1... causing off by one errors in the json serialization... causing frequent diconnects from the mqtt server
    wenAirFilter->setSpeedRangeMin(1);
    wenAirFilter->setSpeedRangeMax(3);
    wenAirFilter->onSpeedChanged([](uint16_t speed) {
        Serial.write("speed: ");
        Serial.println(speed);
       switch(speed){
           default:
               wenAirFilter->setState(false);
               wenAirFilterDevice.setOffClear();
               break;
           case 1:
               wenAirFilter->setState(true);
               wenAirFilterDevice.setOnState(WenFilterSpeed::Low, WenFilterTime::None);
               break;
           case 2:
               wenAirFilter->setState(true);
               wenAirFilterDevice.setOnState(WenFilterSpeed::Medium, WenFilterTime::None);
               break;
           case 3:
               wenAirFilter->setState(true);
               wenAirFilterDevice.setOnState(WenFilterSpeed::High, WenFilterTime::None);
               break;
       }
    });

    wenAirFilter->onStateChanged([](bool state) {
        Serial.write("state: ");
        Serial.println(state);
        if (state) {
            if (wenAirFilter->getSpeed() == 0)
               wenAirFilter->setSpeed(1);
        } else {
            if (wenAirFilter->getSpeed() > 0)
                wenAirFilter->setSpeed(0);
        }
    });

    wenAirFilterDevice.begin();

    mqtt.begin(mqtt_host,1883);

    while (!mqtt.isConnected())
    {
        mqtt.loop();
    }

    MDNS.begin(mDNSname);
}

void loop(){

    mqtt.loop();
    ArduinoOTA.handle();

}

