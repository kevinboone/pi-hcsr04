/*============================================================================
  
    main.c

    A test driver for the HCSR04 "class". This program uses the HCSR04
    class to collect ultrasonic range data from an HC-SR04 sensor. It
    sets the sensor to collect data at the maximum rate, and displays
    the smoothed value every two seconds. 

    Copyright (c)2020 Kevin Boone, GPL v3.0

============================================================================*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "defs.h" 
#include "hcsr04.h" 

#define PIN_SOUND 17
#define PIN_ECHO 27 

/*============================================================================

  main

============================================================================*/
int main (int argc, char **argv)
  {
  argc = argc; argv = argv; // Suppress warnings

  // Create the HCSR04 object with the specified pins, cycle time, and
  //  smoothing factor
  HCSR04 *hcsr04 = hcsr04_create (PIN_SOUND, PIN_ECHO, 
     4 * HCSR04_MIN_CYCLE, 0.5);
  char *error = NULL;
  if (hcsr04_init (hcsr04, &error))
    {
    while (TRUE)
      {
      if (hcsr04_is_distance_valid (hcsr04))
        printf ("%.2f\n", hcsr04_get_distance (hcsr04));
      else
        printf ("No data\n"); 
      usleep (500000);
      }
    }
  else
    {
    fprintf (stderr, "Can't set up HC-SR04: %s", error);
    free (error); 
    }
  }

