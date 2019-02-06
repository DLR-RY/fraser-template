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

#include <iostream>
#include "Model_1.h"

Model1::Model1(std::string name, std::string description) :
		mName(name), mDescription(description), mCtx(1), mSubscriber(mCtx), mPublisher(
				mCtx), mDealer(mCtx, mName), mCurrentSimTime(0)
{

	registerInterruptSignal();
	mRun = prepare();
	init();
}

void Model1::init()
{
	// Set or calculate other parameters ...
}

bool Model1::prepare()
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
	mSubscriber.subscribeTo("PCDUCommand");
	mSubscriber.subscribeTo("FirstEvent");
	mSubscriber.subscribeTo("ReturnEvent");

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

void Model1::run()
{
	while (mRun)
	{
		if (mSubscriber.receiveEvent())
		{
			handleEvent();
		}
	}
}

void Model1::handleEvent()
{
	auto eventBuffer = mSubscriber.getEventBuffer();

	auto receivedEvent = event::GetEvent(eventBuffer);
	std::string eventName = receivedEvent->name()->str();
	mCurrentSimTime = receivedEvent->timestamp();
	mRun = !foundCriticalSimCycle(mCurrentSimTime);

	// Log
	mPublisher.publishEvent("LogInfo", mCurrentSimTime,
			mName + " received " + eventName);

	if (receivedEvent->event_data() != nullptr)
	{
		auto dataRef = receivedEvent->event_data_flexbuffer_root();

		if (dataRef.IsString())
		{
			std::string configPath = dataRef.ToString();

			if (eventName == "SaveState")
			{
				saveState(configPath + mName + ".config");
			}

			else if (eventName == "LoadState")
			{
				loadState(configPath + mName + ".config");
			}
		}
	}

	if (eventName == "FirstEvent")
	{
		mPublisher.publishEvent("SubsequentEvent", mCurrentSimTime);

		// Log
		mPublisher.publishEvent("LogInfo", mCurrentSimTime,
				mName + " published SubsequentEvent");
	}

	else if (eventName == "ReturnEvent")
	{
		// Do something with the returned event from model 2
	}

	else if (eventName == "End")
	{
		mRun = false;
	}
}

void Model1::saveState(std::string filePath)
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

void Model1::loadState(std::string filePath)
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
