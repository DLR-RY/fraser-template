/*
 * Queue.cpp
 *
 *  Created on: Jan 3, 2017
 *      Author: Annika Ofenloch
 */

#include "Queue.h"

#include <iostream>

static const char BREAKPNTS_PATH[] = "models/event_queue_1/savepoints/";
static const char FILE_EXTENTION[] = "_savefile_queue.xml";

Queue::Queue(std::string name, std::string description) :
		mName(name), mDescription(description), mCtx(1), mSubscriber(mCtx), mPublisher(
				mCtx), mDealer(mCtx, mName), mReceivedEvent(NULL), mCurrentSimTime(
				-1) {

	registerInterruptSignal();

	this->setDefaultEvents();
	mRun = this->prepare();
}

Queue::~Queue() {

}

void Queue::configure(std::string configFile) {
	this->loadState(configFile);
}

void Queue::setDefaultEvents() {
	// For the performance test
	mEventSet.push_back(Event("FirstEvent", 0));

	mScheduler.scheduleEvents(mEventSet);
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

	mSubscriber.subscribeTo("SimTimeChanged");
	mSubscriber.subscribeTo("End");
	mSubscriber.subscribeTo("Restore");
	mSubscriber.subscribeTo("Store");
	mSubscriber.subscribeTo("CreateDefaultConfigFiles");
	mSubscriber.subscribeTo("Configure");

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
		mEventSet.back().set_timestamp(timestamp);

		if (mEventSet.back().getRepeat() != -1) {
			mEventSet.back().setRepeat(mEventSet.back().getRepeat() - 1);
		}

		mScheduler.scheduleEvents(mEventSet);

	} else {
		mEventSet.pop_back();
	}
}

void Queue::handleEvent() {
	mReceivedEvent = event::GetEvent(mSubscriber.getEventBuffer());
	mEventName = mSubscriber.getEventName();
	mCurrentSimTime = mReceivedEvent->timestamp();
	mData = mReceivedEvent->data_as_String()->str();

	if (mEventName == "SimTimeChanged") {

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

	else if (mEventName == "CreateDefaultConfigFiles") {
		this->saveState(mData + mName + ".config");
	}

	else if (mEventName == "Configure") {
		this->loadState(mData + mName + ".config");
	}

	else if (mEventName == "Store" || mEventName == "Restore") {
		std::string filePath = BREAKPNTS_PATH
				+ std::to_string(mCurrentSimTime) + FILE_EXTENTION;

		if (mEventName == "Store") {
			std::cout << "Store events from Queue" << std::endl;
			this->saveState(filePath);

		} else {
			std::cout << "Restore events from Queue" << std::endl;
			this->loadState(filePath);
		}
	}

	else if (mEventName == "End") {
		std::cout << "Queue: End-Event" << std::endl;
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

	mRun = mSubscriber.synchronizeSub();
}
