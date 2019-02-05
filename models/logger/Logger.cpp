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
#include "ColorFormatter.h"

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;

Logger::Logger(std::string name, std::string description,
		std::string logFilePath) :
		mName(name), mDescription(description), mCtx(1), mSubscriber(mCtx), mDealer(
				mCtx, mName), mCurrentSimTime(0), mDebugMode("DebugMode",
				false), mLogFilesPath(logFilePath) {

	mRun = prepare();

	logging::register_simple_formatter_factory<logging::trivial::severity_level,
			char>("Severity");

	logging::add_file_log(
			keywords::file_name = mLogFilesPath + "%Y-%m-%d_%H-%M-%S.log",
			keywords::format = "[%TimeStamp%] [%Severity%] %Message%",
			keywords::auto_flush = true);

	logging::add_common_attributes();
}

void Logger::init() {
	if (mDebugMode.getValue()) {
		typedef sinks::synchronous_sink<sinks::text_ostream_backend> text_sink;
		boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();

		// We have to provide an empty deleter to avoid destroying the global stream object
		boost::shared_ptr<std::ostream> stream(&std::cout,
				boost::null_deleter());
		sink->locked_backend()->add_stream(stream);

		sink->set_formatter(&coloringFormatter);

		logging::core::get()->add_sink(sink);
	}
}

bool Logger::prepare() {
	mSubscriber.setOwnershipName(mName);

	// Connect to all models but not to itself
	for (auto depModel : mDealer.getAllModelNames()) {
		if (depModel != mName) {
			if (!mSubscriber.connectToPub(mDealer.getIPFrom(depModel),
					mDealer.getPortNumFrom(depModel))) {
				return false;
			}
		}
	}

	mSubscriber.subscribeTo("LoadState");
	mSubscriber.subscribeTo("SaveState");
	mSubscriber.subscribeTo("LogTrace");
	mSubscriber.subscribeTo("LogDebug");
	mSubscriber.subscribeTo("LogInfo");
	mSubscriber.subscribeTo("LogWarning");
	mSubscriber.subscribeTo("LogError");
	mSubscriber.subscribeTo("LogFatal");
	mSubscriber.subscribeTo("EndLogger");

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
			auto dataString = dataRef.ToString();

			if (eventName == "SaveState") {
				saveState(dataString + mName + ".config");
			} else if (eventName == "LoadState") {
				loadState(dataString + mName + ".config");
			} else {

				if (eventName == "LogTrace") {
					BOOST_LOG_TRIVIAL(trace)<< dataString.data();

				} else if (eventName == "LogDebug") {
					BOOST_LOG_TRIVIAL(debug) << dataString.data();

				} else if (eventName == "LogInfo") {
					BOOST_LOG_TRIVIAL(info) << dataString.data();

				} else if (eventName == "LogWarning") {
					BOOST_LOG_TRIVIAL(warning) << dataString.data();

				} else if (eventName == "LogError") {
					BOOST_LOG_TRIVIAL(error) << dataString.data();

				} else if (eventName == "LogFatal") {
					BOOST_LOG_TRIVIAL(fatal) << dataString.data();

				}
			}
		}
	} else if (eventName == "EndLogger") {
		mRun = false;
	}
}

void Logger::saveState(std::string filePath) {
	// Store states
	std::ofstream ofs(filePath);
	boost::archive::xml_oarchive oa(ofs, boost::archive::no_header);
	try {
		oa << boost::serialization::make_nvp("FieldSet", *this);

	} catch (boost::archive::archive_exception& ex) {
		std::cerr << mName << "Archive exception during serialization"
				<< std::endl;

		throw ex.what();
	}

	mRun = mSubscriber.synchronizeSub();
}

void Logger::loadState(std::string filePath) {
	// Restore states
	std::ifstream ifs(filePath);
	boost::archive::xml_iarchive ia(ifs, boost::archive::no_header);
	try {
		ia >> boost::serialization::make_nvp("FieldSet", *this);

	} catch (boost::archive::archive_exception& ex) {
		std::cerr << mName << "Archive exception during deserialization"
				<< std::endl;
		throw ex.what();
	}

	init();
	mRun = mSubscriber.synchronizeSub();
}

