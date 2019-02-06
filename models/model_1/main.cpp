/*
 * Copyright (c) 2017-2019, German Aerospace Center (DLR)
 *
 * This file is part of the development version of FRASER.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Authors:
 * - 2017-2019, Annika Ofenloch (DLR RY-AVS)
 */

#include "Model_1.h"

int main(int argc, const char * argv[])
{
	if (argc > 2)
	{
		bool validArgs = true;
		std::string modelName = "";

		if (static_cast<std::string>(argv[1]) == "-n")
		{
			modelName = static_cast<std::string>(argv[2]);
		} else
		{
			validArgs = false;
			std::cout << " Invalid argument/s: --help" << std::endl;
		}

		if (validArgs)
		{
			Model1 model_1(modelName, "Test Model 1");
			try
			{
				model_1.run();

			} catch (zmq::error_t& e)
			{
				std::cerr << modelName + ": Interrupt received: Exit"
						<< std::endl;
			}
		}
	} else if (argc > 1)
	{
		if (static_cast<std::string>(argv[1]) == "--help")
		{
			std::cout << "<< Help >>" << std::endl;
			std::cout << "-n NAME >> " << "Set instance name of Model1"
					<< std::endl;
		} else
		{
			std::cout << " Invalid argument/s: --help" << std::endl;
		}
	} else
	{
		std::cout << " Invalid or missing argument/s: --help" << std::endl;
	}

	return 0;
}

