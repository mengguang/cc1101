#include <Arduino.h>
#include <SPI.h>
#include "cc1101.h"
#include "ccpacket.h"

#define CC1101_GDO0 20
#define CC1101Interrupt digitalPinToInterrupt(CC1101_GDO0)

#define CC1101_CS_PIN 5
#define CC1101_SCK_PIN 2
#define CC1101_MOSI_PIN 3
#define CC1101_MISO_PIN 4

CC1101 radio(CC1101_CS_PIN, CC1101_GDO0, CC1101_MISO_PIN);

byte syncWord[2] = {0x47, 0xB5};
bool packetWaiting = false;

unsigned long lastSend = 0;
unsigned int sendDelay = 500;

void messageReceived()
{
    packetWaiting = true;
}

void setup()
{
    // init SPI for Pico
    SPI = arduino::MbedSPI(CC1101_MISO_PIN, CC1101_MOSI_PIN, CC1101_SCK_PIN);
    SPI.begin();

    // init SPI for ESP32
    // SPI.begin(CC1101_SCK_PIN, CC1101_MISO_PIN, CC1101_MOSI_PIN);
    // SPI.setFrequency(4000000);

    radio.init();
    radio.setSyncWord(syncWord);
    radio.setCarrierFreq(CFREQ_433);
    radio.setChannel(66);
    radio.disableAddressCheck();
    radio.setTxPowerAmp(PA_P10DB);
    Serial.begin(115200);
    Serial.print(F("CC1101_PARTNUM "));
    Serial.println(radio.readReg(CC1101_PARTNUM, CC1101_STATUS_REGISTER));
    Serial.print(F("CC1101_VERSION "));
    Serial.println(radio.readReg(CC1101_VERSION, CC1101_STATUS_REGISTER));
    Serial.print(F("CC1101_MARCSTATE "));
    Serial.println(radio.readReg(CC1101_MARCSTATE, CC1101_STATUS_REGISTER) & 0x1f);

    Serial.println(F("CC1101 radio initialized."));
    attachInterrupt(CC1101Interrupt, messageReceived, FALLING);
}

// Get signal strength indicator in dBm.
// See: http://www.ti.com/lit/an/swra114d/swra114d.pdf
int rssi(char raw)
{
    uint8_t rssi_dec;
    // TODO: This rssi_offset is dependent on baud and MHz; this is for 38.4kbps and 433 MHz.
    uint8_t rssi_offset = 74;
    rssi_dec = (uint8_t)raw;
    if (rssi_dec >= 128)
        return ((int)(rssi_dec - 256) / 2) - rssi_offset;
    else
        return (rssi_dec / 2) - rssi_offset;
}

// Get link quality indicator.
int lqi(char raw)
{
    return 0x3F - raw;
}

char message[64];
uint32_t n = 0;

void loop()
{
    if (packetWaiting)
    {
        detachInterrupt(CC1101Interrupt);
        packetWaiting = false;
        CCPACKET packet;
        if (radio.receiveData(&packet) > 0)
        {
            Serial.println(F("Received packet..."));
            if (!packet.crc_ok)
            {
                Serial.println(F("crc not ok"));
            }
            Serial.print(F("lqi: "));
            Serial.println(lqi(packet.lqi));
            Serial.print(F("rssi: "));
            Serial.print(rssi(packet.rssi));
            Serial.println(F("dBm"));

            if (packet.crc_ok && packet.length > 0)
            {
                Serial.print(F("packet: len "));
                Serial.println(packet.length);
                Serial.println(F("data: "));
                Serial.println((const char *)packet.data);
            }
        }

        attachInterrupt(CC1101Interrupt, messageReceived, FALLING);
    }
    unsigned long now = millis();
    if (now > lastSend + sendDelay)
    {
        detachInterrupt(CC1101Interrupt);

        lastSend = now;
        // const char *message = "hello world";
        snprintf(message, sizeof(message), "Hello %lu", n++);
        CCPACKET packet;
        // We also need to include the 0 byte at the end of the string
        packet.length = strlen(message) + 1;
        strncpy((char *)packet.data, message, packet.length);

        radio.sendData(packet);
        Serial.println(F("Sent packet..."));

        attachInterrupt(CC1101Interrupt, messageReceived, FALLING);
    }
}
