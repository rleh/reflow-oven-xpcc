/* Copyright (c) 2017, Raphael Lehmann
 * All Rights Reserved.
 *
 * The file is part of the reflow-oven-modm project and is released under
 * the GPLv3 license.
 * See the file `LICENSE` for the full license governing this code.
 * ------------------------------------------------------------------------- */
#ifndef OVEN_HPP
#define OVEN_HPP

#include <modm/board.hpp>
#include <modm/driver/display/ssd1306.hpp>

// STM32F103 Blue Pill Pinout: http://wiki.stm32duino.com/index.php?title=File:Bluepillpinout.gif

namespace Oven
{

namespace Ui {
	using ButtonStart = modm::platform::GpioInverted<GpioInputB4>;
	using ButtonStop = modm::platform::GpioInverted<GpioInputB5>;

	inline void
	initialize()
	{
		ButtonStart::setInput(Gpio::InputType::PullUp);
		ButtonStop::setInput(Gpio::InputType::PullUp);
	}
}


namespace Display {
	using Sda = GpioB7;
	using Scl = GpioB6;
	using MyI2cMaster = I2cMaster1;
	modm::Ssd1306<MyI2cMaster> display(0x78);

	inline void
	initialize()
	{
        MyI2cMaster::connect<Scl::Scl, Sda::Sda>();
        MyI2cMaster::initialize<Board::SystemClock, 420_kHz>();

		display.initializeBlocking();
		display.setFont(modm::font::Assertion);
		display.setCursor(0,16);
		display << "reflow-oven-modm";
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

namespace Buzzer {
    using Timer = Timer3;
    using Pin = GpioOutputA6;
    static uint16_t Overflow = 0x5007;

//    struct systemClockTimer3 {
//        static constexpr uint32_t Timer3 = Board::systemClock::Apb2;
//    };

    inline void
    initialize()
    {
        Timer::connect<Pin::Ch1>();
        Timer::enable();
        Timer::setMode(Timer::Mode::UpCounter);
        // Pwm frequency: 5Hz | 0.2s
        Timer::setPrescaler(1); // 84 * 1000 * 1000 / 4100 / 0x5007 = 1
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
        set(0);
    }

    inline void
    enable()
    {
        set(10000);
    }
}

namespace Pwm {
	using Timer = Timer2;
	using Pin = GpioOutputB3;
	static uint16_t Overflow = 0xFFFF;

//	struct systemClockTimer2 {
//		static constexpr uint32_t Timer2 = Board::systemClock::Apb2;
//	};

	inline void
	initialize()
	{
		Timer::connect<Pin::Ch2>();
		Timer::enable();
		Timer::setMode(Timer::Mode::UpCounter);
		// Pwm frequency: 5Hz | 0.2s
		Timer::setPrescaler(256); // 84 * 1000 * 1000 / 5 / 0xFFFF = 256
		Timer::setOverflow(Overflow);
		Timer::configureOutputChannel(2, Timer::OutputCompareMode::Pwm, 0);
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
		set(0);
	}
}

} // namespace Oven

class OvenTimer {
public:
	OvenTimer(modm::Duration duration)
	{
		restart(duration);
	}

	void restart(modm::Duration duration)
	{
		startTime = modm::Clock::now();
		endTime = modm::Clock::now() + duration;
	}

	modm::Duration elapsed()
	{
		modm::Timestamp now = modm::Clock::now();
		return (now < endTime) ? now - startTime : modm::Duration(0);
	}

	bool isRunning()
	{
		return (modm::Clock::now() < endTime);
	}

public:
	modm::Timestamp startTime;
	modm::Timestamp endTime;
};


namespace PT1000 {

    using Adc = Adc1;
    using Pin = GpioInputA1;

    inline void
    initialize() {
        Adc::initialize<Board::SystemClock>();
        Adc::connect<Pin::In1>();
        Adc::setPinChannel<Pin>(Adc::SampleTime::Cycles480);
    }

    inline float
    readTemp(){
        int sensorValue = Adc::readChannel(modm::platform::Adc1::Channel::Channel1); // PA1
        float voltage = sensorValue * ( 3.3f / 4096.0f );
        constexpr float U = 5.0f;
        constexpr float R2 = 1000.0f;
        float R1 = R2 * ( U / voltage - 1.0f );
        constexpr float alpha = 940.98f / 250.0f; // https://delta-r.de/de/aktuelles/wissen/pt1000-widerstandstabelle
        return R1 / alpha;
    }

}


#endif	// OVEN_HPP
