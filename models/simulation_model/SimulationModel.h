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

#ifndef SIMULATION_MODEL_SIMULATIONMODEL_H_
#define SIMULATION_MODEL_SIMULATIONMODEL_H_

#include <thread>
#include <string>
#include <fstream>
#include <chrono>
#include <zmq.hpp>
#include <boost/thread.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/filesystem.hpp>

#include "data-types/SavepointSet.h"
#include "interfaces/IModel.h"
#include "interfaces/IPersist.h"
#include "communication/Publisher.h"
#include "communication/Dealer.h"
#include "data-types/Field.h"
#include "communication/zhelpers.hpp"

#include "resources/idl/event_generated.h"

class SimulationModel: public virtual IModel, public virtual IPersist
{
public:
	SimulationModel(std::string name, std::string description);

	virtual ~SimulationModel();

	// IModel
	virtual void init() override;
	virtual bool prepare() override;
	virtual void run() override;
	virtual std::string getName() const override
	{
		return mName;
	}
	virtual std::string getDescription() const override
	{
		return mDescription;
	}

	// IPersist
	virtual void saveState(std::string configPath) override;
	virtual void loadState(std::string configPath) override;

	void pauseSim()
	{
		mPause = true;
	}

	void continueSim()
	{
		mPause = false;
	}

	void stopSim();

	void setConfigMode(bool status)
	{
		mConfigMode = status;
	}

	// Properties
	int getCurrentSimTime()
	{
		return mCurrentSimTime.getValue();
	}

	/** Set a breakpoint (given in simulation time, e.g. 200 time units).
	 * If the simulation reaches a breakpoint (e.g. after 200 time units),
	 * the store method of all subscribed models and of the simulation-model itself is called. **/
	void setSavepoint(uint64_t time)
	{
		mSavepoints.push_back(time);
	}

	/** Get all breakpoint which were defined. **/
	std::vector<uint64_t> getSavepoints()
	{
		return mSavepoints;
	}

private:
	// IModel
	std::string mName;
	std::string mDescription;

	// For the communication
	zmq::context_t mCtx;  // ZMQ-instance
	Publisher mPublisher; // ZMQ-PUB
	Dealer mDealer;		  // ZMQ-DEALER

	SavepointSet mSavepoints;
	bool mRun = true;
	bool mPause = false;
	bool mConfigMode = false;
	bool mLoadConfigFile = false;

	uint64_t mTotalNumOfModels = 0;
	uint64_t mNumOfPersistModels = 0;

	friend class boost::serialization::access;
	template<typename Archive>
	void serialize(Archive& archive, const unsigned int)
	{
		archive & boost::serialization::make_nvp("IntField", mSimTime);
		archive & boost::serialization::make_nvp("IntField", mSimTimeStep);
		archive & boost::serialization::make_nvp("IntField", mCurrentSimTime);
		archive & boost::serialization::make_nvp("DoubleField", mSpeedFactor);
		archive & boost::serialization::make_nvp("SavepointSet", mSavepoints);
	}

	// Fields
	Field<uint64_t> mSimTime;
	Field<uint32_t> mSimTimeStep;
	Field<uint64_t> mCurrentSimTime;
	Field<uint32_t> mCycleTime;
	Field<double> mSpeedFactor;

};

#endif /* SIMULATION_MODEL_SIMULATIONMODEL_H_ */
