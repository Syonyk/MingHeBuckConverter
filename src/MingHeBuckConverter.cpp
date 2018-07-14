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

#include "MingHeBuckConverter.h"

// Can't fit this in uint16s.  But at least it's in progmem.
const static uint32_t PROGMEM minghe_baud_index_table[8] = 
  {9600, 19200, 38400, 57600, 115200, 1200, 2400, 4800};

const char PROGMEM response_ok[] = "ok";
const char PROGMEM response_err[] = "err";

MingHeBuckConverter::MingHeBuckConverter(const uint8_t tx_pin, 
        const uint8_t rx_pin, const uint8_t device_id, 
        const uint8_t start_baud_index) {
  uint32_t baud_rate;

  swserial_ = new SoftwareSerial(rx_pin, tx_pin);

  baud_rate = pgm_read_word_near(minghe_baud_index_table + start_baud_index);

  swserial_->begin(baud_rate);

  device_id_ = device_id;
}

MingHeBuckConverter::~MingHeBuckConverter() {
  free(swserial_);
}

void MingHeBuckConverter::resetDeviceId(const uint16_t device_id) {
  device_id_ = device_id;
}

void MingHeBuckConverter::resetBaudrate(const uint8_t new_baud_rate_flag) {
  uint32_t baud_rate;
  baud_rate = pgm_read_word_near(minghe_baud_index_table + new_baud_rate_flag);

  swserial_->begin(baud_rate);
}

void MingHeBuckConverter::sendSerialChar(const char c) {
  checksum_.addOutputCharacter(c);
  swserial_->write(c);
}

/*
 * Send a request to the device.  This will either be a set, or a read.  If it's
 * a read, then value is going to be null (it's not sent).  If this is a set
 * command, use a null terminated character string in value and that will be
 * appended after the proper prefix and command.
 */
void MingHeBuckConverter::sendRequest(const bool set, const char command, 
        const char *value) {
  char address[3];
  const char *p;
  
  // Reset the checksum for this sequence.
  checksum_.reset();

  sendSerialChar(':');

  itoa(device_id_, address, 10);

  // Device ID is always 2 characters.
  if (device_id_ < 10) {
    sendSerialChar('0');
    sendSerialChar(address[0]);
  } else {
    sendSerialChar(address[0]);
    sendSerialChar(address[1]);
  }

  // If a set command, the command prefix is s, otherwise read with r.
  sendSerialChar(set ? 's' : 'r');

  sendSerialChar(command);

  // Send the value string - if it's not null.
  if (value) {
    p = value;
    while (*p) {
      sendSerialChar(*p);
      p++;
    }
  }

  // Send the checksum - this is not included in value as it's dependent on the
  // device ID.
  swserial_->print(checksum_.getChecksumCharacter());
  swserial_->print('\n');
}

// Either read a character from serial or return 0 if timeout happens.
char MingHeBuckConverter::readCharUntilTimeout(const uint32_t timeout_ms) {
  uint32_t start_millis = millis();

  while (!swserial_->available()) {
    if ((millis() - start_millis) >= timeout_ms) {
      return 0;
    }
  }
  // Character is available - return it!
  return swserial_->read();
}

// Swallow newlines (\n or \r) and return when the next entry in the buffer is
// something else.  Or the timeout has happened.
void MingHeBuckConverter::swallowNewlines(const uint32_t timeout_ms) {
  uint32_t start_millis = millis();

  while (1) {
    if (!swserial_->available()) {
      delay(MINGHE_POST_READ_DELAY_MS);
      return;
    }
    // Swallow CR or LF, terminate on any other character.
    if (swserial_->peek() == '\r') {
      swserial_->read();
    } else if (swserial_->peek() == '\n') {
      swserial_->read();
    } else {
      // Not a CR or LF, break out.
      return;
    }
  }
}

/*
 * Read the response, given the expected command.  The response will mirror the
 * command and then include the value, if it's a read command.  By this point,
 * the response has been read into the buffer and is validated (checksum match).
 * 
 * This verifies that the set/read character and the command character are
 * correct, then copies the value field (the numbers), ending when the checksum
 * character is found (uppercase, not a digit).
 */
bool MingHeBuckConverter::readResponse(const bool set, const char command, 
        char *value) {
  char response_buffer[16] = {0};

  // If the read fails, return false.
  if (!readResponseIntoBuffer(response_buffer, sizeof(response_buffer) - 1)) {
    return false;
  }
  
  // If we're here, it means the read succeeded, and the checksum is good.

  // First char should be set/request.
  if (response_buffer[0] != (set ? 's' : 'r')) {
    //Serial.println(F("Wrong set/read value.  Failing."));
    return false;
  }

  // Next is the command character.
  if (response_buffer[1] != command) {
    //Serial.println(F("Wrong command value.  Failing."));
    return false;
  }
  
  // Copy bytes into the array until a non-number is found.
  for (byte i = 0; i < sizeof(response_buffer) - 2; i++) {
    value[i] = response_buffer[i + 2];
    if (!isdigit(value[i])) {
      return true;
    }
  }
  
  return true;
}

// Returns true on ok response, false on err response or other error.
bool MingHeBuckConverter::checkForOk() {
  char response_buffer[16] = {0};

  // If the read fails, return false.
  if (!readResponseIntoBuffer(response_buffer, sizeof(response_buffer) - 1)) {
    return false;
  }
  
  if (strcmp_P(response_buffer, response_ok) == 0) {
    return true;
  }
  
  return false;
}

/*
 * Read the response from the device into a provided buffer.  This will strip
 * off the colon and device ID, then copy the two command bytes and the response
 * value into the buffer.  It does NOT null terminate - the calling code should
 * do that.
 */
bool MingHeBuckConverter::readResponseIntoBuffer(char *buffer, 
        const byte buffer_length) {
  uint8_t chars_read = 0, data_length = 0;
  char c;
  char device_id[3] = {0};

  checksum_.reset();
  // Read until we get a ':' that indicates the start of a response.
  do {
    c = readCharUntilTimeout(MINGHE_PER_CHARACTER_TIMEOUT_MS);
    chars_read++;

    // If enough junk characters are read, something is wrong, read has failed.
    if (chars_read >= MINGHE_MAX_JUNK_CHARACTERS) {
      //Serial.println(F("Too many junk characters found, failing."));
      return false;
    }
  } while (c != ':');

  // Presumably, we have a ':' - start reading out data.
  checksum_.addOutputCharacter(c);

  // Next two bytes are the device ID.
  device_id[0] = readCharUntilTimeout(MINGHE_PER_CHARACTER_TIMEOUT_MS);
  checksum_.addOutputCharacter(device_id[0]);
  device_id[1] = readCharUntilTimeout(MINGHE_PER_CHARACTER_TIMEOUT_MS);
  checksum_.addOutputCharacter(device_id[1]);

  // If the device ID does not match, the read is not from this unit - fail.
  if (atoi(device_id) != device_id_) {
    return false;
  }

  // Read into the buffer until an uppercase character is found.
  for (data_length = 0; data_length < buffer_length; data_length++) {
    c = readCharUntilTimeout(MINGHE_PER_CHARACTER_TIMEOUT_MS);
    // Last character will be a capital and the checksum byte.
    if (isupper(c)) {
      // Checksum character: Check it and break.
      if (c != checksum_.getChecksumCharacter()) {
        return false;
      }
      // Checksum is fine, break out.
      break;
    }

    // Populate the buffer.
    buffer[data_length] = c;
    checksum_.addOutputCharacter(c);
  }
  // End of buffer copy - swallow newline.
  swallowNewlines(MINGHE_PER_CHARACTER_TIMEOUT_MS);

  return true;
}

// Execute a get command on the device.   Return the value.
uint32_t MingHeBuckConverter::executeGetCommand(const char command) {
  char response[11] = {0};
  sendRequest(REQUEST_GET, command, NULL);

  // If the response fails, return 0.
  if (!readResponse(REQUEST_GET, command, response)) {
    return 0;
  }
  
  // Convert the value to a uint32 and return it.
  return (uint32_t) atol(response);
}

// Get the machine model - typically 6015, which means 60V max, 15A max.
uint16_t MingHeBuckConverter::getMachineModel() {
  return (uint16_t)executeGetCommand(MINGHE_COMMAND_MACHINE_MODEL);
}

// Basic connection check - ensure the machine matches what is expected.
bool MingHeBuckConverter::testConnection(const uint16_t model_number) {
  if (getMachineModel() == model_number) {
    return true;
  }
  return false;
}

// Get the current voltage limit on the machine.
uint16_t MingHeBuckConverter::getMaxVoltage() {
  return (uint16_t)executeGetCommand(MINGHE_COMMAND_MAX_VOLTAGE);
}

// Get the current amperage limit on the machine.
uint16_t MingHeBuckConverter::getMaxCurrent() {
  return (uint16_t)executeGetCommand(MINGHE_COMMAND_MAX_CURRENT);
}

// Returns true if the output is enabled, false if not.
bool MingHeBuckConverter::getOutputEnabled() {
  return (bool)executeGetCommand(MINGHE_COMMAND_OUTPUT_STATE);
}

// Get the current voltage on the machine output.
uint16_t MingHeBuckConverter::getVoltage() {
  return (uint16_t)executeGetCommand(MINGHE_COMMAND_VOLTAGE);
}

// Get the current amperage on the machine output.
uint16_t MingHeBuckConverter::getCurrent() {
  return (uint16_t)executeGetCommand(MINGHE_COMMAND_CURRENT);
}

/* 
 * Report the limiting factor - voltage, amperage, or the output is off.  This
 * will be one of:
 * MINGHE_LIMITING_FACTOR_OFF (0)
 * MINGHE_LIMITING_FACTOR_VOLTAGE (1)
 * MINGHE_LIMITING_FACTOR_CURRENT (2)
 */
uint8_t MingHeBuckConverter::getLimitingFactor() {
  return (uint8_t)executeGetCommand(MINGHE_COMMAND_LIMITING_FACTOR);
}

// Get the current output watts (volts * amps))
uint32_t MingHeBuckConverter::getWatts() {
  return executeGetCommand(MINGHE_COMMAND_WATTS);
}

// Get the mAh passed through the machine.
uint32_t MingHeBuckConverter::getmAmpHours() {
  return executeGetCommand(MINGHE_COMMAND_MAMP_HOURS);
}

// Get the output on time, in seconds.
uint32_t MingHeBuckConverter::getPowerOnTime() {
  return executeGetCommand(MINGHE_COMMAND_RUNTIME);
}

// Get current machine temperature.
uint16_t MingHeBuckConverter::getTemperature() {
  return (uint16_t)executeGetCommand(MINGHE_COMMAND_TEMPERATURE);
}

// Get the currently set shutdown protection temperature.
uint16_t MingHeBuckConverter::getShutdownTemperature() {
  return (uint16_t)executeGetCommand(MINGHE_COMMAND_SHUTDOWN_TEMPERATURE);
}

// Get the currently set fan start temperature.
uint16_t MingHeBuckConverter::getFanStartTemperature() {
  return (uint16_t)executeGetCommand(MINGHE_COMMAND_FAN_TEMPERATURE);
}

// Get the state of the fast voltage change function.
bool MingHeBuckConverter::getFastVoltageChangeEnabled() {
  return (bool)executeGetCommand(MINGHE_COMMAND_FAST_VOLTAGE_CHANGE);
}

// Determine if output will be on at boot.
bool MingHeBuckConverter::getBootOutputEnabled() {
  return (bool)executeGetCommand(MINGHE_COMMAND_BOOT_OUTPUT_ENABLED);
}

// Determine if the beeper is enabled.
bool MingHeBuckConverter::getBeeperEnabled() {
  return (bool)executeGetCommand(MINGHE_COMMAND_BEEPER_ENABLED);
}

// Get the communcation version (probably 22).
uint16_t MingHeBuckConverter::getCommunicationVersion() {
  return (uint16_t)executeGetCommand(MINGHE_COMMAND_COMMUNICATION_VERSION);
}


bool MingHeBuckConverter::executeSetCommand(const char command, 
        const uint32_t value) {
  char request[12] = {0};

  // Convert the voltage into a string to transmit (with base 10 values)
  ltoa(value, request, 10);

  sendRequest(REQUEST_SET, command, request);

  if (checkForOk()) {
    return true;
  }
  return false;
}




/*
 * The various setters all oerate like this: Execute the command, then verify 
 * that the new value is set properly.
 * 
 * Calling code should respond properly if this returns false.
 */

// Set the maximum allowed voltage.
bool MingHeBuckConverter::setMaxVoltage(const uint16_t volts_100) {
  if (executeSetCommand(MINGHE_COMMAND_MAX_VOLTAGE, volts_100)) {
    return (getMaxVoltage() == volts_100);
  }
  return false;
}

// Set the maximum allowed current.
bool MingHeBuckConverter::setMaxCurrent(const uint16_t amps_100) {
  if (executeSetCommand(MINGHE_COMMAND_MAX_CURRENT, amps_100)) {
    return (getMaxCurrent() == amps_100);
  }
  return false;
}

// Enable or disable the output.
bool MingHeBuckConverter::setOutputEnabled(const bool enabled) {
  if (executeSetCommand(MINGHE_COMMAND_OUTPUT_STATE, enabled)) {
    return (getOutputEnabled() == enabled);
  }
  return false;
}

// Set the mAh field on the device.
bool MingHeBuckConverter::setmAmpHours(const uint32_t mamp_hours) {
  if (executeSetCommand(MINGHE_COMMAND_MAMP_HOURS, mamp_hours)) {
    // mAmp-hours can climb between set and read.  Allow some slack.
    uint32_t diff;

    diff = getmAmpHours() - mamp_hours;

    if (diff < MAMP_HOUR_TOLERANCE) {
      return true;
    }
  }
  return false;
}

// Set the overtemperature shutdown temperature.
bool MingHeBuckConverter::setShutdownTemperature(const uint8_t degrees_c) {
  if (executeSetCommand(MINGHE_COMMAND_SHUTDOWN_TEMPERATURE, degrees_c)) {
    return (getShutdownTemperature() == degrees_c);
  }
  return false;
}

// Set the fan start temperature.
bool MingHeBuckConverter::setFanStartTemperature(const uint8_t degrees_c) {
  if (executeSetCommand(MINGHE_COMMAND_FAN_TEMPERATURE, degrees_c)) {
    return (getFanStartTemperature() == degrees_c);
  }
  return false;
}

// Set the current power on time value.
bool MingHeBuckConverter::setPowerOnTime(const uint32_t seconds) {
  if (executeSetCommand(MINGHE_COMMAND_RUNTIME, seconds)) {
    // Allow a few seconds difference.
    uint32_t diff;
    
    diff = getPowerOnTime() - seconds;

    if (diff < SECOND_TOLERANCE) {
      return true;
    }
  }
  return false;
}

// You do NOT want to call this.  It does not work properly, as the baud rate
// changes in the middle of the executeSetCommand and this does not work right.
// TODO: Fix, or remove.  Any volunteers?
bool MingHeBuckConverter::setBaudRate(const uint8_t baud_rate_index) {
  if (executeSetCommand(MINGHE_COMMAND_BAUD_RATE, baud_rate_index)) {
    // Calling code should probably change the baud rate now...
    return true;
  }
  return false;
}

// Set the device communication address.
bool MingHeBuckConverter::setAddress(const uint8_t new_address) {
  if (executeSetCommand(MINGHE_COMMAND_ADDRESS, new_address)) {
    // You'll need to call .resetDeviceId after this.
    return true;
  }
  return false;
}

// Enable or disable output on boot.
bool MingHeBuckConverter::setBootOutputEnabled(const bool enabled) {
  if (executeSetCommand(MINGHE_COMMAND_BOOT_OUTPUT_ENABLED, enabled)) {
    return (getBootOutputEnabled() == enabled);
  }
  return false;
}

// Enable or disable the beeper.
bool MingHeBuckConverter::setBeeperEnabled(const bool enabled) {
  if (executeSetCommand(MINGHE_COMMAND_BEEPER_ENABLED, enabled)) {
    return (getBeeperEnabled() == enabled);
  }
  return false;
}

// Enable or disable fast voltage change.
bool MingHeBuckConverter::setFastVoltageChangeEnabled(const bool enabled) {
  if (executeSetCommand(MINGHE_COMMAND_FAST_VOLTAGE_CHANGE, enabled)) {
    return (getFastVoltageChangeEnabled() == enabled);
  }
  return false;
}

// Store current current/amperage limit to a memory slot.
bool MingHeBuckConverter::storeToMemory(const uint8_t slot) {
  if (executeSetCommand(MINGHE_COMMAND_STORE_TO_MEMORY, slot)) {
    return true;
  }
  return false;
}

// Load current/amperage limits from a memory slot.
bool MingHeBuckConverter::loadFromMemory(const uint8_t slot) {
  if (executeSetCommand(MINGHE_COMMAND_LOAD_FROM_MEMORY, slot)) {
    return true;
  }
  return false;
}

