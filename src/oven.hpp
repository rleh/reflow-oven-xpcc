/* Copyright (c) 2017, Raphael Lehmann
 * All Rights Reserved.
 *
 * The file is part of the reflow-oven-xpcc project and is released under
 * the GPLv3 license.
 * See the file `LICENSE` for the full license governing this code.
 * ------------------------------------------------------------------------- */
#ifndef OVEN_HPP
#define OVEN_HPP

#include <xpcc/architecture/platform.hpp>
#include <xpcc/driver/display/ssd1306.hpp>

// STM32F103 Blue Pill Pinout: http://wiki.stm32duino.com/index.php?title=File:Bluepillpinout.gif

namespace Oven
{

namespace Ui {
	using ButtonStart = xpcc::GpioInverted<GpioInputB10>;
	using ButtonStop = xpcc::GpioInverted<GpioInputB11>;

	inline void
	initialize()
	{
		ButtonStart::setInput(Gpio::InputType::PullUp);
		ButtonStop::setInput(Gpio::InputType::PullUp);
	}
}


namespace Display {
	using Sda = GpioB9;
	using Scl = GpioB8;
	using MyI2cMaster = I2cMaster1;
	xpcc::Ssd1306<MyI2cMaster> display;

	inline void
	initialize()
	{
		Sda::connect(MyI2cMaster::Sda);
		Scl::connect(MyI2cMaster::Scl);
		MyI2cMaster::initialize<Board::systemClock, 420000, xpcc::Tolerance::TwentyPercent>();

		display.initializeBlocking();
		display.setFont(xpcc::font::Assertion);
		display.setCursor(0,16);
		display << "reflow-oven-xpcc";
		display.update();
	}
}

namespace Fan {
	using Pin = GpioOutputA9;

	inline void
	initialize()
	{
		Pin::setOutput(true);
	}
}

namespace Pwm {
	using Timer = Timer1;
	using Pin = GpioOutputA8;
	static uint16_t Overflow = 0xFFFF;

	struct systemClockTimer1 {
		static constexpr uint32_t Timer1 = Board::systemClock::Apb2;
	};

	inline void
	initialize()
	{
		Pin::connect(Timer::Channel1);
		Timer::enable();
		Timer::setMode(Timer::Mode::UpCounter);
		// Pwm frequency: 5Hz | 0.2s
		Timer::setPrescaler(220); // 72 * 1000 * 1000 / 5 / 0xFFFF = 220
		Timer::setOverflow(Overflow);
		Timer::configureOutputChannel(1, Timer::OutputCompareMode::Pwm, 0);
		Timer::applyAndReset();
		Timer::start();
	}


	inline void
	set(uint16_t value)
	{
		Timer::setCompareValue(1, value);
	}

	inline void
	disable()
	{
		Timer::disableOutput();
	}

	inline void
	enable()
	{
		Timer::enableOutput();
	}
}

} // namespace Oven

namespace reflowProfile {

	using Point = xpcc::Pair<uint32_t, int32_t>;

	// time in milliseconds and temperature in millidegree celsius
	// Reflow profile: https://www.compuphase.com/electronics/reflowsolderprofiles.htm
	Point noPbPoints[7] =
	{
		{ 0,		15000 },
		{ 80000,	115000 },
		{ 180000,	175000 },
		{ 225000,	240000 },
		{ 265000,	245000 },
		{ 285000,	0 },
		{ 360000,	0 }
	};
	xpcc::interpolation::Linear<Point> noPb(noPbPoints, sizeof(noPbPoints));

	Point pbPoints[7] =
	{
		{ 0,		15000 },
		{ 90000,	120000 },
		{ 180000,	150000 },
		{ 225000,	230000 },
		{ 255000,	230000 },
		{ 255001,	0 },
		{ 360000,	0 }
	};
	xpcc::interpolation::Linear<Point> pb(pbPoints, sizeof(pbPoints));
}

class OvenTimer {
public:
	OvenTimer(xpcc::Timestamp duration)
	{
		restart(duration);
	}

	void restart(xpcc::Timestamp duration)
	{
		startTime = xpcc::Clock::now();
		endTime = xpcc::Clock::now() + duration;
	}

	xpcc::Timestamp elapsed()
	{
		xpcc::Timestamp now = xpcc::Clock::now();
		return (now < endTime) ? now - startTime : 0;
	}

	bool isRunning()
	{
		return (xpcc::Clock::now() < endTime);
	}

public:
	xpcc::Timestamp startTime;
	xpcc::Timestamp endTime;
};

#endif	// OVEN_HPP
