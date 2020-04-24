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

#include <cmath>

namespace modm::platform
{
/// @cond
extern uint16_t delay_fcpu_MHz;
extern uint16_t delay_ns_per_loop;
/// @endcond

constexpr Rcc::flash_latency
Rcc::computeFlashLatency(uint32_t Core_Hz, uint16_t Core_mV)
{
	constexpr uint32_t flash_latency_1800[] =
	{
		16000000,
		32000000,
		48000000,
		64000000,
		80000000,
		96000000,
		100000000,
	};
	constexpr uint32_t flash_latency_2100[] =
	{
		18000000,
		36000000,
		54000000,
		72000000,
		90000000,
		100000000,
	};
	constexpr uint32_t flash_latency_2400[] =
	{
		24000000,
		48000000,
		72000000,
		96000000,
		100000000,
	};
	constexpr uint32_t flash_latency_2700[] =
	{
		30000000,
		60000000,
		90000000,
		100000000,
	};
	const uint32_t *lut(flash_latency_1800);
	uint8_t lut_size(sizeof(flash_latency_1800) / sizeof(uint32_t));
	// find the right table for the voltage
	if (2700 <= Core_mV) {
		lut = flash_latency_2700;
		lut_size = sizeof(flash_latency_2700) / sizeof(uint32_t);
	}
	else if (2400 <= Core_mV) {
		lut = flash_latency_2400;
		lut_size = sizeof(flash_latency_2400) / sizeof(uint32_t);
	}
	else if (2100 <= Core_mV) {
		lut = flash_latency_2100;
		lut_size = sizeof(flash_latency_2100) / sizeof(uint32_t);
	}
	// find the next highest frequency in the table
	uint8_t latency(0);
	uint32_t max_freq(0);
	while (latency < lut_size)
	{
		if (Core_Hz <= (max_freq = lut[latency]))
			break;
		latency++;
	}
	return {latency, max_freq};
}

template< uint32_t Core_Hz, uint16_t Core_mV = 3300 >
uint32_t
Rcc::setFlashLatency()
{
	constexpr flash_latency fl = computeFlashLatency(Core_Hz, Core_mV);
	static_assert(Core_Hz <= fl.max_frequency, "CPU Frequency is too high for this core voltage!");

	uint32_t acr = FLASH->ACR & ~FLASH_ACR_LATENCY;
	// set flash latency
	acr |= fl.latency;
	// enable flash prefetch and data and instruction cache
	acr |= FLASH_ACR_PRFTEN | FLASH_ACR_DCEN | FLASH_ACR_ICEN;
	FLASH->ACR = acr;
	__DSB(); __ISB();
	return fl.max_frequency;
}

template< uint32_t Core_Hz >
void
Rcc::updateCoreFrequency()
{
	delay_fcpu_MHz = Core_Hz / 1'000'000;
	delay_ns_per_loop = std::round(3000.f / (Core_Hz / 1'000'000));
}

constexpr bool
rcc_check_enable(Peripheral peripheral)
{
	switch(peripheral) {
		case Peripheral::Adc1:
		case Peripheral::Crc:
		case Peripheral::Dma1:
		case Peripheral::Dma2:
		case Peripheral::I2c1:
		case Peripheral::I2c2:
		case Peripheral::I2c3:
		case Peripheral::Rtc:
		case Peripheral::Sdio:
		case Peripheral::Spi1:
		case Peripheral::Spi2:
		case Peripheral::Spi3:
		case Peripheral::Spi4:
		case Peripheral::Spi5:
		case Peripheral::Tim1:
		case Peripheral::Tim10:
		case Peripheral::Tim11:
		case Peripheral::Tim2:
		case Peripheral::Tim3:
		case Peripheral::Tim4:
		case Peripheral::Tim5:
		case Peripheral::Tim9:
		case Peripheral::Usart1:
		case Peripheral::Usart2:
		case Peripheral::Usart6:
		case Peripheral::Wwdg:
			return true;
		default:
			return false;
	}
}

template< Peripheral peripheral >
void
Rcc::enable()
{
	static_assert(rcc_check_enable(peripheral),
		"Rcc::enable() doesn't know this peripheral!");

	__DSB();
	if constexpr (peripheral == Peripheral::Adc1)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
		}
	if constexpr (peripheral == Peripheral::Crc)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN; __DSB();
			RCC->AHB1RSTR |= RCC_AHB1RSTR_CRCRST; __DSB();
			RCC->AHB1RSTR &= ~RCC_AHB1RSTR_CRCRST;
		}
	if constexpr (peripheral == Peripheral::Dma1)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN; __DSB();
			RCC->AHB1RSTR |= RCC_AHB1RSTR_DMA1RST; __DSB();
			RCC->AHB1RSTR &= ~RCC_AHB1RSTR_DMA1RST;
		}
	if constexpr (peripheral == Peripheral::Dma2)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN; __DSB();
			RCC->AHB1RSTR |= RCC_AHB1RSTR_DMA2RST; __DSB();
			RCC->AHB1RSTR &= ~RCC_AHB1RSTR_DMA2RST;
		}
	if constexpr (peripheral == Peripheral::I2c1)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB1ENR |= RCC_APB1ENR_I2C1EN; __DSB();
			RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST; __DSB();
			RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;
		}
	if constexpr (peripheral == Peripheral::I2c2)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB1ENR |= RCC_APB1ENR_I2C2EN; __DSB();
			RCC->APB1RSTR |= RCC_APB1RSTR_I2C2RST; __DSB();
			RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C2RST;
		}
	if constexpr (peripheral == Peripheral::I2c3)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB1ENR |= RCC_APB1ENR_I2C3EN; __DSB();
			RCC->APB1RSTR |= RCC_APB1RSTR_I2C3RST; __DSB();
			RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C3RST;
		}
	if constexpr (peripheral == Peripheral::Rtc)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->BDCR |= RCC_BDCR_RTCEN;
		}
	if constexpr (peripheral == Peripheral::Sdio)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB2ENR |= RCC_APB2ENR_SDIOEN; __DSB();
			RCC->APB2RSTR |= RCC_APB2RSTR_SDIORST; __DSB();
			RCC->APB2RSTR &= ~RCC_APB2RSTR_SDIORST;
		}
	if constexpr (peripheral == Peripheral::Spi1)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB2ENR |= RCC_APB2ENR_SPI1EN; __DSB();
			RCC->APB2RSTR |= RCC_APB2RSTR_SPI1RST; __DSB();
			RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI1RST;
		}
	if constexpr (peripheral == Peripheral::Spi2)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB1ENR |= RCC_APB1ENR_SPI2EN; __DSB();
			RCC->APB1RSTR |= RCC_APB1RSTR_SPI2RST; __DSB();
			RCC->APB1RSTR &= ~RCC_APB1RSTR_SPI2RST;
		}
	if constexpr (peripheral == Peripheral::Spi3)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB1ENR |= RCC_APB1ENR_SPI3EN; __DSB();
			RCC->APB1RSTR |= RCC_APB1RSTR_SPI3RST; __DSB();
			RCC->APB1RSTR &= ~RCC_APB1RSTR_SPI3RST;
		}
	if constexpr (peripheral == Peripheral::Spi4)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB2ENR |= RCC_APB2ENR_SPI4EN; __DSB();
			RCC->APB2RSTR |= RCC_APB2RSTR_SPI4RST; __DSB();
			RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI4RST;
		}
	if constexpr (peripheral == Peripheral::Spi5)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB2ENR |= RCC_APB2ENR_SPI5EN; __DSB();
			RCC->APB2RSTR |= RCC_APB2RSTR_SPI5RST; __DSB();
			RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI5RST;
		}
	if constexpr (peripheral == Peripheral::Tim1)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB2ENR |= RCC_APB2ENR_TIM1EN; __DSB();
			RCC->APB2RSTR |= RCC_APB2RSTR_TIM1RST; __DSB();
			RCC->APB2RSTR &= ~RCC_APB2RSTR_TIM1RST;
		}
	if constexpr (peripheral == Peripheral::Tim10)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB2ENR |= RCC_APB2ENR_TIM10EN; __DSB();
			RCC->APB2RSTR |= RCC_APB2RSTR_TIM10RST; __DSB();
			RCC->APB2RSTR &= ~RCC_APB2RSTR_TIM10RST;
		}
	if constexpr (peripheral == Peripheral::Tim11)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB2ENR |= RCC_APB2ENR_TIM11EN; __DSB();
			RCC->APB2RSTR |= RCC_APB2RSTR_TIM11RST; __DSB();
			RCC->APB2RSTR &= ~RCC_APB2RSTR_TIM11RST;
		}
	if constexpr (peripheral == Peripheral::Tim2)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; __DSB();
			RCC->APB1RSTR |= RCC_APB1RSTR_TIM2RST; __DSB();
			RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM2RST;
		}
	if constexpr (peripheral == Peripheral::Tim3)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB1ENR |= RCC_APB1ENR_TIM3EN; __DSB();
			RCC->APB1RSTR |= RCC_APB1RSTR_TIM3RST; __DSB();
			RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM3RST;
		}
	if constexpr (peripheral == Peripheral::Tim4)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB1ENR |= RCC_APB1ENR_TIM4EN; __DSB();
			RCC->APB1RSTR |= RCC_APB1RSTR_TIM4RST; __DSB();
			RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM4RST;
		}
	if constexpr (peripheral == Peripheral::Tim5)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB1ENR |= RCC_APB1ENR_TIM5EN; __DSB();
			RCC->APB1RSTR |= RCC_APB1RSTR_TIM5RST; __DSB();
			RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM5RST;
		}
	if constexpr (peripheral == Peripheral::Tim9)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB2ENR |= RCC_APB2ENR_TIM9EN; __DSB();
			RCC->APB2RSTR |= RCC_APB2RSTR_TIM9RST; __DSB();
			RCC->APB2RSTR &= ~RCC_APB2RSTR_TIM9RST;
		}
	if constexpr (peripheral == Peripheral::Usart1)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB2ENR |= RCC_APB2ENR_USART1EN; __DSB();
			RCC->APB2RSTR |= RCC_APB2RSTR_USART1RST; __DSB();
			RCC->APB2RSTR &= ~RCC_APB2RSTR_USART1RST;
		}
	if constexpr (peripheral == Peripheral::Usart2)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB1ENR |= RCC_APB1ENR_USART2EN; __DSB();
			RCC->APB1RSTR |= RCC_APB1RSTR_USART2RST; __DSB();
			RCC->APB1RSTR &= ~RCC_APB1RSTR_USART2RST;
		}
	if constexpr (peripheral == Peripheral::Usart6)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB2ENR |= RCC_APB2ENR_USART6EN; __DSB();
			RCC->APB2RSTR |= RCC_APB2RSTR_USART6RST; __DSB();
			RCC->APB2RSTR &= ~RCC_APB2RSTR_USART6RST;
		}
	if constexpr (peripheral == Peripheral::Wwdg)
		if (not Rcc::isEnabled<peripheral>()) {
			RCC->APB1ENR |= RCC_APB1ENR_WWDGEN; __DSB();
			RCC->APB1RSTR |= RCC_APB1RSTR_WWDGRST; __DSB();
			RCC->APB1RSTR &= ~RCC_APB1RSTR_WWDGRST;
		}
	__DSB();
}

template< Peripheral peripheral >
void
Rcc::disable()
{
	static_assert(rcc_check_enable(peripheral),
		"Rcc::disable() doesn't know this peripheral!");

	__DSB();
	if constexpr (peripheral == Peripheral::Adc1)
		RCC->APB2ENR &= ~RCC_APB2ENR_ADC1EN;
	if constexpr (peripheral == Peripheral::Crc)
		RCC->AHB1ENR &= ~RCC_AHB1ENR_CRCEN;
	if constexpr (peripheral == Peripheral::Dma1)
		RCC->AHB1ENR &= ~RCC_AHB1ENR_DMA1EN;
	if constexpr (peripheral == Peripheral::Dma2)
		RCC->AHB1ENR &= ~RCC_AHB1ENR_DMA2EN;
	if constexpr (peripheral == Peripheral::I2c1)
		RCC->APB1ENR &= ~RCC_APB1ENR_I2C1EN;
	if constexpr (peripheral == Peripheral::I2c2)
		RCC->APB1ENR &= ~RCC_APB1ENR_I2C2EN;
	if constexpr (peripheral == Peripheral::I2c3)
		RCC->APB1ENR &= ~RCC_APB1ENR_I2C3EN;
	if constexpr (peripheral == Peripheral::Rtc)
		RCC->BDCR &= ~RCC_BDCR_RTCEN;
	if constexpr (peripheral == Peripheral::Sdio)
		RCC->APB2ENR &= ~RCC_APB2ENR_SDIOEN;
	if constexpr (peripheral == Peripheral::Spi1)
		RCC->APB2ENR &= ~RCC_APB2ENR_SPI1EN;
	if constexpr (peripheral == Peripheral::Spi2)
		RCC->APB1ENR &= ~RCC_APB1ENR_SPI2EN;
	if constexpr (peripheral == Peripheral::Spi3)
		RCC->APB1ENR &= ~RCC_APB1ENR_SPI3EN;
	if constexpr (peripheral == Peripheral::Spi4)
		RCC->APB2ENR &= ~RCC_APB2ENR_SPI4EN;
	if constexpr (peripheral == Peripheral::Spi5)
		RCC->APB2ENR &= ~RCC_APB2ENR_SPI5EN;
	if constexpr (peripheral == Peripheral::Tim1)
		RCC->APB2ENR &= ~RCC_APB2ENR_TIM1EN;
	if constexpr (peripheral == Peripheral::Tim10)
		RCC->APB2ENR &= ~RCC_APB2ENR_TIM10EN;
	if constexpr (peripheral == Peripheral::Tim11)
		RCC->APB2ENR &= ~RCC_APB2ENR_TIM11EN;
	if constexpr (peripheral == Peripheral::Tim2)
		RCC->APB1ENR &= ~RCC_APB1ENR_TIM2EN;
	if constexpr (peripheral == Peripheral::Tim3)
		RCC->APB1ENR &= ~RCC_APB1ENR_TIM3EN;
	if constexpr (peripheral == Peripheral::Tim4)
		RCC->APB1ENR &= ~RCC_APB1ENR_TIM4EN;
	if constexpr (peripheral == Peripheral::Tim5)
		RCC->APB1ENR &= ~RCC_APB1ENR_TIM5EN;
	if constexpr (peripheral == Peripheral::Tim9)
		RCC->APB2ENR &= ~RCC_APB2ENR_TIM9EN;
	if constexpr (peripheral == Peripheral::Usart1)
		RCC->APB2ENR &= ~RCC_APB2ENR_USART1EN;
	if constexpr (peripheral == Peripheral::Usart2)
		RCC->APB1ENR &= ~RCC_APB1ENR_USART2EN;
	if constexpr (peripheral == Peripheral::Usart6)
		RCC->APB2ENR &= ~RCC_APB2ENR_USART6EN;
	if constexpr (peripheral == Peripheral::Wwdg)
		RCC->APB1ENR &= ~RCC_APB1ENR_WWDGEN;
	__DSB();
}

template< Peripheral peripheral >
bool
Rcc::isEnabled()
{
	static_assert(rcc_check_enable(peripheral),
		"Rcc::isEnabled() doesn't know this peripheral!");

	if constexpr (peripheral == Peripheral::Adc1)
		return RCC->APB2ENR & RCC_APB2ENR_ADC1EN;
	if constexpr (peripheral == Peripheral::Crc)
		return RCC->AHB1ENR & RCC_AHB1ENR_CRCEN;
	if constexpr (peripheral == Peripheral::Dma1)
		return RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN;
	if constexpr (peripheral == Peripheral::Dma2)
		return RCC->AHB1ENR & RCC_AHB1ENR_DMA2EN;
	if constexpr (peripheral == Peripheral::I2c1)
		return RCC->APB1ENR & RCC_APB1ENR_I2C1EN;
	if constexpr (peripheral == Peripheral::I2c2)
		return RCC->APB1ENR & RCC_APB1ENR_I2C2EN;
	if constexpr (peripheral == Peripheral::I2c3)
		return RCC->APB1ENR & RCC_APB1ENR_I2C3EN;
	if constexpr (peripheral == Peripheral::Rtc)
		return RCC->BDCR & RCC_BDCR_RTCEN;
	if constexpr (peripheral == Peripheral::Sdio)
		return RCC->APB2ENR & RCC_APB2ENR_SDIOEN;
	if constexpr (peripheral == Peripheral::Spi1)
		return RCC->APB2ENR & RCC_APB2ENR_SPI1EN;
	if constexpr (peripheral == Peripheral::Spi2)
		return RCC->APB1ENR & RCC_APB1ENR_SPI2EN;
	if constexpr (peripheral == Peripheral::Spi3)
		return RCC->APB1ENR & RCC_APB1ENR_SPI3EN;
	if constexpr (peripheral == Peripheral::Spi4)
		return RCC->APB2ENR & RCC_APB2ENR_SPI4EN;
	if constexpr (peripheral == Peripheral::Spi5)
		return RCC->APB2ENR & RCC_APB2ENR_SPI5EN;
	if constexpr (peripheral == Peripheral::Tim1)
		return RCC->APB2ENR & RCC_APB2ENR_TIM1EN;
	if constexpr (peripheral == Peripheral::Tim10)
		return RCC->APB2ENR & RCC_APB2ENR_TIM10EN;
	if constexpr (peripheral == Peripheral::Tim11)
		return RCC->APB2ENR & RCC_APB2ENR_TIM11EN;
	if constexpr (peripheral == Peripheral::Tim2)
		return RCC->APB1ENR & RCC_APB1ENR_TIM2EN;
	if constexpr (peripheral == Peripheral::Tim3)
		return RCC->APB1ENR & RCC_APB1ENR_TIM3EN;
	if constexpr (peripheral == Peripheral::Tim4)
		return RCC->APB1ENR & RCC_APB1ENR_TIM4EN;
	if constexpr (peripheral == Peripheral::Tim5)
		return RCC->APB1ENR & RCC_APB1ENR_TIM5EN;
	if constexpr (peripheral == Peripheral::Tim9)
		return RCC->APB2ENR & RCC_APB2ENR_TIM9EN;
	if constexpr (peripheral == Peripheral::Usart1)
		return RCC->APB2ENR & RCC_APB2ENR_USART1EN;
	if constexpr (peripheral == Peripheral::Usart2)
		return RCC->APB1ENR & RCC_APB1ENR_USART2EN;
	if constexpr (peripheral == Peripheral::Usart6)
		return RCC->APB2ENR & RCC_APB2ENR_USART6EN;
	if constexpr (peripheral == Peripheral::Wwdg)
		return RCC->APB1ENR & RCC_APB1ENR_WWDGEN;
}

}   // namespace modm::platform
