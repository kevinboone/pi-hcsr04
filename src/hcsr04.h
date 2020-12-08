/*============================================================================
  
  hcsr04.h

  Functions to manage the HC-SR04 ultrasonic ranging device.

  Copyright (c)1990-2020 Kevin Boone. Distributed under the terms of the
  GNU Public Licence, v3.0

  ==========================================================================*/
#pragma once

// Shortest measurement time in msec -- the manufacturer recommends 60 msec
#define HCSR04_MIN_CYCLE 60

// The maximum range of the device, in metres. The usual claim is 4m. Any
//  measurement outside this range is assumed to result from a timeout,
//  and should be discarded.
#define HCSR04_MAX_RANGE 4.0

// The number of good (i.e., not time-out) distance readings that must be
//  made, before the stored result is considered valid. Because we maintain
//  a running average, we can lose a few samples without completely 
//  abandoning the stored value. Each lost sample causes the count of
//  good samples to decrement. When it reaches zero, the data is considered
//  to be invalid.
#define HCSR04_VALID_SAMPLES 4

struct HCSR04;
typedef struct _HCSR04 HCSR04;

BEGIN_DECLS

/** Create a HCSR04 instance. This method only initializes and allocates 
    memory, so it will always succed. Call hcsr04_destroy() when finished.
    This constructor takes two arguments -- the GPIO pin connected to
    the SR04's sound (trigger) pin. Cycle msec is the time between 
    successive distance measurement cycles in milliseconds. This value
    should not be smaller than HCSR04_MIN_CYCLE.  smoothing is a number 
    in the range 0.0-0.9999. It determines how much a particular value
    supplied by users is dependent on the running average if readings.
    When this value is 0, there is no smoothing -- values can change as
    fast as the cycle time allows. However, there will be considerable
    noise. At the other extreme, a value of 0.9 means that it could
    take hundreds of cycles for the value to converge to the latest
    measurement. A value of 0.5 is probably a good starting point. */
HCSR04     *hcsr04_create (int sound_pin, int echo_pin, int cycle_msec,
        double smoothing);

/** Tidy up this HCSR04 instance. Implicitly calls hcsr04_stop(). */
void     hcsr04_destroy (HCSR04 *hcsr04);

/** Initialize the GPIO and start the HCSR04 thread. This method can fail,
    because it accesses hardware. If it does, it fills in *error. Caller
    must free *error if it is set. The caller must set the HCSR04 cycle
    length, in microseconds. */
BOOL     hcsr04_init (HCSR04 *self, char **error);

/** Stop the HCSR04 thread, and uninitialze the GPIO. */
void     hcsr04_uninit (HCSR04 *self);

/** Carry out a single cycle of the distance measurement, no filtering, not
    error checking, just the raw value from the hardware. The return
    value will be a number between 0.0 and the maximum set distance. 
    A negative return indicates that no data was read, usually meaning that
    the measurement timed out. */
double hcsr04_read_one (HCSR04 *self);

/** The distance is considered value if there have been more than
    HCSRO4_VALID_SAMPLES good measurements in a row. */
BOOL hcsr04_is_distance_valid (const HCSR04 *self);

/** Get the current distance, or a negative number if there is no valid
    data. This method returns the stored moving average, and does not
    cause any measurement to take place. */
double hcsr04_get_distance (const HCSR04 *self);

END_DECLS

