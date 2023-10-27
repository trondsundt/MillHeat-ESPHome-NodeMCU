#include "esphome.h"

bool newData;
char receivedChars[16];

// Commands for controlling the device
char togglePowerCommand[] = {0x00, 0x10, 0x20, 0x00, 0x44, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Power toggle is at index 5
char setTemperatureCommand[] = {0x00, 0x10, 0x22, 0x00, 0x46, 0x01, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00}; // Temperature setting is at index 7
char setFanCommand[] = {0x00, 0x10, 0x27, 0x00, 0x48, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Fan control is at index 5

class MyCustomClimate : public Component, public UARTDevice, public Climate {
public:
  MyCustomClimate(UARTComponent *parent) : UARTDevice(parent) {}

  void setup() override {}

  void loop() override {
    recvWithStartEndMarkers();

    if (newData == true) {
      newData = false;
      if (receivedChars[4] == 0xC9) { // Filter out unnecessary information

        if (receivedChars[12] != 0) {
          this->target_temperature = receivedChars[12];
        }

        if (receivedChars[6] != 0) {
          char hexStr[3];
          sprintf(hexStr, "%02x%02x", receivedChars[7], receivedChars[6]);

          // Convert the hexadecimal string to an integer
          int hexValue;
          sscanf(hexStr, "%x", &hexValue);

          // Divide by 10 to get the final current_temperature
          this->current_temperature = (float)hexValue / 10.0;

          ESP_LOGI("ReceivedBytes", "Hex: %s, Decimal: %d, Temperature: %.1f", hexStr, hexValue, this->current_temperature);
        }

        if (receivedChars[10] == 0x00) {
          this->mode = climate::CLIMATE_MODE_OFF;
          this->action = climate::CLIMATE_ACTION_OFF;
        } else if (receivedChars[10] == 0x01) {
          this->mode = climate::CLIMATE_MODE_HEAT;
        }

        if (receivedChars[11] == 0x01) {
          this->action = climate::CLIMATE_ACTION_HEATING;
        } else {
          this->action = climate::CLIMATE_ACTION_IDLE;
        }

        if (receivedChars[14] == 0x00) {
          this->fan_mode = climate::CLIMATE_FAN_OFF;
        } else if (receivedChars[10] == 0x01) {
          this->fan_mode = climate::CLIMATE_FAN_ON;
        }

        this->publish_state();
      }
    }
  }

  void control(const ClimateCall &call) override {
    ESP_LOGD("custom", "Climate change requested");

    if (call.get_fan_mode().has_value()) {
      // User requested fan mode change
      ClimateFanMode fan_mode = *call.get_fan_mode();
      // Send fan mode command to hardware
      if (fan_mode == CLIMATE_FAN_ON) {
        sendCommand(setFanCommand, sizeof(setFanCommand), 0x01);
      } else if (fan_mode == CLIMATE_FAN_OFF) {
        sendCommand(setFanCommand, sizeof(setFanCommand), 0x00);
      }

      this->fan_mode = fan_mode;
      this->publish_state();
    }

    if (call.get_mode().has_value()) {
      switch (call.get_mode().value()) {
        case CLIMATE_MODE_OFF:
          sendCommand(togglePowerCommand, sizeof(togglePowerCommand), 0x00);
          break;
        case CLIMATE_MODE_HEAT:
          sendCommand(togglePowerCommand, sizeof(togglePowerCommand), 0x01);
          break;
      }

      ClimateMode mode = *call.get_mode();
      this->mode = mode;
      this->publish_state();
    }

    if (call.get_target_temperature().has_value()) {
      // User requested target temperature change
      int temp = *call.get_target_temperature();
      sendCommand(setTemperatureCommand, sizeof(setTemperatureCommand), temp);
      this->target_temperature = temp;
      this->publish_state();
    }
  }

  ClimateTraits traits() override {
    // The capabilities of the climate device
    auto traits = climate::ClimateTraits();
    traits.set_supports_current_temperature(true);
    traits.set_supports_two_point_target_temperature(false);
    traits.set_visual_min_temperature(5);
    traits.set_visual_max_temperature(30);
    traits.set_supports_action(true);
    traits.set_supported_fan_modes({
      climate::CLIMATE_FAN_OFF,
      climate::CLIMATE_FAN_ON,
    });
    traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_HEAT,
    });
    return traits;
  }

  void recvWithStartEndMarkers() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = 0x5A;
    char endMarker = 0x5B;
    char lineEndMarker = 0x0A;
    char rc;

    if (available() > 0) {
      rc = read();
      if (recvInProgress == true) {
        if ((rc != endMarker) && (rc != lineEndMarker)) {
          receivedChars[ndx] = (char)rc;
          ndx++;
        } else {
          recvInProgress = false;
          ndx = 0;
          newData = true;
        }
      } else if (rc == startMarker) {
        recvInProgress = true;
      }
    }
  }

  /*--- Function for calculating control byte checksum ---*/
  unsigned char checksum(char *buf, int len) {
    unsigned char chk = 0;
    for (; len != 0; len--) {
      chk += *buf++;
    }
    return chk;
  }

  /* Send serial data to the microcontroller */
  void sendCommand(char *commandArray, int len, int command) {
    ESP_LOGD("custom", "Sending serial command");
    if (commandArray[4] == 0x46) { // Temperature
      commandArray[7] = command;
    }
    if (commandArray[4] == 0x44) { // Power on/off
      commandArray[5] = command;
      commandArray[len] = (byte)0x00; // Padding
    }
    if (commandArray[4] == 0x48) { // Fan
      commandArray[5] = command;
      commandArray[len] = (byte)0x00; // Padding
    }
    char crc = checksum(commandArray, len + 1);
    ESP_LOGD("custom", "writing start byte");
    write((byte)0x5A); // Start byte
    for (int i = 0; i < len + 1; i++) { // Message
      write((byte)commandArray[i]);
    }
    write((byte)crc); // Control byte
    write((byte)0x5B); // Stop byte
  }
};
