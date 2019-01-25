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

#ifndef LOGGER_LOGGER_H_
#define LOGGER_LOGGER_H_

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <zmq.hpp>

#include "communication/zhelpers.hpp"
#include "communication/Subscriber.h"
#include "communication/Publisher.h"
#include "communication/Dealer.h"
#include "interfaces/IModel.h"
#include "resources/idl/event_generated.h"

class Logger: public virtual IModel {
public:
	Logger(std::string name, std::string description);
	virtual ~Logger() = default;

	// IModel
	virtual void init() override;
	virtual bool prepare() override;
	virtual void run() override;

	virtual std::string getName() const override {
		return mName;
	}
	virtual std::string getDescription() const override {
		return mDescription;
	}

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

	// Event Serialization
	flatbuffers::FlatBufferBuilder mFbb;
	flatbuffers::Offset<event::Event> mEventOffset;

	bool mRun;
	int mCurrentSimTime;
};

#endif
