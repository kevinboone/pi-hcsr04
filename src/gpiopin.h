/*============================================================================
  
  gpiopin.h

  Functions to read, write, and detect changes on, a specific GPIO pin. 

  Copyright (c)1990-2020 Kevin Boone. Distributed under the terms of the
  GNU Public Licence, v3.0

  ==========================================================================*/
#pragma once

#include "defs.h"

struct GPIOPin;
typedef struct _GPIOPin GPIOPin;

// Pin directions -- input or output
typedef enum
  {
  GPIOPIN_IN = 0,
  GPIOPIN_OUT = 1
  } GPIOPinDirection; 

// Edge triggering settings, for use with gpiopin_set_trigger
typedef enum
  {
  GPIOPIN_NONE = 0,
  GPIOPIN_RISING = 1,
  GPIOPIN_FALLING = 2,
  GPIOPIN_BOTH = 3
  } GPIOPinTrigger; 

BEGIN_DECLS

/** Initialize the GPIOPin object with pin number. 
    Note that this method only stores values, 
    and will always succeed. */
GPIOPin  *gpiopin_create (int pin);

/** Clean up the object. This method implicitly calls _uninit(). */
void      gpiopin_destroy (GPIOPin *self);

/** Initialize the object. This opens a file handles for the
    sysfs file for the GPIO pin. Consequently, the method
    can fail. If it does, and *error is not NULL, then it is written with
    and error message that the caller should free. If this method 
    succeeds, _uninit() should be called in due course to clean up. */ 
BOOL      gpiopin_init (GPIOPin *self, GPIOPinDirection dir, char **error);

/** Clean up. In principle, this operation can fail, as it involves sysfs
    operations. But what can we do if this happens? Probably nothing, so no
    errors are reported. */
void      gpiopin_uninit (GPIOPin *self);

/** Set the edge which will trigger a priority poll. The trigger value
    is one of the GPIOPinTrigger constants. Default is "none". In order
    to use gpio_wait_for_trigger, this method needs to have been called
    to set the trigger conditions. */
void      gpiopin_set_trigger (GPIOPin *self, GPIOPinTrigger trigger);

/** Set this pin HIGH or LOW. */
void      gpiopin_set (GPIOPin *self, BOOL val);

/** Get the current state of the pin, HIGH or LOW */
BOOL      gpiopin_get (const GPIOPin *self);

/** Wait for a trigger. After the trigger, the state of the pin can 
    be read. In principle, the state will already be known, unless 
    the trigger edge is set to "none". */
void      gpiopin_wait_for_trigger (GPIOPin *self, int usec);

END_DECLS
