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

#include <iostream>

#include "DigitalMCCAdapter.h"

#define MAX_DEV_COUNT  10
#define MAX_STR_LENGTH 64

DigitalMCCAdapter::DigitalMCCAdapter(std::string name, std::string description) :
		mName(name), mDescription(description), mCtx(1), mSubscriber(mCtx), mPublisher(
				mCtx), mDealer(mCtx, mName), mCurrentSimTime(0)
{

	registerInterruptSignal();
	mRun = prepare();
}

void DigitalMCCAdapter::init()
{
	// Initialize model parameters
}

bool DigitalMCCAdapter::prepareDaqDevice()
{
	unsigned int numDevs = MAX_DEV_COUNT;
	DaqDeviceDescriptor devDescriptors[MAX_DEV_COUNT];
	mError = ERR_NO_ERROR;
	bool deviceStatus = true;

	// Get descriptors for all of the available DAQ devices
	mError = ulGetDaqDeviceInventory(USB_IFC, devDescriptors, &numDevs);

	// verify at least one DAQ device is detected
	if (numDevs)
	{
		mPublisher.publishEvent("LogInfo", mCurrentSimTime,
				"DAQ device detected: "
						+ std::string(devDescriptors[0].productName) + " ("
						+ std::string(devDescriptors[0].uniqueId) + ")");

		// get a handle to the DAQ device associated with the first descriptor
		mDaqDeviceHandler = ulCreateDaqDevice(devDescriptors[0]);

		// check if the DAQ device handle is valid
		if (mDaqDeviceHandler)
		{

			// establish a connection to the DAQ device
			mError = ulConnectDaqDevice(mDaqDeviceHandler);

			if (mError == ERR_NO_ERROR)
			{
				mError = ulDConfigPort(mDaqDeviceHandler, FIRSTPORTA,
						DD_OUTPUT);
				mError = ulDConfigPort(mDaqDeviceHandler, SECONDPORTA,
						DD_OUTPUT);
				mError = ulDConfigPort(mDaqDeviceHandler, THIRDPORTA,
						DD_OUTPUT);
				mError = ulDConfigPort(mDaqDeviceHandler, FOURTHPORTA,
						DD_OUTPUT);

				mError = ulDConfigPort(mDaqDeviceHandler, FIRSTPORTB, DD_INPUT);
				mError = ulDConfigPort(mDaqDeviceHandler, SECONDPORTB,
						DD_INPUT);
				mError = ulDConfigPort(mDaqDeviceHandler, THIRDPORTB, DD_INPUT);
				mError = ulDConfigPort(mDaqDeviceHandler, FOURTHPORTB,
						DD_INPUT);
			}
		}
	} else
	{
		// verify at least one DAQ device is detected
		mPublisher.publishEvent("LogError", mCurrentSimTime,
				"No DAQ device is detected");

		deviceStatus = false;
	}

	if (mError != ERR_NO_ERROR)
	{
		char errMsg[ERR_MSG_LEN];
		ulGetErrMsg(mError, errMsg);

		mPublisher.publishEvent("LogError", mCurrentSimTime,
				"UL DAQ Error Message: " + std::string(errMsg) + "(Error Code "
						+ std::to_string(mError) + ")");

		deviceStatus = false;
	}

	return deviceStatus;
}

bool DigitalMCCAdapter::prepare()
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
	mSubscriber.subscribeTo("LoadState");
	mSubscriber.subscribeTo("SaveState");
	mSubscriber.subscribeTo("End");

	// TODO: subscribe to port group messages, e.g.:
	// mSubscriber.subscribeTo("FirstPort");
	// mSubscriber.subscribeTo("SecondPort");
	// mSubscriber.subscribeTo("ThirdPort");
	// mSubscriber.subscribeTo("FourthPort");

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

void DigitalMCCAdapter::run()
{
	while (mRun)
	{
		if (mSubscriber.receiveEvent())
		{
			handleEvent();
		}
	}
}

void DigitalMCCAdapter::handleEvent()
{
	auto eventBuffer = mSubscriber.getEventBuffer();

	auto receivedEvent = event::GetEvent(eventBuffer);
	std::string eventName = receivedEvent->name()->str();
	mCurrentSimTime = receivedEvent->timestamp();
	mRun = !foundCriticalSimCycle(mCurrentSimTime);

	mPublisher.publishEvent("LogInfo", mCurrentSimTime,
			mName + " received " + eventName);

	// Do something every simulation cycle
	if (eventName == "SimTimeChanged")
	{
		// Returns the value read from a digital port.
		mError = ulDIn(mDaqDeviceHandler, FIRSTPORTB, &mFirstPort);
		mError = ulDIn(mDaqDeviceHandler, SECONDPORTB, &mSecondPort);
		mError = ulDIn(mDaqDeviceHandler, THIRDPORTB, &mThirdPort);
		mError = ulDIn(mDaqDeviceHandler, FOURTHPORTB, &mFourthPort);

		// Log port data as Info
		mPublisher.publishEvent("LogInfo", mCurrentSimTime,
				"FirstPort: " + std::to_string(mFirstPort) + ", SecondPort: "
						+ std::to_string(mSecondPort) + ", ThirdPort: "
						+ std::to_string(mThirdPort) + ", FourthPort: "
						+ std::to_string(mFourthPort));

		// TODO: Then do something with the data

		// TODO: Writes the specified value to a digital output port.
		mError = ulDOut(mDaqDeviceHandler, FIRSTPORTA, 32);
		mError = ulDOut(mDaqDeviceHandler, SECONDPORTA, 5);
		mError = ulDOut(mDaqDeviceHandler, THIRDPORTA, 11);
		mError = ulDOut(mDaqDeviceHandler, FOURTHPORTA, 98);

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

void DigitalMCCAdapter::saveState(std::string filePath)
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

void DigitalMCCAdapter::loadState(std::string filePath)
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

	if (prepareDaqDevice())
	{
		mRun = mSubscriber.synchronizeSub();
	} else
	{
		mRun = false;
	}
}
