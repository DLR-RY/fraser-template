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

#ifndef CONFIGURATION_SERVER_CONFIGURATIONSERVER_H_
#define CONFIGURATION_SERVER_CONFIGURATIONSERVER_H_

#include <map>
#include <zmq.hpp>
#include <pugixml.hpp>
#include <string>

#include "communication/zhelpers.hpp"
#include "interfaces/IModel.h"

//  This is our external configuration server, which deals with requests and sends the requested IP or Port back to the client.
//  The server can handle one request at time.

class ConfigurationServer: public virtual IModel {
public:
	ConfigurationServer(std::string modelsConfigFilePath);
	virtual ~ConfigurationServer();

	// IModel
	virtual void init() override {
	}
	virtual bool prepare() override;
	virtual void run() override;
	virtual std::string getName() const override {
		return mName;
	}
	virtual std::string getDescription() const override {
		return mDescription;
	}

	// Request Methods
	int getNumberOfModels();
	int getNumberOfPersistModels();
	std::vector<std::string> getModelNames();
	std::string getModelInformation(std::string request);
	std::vector<std::string> getModelDependencies(std::string modelName);

	// Get informations from xml-file
	void setMinAndMaxPort();

	// Set Port numbers
	bool setModelPortNumbers();

	// Set IP addresses
	void setModelIPAddresses();

private:
	// IModel
	std::string mName;
	std::string mDescription;

	zmq::context_t mCtx;
	zmq::socket_t mFrontend;

	pugi::xml_node mRootNode;
	pugi::xml_document mDocument;

	bool mRun = true;
	int mMinPort = 0;
	int mMaxPort = 0;

	std::vector<std::string> mModelNames;
	std::map<std::string, std::string> mModelInformation;
	std::string mModelsConfigFilePath;
};

#endif /* CONFIGURATION_SERVER_CONFIGURATIONSERVER_H_ */
