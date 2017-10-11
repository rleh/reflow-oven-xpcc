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

#include "oven.hpp"


xpcc::IODeviceWrapper< Usart2, xpcc::IOBuffer::BlockIfFull > device;
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
		PT_BEGIN()

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


int16_t reflowCurve(xpcc::Timestamp time) {
	(void) time;
	return 150;
}

xpcc::Pid<int16_t, 10> pid(0.4, 0.5, 0, 200, 512);
//v_target = ... // setpoint
//v_input  = ... // input value
//pid.update(v_target - v_input);
//pwm = pid.getValue();

xpcc::Timeout reflowProcessTimeout(0);
static const xpcc::Timestamp reflowProcessDuration = 600 * 1000;


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
					pid.update(reflowCurve(reflowProcessTimeout.remaining()) - temp.getTemperatureInteger());
					Oven::Pwm::set(pid.getValue()); // >= 0 ?!?
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
	UiThread() : uiTimer(50), debounceStart(5)
	{
	}

	bool
	update()
	{
		PT_BEGIN();
		while (1)
		{
			PT_WAIT_UNTIL(uiTimer.execute());
			debounceStart.update(Oven::ButtonStart::read());
			if(debounceStart.getValue() && reflowProcessTimeout.isStopped()) {
				reflowProcessTimeout.restart(reflowProcessDuration);
				Oven::Pwm::enable();
				logger << "Info: Starting reflow process." << xpcc::endl;
				// Display...
			}
			else if(debounceStart.getValue()) {
				logger << "Error: Reflow process runing." << xpcc::endl;
				// Display...
			}
			temp = pt100SensorThread.getLastTemp();
			if(temp.isValid()) {
				Oven::Display::display.setCursor(10,50);
				Oven::Display::display.printf("%.1f", temp.getTemperatureFloat());
			}
		}
		PT_END();
	}
private:
	xpcc::PeriodicTimer uiTimer;
	xpcc::filter::Debounce<uint8_t> debounceStart;
	xpcc::ltc2984::Data temp;
};
UiThread uiThread;


int main()
{
	Board::initialize();
	Board::LedGreen::setOutput(xpcc::Gpio::Low);

	GpioOutputB6::connect(Usart1::Tx);
	Usart1::initialize<Board::systemClock, xpcc::Uart::B115200>(10);
	logger << "Info: reflow-oven-xpcc starting ..." << xpcc::endl;

	Oven::Pwm::initialize();
	Oven::Display::initialize();

	while (1)
	{
		pt100SensorThread.update();
		pidThread.update();
		uiThread.update();
	}
	return 0;
}
