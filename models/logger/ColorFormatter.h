/*
 * ColorFormatter.h
 *
 *  Created on: Feb 1, 2019
 *      Author: user
 */

#ifndef MODELS_LOGGER_COLORFORMATTER_H_
#define MODELS_LOGGER_COLORFORMATTER_H_

#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;

void coloringFormatter(logging::record_view const& record,
		logging::formatting_ostream& stream) {

	auto severity = record[logging::trivial::severity];
	assert(severity);

	stream << "\e[1m";

	switch (severity.get()) {
	case logging::trivial::severity_level::trace:
		stream << "\e[97m";
		break;
	case logging::trivial::severity_level::debug:
		stream << "\e[34m";
		break;
	case logging::trivial::severity_level::info:
		stream << "\e[32m";
		break;
	case logging::trivial::severity_level::warning:
		stream << "\e[93m";
		break;
	case logging::trivial::severity_level::error:
		stream << "\e[91m";
		break;
	case logging::trivial::severity_level::fatal:
		stream << "\e[41m";
		break;
	}

	stream << " [" << severity << "] " << record[logging::expressions::smessage]
			<< "\e[0m";
}

#endif /* MODELS_LOGGER_COLORFORMATTER_H_ */
