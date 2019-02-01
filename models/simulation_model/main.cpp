/*
 * Copyright (c) 2017-2018, German Aerospace Center (DLR)
 *
 * This file is part of the development version of FRASER.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Authors:
 * - 2017-2018, Annika Ofenloch (DLR RY-AVS)
 */

#include <iostream>
#include <string>

#include "SimulationModel.h"

int main(int argc, char* argv[]) {
	if (argc > 2) {
		std::string configFilePath = argv[2];
		SimulationModel simulation("simulation_model", "Simulation Environment");

		if (static_cast<std::string>(argv[1]) == "--create-config-files") {
			simulation.setConfigMode(true);
			simulation.saveState(configFilePath);

		} else if (static_cast<std::string>(argv[1]) == "--load-config") {
			simulation.loadState(configFilePath);
			simulation.run();

		} else {
			std::cout << " Invalid argument/s: --help" << std::endl;
		}
	} else if (argc > 1) {
		if (static_cast<std::string>(argv[1]) == "--help") {
			std::cout << "<< Help >>" << std::endl;
			std::cout << "--create-config-files CONFIG-PATH >> "
					<< "Create default configuration files with initialized "
					<< "values and save them in CONFIG-PATH" << std::endl;
			std::cout
					<< "--load-config CONFIG-PATH >> Define path of configuration file/s"
					<< std::endl;
		} else {
			std::cout << " Invalid argument/s: --help" << std::endl;
		}
	} else {
		std::cout << " Invalid argument/s: --help" << std::endl;
	}

	return 0;
}
