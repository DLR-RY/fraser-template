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

#include "ConfigurationServer.h"

#define FRONTEND_PORT std::string("5570")

ConfigurationServer::ConfigurationServer(std::string modelsConfigFilePath) :
		mModelsConfigFilePath(modelsConfigFilePath), mCtx(1), mFrontend(mCtx,
		ZMQ_ROUTER) {
	std::cout << "ConfigurationServer-Model Constructor" << std::endl;

	registerInterruptSignal();
	mRun = this->prepare();

	if (mRun) {
		mModelNames = getModelNames();
		setMinAndMaxPort();
		setModelPortNumbers();
		setModelIPAddresses();

		try {
			mFrontend.bind("tcp://*:" + FRONTEND_PORT);
		} catch (std::exception &e) {
			std::cout << "Could not bind to frontend port of configuration server: "
			<< e.what() << std::endl;
			mRun = false;
		}

	} else {
		std::cout << "Error: Exit configuration model (Something went wrong)"
		<< std::endl;
	}

}

ConfigurationServer::~ConfigurationServer() {
	mFrontend.close();
}

bool ConfigurationServer::prepare() {
	pugi::xml_parse_result result = mDocument.load_file(
			mModelsConfigFilePath.c_str());

	if (!result) {
		std::cout << "Parse error: " << result.description()
				<< ", character pos= " << result.offset;
		return false;
	} else {
		mRootNode = mDocument.document_element();
		return true;
	}
}

void ConfigurationServer::setMinAndMaxPort() {
	mMinPort = mRootNode.child("Hosts").attribute("minPort").as_int();
	mMaxPort = mRootNode.child("Hosts").attribute("maxPort").as_int();

	std::cout << "minPort: " << mMinPort << std::endl;
	std::cout << "maxPort: " << mMaxPort << std::endl;
}

bool ConfigurationServer::setModelPortNumbers() {
	int portCnt = mMinPort;

	for (auto name : mModelNames) {
		if (portCnt > mMaxPort) {
			std::cout << "Error: Exceeded max. port number (" << mMaxPort
					<< ") --> Increase the interval" << std::endl;
			return false;
		}

		mModelInformation[name + "_port"] = std::to_string(portCnt);
		std::cout << name << ": " << std::to_string(portCnt) << std::endl;
		portCnt++;
	}

	mModelInformation["sim_sync_port"] = std::to_string(portCnt);

	return true;
}

void ConfigurationServer::setModelIPAddresses() {
	std::string hostAddress = "";

	for (auto name : mModelNames) {
		// Search for the first matching entry with the given hint attribute
		std::string specificModelSearch = ".//Models/Model[@id='" + name + "']";

		pugi::xpath_node xpathSpecificModel = mRootNode.select_single_node(
				specificModelSearch.c_str());

		if (xpathSpecificModel) {
			std::string hostID = xpathSpecificModel.node().child(
					"HostReference").attribute("hostID").value();

			std::string hostNameSearch = ".//Hosts/Host[@id='" + hostID
					+ "']/Address";

			pugi::xpath_node xpath_hostName = mRootNode.select_single_node(
					hostNameSearch.c_str());

			hostAddress = xpath_hostName.node().text().get();
			mModelInformation[name + "_ip"] = hostAddress;
		}
	}

}

int ConfigurationServer::getNumberOfModels() {
	std::string allModelsSearch = ".//Models/Model";
	pugi::xpath_node_set xpathAllModels = mRootNode.select_nodes(
			allModelsSearch.c_str());

	return xpathAllModels.size();
}

std::string ConfigurationServer::getModelInformation(std::string request) {
	return mModelInformation[request];
}

int ConfigurationServer::getNumberOfPersistModels() {
	std::string allModelsSearch = ".//Models/Model[@persist='true']";
	auto xpathAllModels = mRootNode.select_nodes(allModelsSearch.c_str());

	return xpathAllModels.size();
}

std::vector<std::string> ConfigurationServer::getModelNames() {
	std::vector<std::string> modelNames;

	std::string allModelsSearch = ".//Models/Model";

	pugi::xpath_node_set xpathAllModels = mRootNode.select_nodes(
			allModelsSearch.c_str());

	if (!xpathAllModels.empty()) {
		for (auto &modelNode : xpathAllModels) {
			modelNames.push_back(modelNode.node().attribute("id").value());
		}
	}
	return modelNames;
}

std::vector<std::string> ConfigurationServer::getModelDependencies(
		std::string modelName) {
	std::vector<std::string> modelDependencies;

	// Search for the first matching entry with the given hint attribute
	std::string specificModelSearch = ".//Models/Model[@id='" + modelName
			+ "']";

	pugi::xpath_node xpathModel = mRootNode.select_node(
			specificModelSearch.c_str());

	if (xpathModel) {
		// Search for the first matching entry with the given hint attribute
		std::string specificModelDependSearch = ".//Dependencies/ModelReference";
		pugi::xpath_node_set xpathModelDepends = xpathModel.node().select_nodes(
				specificModelDependSearch.c_str());

		for (auto &modelDepend : xpathModelDepends) {
			std::string modelID =
					modelDepend.node().attribute("modelID").value();

			modelDependencies.push_back(modelID);
		}
	}

	return modelDependencies;
}

void ConfigurationServer::run() {

	while (mRun) {
		std::string identity = s_recv(mFrontend);
		//std::cout << "Identity: " << identity << std::endl;
		std::string msg = s_recv(mFrontend);
		//std::cout << "Msg: " << msg << std::endl;

		if (msg == "End") {
			// Stop the DNS server
			break;
		}

		s_sendmore(mFrontend, identity);
		if (msg == "total_num_models") {
			s_send(mFrontend, std::to_string(getNumberOfModels()));
		} else if (msg == "num_persist_models") {
			s_send(mFrontend, std::to_string(getNumberOfPersistModels()));
		} else if (msg == "all_model_names") {
			v_send(mFrontend, getModelNames());
		} else if (msg == "model_dependencies") {
			std::string sought = "_dependencies";
			v_send(mFrontend, getModelDependencies(identity));
		} else {
			s_send(mFrontend, getModelInformation(msg));
		}

		if (interruptOccured) {
			break;
		}
	}
}
