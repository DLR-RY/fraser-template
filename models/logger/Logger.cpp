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
#include "Logger.h"

namespace logging = boost::log;
namespace keywords = boost::log::keywords;

Logger::Logger(std::string name, std::string description) :
		mName(name), mDescription(description), mCtx(1), mSubscriber(mCtx), mPublisher(
				mCtx), mDealer(mCtx, mName), mCurrentSimTime(0) {

	mRun = prepare();
	init();
}

void Logger::init() {
	logging::register_simple_formatter_factory<logging::trivial::severity_level,
			char>("Severity");

	logging::add_file_log(keywords::file_name = "sample.log", keywords::format =
			"[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%");

	logging::core::get()->set_filter(
			logging::trivial::severity >= logging::trivial::info);

	logging::add_common_attributes();
}

bool Logger::prepare() {
	mSubscriber.setOwnershipName(mName);

	if (!mPublisher.bindSocket(mDealer.getPortNumFrom(mName))) {
		return false;
	}

	if (!mSubscriber.connectToPub(mDealer.getIPFrom("simulation_model"),
			mDealer.getPortNumFrom("simulation_model"))) {
		return false;
	}

	for (auto depModel : mDealer.getModelDependencies()) {
		if (!mSubscriber.connectToPub(mDealer.getIPFrom(depModel),
				mDealer.getPortNumFrom(depModel))) {
			return false;
		}
	}

	mSubscriber.subscribeTo("Logger");
	mSubscriber.subscribeTo("End");

	// Synchronization
	if (!mSubscriber.prepareSubSynchronization(
			mDealer.getIPFrom("simulation_model"),
			mDealer.getSynchronizationPort())) {
		return false;
	}

	if (!mSubscriber.synchronizeSub()) {
		return false;
	}

	return true;
}

void Logger::run() {
	while (mRun) {
		if (mSubscriber.receiveEvent()) {
			handleEvent();
		}
	}
}

void Logger::handleEvent() {
	auto eventBuffer = mSubscriber.getEventBuffer();

	auto receivedEvent = event::GetEvent(eventBuffer);
	std::string eventName = receivedEvent->name()->str();
	mCurrentSimTime = receivedEvent->timestamp();
	mRun = !foundCriticalSimCycle(mCurrentSimTime);

	if (receivedEvent->event_data() != nullptr) {
		auto dataRef = receivedEvent->event_data_flexbuffer_root();

		if (dataRef.IsString()) {
			auto logMsg =
					receivedEvent->event_data_flexbuffer_root().AsString().str();



			if (eventName == "LogTrace") {
				BOOST_LOG_TRIVIAL(trace)<< logMsg.data();

			} else if (eventName == "LogDebug") {
				BOOST_LOG_TRIVIAL(debug) << logMsg.data();

			} else if (eventName == "LogInfo") {
				BOOST_LOG_TRIVIAL(info) << logMsg.data();

			} else if (eventName == "LogWarning") {
				BOOST_LOG_TRIVIAL(warning) << logMsg.data();

			} else if (eventName == "LogError") {
				BOOST_LOG_TRIVIAL(error) << logMsg.data();

			} else if (eventName == "LogFatal") {
				BOOST_LOG_TRIVIAL(fatal) << logMsg.data();

			}
			std::cin.get();
		}
	} else if (eventName == "End") {
		mRun = false;
	}
}
