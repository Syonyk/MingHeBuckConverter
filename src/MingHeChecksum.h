#ifndef __MING_HE_CHECKSUM_H__
#define __MING_HE_CHECKSUM_H__

#include <Arduino.h>

/**
 * The DPS6015A implements a basic Longitudinal Redundancy Check.  The algorithm
 * is pretty simple: For the entire command or response string minus the LRC
 * character, sum the ASCII values.  Take the result modulo 26, and use this as
 * an index into a zero-indexed set of uppercase characters to determine the LRC
 * character.
 * 
 * So, for the string ":01rz6015", the ASCII values (in decimal) are: 58, 48,
 * 49, 114, 122, 54, 48, 49, 53.  These sum to 595, which modulo 26 leaves a
 * remainder of 23.  The 24th letter of the alphabet (remember, zero indexed
 * array) is "X" - and that's the string that comes from the device:
 * ":01rz6015X"
 * 
 * Since command codes are only ever lowercase letters, and other values are
 * numbers, the only place you should see an uppercase character is in the LRC
 * code.
 * 
 * This class implements this functionality.  It is totally and completely not
 * threadsafe, which is fine for a single threaded microcontroller.
 * 
 * - Call .reset() to clear state.
 * - Add a character at a time, or a null terminated string.
 * - Call .getChecksumCharacter() to get the current checksum.
 * 
 * Simple, easy to use, and uses a whopping one byte of SRAM to store the state.
 * 
 * Written by Russell Graves (https://syonyk.blogspot.com), with no warrantee,
 * of any sort.
 * 
 * This is released as full open source, no license.  Do what you want with it.
 * 
 * However, if you find it useful, tossing a few bucks in the tip jar on my blog
 * would be appreciated.
 */

class MingHeBuckConverterChecksum {
  public:
    MingHeBuckConverterChecksum();
    void addOutputCharacter(const char c);
    void addOutputString(const char *c);
    void addOutputString(const __FlashStringHelper* c);
    char getChecksumCharacter(void);
    void reset(void);
  private:
    // One byte of storage for this class - all it needs.
    unsigned char counter_;
};

#endif // __MING_HE_CHECKSUM_H__
