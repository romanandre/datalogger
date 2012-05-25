#include <Arduino.h>
#include "DLCommon.h"
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

extern unsigned int __bss_end;
extern void *__brkval;

void get_from_flash(void *ptr, char *dst) {
	strcpy_P(dst, (prog_char *)pgm_read_word(ptr));
}

void get_from_flash_P(const prog_char *ptr, char *dst) {
	strcpy_P(dst, ptr);
}

int strcmp_flash(char *str, void *ptr, char *dst) {
	get_from_flash(ptr, dst);	
	return strncmp(str, dst, strlen(dst));
}

int get_free_memory() {
	int free_memory;
	if ((int)__brkval == 0)
		free_memory = ((int)&free_memory) - ((int)&__bss_end);
	else
		free_memory = ((int)&free_memory) - ((int)__brkval);

	return free_memory;
}
int memory_test() {
	// This function will return the number of bytes currently free in SRAM
	int byteCounter = 0; // initialize a counter
	byte *byteArray; // create a pointer to a byte array
	// Use the malloc function to repeatedly attempt allocating a certain number of bytes to memory
	while ( (byteArray = (byte*) malloc (byteCounter * sizeof(byte))) != NULL ) {
		byteCounter++; // If allocation was successful, then up the count for the next try
		free(byteArray); // Free memory after allocating it
	}
	free(byteArray); // Also free memory after the function finishes
	return byteCounter; // Send back the number number of bytes which have been successfully allocated
}

// Very simple XOR checksum
uint8_t get_checksum(char *string) {
	int i;
	uint8_t XOR;
	uint8_t c;
	for (XOR = 0, i = 0; i < strlen(string); i++) {
		c = (uint8_t)string[i];
		XOR ^= c;
	}
	return XOR;
}

//
// Produce a formatted string in a buffer corresponding to the value provided.
// If the 'width' parameter is non-zero, the value will be padded with leading
// zeroes to achieve the specified width.  The number of characters added to
// the buffer (not including the null termination) is returned.
//
unsigned
fmtUnsigned(unsigned long val, char *buf, unsigned bufLen, byte width)
{
  if (!buf || !bufLen)
    return(0);

  // produce the digit string (backwards in the digit buffer)
  char dbuf[12];
  unsigned idx = 0;
  while (idx < sizeof(dbuf))
  {
    dbuf[idx++] = (val % 10) + '0';
    if ((val /= 10) == 0)
      break;
  }

  // copy the optional leading zeroes and digits to the target buffer
  unsigned len = 0;
  byte padding = (width > idx) ? width - idx : 0;
  char c = '0';
  while ((--bufLen > 0) && (idx || padding))
  {
    if (padding)
      padding--;
    else
      c = dbuf[--idx];
    *buf++ = c;
    len++;
  }

  // add the null termination
  *buf = '\0';
  return(len);
}

//
// Format a floating point value with number of decimal places.
// The 'precision' parameter is a number from 0 to 6 indicating the desired decimal places.
// The 'buf' parameter points to a buffer to receive the formatted string.  This must be
// sufficiently large to contain the resulting string.  The buffer's length may be
// optionally specified.  If it is given, the maximum length of the generated string
// will be one less than the specified value.
//
// example: fmtDouble(3.1415, 2, buf); // produces 3.14 (two decimal places)
//
void
fmtDouble(double val, byte precision, char *buf, unsigned bufLen)
{
  if (!buf || !bufLen)
    return;

  // limit the precision to the maximum allowed value
  const byte maxPrecision = 6;
  if (precision > maxPrecision)
    precision = maxPrecision;

  if (--bufLen > 0)
  {
    // check for a negative value
    if (val < 0.0)
    {
      val = -val;
      *buf = '-';
      bufLen--;
    }

    // compute the rounding factor and fractional multiplier
    double roundingFactor = 0.5;
    unsigned long mult = 1;
    for (byte i = 0; i < precision; i++)
    {
      roundingFactor /= 10.0;
      mult *= 10;
    }

    if (bufLen > 0)
    {
      // apply the rounding factor
      val += roundingFactor;

      // add the integral portion to the buffer
      unsigned len = fmtUnsigned((unsigned long)val, buf, bufLen);
      buf += len;
      bufLen -= len;
    }

    // handle the fractional portion
    if ((precision > 0) && (bufLen > 0))
    {
      *buf++ = '.';
      if (--bufLen > 0)
        buf += fmtUnsigned((unsigned long)((val - (unsigned long)val) * mult), buf, bufLen, precision);
    }
  }

  // null-terminate the string
  *buf = '\0';
} 

