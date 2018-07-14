/*
 * The main interface class for the MingHe buck converters.  This works on an
 * Arduino, with software serial (configure as needed), and talks to the device.
 * 
 * Written by Russell Graves (https://syonyk.blogspot.com), with no warrantee,
 * of any sort.
 * 
 * This is released as full open source, no license.  Do what you want with it.
 * 
 * However, if you find it useful, tossing a few bucks in the tip jar on my blog
 * would be appreciated.
 */

#ifndef __MING_HE_BUCK_CONVERTER_H__
#define __MING_HE_BUCK_CONVERTER_H__

#include <SoftwareSerial.h>

#include "MingHeChecksum.h"

// Indexes for setting the baud rate.
#define MINGHE_BAUD_9600 0
#define MINGHE_BAUD_19200 1
#define MINGHE_BAUD_38400 2
#define MINGHE_BAUD_57600 3
#define MINGHE_BAUD_115200 4
#define MINGHE_BAUD_1200 5
#define MINGHE_BAUD_2400 6
#define MINGHE_BAUD_4800 7

#define MINGHE_PER_CHARACTER_TIMEOUT_MS 500

#define MINGHE_MAX_JUNK_CHARACTERS 32

#define MINGHE_COMMAND_MAX_VOLTAGE 'u'
#define MINGHE_COMMAND_MAX_CURRENT 'i'
#define MINGHE_COMMAND_VOLTAGE 'v'
#define MINGHE_COMMAND_CURRENT 'j'
#define MINGHE_COMMAND_OUTPUT_STATE 'o'
#define MINGHE_COMMAND_LIMITING_FACTOR 'c'
#define MINGHE_COMMAND_WATTS 'w'
#define MINGHE_COMMAND_MAMP_HOURS 'a'
#define MINGHE_COMMAND_RUNTIME 't'
#define MINGHE_COMMAND_TEMPERATURE 'p'
#define MINGHE_COMMAND_SHUTDOWN_TEMPERATURE 'e'
#define MINGHE_COMMAND_FAN_TEMPERATURE 'f'
#define MINGHE_COMMAND_FAST_VOLTAGE_CHANGE 'g'
#define MINGHE_COMMAND_BOOT_OUTPUT_ENABLED 's'
#define MINGHE_COMMAND_BEEPER_ENABLED 'x'
#define MINGHE_COMMAND_MACHINE_MODEL 'z'
#define MINGHE_COMMAND_COMMUNICATION_VERSION 'r'
#define MINGHE_COMMAND_BAUD_RATE 'b'
#define MINGHE_COMMAND_ADDRESS 'd'
#define MINGHE_COMMAND_STORE_TO_MEMORY 'm'
#define MINGHE_COMMAND_LOAD_FROM_MEMORY 'n'

#define MINGHE_LIMITING_FACTOR_OFF 0
#define MINGHE_LIMITING_FACTOR_VOLTAGE 1
#define MINGHE_LIMITING_FACTOR_CURRENT 2

// Delay after reads in case the next command is a write.  The device cannot
// handle rapid read-then-write behavior.  1ms is enough, normally, so use 5.
#define MINGHE_POST_READ_DELAY_MS 5

// To make request code more readable...
#define REQUEST_GET 0
#define REQUEST_SET 1

// At 15A, 4.17mAh/sec passes through.
// Allow a few seconds between the write and read.
#define MAMP_HOUR_TOLERANCE 100
#define SECOND_TOLERANCE 2

class MingHeBuckConverter {
public:
  /**
   * Constructor.  Pass in the pin number (Arduino style) of the transmit and
   * receive pins for software serial, the device ID you expect to talk with,
   * and the starting baud rate (as a baud index).
   */
  MingHeBuckConverter(const uint8_t tx_pin, const uint8_t rx_pin,
          const uint8_t device_id, const uint8_t start_baud);
  ~MingHeBuckConverter();

  // Test the connection to see if it's alive.
  bool testConnection(const uint16_t model_number);

  // Set (reset) the device ID.
  void resetDeviceId(const uint16_t device_id);
  // Reset the baud rate
  void resetBaudrate(const uint8_t new_baud_rate_flag);

  // All voltages/currents are passed in as an integer, per the docs.
  // Voltage and current are times 100: 100 = 1V/1A, 1500 = 15V/A

  uint16_t getMachineModel();
  uint16_t getMaxVoltage();
  uint16_t getMaxCurrent();

  // These report the actual voltage and current at the time of the command.
  uint16_t getVoltage();
  uint16_t getCurrent();
  bool getOutputEnabled();
  uint8_t getLimitingFactor();
  uint32_t getWatts();
  uint32_t getmAmpHours();
  uint32_t getPowerOnTime();
  uint16_t getTemperature();
  uint16_t getShutdownTemperature();
  uint16_t getFanStartTemperature();
  bool getFastVoltageChangeEnabled();
  bool getBootOutputEnabled();
  bool getBeeperEnabled();
  uint16_t getCommunicationVersion();
  
  bool setMaxVoltage(const uint16_t volts_100);
  bool setMaxCurrent(const uint16_t amps_100);
  bool setOutputEnabled(const bool enabled);
  bool setmAmpHours(const uint32_t mamp_hours);
  bool setShutdownTemperature(const uint8_t degrees_c);
  bool setFanStartTemperature(const uint8_t degrees_c);
  bool setPowerOnTime(const uint32_t seconds);
  bool setBaudRate(const uint8_t baud_rate_index);
  bool setAddress(const uint8_t new_address);
  bool setBootOutputEnabled(const bool enabled);
  bool setBeeperEnabled(const bool enabled);
  bool setFastVoltageChangeEnabled(const bool enabled);

  bool storeToMemory(const uint8_t slot);
  bool loadFromMemory(const uint8_t slot);
  
private:
  /**
   * sendRequest transmits data to the device for execution.
   * @param set True if this is a set command (s), false for read (r).
   * @param command The command letter to use.
   * @param value A null terminated string of the value sequence to send.
   */
  void sendRequest(const bool set, const char command, const char *value);

  // Send a character, copy it to the checksum instance.
  void sendSerialChar(const char c);

  /**
   * Read a response from the serial bus.  Will spin until it sees a ':' at the 
   * start.
   * @param set True if this is reading a set (s), false for a read (r) byte.
   * @param command The expected command byte.
   * @param value A properly sized array for the response.
   */
  bool readResponse(const bool set, const char command, char *value);

  bool checkForOk(void);

  bool readResponseIntoBuffer(char *buffer, const byte buffer_length);

  // Read a character off the software serial, or return 0 if the timeout is hit.
  char readCharUntilTimeout(const uint32_t timeout_millis_value);

  void swallowNewlines(const uint32_t timeout_ms);

  uint32_t executeGetCommand(const char command);
  bool executeSetCommand(const char command, const uint32_t value);

  // SoftwareSerial interface - created on startup, deleted on destruction.
  SoftwareSerial *swserial_;
  MingHeBuckConverterChecksum checksum_;

  // Device ID (01-99).  Stored for later use.
  uint8_t device_id_;
};

#endif // __MING_HE_BUCK_CONVERTER_H__

