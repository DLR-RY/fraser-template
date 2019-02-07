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

#include "SimulationModel.h"

#include <iostream>

SimulationModel::SimulationModel(std::string name, std::string description) :
		mName(name), mDescription(description), mCtx(1), mPublisher(mCtx), mDealer(
				mCtx, mName), mSimTime("SimTime", 5000), mSimTimeStep(
				"SimTimeStep", 100), mCurrentSimTime("CurrentSimTime", 0), mCycleTime(
				"CylceTime", 0), mSpeedFactor("SpeedFactor", 1.0)
{
	registerInterruptSignal();
	mRun = prepare();
}

SimulationModel::~SimulationModel()
{
	stopSim();
}

void SimulationModel::init()
{
	mCycleTime.setValue(
			double(mSimTimeStep.getValue()) / mSpeedFactor.getValue()); // Wait-time between the cycles in milliseconds
}

bool SimulationModel::prepare()
{
	mTotalNumOfModels = mDealer.getTotalNumberOfModels();
	mNumOfPersistModels = mDealer.getNumberOfPersistModels();

	if (!mPublisher.bindSocket(mDealer.getPortNumFrom(mName)))
	{
		return false;
	}

	// Prepare Synchronization
	if (!mPublisher.preparePubSynchronization(mDealer.getSynchronizationPort()))
	{
		return false;
	}

	// (mTotalNumOfModels - 2), because the simulation and configuration models should not be included
	if (!mPublisher.synchronizePub(mTotalNumOfModels - 2,
			mCurrentSimTime.getValue()))
	{
		return false;
	}

	mPublisher.publishEvent("LogInfo", 0,
			"Synchronized simulation model with the other models (after preparation phase)");

	return true;
}

void SimulationModel::run()
{
	uint64_t currentSimTime = getCurrentSimTime();
	if (mRun)
	{
		while (currentSimTime <= mSimTime.getValue())
		{
			if (!mPause)
			{
				std::chrono::high_resolution_clock::time_point t1 =
						std::chrono::high_resolution_clock::now();
				// Log
				mPublisher.publishEvent("LogInfo", currentSimTime,
						"Simulation Time: " + std::to_string(currentSimTime));

				// Publish current simulation time
				mPublisher.publishEvent("SimTimeChanged", currentSimTime);

				for (auto savepoint : getSavepoints())
				{
					if (currentSimTime == savepoint)
					{
						std::string filePath = "configurations/savepnt_"
								+ std::to_string(savepoint) + "/";

						boost::filesystem::path dir(filePath);
						if (boost::filesystem::create_directory(dir))
						{
							mPublisher.publishEvent("LogInfo", currentSimTime,
									"Directory Created: " + filePath);
						}

						saveState(filePath);
						break;
					}
				}

				currentSimTime += mSimTimeStep.getValue();
				mCurrentSimTime.setValue(currentSimTime);

				std::chrono::high_resolution_clock::time_point t2 =
						std::chrono::high_resolution_clock::now();

				auto delay = t2 - t1;
				std::this_thread::sleep_for(
						std::chrono::milliseconds(mCycleTime.getValue())
								- delay);
			}

			if (interruptOccured)
			{
				break;
			}
		}
	}

	stopSim();
}

void SimulationModel::stopSim()
{
	// Stop all running models and the dns server
	mPublisher.publishEvent("End", mCurrentSimTime.getValue());

	// Sleep for a second to wait that all models are terminated
	// before terminating the logger otherwise log messages could get lost.
	sleep(1);
	mPublisher.publishEvent("EndLogger", mCurrentSimTime.getValue());

	mDealer.stopDNSserver();
}

void SimulationModel::loadState(std::string filePath)
{
	pauseSim();
	auto currentSimTime = mCurrentSimTime.getValue();

	// Restore states
	std::ifstream ifs(filePath + mName + ".config");
	boost::archive::xml_iarchive ia(ifs, boost::archive::no_header);
	try
	{
		ia >> boost::serialization::make_nvp("FieldSet", *this);

	} catch (boost::archive::archive_exception& ex)
	{
		// Log
		mPublisher.publishEvent("LogError", currentSimTime,
				mName + ": Archive Exception during deserializing");
		throw ex.what();
	}
	// Event Data Serialization
	mPublisher.publishEvent("LoadState", currentSimTime, filePath);

	init();

	// Synchronization is necessary, because the simulation
	// has to wait until the other models finished their Restore-method
	// (mNumOfPersistModels - 1), because the simulation model itself should not be included
	mRun = mPublisher.synchronizePub(mNumOfPersistModels - 1, currentSimTime);

	mPublisher.publishEvent("LogInfo", currentSimTime,
			"Synchronized simulation model with the other models (after initialization phase)");

	continueSim();
}

void SimulationModel::saveState(std::string filePath)
{
	pauseSim();
	auto currentSimTime = mCurrentSimTime.getValue();

	// Event Data Serialization
	mPublisher.publishEvent("SaveState", currentSimTime, filePath);

	// Store states
	std::ofstream ofs(filePath + mName + ".config");
	boost::archive::xml_oarchive oa(ofs, boost::archive::no_header);

	try
	{
		oa << boost::serialization::make_nvp("FieldSet", *this);

	} catch (boost::archive::archive_exception& ex)
	{
		// Log
		mPublisher.publishEvent("LogError", currentSimTime,
				mName + ": Archive Exception during serializing");
		throw ex.what();
	}

	// Synchronization is necessary, because the simulation
	// has to wait until the other models finished their Store-method
	// (mNumOfPersistModels - 1), because the simulation model itself should not be included
	mRun = mPublisher.synchronizePub(mNumOfPersistModels - 1, currentSimTime);

	mPublisher.publishEvent("LogInfo", currentSimTime,
			"Synchronized simulation model with the other models (after save state phase)");

	if (mConfigMode)
	{
		mPublisher.publishEvent("LogInfo", currentSimTime,
				"Default configuration files were created");

		stopSim();
	} else
	{
		this->continueSim();
	}
}
