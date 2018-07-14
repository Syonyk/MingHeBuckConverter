#include <LiteSerialLogger.h>


#define LOGGER LiteSerial
//#define LOGGER Serial

#define VERBOSE_OUTPUT

// If you're using this on different hardware, you'll want to change this.
#define MACHINE_MODEL 6015

// Set this if you have a load resistor connected.
// I suggest a large 1 ohm load resistor.
#define LOAD_RESISTOR_CONNECTED
#define LOAD_RESISTOR_RESISTANCE 1.00

#define MAXIMUM_TEST_VOLTAGE 6000
#define VOLTAGE_STEP 100
#define MAXIMUM_TEST_AMPS 1500
#define AMPS_STEP 100

// For load resistor testing.
#define LOAD_MAXIMUM_TEST_VOLTAGE 1000
#define LOAD_VOLTAGE_STEP 100
#define LOAD_MAXIMUM_TEST_AMPS 500
#define LOAD_AMPS_STEP 100

#define TEST_TEMPERATURE 70
// Time and amp-hours are 16 bit values. :/
#define SETTABLE_VALUE 65535

#include "MingHeBuckConverter.h"

// TX pin 3, RX pin 2, Device ID 01
MingHeBuckConverter converter(3, 2, 1, MINGHE_BAUD_9600);



  uint32_t getWatts();

  
bool testVoltageSet(void) {
  bool success = true;

  for (uint16_t volts = 0; volts <= MAXIMUM_TEST_VOLTAGE; 
          volts += VOLTAGE_STEP) {
    uint16_t max_voltage;
    if (!converter.setMaxVoltage(volts)) {
      success = false;
    }

#ifdef VERBOSE_OUTPUT
    LOGGER.print(F("Set max voltage: "));
    LOGGER.print(volts);
    LOGGER.println(F("0 mV"));
#endif

    max_voltage = converter.getMaxVoltage();
    if (max_voltage != volts) {
      success = false;
    }
    
#ifdef VERBOSE_OUTPUT
    LOGGER.print(F("Read max voltage: "));
    LOGGER.print(max_voltage);
    LOGGER.println(F("0 mV"));
#endif
  }
  return success;  
}

bool testCurrentSet(void) {
  bool success = true;

  for (uint16_t amps = 0; amps <= MAXIMUM_TEST_AMPS; amps += AMPS_STEP) {
    uint16_t max_amps;
    if (!converter.setMaxCurrent(amps)) {
      success = false;
    }
#ifdef VERBOSE_OUTPUT
    LOGGER.print(F("Set max amps: "));
    LOGGER.print(amps);
    LOGGER.println(F("0 mA"));
#endif

    max_amps = converter.getMaxCurrent();
    if (max_amps != amps) {
      success = false;
    }
    
#ifdef VERBOSE_OUTPUT    
    LOGGER.print(F("Read max amps: "));
    LOGGER.print(max_amps);
    LOGGER.println(F("0 mA"));
#endif
  }
  return success;  
}

bool testOutputEnable(void) {
  bool success = true;
  
  // Set the voltage and current to something reasonable - my bench PSU doesn't
  // run past 60V.  10V/1A is sane enough.

  converter.setMaxVoltage(1000);
  converter.setMaxCurrent(100);

  converter.setOutputEnabled(false);

#ifdef VERBOSE_OUTPUT
  // With the output off, voltage and current should be low.
  LOGGER.print(F("Output Off Voltage:"));
  LOGGER.println(converter.getVoltage());

  LOGGER.print(F("Output Off Current:"));
  LOGGER.println(converter.getCurrent());
#endif

  if (converter.getVoltage() > 10) {
    LOGGER.println(F("Error: Voltage with output off too high!"));
    success = false;
  }
  if (converter.getCurrent() > 10) {
    LOGGER.println(F("Error: Amperage with output off too high!"));
    success = false;
  }

  // Ensure the limiting factor is "off"
  if (converter.getLimitingFactor() != MINGHE_LIMITING_FACTOR_OFF) {
    LOGGER.println(F("Error: Limiting factor not 'off' with output disabled."));
    success = false;
  }

  // Enable the output and make sure that the voltage is higher.
  // Don't assume a load resistor is attached.
  // It takes a moment for the output to stabilize.
  converter.setOutputEnabled(true);
  delay(500);
  
#ifdef VERBOSE_OUTPUT
  // With the output off, voltage and current should be low.
  LOGGER.print(F("Output On Voltage:"));
  LOGGER.println(converter.getVoltage());

  LOGGER.print(F("Output On Current:"));
  LOGGER.println(converter.getCurrent());
#endif

  if (converter.getVoltage() < 10) {
    LOGGER.println(F("Error: Voltage with output on too low!"));
    success = false;
  }

#ifdef LOAD_RESISTOR_CONNECTED
  if (converter.getLimitingFactor() != MINGHE_LIMITING_FACTOR_CURRENT) {
    LOGGER.println(F("Error: Limiting factor not 'current' with output enabled "
    "and load attached."));
    success = false;
  }
#else
  if (converter.getLimitingFactor() != MINGHE_LIMITING_FACTOR_VOLTAGE) {
    LOGGER.println(F("Error: Limiting factor not 'voltage' with output "
    "enabled."));
    success = false;
  }
#endif

  converter.setOutputEnabled(false);
  
  return success;
}

bool testTemperatureSet(void) {
  bool success = true;
  uint16_t old_shutdown_temperature = 0;
  uint16_t old_fan_start_temperature = 0;

  if (!converter.getTemperature()) {
    LOGGER.println(F("Error: Cannot get temperature."));
    success = false;
  }

#ifdef VERBOSE_OUTPUT
  LOGGER.print(F("Shutdown temperature: "));
  LOGGER.println(converter.getShutdownTemperature());
  LOGGER.print(F("Fan start temperature: "));
  LOGGER.println(converter.getFanStartTemperature());
#endif

  old_shutdown_temperature = converter.getShutdownTemperature();
  old_fan_start_temperature = converter.getFanStartTemperature();

  converter.setShutdownTemperature(TEST_TEMPERATURE);
  converter.setFanStartTemperature(TEST_TEMPERATURE);

#ifdef VERBOSE_OUTPUT
  LOGGER.print(F("Test Shutdown temperature: "));
  LOGGER.println(converter.getShutdownTemperature());
  LOGGER.print(F("Test Fan start temperature: "));
  LOGGER.println(converter.getFanStartTemperature());
#endif

  if (converter.getShutdownTemperature() != TEST_TEMPERATURE) {
    LOGGER.println(F("Error: Cannot set shutdown temperature."));
    success = false;
  }
  
  if (converter.getFanStartTemperature() != TEST_TEMPERATURE) {
    LOGGER.println(F("Error: Cannot set fan start temperature."));
    success = false;
  }

  // Reset to old values.
  converter.setShutdownTemperature(old_shutdown_temperature);
  converter.setFanStartTemperature(old_fan_start_temperature);
  
  return success;
}


bool testMiscSettings(void) {
  bool success = true;
  bool beeper, boot_output, fast_voltage_change;

#ifdef VERBOSE_OUTPUT
  LOGGER.print(F("Beeper: "));
  LOGGER.println(converter.getBeeperEnabled());
  LOGGER.print(F("Boot Output: "));
  LOGGER.println(converter.getBootOutputEnabled());
  LOGGER.print(F("Fast Voltage Change: "));
  LOGGER.println(converter.getFastVoltageChangeEnabled());
#endif

  beeper = converter.getBeeperEnabled();
  boot_output = converter.getBootOutputEnabled();
  fast_voltage_change = converter.getFastVoltageChangeEnabled();

  // Set them all to false.
  converter.setBeeperEnabled(false);
  converter.setBootOutputEnabled(false);
  converter.setFastVoltageChangeEnabled(false);

#ifdef VERBOSE_OUTPUT
  LOGGER.print(F("False Beeper: "));
  LOGGER.println(converter.getBeeperEnabled());
  LOGGER.print(F("False Boot Output: "));
  LOGGER.println(converter.getBootOutputEnabled());
  LOGGER.print(F("False Fast Voltage Change: "));
  LOGGER.println(converter.getFastVoltageChangeEnabled());
#endif

  if (converter.getBeeperEnabled()) {
    LOGGER.println(F("Error: Beeper enabled when disabled."));
    success = false;
  }
  
  if (converter.getBootOutputEnabled()) {
    LOGGER.println(F("Error: Boot Output enabled when disabled."));
    success = false;
  }
  
  if (converter.getFastVoltageChangeEnabled()) {
    LOGGER.println(F("Error: Fast Voltage Changed enabled when disabled."));
    success = false;
  }
  
  // Set them all to true.
  converter.setBeeperEnabled(true);
  converter.setBootOutputEnabled(true);
  converter.setFastVoltageChangeEnabled(true);

#ifdef VERBOSE_OUTPUT
  LOGGER.print(F("True Beeper: "));
  LOGGER.println(converter.getBeeperEnabled());
  LOGGER.print(F("True Boot Output: "));
  LOGGER.println(converter.getBootOutputEnabled());
  LOGGER.print(F("True Fast Voltage Change: "));
  LOGGER.println(converter.getFastVoltageChangeEnabled());
#endif

  if (!converter.getBeeperEnabled()) {
    LOGGER.println(F("Error: Beeper disabled when enabled."));
    success = false;
  }
  
  if (!converter.getBootOutputEnabled()) {
    LOGGER.println(F("Error: Boot Output disabled when enabled."));
    success = false;
  }
  
  if (!converter.getFastVoltageChangeEnabled()) {
    LOGGER.println(F("Error: Fast Voltage Changed disabled when enabled."));
    success = false;
  }

  // Reset them.
  converter.setBeeperEnabled(beeper);
  converter.setBootOutputEnabled(boot_output);
  converter.setFastVoltageChangeEnabled(fast_voltage_change);

  return success;
}

bool testSettables(void) {
  bool success = true;

  converter.setmAmpHours(SETTABLE_VALUE);
  converter.setPowerOnTime(SETTABLE_VALUE);

  if (converter.getmAmpHours() != SETTABLE_VALUE) {
    LOGGER.println(F("Error: Amp-hour setting did not work!"));
    success = false;
  }
  
  if (converter.getPowerOnTime() != SETTABLE_VALUE) {
    LOGGER.println(F("Error: Power-on-time setting did not work!"));
    success = false;
  }

  return success;
}

bool testMemory(void) {
  bool success = 0;

  converter.setMaxVoltage(MAXIMUM_TEST_VOLTAGE);
  converter.setMaxCurrent(MAXIMUM_TEST_AMPS);

  converter.storeToMemory(9);

  converter.setMaxVoltage(0);
  converter.setMaxCurrent(0);

  converter.loadFromMemory(9);

  if ((converter.getMaxVoltage() != MAXIMUM_TEST_VOLTAGE) || 
          (converter.getMaxCurrent() != MAXIMUM_TEST_AMPS)) {
    LOGGER.println(F("Error: Memory store test to slot 9 failed."));
    success = false;
  }
  
  return success;
}

// Do you REALLY want to run this?  Probably not...
bool testAddressSetting(void) {
  bool success = true;

  // Set address to 2.
  converter.setAddress(2);
  converter.resetDeviceId(2);

  if (!converter.testConnection(MACHINE_MODEL)) {
    LOGGER.println(F("Failure setting new address."));
    success = false;
  }

  // Let's set this back...
  converter.setAddress(1);
  converter.resetDeviceId(1);

  return success;
}

void setup() {
  bool success = true; 
  
  delay(2000);
  
  LOGGER.begin(115200);

  if (converter.testConnection(MACHINE_MODEL)) {
    LOGGER.println(F("Test Connection succeeded."));
  } else {
    LOGGER.println(F("Test Connection failed."));
    while(1);
  }

  LOGGER.print(F("Machine model: "));
  LOGGER.println(converter.getMachineModel());
  LOGGER.print(F("Communication version: "));
  LOGGER.println(converter.getCommunicationVersion());

  // Verify that test voltages are set properly.
  success &= testVoltageSet();
  success &= testCurrentSet();
  success &= testOutputEnable();
  success &= testTemperatureSet();
  success &= testMiscSettings();
  success &= testSettables();
  success &= testMemory();
  //success &= testAddressSetting();
  
  if (success) {
    LOGGER.println(F("All tests passed!"));
  }

}

void loop() {
  // put your main code here, to run repeatedly:

}
