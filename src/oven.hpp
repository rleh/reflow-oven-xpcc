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
#include <modm/math/filter/moving_average.hpp>
#include <modm/driver/adc/adc_sampler.hpp>

#define UTILS_LP_FAST(value, sample, filter_constant)	(value -= (filter_constant) * ((value) - (sample)))

// STM32F103 Blue Pill Pinout: http://wiki.stm32duino.com/index.php?title=File:Bluepillpinout.gif

namespace Oven
{

namespace Ui {
	using ButtonStart = GpioInputB5;
	using ButtonStop = GpioInputB4;

	inline void
	initialize()
	{
		ButtonStart::setInput();
		ButtonStop::setInput();
	}
}


namespace Display {
	using Sda = GpioB7;
	using Scl = GpioB6;
	using MyI2cMaster = I2cMaster1;
	modm::Ssd1306<MyI2cMaster> display;

	inline void
	initialize()
	{
        MyI2cMaster::connect<Scl::Scl, Sda::Sda>();
        MyI2cMaster::initialize<Board::SystemClock, 100_kHz>();

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
	    Pin::setOutput();
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
		Timer::setCompareValue(2, value);
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
    using AdcIn1 = GpioInputA1;
    using AdcIn2 = GpioInputA2;
    using AdcIn3 = GpioInputA3;

    using Pin1 = GpioOutputB0;
    using Pin2 = GpioOutputB1;
    using Pin3 = GpioOutputB2;

//    using AdcIn4 = GpioInputA4;

    inline void
    initialize() {
        Pin1::setOutput();
        Pin2::setOutput();
        Pin3::setOutput();
        Adc::connect<AdcIn1::In1, AdcIn2::In2, AdcIn3::In3>();
        Adc::initialize<Board::SystemClock>();
    }

    const std::array<modm::platform::Adc1::Channel, 4> channels{
            modm::platform::Adc1::Channel::Channel1,
            modm::platform::Adc1::Channel::Channel2,
            modm::platform::Adc1::Channel::Channel3//,
//            modm::platform::Adc1::Channel::Channel4
    };

    std::array<float,3> readTemp(){
        std::array<uint16_t, 3> sensorValues;

        Pin1::set(1);
        modm::delay(20us);
        sensorValues.at(0) = Adc::readChannel(channels.at(0));
        Pin1::set(0);
        modm::delay(4us);

        Pin2::set(1);
        modm::delay(20us);
        sensorValues.at(1) = Adc::readChannel(channels.at(1));
        Pin2::set(0);
        modm::delay(4us);

        Pin3::set(1);
        modm::delay(20us);
        sensorValues.at(2) = Adc::readChannel(channels.at(2));
        Pin3::set(0);
        modm::delay(4us);

        const float VCC = 3.3f;
        static std::array<float,3> voltages{1.5,1.5,1.5};
        for ( std::size_t i=0; i<3; i++){
            const float sample = sensorValues.at(i) * VCC / 4096.0f;
            UTILS_LP_FAST(voltages.at(i), sample, 0.1);
        }
        // calculate reference voltage:
        const float U = 3.3;//voltages.at(3) * 2.0; // [V] the voltage divider for measuring the supply voltage has two 1kOhm resistors


        std::array<float,3> temps;
        constexpr std::array<float,3> R2 = {980.f, 980.0f, 980.0f}; // [Ohm] gemessen
        constexpr float alpha = 2.0 * 980.0 / 250.0f; // [Ohm/°C] for PT1000, see https://delta-r.de/de/aktuelles/wissen/pt1000-widerstandstabelle
        for ( std::size_t i = 0; i<3; i++) {
            const float R1 = R2.at(i) * ( U / voltages.at(i) - 1.0f ); // [Ohm]
            temps.at(i) = (R1-2000.0f) / alpha;
        }
        return temps;
    }

}


#endif	// OVEN_HPP
