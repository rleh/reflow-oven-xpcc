/* Copyright (c) 2017, Raphael Lehmann
 * All Rights Reserved.
 *
 * The file is part of the reflow-oven-modm project and is released under
 * the GPLv3 license.
 * See the file `LICENSE` for the full license governing this code.
 * ------------------------------------------------------------------------- */

#include <modm/board.hpp>
#include <modm/math/filter/pid.hpp>
#include <modm/math/filter/debounce.hpp>
#include <modm/io/iostream.hpp>
#include <modm/processing.hpp>
#include <modm/container.hpp>
#include <modm/math/interpolation/linear.hpp>
#include <modm/debug/logger.hpp>
#include <array>

#include "oven.hpp"

modm::IODeviceWrapper< Usart1, modm::IOBuffer::BlockIfFull > loggerDevice;

// Set all four logger streams to use the UART
modm::log::Logger modm::log::debug(loggerDevice);
modm::log::Logger modm::log::info(loggerDevice);
modm::log::Logger modm::log::warning(loggerDevice);
modm::log::Logger modm::log::error(loggerDevice);

modm::Pid<int32_t, 1000> pid;
OvenTimer ovenTimer(modm::Duration(0));
static const modm::Duration reflowProcessDuration(360 * 1000);

using Point = modm::Pair<uint32_t, int32_t>;

// time in milliseconds and temperature in millidegree celsius
// Reflow profile: https://www.compuphase.com/electronics/reflowsolderprofiles.htm
Point reflowCurveNoPbPoints[7] =
{
	{ 0,		15000 },
	{ 80000,	115000 },
	{ 180000,	175000 },
	{ 225000,	240000 },
	{ 265000,	245000 },
	{ 285000,	0 },
	{ 360000,	0 }
};
modm::interpolation::Linear<Point> reflowCurveNoPb(reflowCurveNoPbPoints, 7);

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

uint32_t startCooldownPb = 255001;
uint32_t startCooldownNoPb = 285000;

modm::interpolation::Linear<Point> reflowCurvePb(reflowCurveNoPbPoints, 7);

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

class AdcThread : public modm::pt::Protothread
{
public:
    AdcThread() : adcTimer(10ms) {
    }

    bool update() {
        PT_BEGIN();
        while(1) {
            PT_WAIT_UNTIL(adcTimer.execute());
            temps = PT1000::readTemp();
            PT_YIELD();
        }
        PT_END();
    }
    std::array<float,3> getTemps() const {
        return temps;
    }

    float getPcbTemp() const {
        return temps.at(0);
    }

private:
    modm::PeriodicTimer adcTimer;
    std::array<float, 3> temps{0,0,0};
};
AdcThread adcThread;

class PidThread : public modm::pt::Protothread
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
				temp = adcThread.getPcbTemp();
				if( 0.0f <= temp && temp <= 400.0f ) {
					MODM_LOG_INFO << "Temperature: actual ";
                    MODM_LOG_INFO << printf("%03.2f", temp);
                    MODM_LOG_INFO << "C, target: " << reflowCurve(ovenTimer.elapsed().count()) / 1000 << "C" << modm::endl;
                    MODM_LOG_INFO << "Time: " << ovenTimer.elapsed() << "ms" << modm::endl;
					pid.update(reflowCurve(ovenTimer.elapsed().count()) - static_cast<int32_t>(temp * 1000));
					pwmValue = static_cast<uint16_t>(std::max<int32_t>(0l, pid.getValue()));
                    MODM_LOG_INFO << "Pwm: " << pwmValue << modm::endl;
					Oven::Pwm::set(pwmValue);
				}
				else {
                    MODM_LOG_INFO << "Error: Invalid temperature measurement" << modm::endl;
                    MODM_LOG_INFO << temp << modm::endl;
				}
			}
			else {
				Oven::Pwm::disable();
			}
			PT_YIELD();
		}

		PT_END();
	}

	uint16_t getPwmValue(){
	 return pwmValue;
	}
private:
	modm::PeriodicTimer pidTimer;
	float temp;
public:
	uint16_t pwmValue;
};
PidThread pidThread;




// UiThread updates the display and reads the
// button states (with debouncing).
class UiThread : public modm::pt::Protothread
{
public:
	UiThread() :
		debounceTimer(10),
		displayTimer(200),
		tempPlotTimer(reflowProcessDuration / tempPlotLength),
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
						MODM_LOG_INFO << "Starting reflow process." << modm::endl;
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
						ovenTimer.restart(modm::Duration(0));
						Oven::Pwm::disable();
                        Oven::Buzzer::disable();
						MODM_LOG_INFO << "Stopped reflow process." << modm::endl;
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

				auto temp = adcThread.getTemps();

				//if(0.0f <= temp && temp <= 400.0f) {
				if(true) {

//                    Oven::Display::display.setCursor(80,0);
//					Oven::Display::display.printf("%d", pidThread.getPwmValue());

                    Oven::Display::display.setCursor(86,0);
					Oven::Display::display.printf("%3.1fC", temp.at(0));
//                    Oven::Display::display.setCursor(86,20);
//					Oven::Display::display.printf("%3.1fC", temp.at(1));
//                    Oven::Display::display.setCursor(86,40);
//					Oven::Display::display.printf("%3.1fC", temp.at(2));
				}
				else {
					Oven::Display::display << " T-ERR";
				}


				Oven::Display::display.setCursor(0,16);
				switch (reflowMode) {
				case ReflowMode::NoPb:
					Oven::Display::display << "NoPb";
                    if(ovenTimer.elapsed().count() >= startCooldownNoPb) {
                        Oven::Buzzer::enable();
                    }
					break;
				case ReflowMode::Pb:
					Oven::Display::display << "  Pb";
                    if(ovenTimer.elapsed().count() >= startCooldownPb) {
                        Oven::Buzzer::enable();
                    }
					break;
				case ReflowMode::ConstTemperature:
					Oven::Display::display << "T" << constTemperature;
					break;
				}

				Oven::Display::display.setCursor(0,0);
				if(ovenTimer.isRunning()) {
					time = ovenTimer.elapsed().count() / 1000u;
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
					Oven::Display::display.setColor(modm::glcd::Color::black());
					for (uint8_t i = 0; i < tempPlotLength; i+=2) {
						Oven::Display::display.drawPixel(i, 63 - (reflowCurve(i * reflowProcessDuration.count() / 128) * 48 / 260000));
					}
				}
				Oven::Display::display.update();
			}
			if(tempPlotTimer.execute()) {
				temp = adcThread.getPcbTemp();
				// Scale temperature from 0..260°C (fixed point int 1/1024°C) to 48px display height
				tempPlot[tempPlotIndex] = (0.0f <= temp && temp <= 400.f) ? static_cast<uint8_t>(temp * 48 / (260*1024)) : 0;
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
	modm::PeriodicTimer debounceTimer;
	modm::PeriodicTimer displayTimer;
	modm::PeriodicTimer tempPlotTimer;
	modm::Timeout buttonPressed;
	modm::filter::Debounce<uint8_t> debounceStartButton;
	modm::filter::Debounce<uint8_t> debounceStopButton;
	float temp;
	uint32_t time;
	static constexpr size_t tempPlotLength = 128;
	std::array<uint8_t, tempPlotLength> tempPlot;
	size_t tempPlotIndex = 0;
};
UiThread uiThread;


int main()
{
    modm::delay(500ms);
	Board::initialize();
	Board::LedGreen::setOutput(modm::Gpio::Low);

    Usart1::connect<GpioA9::Tx>();
	Usart1::initialize<Board::systemClock,115200_Bd>();
	MODM_LOG_INFO << "reflow-oven-modm starting ..." << modm::endl;

	Oven::Pwm::initialize();
	Oven::Fan::initialize();
	Oven::Display::initialize();
    PT1000::initialize();
	Oven::Ui::initialize();
	Oven::Buzzer::initialize();
	Oven::Buzzer::enable();
	modm::delay(500ms);
	Oven::Buzzer::disable();

	pid.setParameter(modm::Pid<int32_t, 1000>::Parameter(2.1337f, -0.25f, 3, 40, Oven::Pwm::Overflow));

	MODM_LOG_DEBUG << "PWM Timer Overflow: " << Oven::Pwm::Overflow << modm::endl;

	// Fan is always on
	Oven::Fan::Pin::set();

    Board::LedGreen::toggle();

	while (1)
	{
        adcThread.update();
		pidThread.update();
		uiThread.update();
	}
	return 0;
}
