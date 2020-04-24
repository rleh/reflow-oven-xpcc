/*
 * Copyright (c) 2016-2018, Niklas Hauser
 *
 * This file is part of the modm project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
// ----------------------------------------------------------------------------

#ifndef MODM_STM32_GPIO_BASE_HPP
#define MODM_STM32_GPIO_BASE_HPP

#include "../device.hpp"
#include <modm/architecture/interface/gpio.hpp>
#include <modm/math/utils/bit_operation.hpp>
#include <modm/platform/core/peripherals.hpp>

namespace modm::platform
{

/// @ingroup modm_platform_gpio
struct Gpio
{
	enum class
	InputType
	{
		Floating = 0x0,	///< floating on input
		PullUp = 0x1,	///< pull-up on input
		PullDown = 0x2,	///< pull-down on input
	};

	enum class
	OutputType
	{
		PushPull = 0x0,		///< push-pull on output
		OpenDrain = 0x1,	///< open-drain on output
	};

	enum class
	OutputSpeed
	{
		Low      = 0,
		Medium   = 0x1,
		High     = 0x2,
		VeryHigh = 0x3,		///< 30 pF (80 MHz Output max speed on 15 pF)
		MHz2     = Low,
		MHz25    = Medium,
		MHz50    = High,
		MHz100   = VeryHigh,
	};

	enum class
	InputTrigger
	{
		RisingEdge,
		FallingEdge,
		BothEdges,
	};

	/// The Port a Gpio Pin is connected to.
	enum class
	Port
	{
		A = 0,
		B = 1,
		C = 2,
		H = 7,
	};

	/// @cond
	enum class
	Signal
	{
		BitBang,
		Af1,
		Bkin,
		Ch1,
		Ch1n,
		Ch2,
		Ch2n,
		Ch3,
		Ch3n,
		Ch4,
		Ck,
		Ckin,
		Cmd,
		Cts,
		D0,
		D1,
		D2,
		D3,
		D4,
		D5,
		D6,
		D7,
		Dm,
		Dp,
		Etr,
		ExtSd,
		Id,
		In0,
		In1,
		In2,
		In3,
		In4,
		In5,
		In6,
		In7,
		In8,
		In9,
		Jtck,
		Jtdi,
		Jtdo,
		Jtms,
		Jtrst,
		Mck,
		Mco1,
		Miso,
		Mosi,
		Nss,
		Osc32In,
		Osc32Out,
		OscIn,
		OscOut,
		Refin,
		Rts,
		Rx,
		Sck,
		Scl,
		Sd,
		Sda,
		Smba,
		Sof,
		Swclk,
		Swdio,
		Swo,
		Tx,
		Vbus,
		Wkup,
		Ws,
	};
	/// @endcond

protected:
	/// @cond
	/// I/O Direction Mode values for this specific pin.
	enum class
	Mode
	{
		Input  = 0x0,
		Output = 0x1,
		AlternateFunction = 0x2,
		Analog = 0x3,
		Mask   = 0x3,
	};

	static constexpr uint32_t
	i(Mode mode) { return uint32_t(mode); }
	// Enum Class To Integer helper functions.
	static constexpr uint32_t
	i(InputType pull) { return uint32_t(pull); }
	static constexpr uint32_t
	i(OutputType out) { return uint32_t(out); }
	static constexpr uint32_t
	i(OutputSpeed speed) { return uint32_t(speed); }
	/// @endcond
};

} // namespace modm::platform

#endif // MODM_STM32_GPIO_BASE_HPP