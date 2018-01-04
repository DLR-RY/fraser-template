/*
 * SimulationModel.cpp
 *
 *  Created on: Dec 20, 2016
 *      Author: Annika Ofenloch
 */

#include "SimulationModel.h"

#include <iostream>

static const char BREAKPNTS_PATH[] = "../models/simulation_model/savepoints/";
static const char FILE_EXTENTION[] = "_savefile_simulation.xml";

SimulationModel::SimulationModel(std::string name, std::string description) :
		mName(name), mDescription(description), mCtx(1), mPublisher(mCtx), mDealer(
				mCtx, mName), mSimTime("SimTime", 1000), mSimTimeStep(
				"SimTimeStep", 100), mCurrentSimTime("CurrentSimTime", 0), mCycleTime(
				"CylceTime", 0), mSpeedFactor("SpeedFactor", 1.0) {

	registerInterruptSignal();

	mRun = this->prepare();
}

SimulationModel::~SimulationModel() {
}

void SimulationModel::configure(std::string configPath) {
	if (mRun) {
		mLoadConfigFile = true;
		this->loadState(configPath);

		mCycleTime.setValue(
				double(mSimTimeStep.getValue()) / mSpeedFactor.getValue()); // Wait-time between the cycles in milliseconds
	}
}

bool SimulationModel::prepare() {
//	boost::filesystem::path dir1(BREAKPNTS_PATH);
//	if (!boost::filesystem::exists(dir1)) {
//		boost::filesystem::create_directory(dir1);
//		std::cout << "Create savepoints-directory for SimulationModel" << "\n";
//	}
//
//	boost::filesystem::path dir2(CONFIG_DIR);
//	if (!boost::filesystem::exists(dir2)) {
//		boost::filesystem::create_directory(dir2);
//		std::cout << "Create config-directory for SimulationModel" << "\n";
//	}

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
	if (!mPublisher.synchronizePub(mTotalNumOfModels - 2,
			mCurrentSimTime.getValue())) {
		return false;
	}

	return true;
}

void SimulationModel::run() {
	uint64_t currentSimTime = mCurrentSimTime.getValue();
	if (mRun) {
		while (currentSimTime <= mSimTime.getValue()) {
			if (!mPause) {

				for (auto breakpoint : getBreakpoints()) {
					if (currentSimTime == breakpoint) {
						std::string filePath = BREAKPNTS_PATH
								+ std::to_string(breakpoint) + FILE_EXTENTION;

						this->saveState(filePath);
						break;
					}
				}

				std::cout << "[SIMTIME] --> " << currentSimTime << std::endl;
				mEventOffset = event::CreateEvent(mFbb,
						mFbb.CreateString("simTimeChanged"), currentSimTime);
				mFbb.Finish(mEventOffset);
				mPublisher.publishEvent("simTimeChanged",
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
	this->pauseSim();

	// Restore states
	std::ifstream ifs(filePath + mName + ".config");
	boost::archive::xml_iarchive ia(ifs, boost::archive::no_header);
	try {
		ia >> boost::serialization::make_nvp("FieldSet", *this);

	} catch (boost::archive::archive_exception& ex) {
		std::cout << "Archive Exception during deserializing:" << std::endl;
		std::cout << ex.what() << std::endl;
	}

	if (mLoadConfigFile) {
		mEventOffset = event::CreateEvent(mFbb,
				mFbb.CreateString("Configure"), mCurrentSimTime.getValue(),
				event::Priority_NORMAL_PRIORITY, 0, 0, event::EventData_String,
				mFbb.CreateString(filePath).Union());
		mFbb.Finish(mEventOffset);
		mPublisher.publishEvent("Configure", mFbb.GetBufferPointer(),
				mFbb.GetSize());
	} else {
		mEventOffset = event::CreateEvent(mFbb,
				mFbb.CreateString("Restore"), mCurrentSimTime.getValue());
		mFbb.Finish(mEventOffset);
		mPublisher.publishEvent("Restore", mFbb.GetBufferPointer(),
				mFbb.GetSize());
	}

	// Synchronization is necessary, because the simulation
	// has to wait until the other models finished their Restore-method
	// (mNumOfPersistModels - 1), because the simulation model itself should not be included
	mRun = mPublisher.synchronizePub(mNumOfPersistModels - 1,
			mCurrentSimTime.getValue());

	this->continueSim();
}

void SimulationModel::saveState(std::string filePath) {
	this->pauseSim();

	if (mConfigMode) {
		mEventOffset = event::CreateEvent(mFbb,
				mFbb.CreateString("CreateDefaultConfigFiles"),
				mCurrentSimTime.getValue(), event::Priority_NORMAL_PRIORITY, 0,
				0, event::EventData_String,
				mFbb.CreateString(filePath).Union());
		mFbb.Finish(mEventOffset);
		mPublisher.publishEvent("CreateDefaultConfigFiles",
				mFbb.GetBufferPointer(), mFbb.GetSize());
	} else {
		mEventOffset = event::CreateEvent(mFbb, mFbb.CreateString("Store"),
				mCurrentSimTime.getValue());
		mFbb.Finish(mEventOffset);
		mPublisher.publishEvent("Store", mFbb.GetBufferPointer(),
				mFbb.GetSize());
	}

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
	mRun = mPublisher.synchronizePub(mNumOfPersistModels - 1,
			mCurrentSimTime.getValue());

	if (mConfigMode) {
		this->stopSim();
	} else {
		this->continueSim();
	}
}
