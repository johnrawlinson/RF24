/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.


 06/04/2021 : Brendan Doherty (https://github.com/2bndy5)
              Modified to use with PicoSDK

 */

/**
 * Channel scanner
 *
 * Example to detect interference on the various channels available.
 * This is a good diagnostic tool to check whether you're picking a
 * good channel for your application.
 *
 * Inspired by cpixip.
 * See https://forum.arduino.cc/t/poor-mans-2-4-ghz-scanner/54846
 */

/*
 * How to read the output:
 * - The header is a list of supported channels in decimal written vertically.
 * - Each column corresponding to the vertical header is a hexadecimal count of
 *   detected signals (max is 15 or 'f').
 *
 * The following example
 *    000
 *    111
 *    789
 *    ~~~   <- just a divider between the channel's vertical labels and signal counts
 *    1-2
 * can be interpreted as
 * - 1 signal detected on channel 17
 * - 0 signals (denoted as '-') detected on channel 18
 * - 2 signals detected on channel 19
 *
 * Each line of signal counts represent 100 passes of the supported spectrum.
 */

#include "pico/stdlib.h"  // printf(), sleep_ms(), getchar_timeout_us(), to_us_since_boot(), get_absolute_time()
#include "pico/bootrom.h" // reset_usb_boot()
//#include <tusb.h>         // tud_cdc_connected()
#include <RF24.h>         // RF24 radio object, rf24_min()
#include "defaultPins.h"  // board presumptive default pin numbers for CE_PIN and CSN_PIN

// instantiate an object for the nRF24L01 transceiver
RF24 radio(CE_PIN, CSN_PIN);

// Channel info
const uint8_t num_channels = 126; // 0-125 are supported
uint8_t values[num_channels];     // the array to store summary of signal counts per channel

// To detect noise, we'll use the worst addresses possible (a reverse engineering tactic).
// These addresses are designed to confuse the radio into thinking
// that the RF signal's preamble is part of the packet/payload.
const uint8_t noiseAddress[][6] = {{0x55, 0x55}, {0xAA, 0xAA}, {0x0A, 0xAA}, {0xA0, 0xAA}, {0x00, 0xAA}, {0xAB, 0xAA}};

const int num_reps = 126; // number of passes for each scan of the entire spectrum

void printHeader();
void scanChannel(uint8_t);
void initRadio();
bool SetNextAddress();

int main()
{
    stdio_init_all(); // init necessary IO for the RP2040
    
    // wait here until the CDC ACM (serial port emulation) is connected
//    while (!tud_cdc_connected()) {
//        sleep_ms(10);
//    }

    // print example's name
    printf("RF24/examples_pico/scanner\n");

    // print a line that should not be wrapped
    printf("\n!!! This example requires a width of at least 126 characters. ");
    printf("If this text uses multiple lines, then the output will look bad.\n");

    // initialize the transceiver on the SPI bus
    while (!radio.begin()) {
        printf("radio hardware is not responding!!\n");
    }
    initRadio();

    // Print out header
    printHeader();

    // forever loop
    while (1) {
        // Clear measurement values
        memset(values, 0, sizeof(values));

        // Scan all channels num_reps times
        int rep_counter = num_reps;
        while (rep_counter--) {

            for (uint8_t i = 0; i < num_channels; ++i) 
            {
                printf("Scanning channel %d\n\r", i);
                // Select this channel
                bool last_address_reached = false;                
                while (!last_address_reached)
                {
                    last_address_reached = SetNextAddress();
                    scanChannel(i); // updates values[i] accordingly
                }
            }
        }
        for (uint8_t i = 0; i < num_channels; ++i) {
            // Print out channel measurements, clamped to a single hex digit      
            if (values[i])
                printf("%x", rf24_min(0xf, values[i]));
            else
                printf("-");
        }
        printf("\r\n");
        printf("\n");

        char input = getchar_timeout_us(0); // get char from buffer for user input
        if (input != PICO_ERROR_TIMEOUT) {
            if (input == 'b' || input == 'B') {
                // reset to bootloader
                radio.powerDown();
                reset_usb_boot(0, 0);
            }
        }
    }

    return 0;
}

static uint8_t addresses [6][2];
static uint8_t current_address[2];
bool IncrementAddress()
{
    current_address[1]++;
    if (current_address[1] > 'Z')
    {
        current_address[1] = '0';
        current_address[0]++;
        if (current_address[0] > 'Z')
        {
            current_address[0] = '0';
            return true;
        }
    }
    return false;
}

bool SetNextAddress()
{
    bool reached_last_address = false;
    for (uint8_t i = 0; i < 6; ++i) 
    {
        if (reached_last_address)
        {
            IncrementAddress();
        } 
        else
        {
            reached_last_address = IncrementAddress();
        }
        addresses[i][0] = current_address[0];
        addresses[i][1] = current_address[1];
        radio.closeReadingPipe(i);
        radio.openReadingPipe(i, addresses[i]);
    }
    return reached_last_address;
}

void initRadio()
{
    current_address[0] = '0';
    current_address[1] = '0';
    for (int i = 0; i++; i < 6)
    {
        addresses[i][0] = current_address[0];
        addresses[i][1] = current_address[1];
        IncrementAddress();
    }
    // configure the radio
    radio.setAutoAck(false);  // Don't acknowledge arbitrary signals
    radio.disableCRC();       // Accept any signal we find
    radio.setAddressWidth(2); // A reverse engineering tactic (not typically recommended)
    for (uint8_t i = 0; i < 6; ++i) {
        radio.openReadingPipe(i, addresses[i]);
    }

    // To set the radioNumber via the Serial terminal on startup
    printf("\nSelect your data rate. ");
    printf("Enter '1' for 1 Mbps, '2' for 2 Mbps, or '3' for 250 kbps. ");
    printf("Defaults to 1 Mbps.\n");
    char input = getchar();
    if (input == 50) {
        printf("\nUsing 2 Mbps.\n");
        radio.setDataRate(RF24_2MBPS);
    }
    else if (input == 51) {
        printf("\nUsing 250 kbps.\n");
        radio.setDataRate(RF24_250KBPS);
    }
    else {
        printf("\nUsing 1 Mbps.\n");
        radio.setDataRate(RF24_1MBPS);
    }

    // Get into standby mode
    radio.startListening();
    radio.stopListening();
    radio.flush_rx();
    // radio.printPrettyDetails();
}

bool IsAscii(uint8_t character)
{
    if (character >= '0' && character <= 'Z')
    {
        return true;
    }
    else
    {
        return false;
    }
}

void scanChannel(uint8_t channel)
{
    radio.setChannel(channel);

    // Listen for a little
    radio.startListening();
    sleep_us(500000);
    bool foundSignal = radio.testRPD();
    radio.stopListening();

    // Did we get a carrier?
    if (foundSignal || radio.testRPD() || radio.available()) {
        uint8_t pipe_num;
        while (radio.available(&pipe_num))
        {
            uint8_t rx_buff[32];

            int nbytes = radio.read_register(0x11+pipe_num);

            radio.read(rx_buff, nbytes);
            if (IsAscii(rx_buff[0] && IsAscii(rx_buff[0])))
            {
                printf("Channel %d, Pipe %d, %d bytes, Address %c %c  Data: ", channel, pipe_num, nbytes, 
                    addresses[pipe_num][0], addresses[pipe_num][1]);
                for (int i = 0; i < sizeof(rx_buff); i++)
                {
                    if (rx_buff[i] >= '0' && rx_buff[i] <= 'Z')
                    {
                        printf("%2c ", rx_buff[i]);
                    }
                    else
                    {
                        printf("%2x ", rx_buff[i]);
                        //printf(" - ");
                    }

                }
                printf("\n\r");    
            }    
        }
        ++values[channel];
        radio.flush_rx();
    }
}

void printHeader()
{
    // print the hundreds digits
    for (uint8_t i = 0; i < num_channels; ++i)
        printf("%d", (i / 100));
    printf("\n");

    // print the tens digits
    for (uint8_t i = 0; i < num_channels; ++i)
        printf("%d", ((i % 100) / 10));
    printf("\n");

    // print the singles digits
    for (uint8_t i = 0; i < num_channels; ++i)
        printf("%d", (i % 10));
    printf("\n");

    // print the header's divider
    for (uint8_t i = 0; i < num_channels; ++i)
        printf("~");
    printf("\n");
}
