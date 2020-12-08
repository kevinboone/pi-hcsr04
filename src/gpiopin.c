/*==========================================================================
  
    gpiopin.c

    A "class" for setting values of specific GPIO pins.

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
#include <fcntl.h>
#include <poll.h>
#include "defs.h" 
#include "gpiopin.h" 

struct _GPIOPin
  {
  int pin; 
  int value_fd;
  };

/*============================================================================
  gpiopin_create
============================================================================*/
GPIOPin *gpiopin_create (int pin)
  {
  GPIOPin *self = malloc (sizeof (GPIOPin));
  memset (self, 0, sizeof (GPIOPin));
  self->pin = pin;
  self->value_fd = -1;
  return self;
  }

/*============================================================================
  gpiopin_write_to_file
============================================================================*/
static BOOL gpiopin_write_to_file (const char *filename, 
    const char *text, char **error)
  {
  assert (filename != NULL);
  assert (text != NULL);
  BOOL ret = FALSE;
  FILE *f = fopen (filename, "w");
  if (f)
    {
    fprintf (f, text);
    fclose (f);
    ret = TRUE;
    }
  else
    {
    if (error)
      asprintf (error, "Can't open %s for writing: %s", filename, 
        strerror (errno));
    ret = FALSE;
    }
  return ret;
  }


/*============================================================================
  gpiopin_destroy
============================================================================*/
void gpiopin_destroy (GPIOPin *self)
  {
  if (self)
    {
    gpiopin_uninit (self);
    free (self);
    }
  }

/*============================================================================
  gpiopin_init
============================================================================*/
BOOL gpiopin_init (GPIOPin *self, GPIOPinDirection dir, char **error)
  {
  assert (self != NULL);
  char s[50];
  snprintf (s, sizeof(s), "%d", self->pin);
  BOOL ret = gpiopin_write_to_file ("/sys/class/gpio/export", s, error);
  if (ret)
    {
    char s[50];
    snprintf (s, sizeof(s), "/sys/class/gpio/gpio%d/direction", self->pin);
    if (dir == GPIOPIN_OUT)
      gpiopin_write_to_file (s, "out", NULL); 
    else
      gpiopin_write_to_file (s, "in", NULL); 
    snprintf (s, sizeof(s), "/sys/class/gpio/gpio%d/value", self->pin);
    if (dir == GPIOPIN_OUT)
      self->value_fd = open (s, O_RDWR);
    else
      self->value_fd = open (s, O_RDONLY | O_NONBLOCK);
      
    if (self->value_fd >= 0) 
      {
      ret = TRUE;
      }
    else
      {
      if (error)
        asprintf (error, "Can't open %s for writing: %s", s, strerror (errno));
      ret = FALSE;
      }
    }
  return ret;
  }

/*============================================================================
  gpiopin_uninit
============================================================================*/
void gpiopin_uninit (GPIOPin *self)
  {
  assert (self != NULL);
  if (self->value_fd >= 0)
    close (self->value_fd);
  self->value_fd = -1;
  char s[50];
  snprintf (s, sizeof(s), "%d", self->pin);
  gpiopin_write_to_file ("/sys/class/gpio/unexport", s, NULL);
  }

/*============================================================================
  gpiopin_set
============================================================================*/
void gpiopin_set (GPIOPin *self, BOOL val)
  {
  assert (self != NULL);
  assert (self->value_fd >= 0);
  char c = val ? '1' : '0';
  write (self->value_fd, &c, 1);
  }

/*============================================================================
  gpiopin_get
============================================================================*/
BOOL gpiopin_get (const GPIOPin *self)
  {
  char c;
  lseek (self->value_fd, 0, SEEK_SET);
  /* int n = */ read (self->value_fd, &c, 1);
  BOOL ret = (c == '1');
  // printf ("n=%d c=%d s=%d\n", n, c, self->value_fd);
  return ret; 
  }

/*============================================================================
  gpiopin_set
============================================================================*/
void gpiopin_set_trigger (GPIOPin *self, GPIOPinTrigger trigger)
  {
  char s[50];
  snprintf (s, sizeof(s), "/sys/class/gpio/gpio%d/edge", self->pin);
  int f = open (s, O_WRONLY);
  assert (f >= 0);
  switch (trigger)
    {
    case GPIOPIN_BOTH:
      gpiopin_write_to_file (s, "both", NULL); 
      break;
    case GPIOPIN_RISING:
      gpiopin_write_to_file (s, "rising", NULL); 
      break;
    case GPIOPIN_FALLING:
      gpiopin_write_to_file (s, "falling", NULL); 
      break;
    default:
      gpiopin_write_to_file (s, "none", NULL); 
      break;
    }
  close (f);
  }

/*============================================================================
 
  gpiopin_wait_for_trigger

  The poll() interface to GPIO is not well-documented, either for the
  Raspberry Pi or anything else. The precise sequence of polls, lseeks,
  and reads are what I've arrived at by trial-and-error 

============================================================================*/
void gpiopin_wait_for_trigger (GPIOPin *self, int usec)
  {
  assert (self != NULL);
  assert (self->value_fd >= 0);
  struct pollfd fdset[1];
  fdset[0].fd = self->value_fd;
  fdset[0].events = POLLPRI; 
  fdset[0].revents = 0; 
  char  buff[50];
  lseek (self->value_fd, 0, 0); 
  poll (fdset, 1, usec / 1000);
  // We should not read more the one byte here, but better to be safe.
  read (self->value_fd, buff, sizeof (buff));
  }


