// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioSystem.h>

class XmlNodeRef;

namespace CryAudio
{
class CSwitch;
class CPreloadRequest;
class CEnvironment;

class CXMLProcessor final
{
public:

	CXMLProcessor() = default;

	CXMLProcessor(CXMLProcessor const&) = delete;
	CXMLProcessor(CXMLProcessor&&) = delete;
	CXMLProcessor& operator=(CXMLProcessor const&) = delete;
	CXMLProcessor& operator=(CXMLProcessor&&) = delete;

	void           ParseSystemData();
	bool           ParseControlsData(char const* const szFolderPath, ContextId const contextId, char const* const szContextName);
	void           ClearControlsData(ContextId const contextId, bool const clearAll);
	void           ParsePreloadsData(char const* const szFolderPath, ContextId const contextId);
	void           ClearPreloadsData(ContextId const contextId, bool const clearAll);
	void           ParseControlsFile(XmlNodeRef const& rootNode, ContextId const contextId);
	void           ParseDefaultControlsFile(XmlNodeRef const& rootNode);

private:

	void ParseTriggers(XmlNodeRef const& rootNode, ContextId const contextId);
	void ParseDefaultTriggers(XmlNodeRef const& rootNode);
	void ParseSwitches(XmlNodeRef const& rootNode, ContextId const contextId);
	void ParseParameters(XmlNodeRef const& rootNode, ContextId const contextId);
	void ParsePreloads(XmlNodeRef const& rootNode, ContextId const contextId, char const* const szFolderName, uint const version);
	void ParseEnvironments(XmlNodeRef const& rootNode, ContextId const contextId);
	void ParseSettings(XmlNodeRef const& rootNode, ContextId const contextId);

	void DeletePreloadRequest(CPreloadRequest const* const pPreloadRequest);
};
} // namespace CryAudio
