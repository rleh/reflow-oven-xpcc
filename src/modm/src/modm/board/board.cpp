/*
 * Copyright (c) 2016-2017, Niklas Hauser
 *
 * This file is part of the modm project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
// ----------------------------------------------------------------------------

#include "board.hpp"
#include <modm/architecture/interface/delay.hpp>
#include <modm/architecture/interface/assert.hpp>
modm_extern_c void
modm_abandon(const modm::AssertionInfo &info)
{
	(void)info;
	Board::Leds::setOutput();
	for(int times=10; times>=0; times--)
	{
		Board::Leds::write(1);
		modm::delay_ms(20);
		Board::Leds::write(0);
		modm::delay_ms(180);
	}
}
