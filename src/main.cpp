/* Copyright (c) 2017, Raphael Lehmann
 * All Rights Reserved.
 *
 * The file is part of the reflow-oven-xpcc project and is released under
 * the GPLv3 license.
 * See the file `LICENSE` for the full license governing this code.
 * ------------------------------------------------------------------------- */

#include <xpcc/architecture/platform.hpp>

#include "oven.hpp"

int main()
{
	Board::initialize();
	LedGreen::setOutput(xpcc::Gpio::Low);

	Oven::initialize();

	while (1)
	{
		//todo
	}
	return 0;
}
