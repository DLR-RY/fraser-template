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

#include "Queue.h"

#include <iostream>

Queue::Queue(std::string name, std::string description) :
		mName(name), mDescription(description), mCtx(1), mSubscriber(mCtx), mPublisher(
				mCtx), mDealer(mCtx, mName), mReceivedEvent(NULL), mCurrentSimTime(
				-1)
{

	registerInterruptSignal();
	mRun = prepare();
	init();
}

void Queue::init()
{
	// Set or calculate other parameters ...
	mEventSet.push_back(
			Event("FirstEvent", 500, 300, 10, Priority::NORMAL_PRIORITY));
}

bool Queue::prepare()
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

	mSubscriber.subscribeTo("SimTimeChanged");
	mSubscriber.subscribeTo("End");
	mSubscriber.subscribeTo("LoadState");
	mSubscriber.subscribeTo("SaveState");

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

void Queue::run()
{
	while (mRun)
	{
		if (mSubscriber.receiveEvent())
		{
			handleEvent();
		}
	}
}

void Queue::updateEvents()
{
	if (mEventSet.back().getRepeat() != 0)
	{
		int timestamp = mCurrentSimTime + mEventSet.back().getPeriod();
		mEventSet.back().setTimestamp(timestamp);

		if (mEventSet.back().getRepeat() != -1)
		{
			mEventSet.back().setRepeat(mEventSet.back().getRepeat() - 1);
		}

		mScheduler.scheduleEvents(mEventSet);

	} else
	{
		mEventSet.pop_back();
	}
}

void Queue::handleEvent()
{
	auto eventBuffer = mSubscriber.getEventBuffer();

	auto receivedEvent = event::GetEvent(eventBuffer);
	std::string eventName = receivedEvent->name()->str();
	mCurrentSimTime = receivedEvent->timestamp();
	mRun = !foundCriticalSimCycle(mCurrentSimTime);

	if (eventName == "SaveState")
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
	} else if (eventName == "SimTimeChanged")
	{
		// Send new Flit every clock cycle
		if (!mEventSet.empty())
		{
			auto nextEvent = mEventSet.back();

			if (mCurrentSimTime >= nextEvent.getTimestamp())
			{
				nextEvent.setCurrentSimTime(mCurrentSimTime);

				mPublisher.publishEvent(nextEvent.getName(), mCurrentSimTime);

				// Log
				mPublisher.publishEvent("LogInfo", mCurrentSimTime,
						mName + " published " + nextEvent.getName());

				this->updateEvents();
			}
		}
	} else if (eventName == "End")
	{
		// Log
		mPublisher.publishEvent("LogInfo", mCurrentSimTime,
				mName + " received " + eventName);

		mRun = false;
	}
}

void Queue::saveState(std::string filePath)
{
	// Store states
	std::ofstream ofs(filePath);
	boost::archive::xml_oarchive oa(ofs, boost::archive::no_header);

	try
	{
		oa << boost::serialization::make_nvp("EventSet", mEventSet);

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

void Queue::loadState(std::string filePath)
{
	// Restore states
	std::ifstream ifs(filePath);
	boost::archive::xml_iarchive ia(ifs, boost::archive::no_header);

	try
	{
		ia >> boost::serialization::make_nvp("EventSet", mEventSet);

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

	mScheduler.scheduleEvents(mEventSet);

	mRun = mSubscriber.synchronizeSub();
}
