/* Copyright (c) 2017, Raphael Lehmann
 * All Rights Reserved.
 *
 * The file is part of the reflow-oven-xpcc project and is released under
 * the GPLv3 license.
 * See the file `LICENSE` for the full license governing this code.
 * ------------------------------------------------------------------------- */

#include <xpcc/architecture/platform.hpp>
#include <xpcc/math/filter/pid.hpp>
#include <xpcc/math/filter/debounce.hpp>
#include <xpcc/io/iostream.hpp>
#include <xpcc/processing.hpp>
#include <xpcc/driver/temperature/ltc2984.hpp>
#include <array>

#include "oven.hpp"


xpcc::IODeviceWrapper< Usart1, xpcc::IOBuffer::BlockIfFull > device;
xpcc::IOStream logger(device);


using Cs = GpioOutputB12;
using Sck = GpioOutputB13;
using Mosi = GpioOutputB15;
using Miso = GpioInputB14;
using SpiMaster = SpiMaster2;

class Pt100SensorThread : public xpcc::pt::Protothread
{
public:
	Pt100SensorThread() :
		tempSensor()
	{
	}

	bool
	update()
	{
		PT_BEGIN();

		// Configure the device
		PT_CALL(tempSensor.configureChannel(xpcc::ltc2984::Channel::Ch2, xpcc::ltc2984::Configuration::rsense(
												xpcc::ltc2984::Configuration::Rsense::Resistance_t(2000*1024)
												)));
		PT_CALL(tempSensor.configureChannel(xpcc::ltc2984::Channel::Ch4, xpcc::ltc2984::Configuration::rtd(
									 xpcc::ltc2984::Configuration::SensorType::Pt100,
									 xpcc::ltc2984::Configuration::Rtd::RsenseChannel::Ch2_Ch1,
									 xpcc::ltc2984::Configuration::Rtd::Wires::Wire4,
									 xpcc::ltc2984::Configuration::Rtd::ExcitationMode::Rotation_Sharing,
									 xpcc::ltc2984::Configuration::Rtd::ExcitationCurrent::Current_500uA,
									 xpcc::ltc2984::Configuration::Rtd::RtdCurve::European
									 )));
		tempSensor.enableChannel(xpcc::ltc2984::Configuration::MuxChannel::Ch4);
		PT_CALL(tempSensor.setChannels());

		logger << "Debug: LTC2984 configured" << xpcc::endl;


		while (1)
		{
			//PT_CALL(tempSensor.initiateMeasurements());
			PT_CALL(tempSensor.initiateSingleMeasurement(xpcc::ltc2984::Channel::Ch4));
			//stamp = xpcc::Clock::now();

			// we wait until the conversations are done
			while (PT_CALL(tempSensor.isBusy()))
			{
			}
			//logger << "Temperature measurement finished." << xpcc::endl;

			PT_CALL(tempSensor.readChannel(xpcc::ltc2984::Channel::Ch4, temp));
			//logger << "Temperature: " << temp << xpcc::endl;

			//logger << "Time: " << (xpcc::Clock::now() - stamp) << xpcc::endl;

			//this->timeout.restart(1000);
			//PT_WAIT_UNTIL(this->timeout.isExpired());
		}

		PT_END();
	}
public:

	xpcc::ltc2984::Data
	getLastTemp() { return temp; }

private:
	xpcc::Ltc2984<SpiMaster, Cs> tempSensor;
	xpcc::ltc2984::Data temp;
	//xpcc::Timeout timeout;
	//xpcc::Timestamp stamp;
};
Pt100SensorThread pt100SensorThread;


xpcc::Pid<int32_t> pid(5.f, 0.5f, 0, 200, Oven::Pwm::Overflow);
//v_target = ... // setpoint
//v_input  = ... // input value
//pid.update(v_target - v_input);
//pwm = pid.getValue();

xpcc::Timeout reflowProcessTimeout(0);
static const xpcc::Timestamp reflowProcessDuration(360 * 1000);


// time in milliseconds and temperature in millidegree celsius
// Reflow profile: https://www.compuphase.com/electronics/reflowsolderprofiles.htm
int32_t reflowCurve(uint32_t remaining) {
	uint32_t time = reflowProcessDuration.getTime() - remaining;
	if(time < 90000) {
		// Ramp to soak: 0s to 1:30 | 1.5°C/s = 1.5°mC/ms
		return 15000 + static_cast<int32_t>(1.5f * time);
	}
	else if(time < 180000) {
		// Preheat/soak: 1:30 to 3:00 | 150°C to 180°C
		return 150000 + static_cast<int32_t>(0.3333f * (time - 90000));
	}
	else if(time < 225000) {
		// Ramp to peak: 3:00 to 3:45 | 180°C to 245°C (1.4444°C/s)
		return 180000 + static_cast<int32_t>(1.4444f * (time - 180000));
	}
	else if (time < 255000) {
		// Reflow: 3:45 to 4:15 | 245°C
		return 245000;
	}
	else {
		// Cooling: 4:15 to 6:00
		return 0;
	}
}

class PidThread : public xpcc::pt::Protothread
{
public:
	PidThread() : pidTimer(500)
	{
	}

	bool
	update()
	{
		PT_BEGIN();

		while (1)
		{
			if(!reflowProcessTimeout.isStopped()) {
				PT_WAIT_UNTIL(pidTimer.execute());
				temp = pt100SensorThread.getLastTemp();
				if(temp.isValid()) {
					logger << "Temperature: actual " << temp.getTemperatureFloat() << "C, target: " << reflowCurve(reflowProcessTimeout.remaining()) << "C" << xpcc::endl;
					pid.update(reflowCurve(reflowProcessTimeout.remaining()) - static_cast<int32_t>(temp.getTemperatureFloat() * 1000) );
					Oven::Pwm::set(pid.getValue() > 0 ? static_cast<uint16_t>(pid.getValue()) : 0); // >= 0 ?!?
					// Display
				}
				else {
					logger << "Error: Invalid temperature measurement" << xpcc::endl;
					logger << temp << xpcc::endl;
				}
			}
			else {
				Oven::Pwm::disable();
			}
		}

		PT_END();
	}
private:
	xpcc::PeriodicTimer pidTimer;
	xpcc::ltc2984::Data temp;
};
PidThread pidThread;




/**
 * UiThread updates the display andreads the
 * button states (with debouncing).
 */
class UiThread : public xpcc::pt::Protothread
{
public:
	UiThread() : debounceTimer(50), displayTimer(250), tempPlotTimer(reflowProcessDuration.getTime() / tempPlotLength), debounceStart(5), tempPlot{}
	{
	}

	bool
	update()
	{
		PT_BEGIN();
		while (1)
		{
			if(debounceTimer.execute()) {
				debounceStart.update(Oven::Ui::ButtonStart::read());
				if(debounceStart.getValue() && reflowProcessTimeout.isStopped()) {
					reflowProcessTimeout.restart(reflowProcessDuration);
					Oven::Pwm::enable();
					logger << "Info: Starting reflow process." << xpcc::endl;
					tempPlotIndex = 0;
					tempPlot.fill(0);
				}
				else if(debounceStart.getValue()) {
					logger << "Error: Reflow process runing." << xpcc::endl;
				}
			}
			if(displayTimer.execute()) {
				Oven::Display::display.clear();

				temp = pt100SensorThread.getLastTemp();
				Oven::Display::display.setCursor(86,16);
				if(temp.isValid()) {
					Oven::Display::display.printf("%3.1fC", temp.getTemperatureFloat());
				}
				else {
					Oven::Display::display.printf(" T-ERR");
				}

				Oven::Display::display.setCursor(0,16);
				if(reflowProcessTimeout.isArmed()) {
					time = reflowProcessDuration.getTime() - static_cast<uint32_t>(reflowProcessTimeout.remaining());
					Oven::Display::display.printf("%lu:%02lu", time / 60, time % 60);
					Oven::Display::display << " ON";
				}
				else {
					Oven::Display::display << "OFF";
				}
				for (uint8_t i = 0;i < tempPlotLength; i++) {
					Oven::Display::display.drawPixel(i, 64 - tempPlot[i]);
				}
				Oven::Display::display.update();
			}
			if(tempPlotTimer.execute()) {
				temp = pt100SensorThread.getLastTemp();
				// Scale temperature from 0..260°C (fixed point int 1/1024°C) to 48px display height
				tempPlot[tempPlotIndex] = temp.isValid() ? static_cast<uint8_t>(temp.getTemperatureFixed() * 48 / (260*1024)) : 0;
				tempPlotIndex++;
				if(tempPlotIndex >= tempPlotLength) {
					tempPlotIndex = 0;
				}
			}
			PT_YIELD();
		}
		PT_END();
	}
private:
	xpcc::PeriodicTimer debounceTimer;
	xpcc::PeriodicTimer displayTimer;
	xpcc::PeriodicTimer tempPlotTimer;
	xpcc::filter::Debounce<uint8_t> debounceStart;
	xpcc::ltc2984::Data temp;
	uint32_t time;
	static constexpr size_t tempPlotLength = 128;
	std::array<uint8_t, tempPlotLength> tempPlot;
	size_t tempPlotIndex = 0;
};
UiThread uiThread;


int main()
{
	//Disable JTAG
	AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
	// Enable AxB clocks
	RCC->APB2ENR |= (RCC_APB2ENR_AFIOEN | RCC_APB2ENR_TIM1EN | RCC_APB2ENR_USART1EN);
	RCC->APB1ENR |= (RCC_APB1ENR_I2C1EN | RCC_APB1ENR_SPI2EN);

	Board::initialize();
	Board::LedGreen::setOutput(xpcc::Gpio::Low);

	GpioOutputB6::connect(Usart1::Tx);
	Usart1::initialize<Board::systemClock, xpcc::Uart::B115200>(10);
	logger << "Info: reflow-oven-xpcc starting ..." << xpcc::endl;


	// Connect the GPIOs to the SPIs alternate function
	Sck::connect(SpiMaster::Sck);
	Mosi::connect(SpiMaster::Mosi);
	Miso::connect(SpiMaster::Miso);
	SpiMaster::initialize<Board::systemClock, MHz72/64>();

	AFIO->MAPR = (AFIO->MAPR & ~(3 << 4)) | (2 << 4);
	AFIO->MAPR = (AFIO->MAPR & ~(3 << 6)) | (1 << 6);
	Oven::Pwm::initialize();
	Oven::Fan::initialize();
	Oven::Display::initialize();
	Oven::Ui::initialize();

	logger << "Debug: Timer1 Overflow: " << Oven::Pwm::Overflow << xpcc::endl;

	while (1)
	{
		pt100SensorThread.update();
		pidThread.update();
		uiThread.update();
	}
	return 0;
}
