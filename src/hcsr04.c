/*==========================================================================
  
    hcsr04.c

    This "class" runs a data collection thread that collects distance
    measurements by timing pulses from the HC-SR04 ultrasonic sensor.
    Data is subject to a configurable running average to reduce
    noise.

    Copyright (c)2020 Kevin Boone, GPL v3.0

============================================================================*/
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>
#include "defs.h" 
#include "gpiopin.h" 
#include "hcsr04.h" 

// How far sound travels in a microsecond, at standard temperature and
//  pressure. 343 / 1E6 / 2. The division by two is to account for the 
//  fact that the sound pulse has to travel from the transucer and back.
// In principle we should correct for temperature and pressure but,
//  to be frank, the whole measurement process on a Pi is not stable enough
//  to worry about such small error sources.
#define USEC_TO_METRES 0.0001715

// HCSR04 structure -- stores all internal data related to this
//  HCSR04 instance
struct _HCSR04
  {
  // GPIO pins connected to the transducer
  int sound_pin; 
  int echo_pin;
  // Longest response time for which a response time reading is considered
  //  valid. We work this out at initialization time from the maximum
  //  valid range. 
  int max_time;
  pthread_t pthread; // Reference to the running thread
  BOOL stop; // Set when hcsr04_stop() is called, to stop the HCSR04 thead
  GPIOPin *gpiopin_sound; // Object referring to the sound pin
  GPIOPin *gpiopin_echo;  // Object referring to the echo pin
  int cycle_usec;    // Number of microseconds between measurement cycles.
  double avg;        // The current smoothed distance value
  double smoothing;  // Smoothing factor (0-1);
  // good_count is the number of valid measurements made, constrained
  //  between 0 and HCS04_VALID_SAMPLES. Every valid measurement
  //  increases this count, and every invalid measurement decreases it.
  int good_count;    
  };

/*============================================================================

  get_system_time_usec

============================================================================*/
static long get_system_time_usec (void)
  {
  struct timeval tv;
  gettimeofday (&tv, NULL);
  return tv.tv_usec  + tv.tv_sec * 1000000;
  }

/*============================================================================

  hcsr04_create

============================================================================*/
HCSR04 *hcsr04_create (int sound_pin, int echo_pin, int cycle_msec, 
    double smoothing)
  {
  HCSR04 *self = malloc (sizeof (HCSR04));
  memset (self, 0, sizeof (HCSR04));
  self->sound_pin = sound_pin;
  self->echo_pin = echo_pin;
  self->gpiopin_sound = gpiopin_create (self->sound_pin);
  self->gpiopin_echo = gpiopin_create (self->echo_pin);
  self->cycle_usec = cycle_msec * 1000;
  self->max_time = (int) (HCSR04_MAX_RANGE / USEC_TO_METRES); 
  self->smoothing = smoothing; 
  return self;
  }

/*============================================================================
  hcsr04_destroy
============================================================================*/
void hcsr04_destroy (HCSR04 *self)
  {
  if (self)
    {
    hcsr04_uninit (self);
    gpiopin_destroy (self->gpiopin_sound);
    gpiopin_destroy (self->gpiopin_echo);
    free (self);
    }
  }

/*============================================================================

  hcsr04_loop

  This is the main measurement loop, that runs in its own thread.

============================================================================*/
void *hcsr04_loop (void *arg)
  {
  HCSR04 *self = (HCSR04 *)arg;
  while (!self->stop)
    {
    double d = hcsr04_read_one (self);
    if (d > 0)
      {
      self->avg = d * (1 - self->smoothing) + self->avg * (self->smoothing); 
      self->good_count++;
      if (self->good_count > HCSR04_VALID_SAMPLES) 
         self->good_count = HCSR04_VALID_SAMPLES;
      }
    else
      {
      self->good_count--;
      if (self->good_count < 0) self->good_count = 0;
      }
    usleep (self->cycle_usec);
    }
  return NULL;
  }

/*============================================================================

  hcsr04_init

============================================================================*/
BOOL hcsr04_init (HCSR04 *self, char **error)
  {
  assert (self != NULL);
  self->avg = 0.0;
  self->good_count = 0;
  BOOL ret = FALSE;
  if (gpiopin_init (self->gpiopin_echo, GPIOPIN_IN, error))
    {
    // If we can initialize one GPIO pin, we'll assume that others
    //  initialize OK as well.
    gpiopin_init (self->gpiopin_sound, GPIOPIN_OUT, NULL);

    // Start off with the "sound" pin low. It will be pulsed high during
    //  each measurement cycle.
    gpiopin_set (self->gpiopin_sound, LOW);

    pthread_t pt;
    pthread_create (&pt, NULL, hcsr04_loop, self);
    pthread_detach (pt); // This doesn't seem to prevent the small mem leak,
                       //  perhaps because the application closes before the
                       //  loop has time to complete nicely.
    self->pthread = pt;

    ret = TRUE;
    }
  return ret;
  }

/*============================================================================
  hcsr04_uninit
============================================================================*/
void hcsr04_uninit (HCSR04 *self)
  {
  assert (self != NULL);
  self->stop = TRUE;
  gpiopin_uninit (self->gpiopin_sound);
  gpiopin_uninit (self->gpiopin_echo);
  }

/*============================================================================
  hcsr04_read_one
============================================================================*/
double hcsr04_read_one (HCSR04 *self)
  {
  // Pulse the sound pin high. This should be for 10usec, but the Pi
  //  can't time with that precision. Longer doesn't seem to be a 
  //  problem.
  gpiopin_set (self->gpiopin_sound, HIGH);
  usleep (100);
  gpiopin_set (self->gpiopin_sound, LOW);

  // Set the echo pin to trigger on the rising edge, and wait for
  //  the edge
  gpiopin_set_trigger (self->gpiopin_echo, GPIOPIN_RISING);
  gpiopin_wait_for_trigger (self->gpiopin_echo, 500000);

  // Start the timer, and the start of the rising edge
  long start = get_system_time_usec();

  // Now wait for the falling edige
  gpiopin_set_trigger (self->gpiopin_echo, GPIOPIN_FALLING);
  gpiopin_wait_for_trigger (self->gpiopin_echo, 500000);

  long end = get_system_time_usec();

  //printf ("trigger %ld %g\n", end - start, (end - start) * 343.0 / 1e6 / 2.0);
 
  // If the response time is within limits, return the calculated
  //  distance. 
  int diff = (int) (end - start);
  if (diff > self->max_time)
    return -1.0;
  else
    return USEC_TO_METRES * (end - start);
  }

/*============================================================================
  hcsr04_is_distance_valid
============================================================================*/
BOOL hcsr04_is_distance_valid (const HCSR04 *self)
  {
  return self->good_count >= HCSR04_VALID_SAMPLES; 
  }

/*============================================================================
  hcsr04_get_distance
============================================================================*/
double hcsr04_get_distance (const HCSR04 *self)
  {
  if (hcsr04_is_distance_valid (self))
    return self->avg; 
  else
    return -1.0;
  }

