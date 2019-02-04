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

#include "Logger.h"

int main(int argc, const char * argv[]) {
	if (argc > 2) {
		if (static_cast<std::string>(argv[1]) == "--log-files-path") {
			Logger logger("logger", "Log messages to the log file", argv[2]);
			try {
				logger.run();

			} catch (zmq::error_t& e) {
				throw "Logger: Interrupt received: Exit";
			}
		} else {
			std::cout << " Invalid argument/s: --help" << std::endl;
		}
	} else if (argc > 1) {
		if (static_cast<std::string>(argv[1]) == "--help") {
			std::cout << "<< Help >>" << std::endl;
			std::cout << "--log-files-path LOG-FILES-PATH >> "
					<< "Save log-files in LOG-FILES-PATH" << std::endl;
		} else {
			std::cout << " Invalid argument/s: --help" << std::endl;
		}
	} else {
		std::cout << " Invalid or missing argument/s: --help" << std::endl;
	}

	return 0;
}

