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

#ifndef EVENT_QUEUE_1_QUEUE_H_
#define EVENT_QUEUE_1_QUEUE_H_

#include <fstream>
#include <functional>
#include <boost/serialization/vector.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <zmq.hpp>

#include "communication/Dealer.h"
#include "communication/Publisher.h"
#include "communication/Subscriber.h"
#include "communication/zhelpers.hpp"
#include "interfaces/IModel.h"
#include "interfaces/IPersist.h"
#include "interfaces/IQueue.h"
#include "scheduler/Scheduler.h"
#include "data-types/Event.h"

#include "resources/idl/event_generated.h"

class Queue: public virtual IModel,
		public virtual IPersist,
		public virtual IQueue {

public:
	Queue(std::string name, std::string description);
	virtual ~Queue() = default;

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

	// IPersist
	virtual void saveState(std::string filePath) override;
	virtual void loadState(std::string filePath) override;

private:
	void handleEvent();

	// IQueue
	virtual void updateEvents() override;

	std::vector<Event> mEventSet;

	std::string mName;
	std::string mDescription;

	friend class boost::serialization::access;
	template<typename Archive>
	void serialize(Archive& archive, const unsigned int) {
		for (auto event : mEventSet) {
			archive & boost::serialization::make_nvp("Event", event);
		}
	}

	// Subscriber & Publisher
	zmq::context_t mCtx;
	Subscriber mSubscriber;
	Publisher mPublisher;
	Dealer mDealer;

	bool mRun;
	const event::Event* mReceivedEvent;
	std::string mEventName;
	std::string mData;

	Scheduler mScheduler;
	uint64_t mCurrentSimTime;

	// Serialization
	flatbuffers::FlatBufferBuilder mFbb;
	flatbuffers::Offset<event::Event> mEventOffset;
};

#endif /* EVENT_QUEUE_1_QUEUE_H_ */
