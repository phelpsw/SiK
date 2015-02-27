// -*- Mode: C; c-basic-offset: 8; -*-
//
// Copyright (c) 2012 Andrew Tridgell, All Rights Reserved
// Copyright (c) 2011 Michael Smith, All Rights Reserved
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//  o Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  o Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//

///
/// @file	tdm.c
///
/// time division multiplexing code
///

#include <stdarg.h>
#include "radio.h"
#include "tdm.h"
#include "timer.h"
#include "crc.h"

#define USE_TICK_YIELD 1

/// a packet buffer for the TDM code
__xdata uint8_t	pbuf[MAX_PACKET_LENGTH];

/// how many 16usec ticks are remaining in the current state
__pdata static uint16_t tdm_state_remaining;

/// This is enough to hold at least 3 packets and is based
/// on the configured air data rate.
__pdata static uint16_t tx_window_width;

/// the maximum data packet size we can fit
__pdata static uint8_t max_data_packet_length;

/// the silence period between transmit windows
/// This is calculated as the number of ticks it would take to transmit
/// two zero length packets
__pdata static uint16_t silence_period;

/// whether we can transmit in the other radios transmit window
/// due to the other radio yielding to us
static __bit bonus_transmit;

/// whether we have yielded our window to the other radio
static __bit transmit_yield;

// activity indication
// when the 16 bit timer2_tick() value wraps we check if we have received a
// packet since the last wrap (ie. every second)
// If we have the green radio LED is held on.
// Otherwise it blinks every 1 seconds. The received_packet flag
// is set for any received packet, whether it contains user data or
// not.
static __bit blink_state;
static __bit received_packet;

/// the latency in 16usec timer2 ticks for sending a zero length packet
__pdata static uint16_t packet_latency;

/// the time in 16usec ticks for sending one byte
__pdata static uint16_t ticks_per_byte;

/// number of 16usec ticks to wait for a preamble to turn into a packet
/// This is set when we get a preamble interrupt, and causes us to delay
/// sending for a maximum packet latency. This is used to make it more likely
/// that two radios that happen to be exactly in sync in their sends
/// will eventually get a packet through and get their transmit windows
/// sorted out
__pdata uint16_t transmit_wait;

/// the long term duty cycle we are aiming for
__pdata uint8_t duty_cycle;

/// the average duty cycle we have been transmitting
__data static float average_duty_cycle;

/// duty cycle offset due to temperature
__pdata uint8_t duty_cycle_offset;

/// set to true if we need to wait for our duty cycle average to drop
static bool duty_cycle_wait;

/// how many ticks we have transmitted for in this TDM round
__pdata static uint16_t transmitted_ticks;

/// the LDB (listen before talk) RSSI threshold
__pdata uint8_t lbt_rssi;

/// how long we have listened for for LBT
__pdata static uint16_t lbt_listen_time;

/// how long we have to listen for before LBT is OK
__pdata static uint16_t lbt_min_time;

/// random addition to LBT listen time (see European regs)
__pdata static uint16_t lbt_rand;

/// test data to display in the main loop. Updated when the tick
/// counter wraps, zeroed when display has happened
__pdata uint8_t test_display;

// Varibles used to hunt for a target RSSI by changing the power levels
__pdata uint8_t maxPower, presentPower, target_RSSI, powerHysteresis;
__pdata enum RSSI_Hunt_ID Hunt_RSSI;

/// buffer to hold a remote AT command before sending
static bool send_at_command;
static __pdata char remote_at_cmd[AT_CMD_MAXLEN + 1];


/// estimate the flight time for a packet given the payload size
///
/// @param packet_len		payload length in bytes
///
/// @return			flight time in 16usec ticks
static uint16_t flight_time_estimate(__pdata uint8_t packet_len)
{
	return packet_latency + (packet_len * ticks_per_byte);
}

/// called to check temperature
///
static void temperature_update(void)
{
	register int16_t diff;
	if (radio_get_transmit_power() <= 20) {
		duty_cycle_offset = 0;
		return;
	}

	diff = radio_temperature() - MAX_PA_TEMPERATURE;
	if (diff <= 0 && duty_cycle_offset > 0) {
		// under temperature
		duty_cycle_offset -= 1;
	} else if (diff > 10) {
		// getting hot!
		duty_cycle_offset += 10;
	} else if (diff > 5) {
		// well over temperature
		duty_cycle_offset += 5;
	} else if (diff > 0) {
		// slightly over temperature
		duty_cycle_offset += 1;				
	}
	// limit to minimum of 20% duty cycle to ensure link stays up OK
	if ((duty_cycle-duty_cycle_offset) < 20) {
		duty_cycle_offset = duty_cycle - 20;
	}
}

/// blink the radio LED
///
static void
link_update(void)
{
	static uint8_t temperature_count;

  if(transmit_only)
  {
    LED_RADIO = blink_state;
    LED_ACTIVITY = !blink_state;
  }
  else
  {
    LED_RADIO = LED_OFF;
    LED_ACTIVITY = !blink_state;
    if(received_packet)
    {
      received_packet = false;
      LED_RADIO = LED_ON;
    }
  }
  blink_state = !blink_state;

	temperature_count++;
	if (temperature_count == 4) {
		// check every 2 seconds
		temperature_update();
		temperature_count = 0;
	}
}

// a stack carary to detect a stack overflow
__at(0xFF) uint8_t __idata _canary;

/// main loop for time division multiplexing transparent serial
///
void
tdm_serial_loop(void)
{
	__pdata uint16_t last_t = timer2_tick();
	__pdata uint16_t last_link_update = last_t;

	_canary = 42;

	for (;;) {
		__pdata uint8_t	len, i;
		__pdata uint16_t tnow;

		if (_canary != 42) {
			panic("stack blown\n");
		}

		if (pdata_canary != 0x41) {
			panic("pdata canary changed\n");
		}
    
#if PIN_MAX > 0
    // Check to see if any pins have changed state
    pins_user_check();
#endif
    
    if(at_mode)
    {
      LED_ACTIVITY = LED_ON;
      at_mode_active = 1;
      // give the AT command processor a chance to handle a command
      at_command();
    }
    else
    {
      at_mode_active = 0;
    }

		// get the time before we check for a packet coming in
		tnow = timer2_tick();
    
    // update link status every 0.5s
    if (tnow - last_link_update > 32768) {
      link_update();
      last_link_update = tnow;
    }
    
    // If we are in recive mode lets only do this
    if(!transmit_only)
    {
      // see if we have received a packet
      if (radio_receive_packet(&len, pbuf)) {
        
        // we're not waiting for a preamble
        // any more
        transmit_wait = 0;
        if (len != sizeof(pinStatePacket)) {
          // not a valid packet.
          continue;
        }

        // update the activity indication
        received_packet = true;
        
        // extract control bytes from end of packet
        memcpy(&pinStatePacket, pbuf, sizeof(pinStatePacket));
      }
      continue;
    }

    if ((tnow - last_t) > 32768 || sendUpdateNow)
    {
      last_t = tnow;
      // start transmitting the packet
      memcpy(pbuf, &pinStatePacket, sizeof(pinStatePacket));
      
      if(sendUpdateNow)
      {
        for (i=0; i<5; i++) {
          while (radio_transmit_in_progress()) {}
          radio_transmit(sizeof(pinStatePacket), pbuf, silence_period/2);
        }
      }
      while (radio_transmit_in_progress()) {}
      radio_transmit(sizeof(pinStatePacket), pbuf, silence_period/2);
      sendUpdateNow = false;
    }
	}
}


// initialise the TDM subsystem
void
tdm_init(void)
{
	__pdata uint8_t air_rate = radio_air_rate();

#define REGULATORY_MAX_WINDOW (((1000000UL/16)*4)/10)
#define LBT_MIN_TIME_USEC 5000

	// calculate how many 16usec ticks it takes to send each byte
	ticks_per_byte = (8+(8000000UL/(air_rate*1000UL)))/16;
	ticks_per_byte++;

	// calculate the minimum packet latency in 16 usec units
	// we initially assume a preamble length of 40 bits, then
	// adjust later based on actual preamble length. This is done
	// so that if one radio has antenna diversity and the other
	// doesn't, then they will both using the same TDM round timings
	packet_latency = (8+(10/2)) * ticks_per_byte + 13;

	// now adjust the packet_latency for the actual preamble
	// length, so we get the right flight time estimates, while
	// not changing the round timings
	packet_latency += ((settings.preamble_length-10)/2) * ticks_per_byte;

#ifdef TDM_SYNC_LOGIC
	TDM_SYNC_PIN = false;
#endif // TDM_SYNC_LOGIC
}

