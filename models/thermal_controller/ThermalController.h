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

#ifndef THERMAL_CONTROL_THERMAL_CONTROLLER_H_
#define THERMAL_CONTROL_THERMAL_CONTROLLER_H_

#include <fstream>
#include <boost/serialization/serialization.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <zmq.hpp>

#include "resources/idl/event_generated.h"
#include "communication/zhelpers.hpp"
#include "communication/Subscriber.h"
#include "communication/Publisher.h"
#include "communication/Dealer.h"
#include "interfaces/IModel.h"
#include "interfaces/IPersist.h"
#include "data-types/Field.h"

class ThermalController: public virtual IModel, public virtual IPersist
{
public:
	ThermalController(std::string name, std::string description);
	virtual ~ThermalController() = default;

	enum State
	{
		OFF = 0, COOLING = 1, HEATING = 2
	};

	// The maximum temperature that is reached if we are HEATING long enough.
	static constexpr float maxTemperature = 10.0f;

	// The minimum temperature that is reached if we are COOLING long enough.
	static constexpr float minTemperature = -20.0f;

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
	virtual void saveState(std::string filename) override;
	virtual void loadState(std::string filename) override;

private:
	// IModel
	std::string mName;
	std::string mDescription;

	// Subscriber
	void handleEvent();
	zmq::context_t mCtx;
	Subscriber mSubscriber;
	Publisher mPublisher;
	Dealer mDealer;

	bool mRun;
	int mCurrentSimTime;
	ThermalController::State mCurrentState = OFF;

	friend class boost::serialization::access;
	template<typename Archive>
	void serialize(Archive& archive, const unsigned int)
	{
	}
};

#endif
