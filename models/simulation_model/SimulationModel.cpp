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

#include "SimulationModel.h"

#include <iostream>

SimulationModel::SimulationModel(std::string name, std::string description) :
		mName(name), mDescription(description), mCtx(1), mPublisher(mCtx), mDealer(
				mCtx, mName), mSimTime("SimTime", 5000), mSimTimeStep(
				"SimTimeStep", 100), mCurrentSimTime("CurrentSimTime", 0), mCycleTime(
				"CylceTime", 0), mSpeedFactor("SpeedFactor", 1.0) {

	registerInterruptSignal();

	mRun = prepare();
}

SimulationModel::~SimulationModel() {
	this->stopSim();
}

void SimulationModel::init() {
	mCycleTime.setValue(
			double(mSimTimeStep.getValue()) / mSpeedFactor.getValue()); // Wait-time between the cycles in milliseconds
}

bool SimulationModel::prepare() {

	mTotalNumOfModels = mDealer.getTotalNumberOfModels();
	mNumOfPersistModels = mDealer.getNumberOfPersistModels();

	if (!mPublisher.bindSocket(mDealer.getPortNumFrom(mName))) {
		return false;
	}

	// Synchronization
	if (!mPublisher.preparePubSynchronization(
			mDealer.getSynchronizationPort())) {
		return false;
	}

	// (mTotalNumOfModels - 2), because the simulation and configuration models should not be included
	std::cout
			<< "Synchronize simulation model with the other models (after preparation phase)."
			<< std::endl;
	if (!mPublisher.synchronizePub(mTotalNumOfModels - 2,
			mCurrentSimTime.getValue())) {
		return false;
	}

	return true;
}

void SimulationModel::run() {
	uint64_t currentSimTime = getCurrentSimTime();
	if (mRun) {
		while (currentSimTime <= mSimTime.getValue()) {
			if (!mPause) {

				for (auto savepoint : getSavepoints()) {
					if (currentSimTime == savepoint) {
						std::string filePath = "../savepoints/savepnt_"
								+ std::to_string(savepoint) + "/" + mName
								+ ".config";

						this->saveState(filePath);
						break;
					}
				}

				// Event Data Serialization
				flexbuffers::Builder logInfo;
				std::string logMsg = "Simulation Time: "
						+ std::to_string(currentSimTime);
				logInfo.Add(logMsg);
				logInfo.Finish();
				auto logData = mFbb.CreateVector(logInfo.GetBuffer());
				mFbb.Finish(
						event::CreateEvent(mFbb, mFbb.CreateString("LogInfo"),
								mCurrentSimTime.getValue(),
								event::Priority_NORMAL_PRIORITY, 0, 0,
								logData));
				mPublisher.publishEvent("Logger", mFbb.GetBufferPointer(),
						mFbb.GetSize());

				mEventOffset = event::CreateEvent(mFbb,
						mFbb.CreateString("SimTimeChanged"), currentSimTime);
				mFbb.Finish(mEventOffset);
				mPublisher.publishEvent("SimTimeChanged",
						mFbb.GetBufferPointer(), mFbb.GetSize());

				std::this_thread::sleep_for(
						std::chrono::milliseconds(mCycleTime.getValue()));

				currentSimTime += mSimTimeStep.getValue();
				mCurrentSimTime.setValue(currentSimTime);
			}

			if (interruptOccured) {
				break;
			}
		}
	}

	this->stopSim();
}

void SimulationModel::stopSim() {
	// Stop all running models and the dns server
	mEventOffset = event::CreateEvent(mFbb, mFbb.CreateString("End"),
			mCurrentSimTime.getValue());
	mFbb.Finish(mEventOffset);

	mPublisher.publishEvent("End", mFbb.GetBufferPointer(), mFbb.GetSize());

	mDealer.stopDNSserver();
}

void SimulationModel::loadState(std::string filePath) {
	std::cout << mName << " ... Load State" << std::endl;
	this->pauseSim();

	// Restore states
	std::ifstream ifs(filePath + mName + ".config");
	boost::archive::xml_iarchive ia(ifs, boost::archive::no_header);
	try {
		ia >> boost::serialization::make_nvp("FieldSet", *this);

	} catch (boost::archive::archive_exception& ex) {
		std::cout << mName << ": Archive Exception during deserializing: "
				<< std::endl;
		std::cout << ex.what() << std::endl;
	}

	// Event Data Serialization
	flexbuffers::Builder flexbuild;
	flexbuild.Add(filePath);
	flexbuild.Finish();
	auto data = mFbb.CreateVector(flexbuild.GetBuffer());

	mEventOffset = event::CreateEvent(mFbb, mFbb.CreateString("LoadState"),
			mCurrentSimTime.getValue(), event::Priority_NORMAL_PRIORITY, 0, 0,
			data);
	mFbb.Finish(mEventOffset);

	mPublisher.publishEvent("LoadState", mFbb.GetBufferPointer(),
			mFbb.GetSize());

	this->init();

	// Synchronization is necessary, because the simulation
	// has to wait until the other models finished their Restore-method
	// (mNumOfPersistModels - 1), because the simulation model itself should not be included
	std::cout
			<< "Synchronize simulation model with the other models (after initialization phase)."
			<< std::endl;
	mRun = mPublisher.synchronizePub(mNumOfPersistModels - 1,
			mCurrentSimTime.getValue());

	this->continueSim();
}

void SimulationModel::saveState(std::string filePath) {
	std::cout << mName << " ... Save State" << std::endl;
	this->pauseSim();

	// Event Data Serialization
	flexbuffers::Builder flexbuild;
	flexbuild.Add(filePath);
	flexbuild.Finish();
	auto data = mFbb.CreateVector(flexbuild.GetBuffer());

	mEventOffset = event::CreateEvent(mFbb, mFbb.CreateString("SaveState"),
			mCurrentSimTime.getValue(), event::Priority_NORMAL_PRIORITY, 0, 0,
			data);
	mFbb.Finish(mEventOffset);
	mPublisher.publishEvent("SaveState", mFbb.GetBufferPointer(),
			mFbb.GetSize());

	// Store states
	std::ofstream ofs(filePath + mName + ".config");
	boost::archive::xml_oarchive oa(ofs, boost::archive::no_header);

	try {
		oa << boost::serialization::make_nvp("FieldSet", *this);

	} catch (boost::archive::archive_exception& ex) {
		std::cout << mName << ": Archive Exception during serializing:"
				<< std::endl;
		std::cout << ex.what() << std::endl;
	}

	// Synchronization is necessary, because the simulation
	// has to wait until the other models finished their Store-method
	// (mNumOfPersistModels - 1), because the simulation model itself should not be included
	std::cout
			<< "Synchronize simulation model with the other models (after save state phase)."
			<< std::endl;
	mRun = mPublisher.synchronizePub(mNumOfPersistModels - 1,
			mCurrentSimTime.getValue());

	if (mConfigMode) {
		std::cout << "Default configuration files were created" << std::endl;
		this->stopSim();
	} else {
		this->continueSim();
	}
}
