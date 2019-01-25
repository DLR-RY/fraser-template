/*
 * Copyright (c) 2019, German Aerospace Center (DLR)
 *
 * This file is part of the development version of FRASER.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Authors:
 * - 2019, Annika Ofenloch (DLR RY-AVS)
 */

#include <boost/chrono.hpp>
#include <boost/thread.hpp>
#include <zmq.hpp>

#include "Logger.h"

int main(int argc, const char * argv[]) {

	Logger logger("logger", "Log messages to the log file");
	try {
		logger.run();

	} catch (zmq::error_t& e) {
		std::cout << "Logger: Interrupt received: Exit" << std::endl;
	}

	return 0;
}

