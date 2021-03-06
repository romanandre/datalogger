#include <Arduino.h>
#include "DLCommon.h"
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

extern unsigned int __bss_end;
extern void *__brkval;

static long _supply_voltage = 0;
static long InternalReferenceVoltage = 1080L;  // Adust this value to your specific internal BG voltage x1000

static PROGMEM prog_uint32_t crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

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

unsigned long crc_update(unsigned long crc, byte data) {
	byte tbl_idx;
	tbl_idx = crc ^ (data >> (0 * 4));
	crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
	tbl_idx = crc ^ (data >> (1 * 4));
	crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
	return crc;
}

unsigned long crc_string(char *s) {
	unsigned long crc = ~0L;
	while (*s)
		crc = crc_update(crc, *s++);
	crc = ~crc;
	return crc;
}

unsigned long crc_struct(char *s, int len) {
	unsigned long crc = ~0L;
	int i = 0;
	for(i=0;i<len;i++)
		if (s[i] != 0)
			crc = crc_update(crc, s[i]);
	crc = ~crc;
	return crc;
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

void set_supply_voltage(long sv) {
	_supply_voltage = sv;
}

long get_supply_voltage() {
	return _supply_voltage;
}

void set_bandgap(long iref, int offset) {
	InternalReferenceVoltage = iref + offset;
}

int get_bandgap(void)
{
   	int _bandgap;

        // REFS1 REFS0          --> 0 1, AVcc internal ref.
        // MUX4 MUX3 MUX2 MUX1 MUX0  --> 1110 1.1V (VBG)
	ADMUX = (0<<REFS1) | (1<<REFS0) | (0<<ADLAR) | (1<<MUX4) | (1<<MUX3) | (1<<MUX2) | (1<<MUX1) | (0<<MUX0);

	for(uint8_t i=0;i<8;i++) { // Read out 3 times for it to stabilize
                // Start a conversion  
                ADCSRA |= _BV( ADSC );
                // Wait for it to complete
                while( ( (ADCSRA & (1<<ADSC)) != 0 ) );
                // Scale the value
		_bandgap = (((InternalReferenceVoltage * 1023L) / ADC) + 5L) / 10L;
        }

      return _bandgap;
}

void reboot_now() {
        wdt_enable(WDTO_15MS); 
	while (1);
}

void digital_clock_display(){
  // digital clock display of the time
  Serial.print(hour());
  print_digits(minute());
  print_digits(second());
  Serial.print(" ");
  Serial.print(dayStr(weekday()));
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(monthShortStr(month()));
  Serial.print(" ");
  Serial.print(year());
  Serial.println();
}

void print_digits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

