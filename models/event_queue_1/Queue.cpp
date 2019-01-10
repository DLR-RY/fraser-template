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

#include "Queue.h"

#include <iostream>

Queue::Queue(std::string name, std::string description) :
		mName(name), mDescription(description), mCtx(1), mSubscriber(mCtx), mPublisher(
				mCtx), mDealer(mCtx, mName), mReceivedEvent(NULL), mCurrentSimTime(
				-1) {

	registerInterruptSignal();

	mRun = prepare();

	init();
}

void Queue::init() {
	// Set or calculate other parameters ...
	mEventSet.push_back(
			Event("Test_Event", 0, 3, 199, Priority::NORMAL_PRIORITY));
	mEventSet.push_back(
			Event("Test_Event", 0, 3, 199, Priority::HIGH_PRIORITY));
}

bool Queue::prepare() {
	mSubscriber.setOwnershipName(mName);

	if (!mPublisher.bindSocket(mDealer.getPortNumFrom(mName))) {
		return false;
	}

	if (!mSubscriber.connectToPub(mDealer.getIPFrom("simulation_model"),
			mDealer.getPortNumFrom("simulation_model"))) {
		return false;
	}

	for (auto depModel : mDealer.getModelDependencies()) {
		if (!mSubscriber.connectToPub(mDealer.getIPFrom(depModel),
				mDealer.getPortNumFrom(depModel))) {
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
			mDealer.getSynchronizationPort())) {
		return false;
	}

	if (!mSubscriber.synchronizeSub()) {
		return false;
	}

	return true;
}

void Queue::run() {
	while (mRun) {
		mSubscriber.receiveEvent();
		this->handleEvent();

		if (interruptOccured) {
			break;
		}
	}
}

void Queue::updateEvents() {
	if (mEventSet.back().getRepeat() != 0) {
		int timestamp = mCurrentSimTime + mEventSet.back().getPeriod();
		mEventSet.back().setTimestamp(timestamp);

		if (mEventSet.back().getRepeat() != -1) {
			mEventSet.back().setRepeat(mEventSet.back().getRepeat() - 1);
		}

		mScheduler.scheduleEvents(mEventSet);

	} else {
		mEventSet.pop_back();
	}
}

void Queue::handleEvent() {
	auto eventBuffer = mSubscriber.getEventBuffer();

	auto receivedEvent = event::GetEvent(eventBuffer);
	std::string eventName = receivedEvent->name()->str();
	mCurrentSimTime = receivedEvent->timestamp();
	mRun = !foundCriticalSimCycle(mCurrentSimTime);

	if (receivedEvent->event_data() != nullptr) {
		auto dataRef = receivedEvent->event_data_flexbuffer_root();

		if (dataRef.IsString()) {
			std::string configPath =
					receivedEvent->event_data_flexbuffer_root().AsString().str();

			if (eventName == "SaveState") {
				saveState(
						std::string(configPath.begin(), configPath.end())
								+ mName + ".config");
			}

			else if (eventName == "LoadState") {
				loadState(
						std::string(configPath.begin(), configPath.end())
								+ mName + ".config");
			}
		}
	}

	else if (eventName == "SimTimeChanged") {
		// Send new Flit every clock cycle
		if (!mEventSet.empty()) {
			auto nextEvent = mEventSet.back();

			if (mCurrentSimTime >= nextEvent.getTimestamp()) {
				nextEvent.setCurrentSimTime(mCurrentSimTime);
				mEventOffset = event::CreateEvent(mFbb,
						mFbb.CreateString(nextEvent.getName()),
						nextEvent.getTimestamp());
				mFbb.Finish(mEventOffset);
				mPublisher.publishEvent(nextEvent.getName(),
						mFbb.GetBufferPointer(), mFbb.GetSize());
				this->updateEvents();
			}
		}
	}

	else if (eventName == "End") {
		mRun = false;
	}
}

void Queue::saveState(std::string filePath) {
	// Store states
	std::ofstream ofs(filePath);
	boost::archive::xml_oarchive oa(ofs, boost::archive::no_header);

	try {
		oa << boost::serialization::make_nvp("EventSet", *this);

	} catch (boost::archive::archive_exception& ex) {
		std::cout << mName << ": Archive Exception during serializing: "
				<< std::endl;
		throw ex.what();
	}

	mRun = mSubscriber.synchronizeSub();
}

void Queue::loadState(std::string filePath) {
	// Restore states
	std::ifstream ifs(filePath);
	boost::archive::xml_iarchive ia(ifs, boost::archive::no_header);

	try {
		ia >> boost::serialization::make_nvp("EventSet", *this);

	} catch (boost::archive::archive_exception& ex) {
		std::cout << mName << ": Archive Exception during deserializing:"
				<< std::endl;
		throw ex.what();
	}

	mScheduler.scheduleEvents(mEventSet);

	init();

	mRun = mSubscriber.synchronizeSub();
}
