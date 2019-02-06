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

#include "ConfigurationServer.h"

int main(int argc, char* argv[])
{
	if (argc > 2)
	{
		if (static_cast<std::string>(argv[1]) == "--config-file")
		{
			ConfigurationServer configServerModel(argv[2]);
			try
			{
				configServerModel.run();

			} catch (zmq::error_t& e)
			{
				std::cout << "ConfigurationServer: Interrupt received: Exit"
						<< std::endl;
			}
		} else
		{
			std::cout << " Invalid argument/s: --help" << std::endl;
		}
	} else if (argc > 1)
	{
		if (static_cast<std::string>(argv[1]) == "--help")
		{
			std::cout << "<< Help >>" << std::endl;
			std::cout << "--config-file CONFIG-PATH >> "
					<< "Set path of models-configuration file" << std::endl;
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
