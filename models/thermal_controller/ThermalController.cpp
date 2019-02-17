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

#include "ThermalController.h"

#include <iostream>

ThermalController::ThermalController(std::string name, std::string description) :
		mName(name), mDescription(description), mCtx(1), mSubscriber(mCtx), mPublisher(
				mCtx), mDealer(mCtx, mName), mCurrentSimTime(0)
{

	registerInterruptSignal();
	mRun = prepare();
	init();
}

void ThermalController::init()
{
	// Set or calculate other parameters ...
}

bool ThermalController::prepare()
{
	mSubscriber.setOwnershipName(mName);

	if (!mPublisher.bindSocket(mDealer.getPortNumFrom(mName)))
	{
		return false;
	}

	if (!mSubscriber.connectToPub(mDealer.getIPFrom("simulation_model"),
			mDealer.getPortNumFrom("simulation_model")))
	{
		return false;
	}

	for (auto depModel : mDealer.getModelDependencies())
	{
		if (!mSubscriber.connectToPub(mDealer.getIPFrom(depModel),
				mDealer.getPortNumFrom(depModel)))
		{
			return false;
		}
	}

	mSubscriber.subscribeTo("LoadState");
	mSubscriber.subscribeTo("SaveState");
	mSubscriber.subscribeTo("End");
	mSubscriber.subscribeTo("TemperatureSensor");

	// Synchronization
	if (!mSubscriber.prepareSubSynchronization(
			mDealer.getIPFrom("simulation_model"),
			mDealer.getSynchronizationPort()))
	{
		return false;
	}

	if (!mSubscriber.synchronizeSub())
	{
		return false;
	}

	return true;
}

void ThermalController::run()
{
	while (mRun)
	{
		if (mSubscriber.receiveEvent())
		{
			handleEvent();
		}
	}
}

void ThermalController::handleEvent()
{
	auto eventBuffer = mSubscriber.getEventBuffer();

	auto receivedEvent = event::GetEvent(eventBuffer);
	std::string eventName = receivedEvent->name()->str();
	mCurrentSimTime = receivedEvent->timestamp();
	mRun = !foundCriticalSimCycle(mCurrentSimTime);

	if (eventName == "TemperatureSensor")
	{
		std::string sensorName;
		if (receivedEvent->event_data_flexbuffer_root().IsMap())
		{
			auto dataRef = receivedEvent->event_data_flexbuffer_root().AsMap();
			if (dataRef["name"].IsString())
			{
				sensorName = dataRef["name"].ToString();
			}

			if (dataRef["temperature"].IsFloat())
			{
				auto sensorTemperature = dataRef["temperature"].AsFloat();
				std::string heaterState = "Off";

				if (sensorTemperature > maxTemperature)
				{
					heaterState = "Cooling";
				} else if (sensorTemperature < minTemperature)
				{
					heaterState = "Heating";
				}

				mPublisher.publishEvent(sensorName + "HeaterState", mCurrentSimTime,
						heaterState);

				mPublisher.publishEvent("LogInfo", mCurrentSimTime,
						mName + ": " + sensorName + " temperature = "
								+ std::to_string(sensorTemperature));
			}
		}

	} else if (eventName == "SaveState")
	{
		if (receivedEvent->event_data() != nullptr)
		{
			auto dataRef = receivedEvent->event_data_flexbuffer_root();
			if (dataRef.IsString())
			{
				std::string configPath = dataRef.ToString();
				saveState(configPath + mName + ".config");
			}
		}

	} else if (eventName == "LoadState")
	{
		if (receivedEvent->event_data() != nullptr)
		{
			auto dataRef = receivedEvent->event_data_flexbuffer_root();
			if (dataRef.IsString())
			{
				std::string configPath = dataRef.ToString();
				loadState(configPath + mName + ".config");
			}
		}

	} else if (eventName == "End")
	{
		mRun = false;
	}
}

void ThermalController::saveState(std::string filePath)
{
	// Store states
	std::ofstream ofs(filePath);
	boost::archive::xml_oarchive oa(ofs, boost::archive::no_header);
	try
	{
		oa << boost::serialization::make_nvp("FieldSet", *this);

	} catch (boost::archive::archive_exception& ex)
	{
		// Log
		mPublisher.publishEvent("LogError", mCurrentSimTime,
				mName + ": Archive Exception during serializing");
		throw ex.what();
	}
	// Log
	mPublisher.publishEvent("LogInfo", mCurrentSimTime,
			mName + " stored its state");

	mRun = mSubscriber.synchronizeSub();
}

void ThermalController::loadState(std::string filePath)
{
	// Restore states
	std::ifstream ifs(filePath);
	boost::archive::xml_iarchive ia(ifs, boost::archive::no_header);
	try
	{
		ia >> boost::serialization::make_nvp("FieldSet", *this);

	} catch (boost::archive::archive_exception& ex)
	{
		// Log
		mPublisher.publishEvent("LogError", mCurrentSimTime,
				mName + ": Archive Exception during deserializing");
		throw ex.what();
	}
	// Log
	mPublisher.publishEvent("LogInfo", mCurrentSimTime,
			mName + " restored its state");

	init();

	mRun = mSubscriber.synchronizeSub();
}
