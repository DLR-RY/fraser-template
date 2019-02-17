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

#include "TemperatureSensorReader.h"

#include <iostream>

TemperatureSensorReader::TemperatureSensorReader(std::string name,
		std::string description) :
		mName(name), mDescription(description), mCtx(1), mSubscriber(mCtx), mPublisher(
				mCtx), mDealer(mCtx, mName), mCurrentSimTime(0)
{

	registerInterruptSignal();
	mRun = prepare();
	init();
}

void TemperatureSensorReader::init()
{
	// Set or calculate other parameters ...
}

bool TemperatureSensorReader::prepare()
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
	mSubscriber.subscribeTo("SimTimeChanged");
	mSubscriber.subscribeTo(mName + "HeaterState");

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

void TemperatureSensorReader::run()
{
	while (mRun)
	{
		if (mSubscriber.receiveEvent())
		{
			handleEvent();
		}
	}
}

void TemperatureSensorReader::handleEvent()
{
	auto eventBuffer = mSubscriber.getEventBuffer();

	auto receivedEvent = event::GetEvent(eventBuffer);
	std::string eventName = receivedEvent->name()->str();
	mCurrentSimTime = receivedEvent->timestamp();
	mRun = !foundCriticalSimCycle(mCurrentSimTime);

	if (eventName == "SimTimeChanged")
	{
		updateTemperature();

		// Publish current temperature
		mFlexbuffer.Clear();
		mFlexbuffer.Map([&]()
		{
			mFlexbuffer.String("name", mName);
			mFlexbuffer.Float("temperature", mCurrentTemperature.getValue());

		});
		mFlexbuffer.Finish();

		mPublisher.publishEvent("TemperatureSensor", mCurrentSimTime,
				mFlexbuffer.GetBuffer());

	} else if (eventName == (mName + "HeaterState"))
	{
		if (receivedEvent->event_data() != nullptr)
		{
			auto dataRef = receivedEvent->event_data_flexbuffer_root();
			if (dataRef.IsString())
			{
				mCurrentState = dataRef.ToString();
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

void TemperatureSensorReader::updateTemperature()
{
	// A function that models reading the temperature from the sensor. It
	// updates the temperature since the last poll and returns the value,
	// but with an additional measurement error:
	auto sensorTemperature = mCurrentTemperature.getValue();

	if (mCurrentState == "Heating")
	{
		mCurrentTemperature.setValue(sensorTemperature + 0.5f);
	} else if (mCurrentState == "Cooling")
	{
		mCurrentTemperature.setValue(sensorTemperature - 0.5f);
	}
	// if the controller is in the state OFF, then the temperature does not
	// change.

	mPublisher.publishEvent("LogInfo", mCurrentSimTime,
			mName + " temperature: "
					+ std::to_string(mCurrentTemperature.getValue()));
}

void TemperatureSensorReader::saveState(std::string filePath)
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

void TemperatureSensorReader::loadState(std::string filePath)
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
