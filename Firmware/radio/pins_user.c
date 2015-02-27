// -*- Mode: C; c-basic-offset: 8; -*-
//
// Copyright (c) 2013 Luke Hovington, All Rights Reserved
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
/// @file	pins_user.c
///
/// AT Controlled Pins
///

#include "pins_user.h"
#include "radio.h"
#include "tdm.h"

#if PIN_MAX > 0

// Pin location in the array
#define AT_DISABLE_PIN 4
#define AT_V_GND_PIN   5
#define FOOTER_CHECK 0xA5

// pin_values defined as extern in parameters
__code const struct pins_user_map {
	uint8_t port;
	uint8_t	pin;
} pins_user_map[] = {
// Pin rfd900a  Mapping
#ifdef BOARD_rfd900a
	{2, 3}, // 0 - P2.3
	{2, 2}, // 1 - P2.2
	{2, 1}, // 2 - P2.1
	{2, 0}, // 3 - P2.0
	{2, 6}, // 4 - P2.6
	{0, 1}, // 5 - P0.1
};
#elif defined BOARD_rfd900u
	{1, 0}, // 0 - P1.0
	{1, 1}, // 1 - P1.1
};
#elif defined BOARD_rfd900p
	{3, 4}, // 0 - P3.4
	{3, 3}, // 1 - P3.3
	{1, 2}, // 2 - P1.2
	{1, 3}, // 3 - P1.3
	{1, 1}, // 4 - P1.1
	{1, 0}, // 5 - P1.0
};
#else
	{0xff, 0xff} // Default pin mapping (not supported)
};
#endif

__pdata struct pinState_packet pinStatePacket;
bool at_mode, sendUpdateNow;

// At max we can send 4 pin states
#define PIN_SEND_MAX (4 < PIN_MAX ? 4 : PIN_MAX)

void
pins_user_init(void)
{
	__pdata uint8_t i;
	
  pinStatePacket.no_pins = PIN_SEND_MAX;
  pinStatePacket.footer = FOOTER_CHECK;
	// Set the Default pin behaviour
  if(transmit_only)
  {
    for(i=0; i<PIN_MAX; i++)
    {
      pins_user_set_io(i, PIN_INPUT);
      pins_user_set_value(i, PIN_HIGH);
    }
  }
  else
  {
    for(i=0; i<PIN_MAX; i++)
    {
      pins_user_set_io(i, PIN_OUTPUT);
      pins_user_set_value(i, PIN_LOW);
    }
  }
  
  if(AT_V_GND_PIN < PINS_USER_MAX)
  {
    pins_user_set_io(AT_DISABLE_PIN, PIN_INPUT);
    pins_user_set_value(AT_DISABLE_PIN, PIN_HIGH);
    
    pins_user_set_io(AT_V_GND_PIN, PIN_OUTPUT);
    pins_user_set_value(AT_V_GND_PIN, PIN_LOW);
  }
}

bool
pins_user_set_io(__pdata uint8_t pin, bool in_out)
{
	if (PINS_USER_MAX > pin)
	{
		pin_values[pin].output = in_out;
		pin_values[pin].pin_mirror = PIN_NULL;
		
		// Esure we are on the Legacy page (SFR Page 0x0)
		SFRPAGE	= LEGACY_PAGE;
		
		switch(pins_user_map[pin].port)
		{
			case 0:
				if(in_out)
					P0MDOUT |= (1<<pins_user_map[pin].pin);
				else
					P0MDOUT &= ~(1<<pins_user_map[pin].pin);
				SFRPAGE	= CONFIG_PAGE;
				if(in_out)
					P0DRV |= (1<<pins_user_map[pin].pin);
				else
					P0DRV &= ~(1<<pins_user_map[pin].pin);
				break;
				
			case 1:
				if(in_out)
					P1MDOUT |= (1<<pins_user_map[pin].pin);
				else
					P1MDOUT &= ~(1<<pins_user_map[pin].pin);
				SFRPAGE	= CONFIG_PAGE;
				if(in_out)
					P1DRV |= (1<<pins_user_map[pin].pin);
				else
					P1DRV &= ~(1<<pins_user_map[pin].pin);
				break;
				
			case 2:
				if(in_out)
					P2MDOUT |= (1<<pins_user_map[pin].pin);
				else
					P2MDOUT &= ~(1<<pins_user_map[pin].pin);
				SFRPAGE	= CONFIG_PAGE;
				if(in_out)
					P2DRV |= (1<<pins_user_map[pin].pin);
				else
					P2DRV &= ~(1<<pins_user_map[pin].pin);
				break;
#ifdef CPU_SI1030
      case 3:
        if(in_out)
          P3MDOUT |= (1<<pins_user_map[pin].pin);
        else
          P3MDOUT &= ~(1<<pins_user_map[pin].pin);
        SFRPAGE	= CONFIG_PAGE;
        if(in_out)
          P3DRV |= (1<<pins_user_map[pin].pin);
        else
          P3DRV &= ~(1<<pins_user_map[pin].pin);
        break;
#endif // CPU_SI1030
			default:
				SFRPAGE	= LEGACY_PAGE;
				return false;
		}
		SFRPAGE	= LEGACY_PAGE;
		return true;
	}
	return false;
}

bool
pins_user_get_io(__pdata uint8_t pin)
{
	return pin_values[pin].output;
}

bool
pins_user_set_value(__pdata uint8_t pin, bool high_low)
{
	pin_values[pin].pin_dir = high_low;
	if(PINS_USER_MAX > pin && pin_values[pin].pin_mirror == PIN_NULL)
	{
		switch(pins_user_map[pin].port)
		{
			case 0:
				if(high_low)
				{
					P0 |= (1<<pins_user_map[pin].pin);
				}
				else
				{
					P0 &= ~(1<<pins_user_map[pin].pin);
				}
				break;
			
			case 1:
				if(high_low)
				{
					P1 |= (1<<pins_user_map[pin].pin);
				}
				else
				{
					P1 &= ~(1<<pins_user_map[pin].pin);
				}
				break;
				
			case 2:
				if(high_low)
				{
					P2 |= (1<<pins_user_map[pin].pin);
				}
				else
				{
					P2 &= ~(1<<pins_user_map[pin].pin);
				}
				break;
#ifdef CPU_SI1030
      case 3:
        if(high_low)
        {
          P3 |= (1<<pins_user_map[pin].pin);
        }
        else
        {
          P3 &= ~(1<<pins_user_map[pin].pin);
        }
        break;
#endif // CPU_SI1030
			default:
				return false;
		}
		return true;
	}
	return false;
}

bool
pins_user_get_value(__pdata uint8_t pin)
{
	return pin_values[pin].pin_dir;
}

uint8_t
pins_user_get_adc(__pdata uint8_t pin)
{
	if(PINS_USER_MAX > pin && pin_values[pin].output == PIN_INPUT)
	{
		switch(pins_user_map[pin].port)
		{
			case 0:
				return P0 & (1<<pins_user_map[pin].pin);
			case 1:
				return P1 & (1<<pins_user_map[pin].pin);
			case 2:
				return P2 & (1<<pins_user_map[pin].pin);
#ifdef CPU_SI1030
      case 3:
        return P3 & (1<<pins_user_map[pin].pin);
#endif // CPU_SI1030
      default:
				return PIN_ERROR;
		}
	}
	return PIN_ERROR;
}

#define PIN_AB_MAX ((uint8_t) pinStatePacket.no_pins < PIN_SEND_MAX ? (uint8_t) pinStatePacket.no_pins : PIN_SEND_MAX)

void
pins_user_check()
{
  __pdata uint8_t i;
  static __pdata struct pinState_packet pinChange;
  
  // Update pin states
  if(transmit_only)
  {
    pinChange.pin_state = pinStatePacket.pin_state;
    pinStatePacket.pin_state = 0;

    for(i=0;i<PIN_SEND_MAX;i++)
    {
      pinStatePacket.pin_state |= (pins_user_get_adc(i)?1:0) << i;
    }
    if (pinChange.pin_state != pinStatePacket.pin_state) {
      sendUpdateNow = true;
    }
  }
  // Set pin state if we have a valid packet
  else if (FOOTER_CHECK == pinStatePacket.footer && pinChange.pin_state != pinStatePacket.pin_state)
  {
    pinChange.pin_state = pinStatePacket.pin_state;
    
    for(i=0;i<PIN_AB_MAX;i++)
    {
//      // PinMax is 4 so these pins arn't touched
//      if(AT_DISABLE_PIN == i || AT_V_GND_PIN == i)
//      {
//        continue;
//      }
      pins_user_set_value(i, (pinStatePacket.pin_state & (1 << i))?1:0);
    }
  }
  
  // check the AT_DISABLE_PIN
  if(AT_DISABLE_PIN < PINS_USER_MAX)
  {
    at_mode = pins_user_get_adc(AT_DISABLE_PIN)?1:0;
  }
}

#endif // PIN_MAX > 0
