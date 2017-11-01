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

		// Wait before initializing LTC298x
		while(!PT_CALL(tempSensor.ping())) {
			logger << "LTC2984 not reachable" << xpcc::endl;
			this->timeout.restart(100);
			PT_WAIT_UNTIL(this->timeout.isExpired());
		}

		// Configure the device
		PT_CALL(tempSensor.configureChannel(xpcc::ltc2984::Channel::Ch2, xpcc::ltc2984::Configuration::rsense(
												xpcc::ltc2984::Configuration::Rsense::Resistance_t(2000*1024)
												)));
		PT_CALL(tempSensor.configureChannel(xpcc::ltc2984::Channel::Ch4, xpcc::ltc2984::Configuration::rtd(
									 xpcc::ltc2984::Configuration::SensorType::Pt1000,
									 xpcc::ltc2984::Configuration::Rtd::RsenseChannel::Ch2_Ch1,
									 xpcc::ltc2984::Configuration::Rtd::Wires::Wire4,
									 xpcc::ltc2984::Configuration::Rtd::ExcitationMode::Rotation_Sharing,
									 xpcc::ltc2984::Configuration::Rtd::ExcitationCurrent::Current_100uA,
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
	xpcc::Timeout timeout;
	//xpcc::Timestamp stamp;
};
Pt100SensorThread pt100SensorThread;


xpcc::Pid<int32_t, 1000> pid;
OvenTimer ovenTimer(xpcc::Timestamp(0));
static const xpcc::Timestamp reflowProcessDuration(360 * 1000);

using Point = xpcc::Pair<uint32_t, int32_t>;

// time in milliseconds and temperature in millidegree celsius
// Reflow profile: https://www.compuphase.com/electronics/reflowsolderprofiles.htm
Point reflowCurveNoPbPoints[7] =
{
	{ 0,		15000 },
	{ 60000,	125000 },
	{ 180000,	180000 },
	{ 225000,	245000 },
	{ 265000,	245000 },
	{ 270000,	0 },
	{ 360000,	0 }
};
xpcc::interpolation::Linear<Point> reflowCurveNoPb(reflowCurveNoPbPoints, 7);

Point reflowCurvePbPoints[7] =
{
	{ 0,		15000 },
	{ 90000,	120000 },
	{ 180000,	150000 },
	{ 225000,	230000 },
	{ 255000,	230000 },
	{ 255001,	0 },
	{ 360000,	0 }
};
xpcc::interpolation::Linear<Point> reflowCurvePb(reflowCurveNoPbPoints, 7);

enum class ReflowMode : uint8_t {
	NoPb,
	Pb,
	ConstTemperature,
};
ReflowMode reflowMode = ReflowMode::NoPb;

int16_t constTemperature = 50;

int32_t reflowCurve(uint32_t time) {
	switch (reflowMode) {
	case ReflowMode::NoPb:
		return reflowCurveNoPb.interpolate(time);
	case ReflowMode::Pb:
		return reflowCurvePb.interpolate(time);
	case ReflowMode::ConstTemperature:
		return constTemperature * 1000;
	default:
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
			if(ovenTimer.isRunning()) {
				PT_WAIT_UNTIL(pidTimer.execute());
				temp = pt100SensorThread.getLastTemp();
				if(temp.isValid()) {
					logger << "Temperature: actual ";
					logger.printf("%03.2f", temp.getTemperatureFloat());
					logger << "C, target: " << reflowCurve(ovenTimer.elapsed().getTime()) / 1000 << "C" << xpcc::endl;
					logger << "Time: " << ovenTimer.elapsed().getTime() << "ms" << xpcc::endl;
					pid.update(reflowCurve(ovenTimer.elapsed().getTime()) - static_cast<int32_t>(temp.getTemperatureFloat() * 1000));
					pwmValue = static_cast<uint16_t>(std::max<int32_t>(0l, pid.getValue()));
					logger << "Pwm: " << pwmValue << xpcc::endl;
					Oven::Pwm::set(pwmValue);
				}
				else {
					logger << "Error: Invalid temperature measurement" << xpcc::endl;
					logger << temp << xpcc::endl;
				}
			}
			else {
				Oven::Pwm::disable();
			}
			PT_YIELD();
		}

		PT_END();
	}
private:
	xpcc::PeriodicTimer pidTimer;
	xpcc::ltc2984::Data temp;
public:
	uint16_t pwmValue;
};
PidThread pidThread;




// UiThread updates the display andreads the
// button states (with debouncing).
class UiThread : public xpcc::pt::Protothread
{
public:
	UiThread() :
		debounceTimer(10),
		displayTimer(100),
		tempPlotTimer(reflowProcessDuration.getTime() / tempPlotLength),
		buttonPressed(0),
		debounceStartButton(5),
		debounceStopButton(5),
		tempPlot{}
	{
	}

	bool
	update()
	{
		PT_BEGIN();
		while (1)
		{
			if(debounceTimer.execute()) {
				// Start/temperature button
				debounceStartButton.update(Oven::Ui::ButtonStart::read());
				if(debounceStartButton.getValue() && buttonPressed.isExpired()) {
					buttonPressed.restart(500);
					if(!ovenTimer.isRunning()) {
						// Start reflow process
						ovenTimer.restart(reflowProcessDuration);
						Oven::Pwm::enable();
						logger << "Info: Starting reflow process." << xpcc::endl;
						tempPlotIndex = 0;
						tempPlot.fill(0);
					}
					else {
						// Increase temperature (only in ConstTemperature mode)
						constTemperature += 5;
						if(constTemperature > 260) {
							constTemperature = 50;
						}
					}
				}
				// Stop/mode button
				debounceStopButton.update(Oven::Ui::ButtonStop::read());
				if(debounceStopButton.getValue() && buttonPressed.isExpired()) {
					buttonPressed.restart(500);
					if(ovenTimer.isRunning()) {
						// stop reflow process if ovenTimer is running
						ovenTimer.restart(0);
						Oven::Pwm::disable();
						logger << "Info: Stopped reflow process." << xpcc::endl;
					}
					else {
						// switch reflow mode if oven is idling
						switch (reflowMode) {
						case ReflowMode::NoPb:
							reflowMode = ReflowMode::Pb;
							break;
						case ReflowMode::Pb:
							reflowMode = ReflowMode::ConstTemperature;
							constTemperature = 50;
							break;
						case ReflowMode::ConstTemperature:
							reflowMode = ReflowMode::NoPb;
							break;
						}
					}
				}
			}
			if(displayTimer.execute()) {
				Oven::Display::display.clear();

				temp = pt100SensorThread.getLastTemp();
				Oven::Display::display.setCursor(86,0);
				if(temp.isValid()) {
					Oven::Display::display.printf("%3.1fC", temp.getTemperatureFloat());
				}
				else {
					Oven::Display::display << " T-ERR";
				}

				Oven::Display::display.setCursor(0,16);
				switch (reflowMode) {
				case ReflowMode::NoPb:
					Oven::Display::display << "NoPb";
					break;
				case ReflowMode::Pb:
					Oven::Display::display << "  Pb";
					break;
				case ReflowMode::ConstTemperature:
					Oven::Display::display << "T" << constTemperature;
					break;
				}

				Oven::Display::display.setCursor(0,0);
				if(ovenTimer.isRunning()) {
					time = ovenTimer.elapsed().getTime() / 1000u;
					Oven::Display::display.printf("%lu:%02lu", time / 60, time % 60);
					Oven::Display::display.setCursor(39,0);
					Oven::Display::display << pidThread.pwmValue * 100 / Oven::Pwm::Overflow << "%";
				}
				else {
					Oven::Display::display << "OFF";
				}
				for (uint8_t i = 0;i < tempPlotLength; i++) {
					Oven::Display::display.drawPixel(i, 63 - tempPlot[i]);
				}
				if(ovenTimer.isRunning()) {
					Oven::Display::display.setColor(xpcc::glcd::Color::black());
					for (uint8_t i = 0; i < tempPlotLength; i+=2) {
						Oven::Display::display.drawPixel(i, 63 - (reflowCurve(i * reflowProcessDuration.getTime() / 128) * 48 / 260000));
					}
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
					tempPlot.fill(0);
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
	xpcc::Timeout buttonPressed;
	xpcc::filter::Debounce<uint8_t> debounceStartButton;
	xpcc::filter::Debounce<uint8_t> debounceStopButton;
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

	AFIO->MAPR = (AFIO->MAPR & ~(3 << 4)) | (2 << 4); // USART3_REMAP[1:0]: 11: Full remap (TX/PD8, RX/PD9, CK/PD10, CTS/PD11, RTS/PD12)
	AFIO->MAPR = (AFIO->MAPR & ~(3 << 6)) | (1 << 6); // TIM1_REMAP[1:0] = “01” (partial remap)
	Oven::Pwm::initialize();

	Oven::Fan::initialize();
	Oven::Display::initialize();
	Oven::Ui::initialize();

	pid.setParameter(xpcc::Pid<int32_t, 1000>::Parameter(2.25f, -0.15f, 0, 50, Oven::Pwm::Overflow));

	logger << "Debug: Timer1 Overflow: " << Oven::Pwm::Overflow << xpcc::endl;

	// Fan is always on
	Oven::Fan::Pin::set();

	while (1)
	{
		pt100SensorThread.update();
		pidThread.update();
		uiThread.update();
	}
	return 0;
}
