#include <Arduino.h>
#include "MingHeChecksum.h"

/*
 * Written by Russell Graves (https://syonyk.blogspot.com), with no warrantee,
 * of any sort.
 * 
 * This is released as full open source, no license.  Do what you want with it.
 * 
 * However, if you find it useful, tossing a few bucks in the tip jar on my blog
 * would be appreciated.
 */

MingHeBuckConverterChecksum::MingHeBuckConverterChecksum() {
  reset();
}

// All the reset does is clear the counter to zero.
void MingHeBuckConverterChecksum::reset(void) {
  counter_ = 0;
}

void MingHeBuckConverterChecksum::addOutputString(const char *c) {
  while (*c) {
    addOutputCharacter(*c);
    c++; // ;-)
  }
}

void MingHeBuckConverterChecksum::addOutputString(const __FlashStringHelper* c) {
  int message_length;
  const char *p = reinterpret_cast<const char *>(c);
  message_length = strlen_P(p);
  for (int i = 0; i < message_length; i++) {
    addOutputCharacter(pgm_read_byte_near(p));
    p++;
  }
}

/**
 * Experimentation indicated that using the "while >; subtract" style loop was
 * about 30% the cycle count of doing it as a modulus.  Modulus is really
 * expensive, and doing 8 bit comparisons and subtraction is really cheap.  Not
 * what I was expecting going in, but the benchmarks don't lie - at least on the
 * 328 series chips, this is radically faster.  So... use it!
 */
void MingHeBuckConverterChecksum::addOutputCharacter(const char c) {
  counter_ += c;
  while (counter_ >= 26) {
    counter_ -= 26;
  }
}

// I love math with letters like this.  So clean.  So hideous.
char MingHeBuckConverterChecksum::getChecksumCharacter(void) {
  return (counter_ + 'A');
}

