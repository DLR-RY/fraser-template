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

#ifndef DIGITAL_MCC_ADAPTER_H_
#define DIGITAL_MCC_ADAPTER_H_

#include <fstream>
#include <boost/serialization/serialization.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <zmq.hpp>
#include <uldaq.h>

#include "communication/zhelpers.hpp"
#include "communication/Subscriber.h"
#include "communication/Publisher.h"
#include "communication/Dealer.h"
#include "interfaces/IModel.h"
#include "interfaces/IPersist.h"
#include "data-types/Field.h"


#include "resources/idl/event_generated.h"

class DigitalMCCAdapter: public virtual IModel, public virtual IPersist
{
public:
	DigitalMCCAdapter(std::string name, std::string description);
	virtual ~DigitalMCCAdapter() = default;

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
	void saveState(std::string filename);
	void loadState(std::string filename);

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

	bool prepareDaqDevice();

	friend class boost::serialization::access;
	template<typename Archive>
	void serialize(Archive& archive, const unsigned int)
	{

	}
};

#endif
