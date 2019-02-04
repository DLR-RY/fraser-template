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

	// Prepare Synchronization
	if (!mPublisher.preparePubSynchronization(
			mDealer.getSynchronizationPort())) {
		return false;
	}

	// (mTotalNumOfModels - 2), because the simulation and configuration models should not be included
	if (!mPublisher.synchronizePub(mTotalNumOfModels - 2,
			mCurrentSimTime.getValue())) {
		return false;
	}

	mPublisher.publishEvent("LogInfo", 0,
			"Synchronized simulation model with the other models (after preparation phase)");

	return true;
}

void SimulationModel::run() {
	uint64_t currentSimTime = getCurrentSimTime();
	if (mRun) {
		while (currentSimTime <= mSimTime.getValue()) {
			if (!mPause) {

				// TODO: Test to set savepoints
//				for (auto savepoint : getSavepoints()) {
//					if (currentSimTime == savepoint) {
//						std::string filePath = "../savepoints/savepnt_"
//								+ std::to_string(savepoint) + "/" + mName
//								+ ".config";
//
//						this->saveState(filePath);
//						break;
//					}
//				}
				// Log
				mPublisher.publishEvent("LogInfo", currentSimTime,
						"Simulation Time: " + std::to_string(currentSimTime));

				// Publish current simulation time
				mPublisher.publishEvent("SimTimeChanged", currentSimTime);

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
	mPublisher.publishEvent("End", mCurrentSimTime.getValue());
	// Wait that all models are terminated before terminating the logger
	sleep(1);
	mPublisher.publishEvent("EndLogger", mCurrentSimTime.getValue());

	mDealer.stopDNSserver();
}

void SimulationModel::loadState(std::string filePath) {
	this->pauseSim();

	// Restore states
	std::ifstream ifs(filePath + mName + ".config");
	boost::archive::xml_iarchive ia(ifs, boost::archive::no_header);
	try {
		ia >> boost::serialization::make_nvp("FieldSet", *this);

	} catch (boost::archive::archive_exception& ex) {
		throw ex.what();
		// Log
		mPublisher.publishEvent("LogError", 0,
				mName + ": Archive Exception during deserializing");
	}
	// Event Data Serialization
	mPublisher.publishEvent("LoadState", mCurrentSimTime.getValue(), filePath);

	init();

	// Synchronization is necessary, because the simulation
	// has to wait until the other models finished their Restore-method
	// (mNumOfPersistModels - 1), because the simulation model itself should not be included
	mRun = mPublisher.synchronizePub(mNumOfPersistModels - 1,
			mCurrentSimTime.getValue());

	mPublisher.publishEvent("LogInfo", 0,
			"Synchronized simulation model with the other models (after initialization phase)");

	this->continueSim();
}

void SimulationModel::saveState(std::string filePath) {
	this->pauseSim();

	// Event Data Serialization
	mPublisher.publishEvent("SaveState", mCurrentSimTime.getValue(), filePath);

	// Store states
	std::ofstream ofs(filePath + mName + ".config");
	boost::archive::xml_oarchive oa(ofs, boost::archive::no_header);

	try {
		oa << boost::serialization::make_nvp("FieldSet", *this);

	} catch (boost::archive::archive_exception& ex) {
		throw ex.what();
		// Log
		mPublisher.publishEvent("LogError", 0,
				mName + ": Archive Exception during serializing");
	}

	// Synchronization is necessary, because the simulation
	// has to wait until the other models finished their Store-method
	// (mNumOfPersistModels - 1), because the simulation model itself should not be included
	mRun = mPublisher.synchronizePub(mNumOfPersistModels - 1,
			mCurrentSimTime.getValue());

	mPublisher.publishEvent("LogInfo", 0,
			"Synchronized simulation model with the other models (after save state phase)");

	if (mConfigMode) {
		mPublisher.publishEvent("LogInfo", 0,
				"Default configuration files were created");

		this->stopSim();
	} else {
		this->continueSim();
	}
}
