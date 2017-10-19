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
	static uint16_t Overflow;

	struct systemClockTimer1 {
		static constexpr uint32_t Timer1 = Board::systemClock::Apb2;
	};

	inline void
	initialize()
	{
		Pin::connect(Timer::Channel1);
		Timer::enable();
		Timer::setMode(Timer::Mode::UpCounter);
		Overflow = Timer::setPeriod<systemClockTimer1>(200 * 1000); // 0.2s
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

#endif	// OVEN_HPP
