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
	DaqDeviceHandle handle = 0;
	UlError error = ERR_NO_ERROR;
	bool deviceStatus = true;

	// Get descriptors for all of the available DAQ devices
	error = ulGetDaqDeviceInventory(USB_IFC, devDescriptors, &numDevs);

	// verify at least one DAQ device is detected
	if (numDevs)
	{
		mPublisher.publishEvent("LogInfo", mCurrentSimTime,
				"DAQ device detected: "
						+ std::string(devDescriptors[0].productName) + " ("
						+ std::string(devDescriptors[0].uniqueId) + ")");

		// get a handle to the DAQ device associated with the first descriptor
		handle = ulCreateDaqDevice(devDescriptors[0]);

		// check if the DAQ device handle is valid
		if (handle)
		{

			// establish a connection to the DAQ device
			error = ulConnectDaqDevice(handle);

			if (error == ERR_NO_ERROR)
			{
				error = ulDConfigPort(handle, FIRSTPORTA, DD_OUTPUT);
				error = ulDConfigPort(handle, SECONDPORTA, DD_OUTPUT);
				error = ulDConfigPort(handle, THIRDPORTA, DD_OUTPUT);
				error = ulDConfigPort(handle, FOURTHPORTA, DD_OUTPUT);

				error = ulDConfigPort(handle, FIRSTPORTB, DD_INPUT);
				error = ulDConfigPort(handle, SECONDPORTB, DD_INPUT);
				error = ulDConfigPort(handle, THIRDPORTB, DD_INPUT);
				error = ulDConfigPort(handle, FOURTHPORTB, DD_INPUT);
			}
		}
	} else
	{
		// verify at least one DAQ device is detected
		mPublisher.publishEvent("LogError", mCurrentSimTime,
				"No DAQ device is detected");

		deviceStatus = false;
	}

	if (error != ERR_NO_ERROR)
	{
		char errMsg[ERR_MSG_LEN];
		ulGetErrMsg(error, errMsg);

		mPublisher.publishEvent("LogError", mCurrentSimTime,
				"UL DAQ Error Message: " + std::string(errMsg) + "(Error Code "
						+ std::to_string(error) + ")");

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

	mSubscriber.subscribeTo("LoadState");
	mSubscriber.subscribeTo("SaveState");
	mSubscriber.subscribeTo("End");

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

	// Log
	mPublisher.publishEvent("LogInfo", mCurrentSimTime,
			mName + " received " + eventName);

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
