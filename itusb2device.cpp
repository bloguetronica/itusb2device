/* ITUSB2 device class - Version 1.2.3
   Requires CP2130 class version 1.1.0 or later
   Copyright (c) 2021-2022 Samuel Lourenço

   This library is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or (at your
   option) any later version.

   This library is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
   License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this library.  If not, see <https://www.gnu.org/licenses/>.


   Please feel free to contact me via e-mail: samuel.fmlourenco@gmail.com */


// Includes
#include <sstream>
#include <unistd.h>
#include <vector>
#include "itusb2device.h"

// Definitions
const uint8_t EPIN = 0x82;   // Address of endpoint assuming the IN direction
const uint8_t EPOUT = 0x01;  // Address of endpoint assuming the OUT direction
const size_t N_SAMPLES = 5;  // Number of samples per measurement, applicable to getCurrent()

// Private convenience function that is used to get the raw current measurement reading from the LTC2312 ADC
uint16_t ITUSB2Device::getRawCurrent(int &errcnt, std::string &errstr)
{
    std::vector<uint8_t> read = cp2130_.spiRead(2, EPIN, EPOUT, errcnt, errstr);
    return read.size() == 2 ? static_cast<uint16_t>(read[0] << 4 | read[1] >> 4) : 0;  // It is important to check if the size of the returned vector matches the number of expected bytes - If not, return zero!
}

ITUSB2Device::ITUSB2Device() :
    cp2130_()
{
}

// Diagnostic function used to verify if the device has been disconnected
bool ITUSB2Device::disconnected() const
{
    return cp2130_.disconnected();
}

// Checks if the device is open
bool ITUSB2Device::isOpen() const
{
    return cp2130_.isOpen();
}

// Attaches the DUT (device under test) to the HUT (host under test)
void ITUSB2Device::attach(int &errcnt, std::string &errstr)
{
    if (getUSBPowerStatus(errcnt, errstr) != getUSBDataStatus(errcnt, errstr)) {  // If true, this condition indicates an unusual state
        switchUSB(false, errcnt, errstr);  // Switch VBUS off and disconnect the data lines
        usleep(100000);  // Wait 100ms to allow for device shutdown
    }
    if (!getUSBPowerStatus(errcnt, errstr) && !getUSBDataStatus(errcnt, errstr)) {  // If both VBUS and data lines are disconnected
        switchUSBPower(true, errcnt, errstr);  // Switch VBUS on
        usleep(100000);  // Wait 100ms in order to emulate a manual attachment of the device
        switchUSBData(true, errcnt, errstr);  // Connect the data lines
        usleep(100000);  // Wait 100ms so that device enumeration process can, at least, start (this is not enough to guarantee enumeration, though)
    }
}

// Closes the device safely, if open
void ITUSB2Device::close()
{
    cp2130_.close();
}

// Detaches the DUT (device under test) to the HUT (host under test)
void ITUSB2Device::detach(int &errcnt, std::string &errstr)
{
    if (getUSBDataStatus(errcnt, errstr)) {  // If the data lines are connected
        switchUSBData(false, errcnt, errstr);  // Disconnect the data lines
        usleep(100000);  // Wait 100ms in order to emulate a manual detachment of the device
    }
    if (getUSBPowerStatus(errcnt, errstr)) {  // If VBUS is switched on
        switchUSBPower(false, errcnt, errstr);  // Switch VBUS off
        usleep(100000);  // Wait 100ms to allow for device shutdown
    }
}

// Returns the silicon version of the CP2130 bridge
CP2130::SiliconVersion ITUSB2Device::getCP2130SiliconVersion(int &errcnt, std::string &errstr)
{
    return cp2130_.getSiliconVersion(errcnt, errstr);
}

// Gets the VBUS current
// Important: SPI mode should be configured for channel 0, before using this function!
float ITUSB2Device::getCurrent(int &errcnt, std::string &errstr)
{
    cp2130_.selectCS(0, errcnt, errstr);  // Enable the chip select corresponding to channel 0, and disable any others
    usleep(100);  // Wait 100us, in order to prevent possible errors after enabling the chip select (workaround implemented in version 1.2.3)
    getRawCurrent(errcnt, errstr);  // Discard this reading, as it will reflect a past measurement
    size_t currentCodeSum = 0;
    for (size_t i = 0; i < N_SAMPLES; ++i) {
        currentCodeSum += getRawCurrent(errcnt, errstr);  // Read the raw value (from the LTC2312 on channel 0) and add it to the sum
    }
    usleep(100);  // Wait 100us, in order to prevent possible errors while disabling the chip select (workaround)
    cp2130_.disableCS(0, errcnt, errstr);  // Disable the previously enabled chip select
    return currentCodeSum / (4.0 * N_SAMPLES);  // Return the average current out of "N_SAMPLES" [5] for each measurement (currentCode / 4.0 for a single reading)
}

// Gets the DUT connection status (true for connection detected or false for connection not detected)
bool ITUSB2Device::getDUTConnectionStatus(int &errcnt, std::string &errstr)
{
    return cp2130_.getGPIO4(errcnt, errstr);  // Return the current state of the UDCD signal
}

// Gets the DUT link speed status (true for high-speed, or false for full/low speed or suspend mode)
bool ITUSB2Device::getDUTSpeedStatus(int &errcnt, std::string &errstr)
{
    return cp2130_.getGPIO5(errcnt, errstr);  // Return the current state of the UDHS signal
}

// Returns the hardware revision of the device
std::string ITUSB2Device::getHardwareRevision(int &errcnt, std::string &errstr)
{
    return hardwareRevision(getUSBConfig(errcnt, errstr));
}

// Gets the manufacturer descriptor from the device
std::u16string ITUSB2Device::getManufacturerDesc(int &errcnt, std::string &errstr)
{
    return cp2130_.getManufacturerDesc(errcnt, errstr);
}

// Gets OC flag
bool ITUSB2Device::getOvercurrentStatus(int &errcnt, std::string &errstr)
{
    return !cp2130_.getGPIO3(errcnt, errstr);  // Return the current state of the negated !UDOC signal
}

// Gets the product descriptor from the device
std::u16string ITUSB2Device::getProductDesc(int &errcnt, std::string &errstr)
{
    return cp2130_.getProductDesc(errcnt, errstr);
}

// Gets the serial descriptor from the device
std::u16string ITUSB2Device::getSerialDesc(int &errcnt, std::string &errstr)
{
    return cp2130_.getSerialDesc(errcnt, errstr);
}

// Gets the USB configuration of the device
CP2130::USBConfig ITUSB2Device::getUSBConfig(int &errcnt, std::string &errstr)
{
    return cp2130_.getUSBConfig(errcnt, errstr);
}

// Gets the status of the data lines
bool ITUSB2Device::getUSBDataStatus(int &errcnt, std::string &errstr)
{
    return !cp2130_.getGPIO2(errcnt, errstr);  // Return the current state of the negated !UDEN signal
}

// Gets the status of VBUS
bool ITUSB2Device::getUSBPowerStatus(int &errcnt, std::string &errstr)
{
    return !cp2130_.getGPIO1(errcnt, errstr);  // Return the current state of the negated !UPEN signal
}

// Opens a device and assigns its handle
// The serial number is optional since version 1.2.0
int ITUSB2Device::open(const std::string &serial)
{
    return cp2130_.open(VID, PID, serial);
}

// Issues a reset to the CP2130, which in effect resets the entire device
void ITUSB2Device::reset(int &errcnt, std::string &errstr)
{
    cp2130_.reset(errcnt, errstr);
}

// Sets up and prepares the device
void ITUSB2Device::setup(int &errcnt, std::string &errstr)
{
    CP2130::SPIMode mode;
    mode.csmode = CP2130::CSMODEPP;  // Chip select pin mode regarding channel 0 is push-pull
    mode.cfrq = CP2130::CFRQ1500K;  // SPI clock frequency set to 1.5MHz
    mode.cpol = CP2130::CPOL0;  // SPI clock polarity is active high (CPOL = 0)
    mode.cpha = CP2130::CPHA0;  // SPI data is valid on each rising edge (CPHA = 0)
    cp2130_.configureSPIMode(0, mode, errcnt, errstr);  // Configure SPI mode for channel 0, using the above settings
    cp2130_.disableSPIDelays(0, errcnt, errstr);  // Disable all SPI delays for channel 0
    cp2130_.selectCS(0, errcnt, errstr);  // Enable the chip select corresponding to channel 0, and disable any others
    getRawCurrent(errcnt, errstr);  // Discard this first reading - This also wakes up the LTC2312, if in nap or sleep mode!
    usleep(1100);  // Wait 1.1ms to ensure that the LTC2312 is awake, and also to prevent possible errors while disabling the chip select (workaround)
    cp2130_.disableCS(0, errcnt, errstr);  // Disable the previously enabled chip select
}

// Switches both VBUS and the data lines on or off
void ITUSB2Device::switchUSB(bool value, int &errcnt, std::string &errstr)
{
    cp2130_.setGPIOs(CP2130::BMGPIOS * !value, CP2130::BMGPIO1 | CP2130::BMGPIO2 , errcnt, errstr);  // This operates GPIO.1 and GPIO.2 simultaneously
}

// Switches the USB data lines on or off
void ITUSB2Device::switchUSBData(bool value, int &errcnt, std::string &errstr)
{
    cp2130_.setGPIO2(!value, errcnt, errstr);  // GPIO.2 corresponds to the !UDEN signal
}

// Switches VBUS on or off
void ITUSB2Device::switchUSBPower(bool value, int &errcnt, std::string &errstr)
{
    cp2130_.setGPIO1(!value, errcnt, errstr);  // GPIO.1 corresponds to the !UPEN signal
}

// Helper function that returns the hardware revision from a given USB configuration
std::string ITUSB2Device::hardwareRevision(const CP2130::USBConfig &config)
{
    std::string revision;
    if (config.majrel > 1 && config.majrel <= 27) {
        revision += static_cast<char>(config.majrel + 'A' - 2);  // Append major revision letter (a major release number value of 2 corresponds to the letter "A" and so on)
    }
    if (config.majrel == 1 || config.minrel != 0) {
        std::ostringstream stream;
        stream << static_cast<int>(config.minrel);
        revision += stream.str();  // Append minor revision number
    }
    return revision;
}

// Helper function to list devices
std::list<std::string> ITUSB2Device::listDevices(int &errcnt, std::string &errstr)
{
    return CP2130::listDevices(VID, PID, errcnt, errstr);
}
