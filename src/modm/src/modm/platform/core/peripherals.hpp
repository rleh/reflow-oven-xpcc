/*
 * Copyright (c) 2019, Niklas Hauser
 *
 * This file is part of the modm project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
// ----------------------------------------------------------------------------

#ifndef MODM_STM32_PERIPHERALS_HPP
#define MODM_STM32_PERIPHERALS_HPP

namespace modm::platform
{

enum class
Peripheral
{
	BitBang,
	Adc1,
	Crc,
	Dma1,
	Dma2,
	Flash,
	I2c1,
	I2c2,
	I2c3,
	I2s,
	I2s1,
	I2s2,
	I2s3,
	I2s4,
	I2s5,
	Iwdg,
	Rcc,
	Rtc,
	Sdio,
	Spi1,
	Spi2,
	Spi3,
	Spi4,
	Spi5,
	Sys,
	Tim1,
	Tim10,
	Tim11,
	Tim2,
	Tim3,
	Tim4,
	Tim5,
	Tim9,
	Usart1,
	Usart2,
	Usart6,
	UsbOtgFs,
	Wwdg,
	Syscfg = Sys,
};

}

#endif // MODM_STM32_PERIPHERALS_HPP