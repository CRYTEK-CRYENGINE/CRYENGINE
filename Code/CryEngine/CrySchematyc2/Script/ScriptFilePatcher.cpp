// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/ScriptFilePatcher.h"

#include <CrySchematyc2/Services/ILog.h>

#define SCHEMATYC2_PREVIEW_DOC_PATCHES 0

namespace Schematyc2
{
	typedef std::vector<XmlNodeRef> XmlNodeRefVector;

	namespace DocPatcherUtils
	{
		inline XmlNodeRef CloneXml(XmlNodeRef xml)
		{
			XmlNodeRef xmlClone = gEnv->pSystem->CreateXmlNode(xml->getTag(), true);
			for(int childIdx = 0, childCount = xml->getChildCount(); childIdx < childCount; ++ childIdx)
			{
				xmlClone->addChild(CloneXml(xml->getChild(childIdx)));
			}
			for(int attributeIdx = 0, attributeCount = xml->getNumAttributes(); attributeIdx < attributeCount; ++ attributeIdx)
			{
				const char* szKey = "";
				const char* szValue = "";
				xml->getAttributeByIndex(attributeIdx, &szKey, &szValue);
				xmlClone->setAttr(szKey, szValue);
			}
			return xmlClone;
		}

		inline void CacheXmlChildren(XmlNodeRef xml, XmlNodeRefVector& xmlChildren)
		{
			const int childCount = xml->getChildCount();
			xmlChildren.resize(childCount);
			for(int childIdx = 0; childIdx < childCount; ++ childIdx)
			{
				xmlChildren[childIdx] = xml->getChild(childIdx);
			}
		}

		inline void RecurseXml(XmlNodeRef xml, const std::function<void (XmlNodeRef)>& callback)
		{
			callback(xml);
			XmlNodeRefVector xmlChildren;
			CacheXmlChildren(xml, xmlChildren);
			for(XmlNodeRef& xmlChild : xmlChildren)
			{
				RecurseXml(xmlChild, callback);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocPatcher::CDocPatcher()
	{
		// Patches must be in sequential order of version number.
		m_patches.push_back(SPatch(100, 101, PatchCallback::FromConstMemberFunction<CDocPatcher, &CDocPatcher::Patch101>(*this)));
		m_patches.push_back(SPatch(101, 102, PatchCallback::FromConstMemberFunction<CDocPatcher, &CDocPatcher::Patch102>(*this)));
		m_patches.push_back(SPatch(102, 103, PatchCallback::FromConstMemberFunction<CDocPatcher, &CDocPatcher::Patch103>(*this)));
		m_patches.push_back(SPatch(103, 104, PatchCallback::FromConstMemberFunction<CDocPatcher, &CDocPatcher::Patch104>(*this)));
	}

	//////////////////////////////////////////////////////////////////////////
	uint32 CDocPatcher::GetCurrentVersion() const
	{
		return m_patches.empty() ? 100 : m_patches.back().toVersion;
	}

	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef CDocPatcher::PatchDoc(XmlNodeRef xml, const char* szFileName, bool bFromPak)
	{
		uint32 version = 0;
		xml->getAttr("version", version);
		const uint32 currentVersion = GetCurrentVersion();
		if(version < currentVersion)
		{
			if(bFromPak)
			{
				// If file is from pak there's a change it's binary so we need to clone before patching.
				xml = DocPatcherUtils::CloneXml(xml);
			}
			for(SPatch& patch : m_patches)
			{
				if(version == patch.fromVersion)
				{
					if(patch.callback(xml, szFileName))
					{
						SCHEMATYC2_SYSTEM_WARNING("Patch successful: %s (v%d->%d)", szFileName, version, patch.toVersion);
						version = patch.toVersion;
						xml->setAttr("version", version);
#if SCHEMATYC2_PREVIEW_DOC_PATCHES
						stack_string            patchFileName = szFileName;
						stack_string::size_type extensionPos = patchFileName.find(".");
						if(extensionPos != stack_string::npos)
						{
							patchFileName.erase(extensionPos);
							stack_string versionInfo;
							versionInfo.Format(".v%d", version);
							patchFileName.append(versionInfo);
							xml->saveToFile(patchFileName.c_str());
						}
#endif
					}
					else
					{
						SCHEMATYC2_SYSTEM_CRITICAL_ERROR("Patch failed: %s (v%d->%d)", szFileName, version, patch.toVersion);
						return XmlNodeRef();
					}
				}
			}
		}
		else if(version > currentVersion)
		{
			SCHEMATYC2_SYSTEM_CRITICAL_ERROR("Unsupported document version: %s (v%d)", szFileName, version);
			return XmlNodeRef();
		}
		return xml;
	}

	//////////////////////////////////////////////////////////////////////////
	CDocPatcher::SPatch::SPatch(uint32 _fromVersion, uint32 _toVersion, const PatchCallback& _callback)
		: fromVersion(_fromVersion)
		, toVersion(_toVersion)
		, callback(_callback)
	{}

	//////////////////////////////////////////////////////////////////////////
	bool CDocPatcher::Patch101(XmlNodeRef xml, const char* szFileName) const
	{
		xml->setTag("schematyc");
		// Patch variables.
		XmlNodeRef xmlVariables = xml->findChild("variables");
		if(xmlVariables)
		{
			for(int32 variableIdx = 0, variableCount = xmlVariables->getChildCount(); variableIdx < variableCount; ++ variableIdx)
			{
				XmlNodeRef xmlVariable = xmlVariables->getChild(variableIdx);
				if(xmlVariable->isTag("variable"))
				{
					XmlNodeRef xmlVariableTypeInfo = gEnv->pSystem->CreateXmlNode("typeInfo", true);
					xmlVariable->addChild(xmlVariableTypeInfo);
					XmlNodeRef xmlVariableTypeOrigin = xmlVariable->findChild("typeInfo.origin");
					if(xmlVariableTypeOrigin)
					{
						xmlVariableTypeOrigin->setTag("origin");
						xmlVariable->removeChild(xmlVariableTypeOrigin);
						xmlVariableTypeInfo->addChild(xmlVariableTypeOrigin);
					}
					XmlNodeRef xmlVariableTypeGUID = xmlVariable->findChild("type_guid");
					if(xmlVariableTypeGUID)
					{
						xmlVariableTypeGUID->setTag("guid");
						xmlVariable->removeChild(xmlVariableTypeGUID);
						xmlVariableTypeInfo->addChild(xmlVariableTypeGUID);
					}
				}
			}
		}
		// Patch graphs.
		XmlNodeRef xmlGraphs = xml->findChild("graphs");
		if(xmlGraphs)
		{
			for(int32 graphIdx = 0, graphCount = xmlGraphs->getChildCount(); graphIdx < graphCount; ++ graphIdx)
			{
				XmlNodeRef xmlGraph = xmlGraphs->getChild(graphIdx);
				if(xmlGraph->isTag("graph"))
				{
					xmlGraph->setTag("Element");
					// Move details into root of element and replace impl section.
					XmlNodeRef xmlGraphImpl = xmlGraph->findChild("impl");
					if(xmlGraphImpl)
					{
						XmlNodeRef xmlGraphDetail = xmlGraphImpl->findChild("detail");
						if(xmlGraphDetail)
						{
							for(int32 detailElementIdx = 0, detailElementCount = xmlGraphDetail->getChildCount(); detailElementIdx < detailElementCount; ++ detailElementIdx)
							{
								xmlGraphImpl->addChild(xmlGraphDetail->getChild(detailElementIdx));
							}
							xmlGraphImpl->removeChild(xmlGraphDetail);
						}
						xmlGraphImpl->setTag("detail");
					}
					// Move guid, scope_guid, name, type and context_guid to header section.
					XmlNodeRef xmlGraphHeader = gEnv->pSystem->CreateXmlNode("header", true);
					xmlGraph->addChild(xmlGraphHeader);
					XmlNodeRef xmlGraphGUID = xmlGraph->findChild("guid");
					if(xmlGraphGUID)
					{
						xmlGraph->removeChild(xmlGraphGUID);
						xmlGraphHeader->addChild(xmlGraphGUID);
					}
					XmlNodeRef xmlGraphScopeGUID = xmlGraph->findChild("scope_guid");
					if(xmlGraphScopeGUID)
					{
						xmlGraph->removeChild(xmlGraphScopeGUID);
						xmlGraphHeader->addChild(xmlGraphScopeGUID);
					}
					XmlNodeRef xmlGraphName = xmlGraph->findChild("name");
					if(xmlGraphName)
					{
						xmlGraph->removeChild(xmlGraphName);
						xmlGraphHeader->addChild(xmlGraphName);
					}
					XmlNodeRef xmlGraphType = xmlGraph->findChild("type");
					if(xmlGraphType)
					{
						xmlGraph->removeChild(xmlGraphType);
						xmlGraphHeader->addChild(xmlGraphType);
					}
					XmlNodeRef xmlGraphContextGUID = xmlGraph->findChild("context_guid");
					if(xmlGraphContextGUID)
					{
						xmlGraph->removeChild(xmlGraphContextGUID);
						xmlGraphHeader->addChild(xmlGraphContextGUID);
					}
					XmlNodeRef xmlGraphDetail = xmlGraph->findChild("detail");
					if(xmlGraphDetail)
					{
						// Patch graph nodes.
						XmlNodeRef xmlGraphNodes = xmlGraphDetail->findChild("nodes");
						if(xmlGraphNodes)
						{
							for(int32 graphNodeIdx = 0, graphNodeCount = xmlGraphNodes->getChildCount(); graphNodeIdx < graphNodeCount; ++ graphNodeIdx)
							{
								XmlNodeRef xmlGraphNode = xmlGraphNodes->getChild(graphNodeIdx);
								if(xmlGraphNode->isTag("node"))
								{
									xmlGraphNode->setTag("Element");
									// Move guid, type, context_guid, ref_guid and pos to header section.
									XmlNodeRef xmlGraphNodeHeader = gEnv->pSystem->CreateXmlNode("header", true);
									xmlGraphNode->addChild(xmlGraphNodeHeader);
									XmlNodeRef xmlGraphNodeGUID = xmlGraphNode->findChild("guid");
									if(xmlGraphNodeGUID)
									{
										xmlGraphNode->removeChild(xmlGraphNodeGUID);
										xmlGraphNodeHeader->addChild(xmlGraphNodeGUID);
									}
									XmlNodeRef xmlGraphNodeType = xmlGraphNode->findChild("type");
									if(xmlGraphNodeType)
									{
										xmlGraphNode->removeChild(xmlGraphNodeType);
										xmlGraphNodeHeader->addChild(xmlGraphNodeType);
									}
									XmlNodeRef xmlGraphNodeContextGUID = xmlGraphNode->findChild("context_guid");
									if(xmlGraphNodeContextGUID)
									{
										xmlGraphNode->removeChild(xmlGraphNodeContextGUID);
										xmlGraphNodeHeader->addChild(xmlGraphNodeContextGUID);
									}
									XmlNodeRef xmlGraphNodeRefGUID = xmlGraphNode->findChild("ref_guid");
									if(xmlGraphNodeRefGUID)
									{
										xmlGraphNode->removeChild(xmlGraphNodeRefGUID);
										xmlGraphNodeHeader->addChild(xmlGraphNodeRefGUID);
									}
									XmlNodeRef xmlGraphNodePos = xmlGraphNode->findChild("pos");
									if(xmlGraphNodePos)
									{
										xmlGraphNode->removeChild(xmlGraphNodePos);
										xmlGraphNodeHeader->addChild(xmlGraphNodePos);
									}
									// Rename impl section to detail.
									XmlNodeRef xmlGraphNodeImpl = xmlGraphNode->findChild("impl");
									if(xmlGraphNodeImpl)
									{
										xmlGraphNodeImpl->setTag("detail");
									}
								}
							}
						}
						// Patch graph links.
						XmlNodeRef xmlGraphLinks = xmlGraphDetail->findChild("links");
						if(xmlGraphLinks)
						{
							for(int32 graphLinkIdx = 0, graphLinkCount = xmlGraphLinks->getChildCount(); graphLinkIdx < graphLinkCount; ++ graphLinkIdx)
							{
								XmlNodeRef xmlGraphLink = xmlGraphLinks->getChild(graphLinkIdx);
								if(xmlGraphLink->isTag("link"))
								{
									xmlGraphLink->setTag("Element");
								}
							}
						}
					}
				}
			}
			return true;
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CDocPatcher::Patch102(XmlNodeRef xml, const char* szFileName) const
	{
		// Patch variables.
		XmlNodeRef xmlVariables = xml->findChild("variables");
		if(xmlVariables)
		{
			xmlVariables->setTag("member_variables");
			for(int32 variableIdx = 0, variableCount = xmlVariables->getChildCount(); variableIdx < variableCount; ++ variableIdx)
			{
				XmlNodeRef xmlVariable = xmlVariables->getChild(variableIdx);
				if(xmlVariable->isTag("variable"))
				{
					xmlVariable->setTag("member_variable");
				}
			}
		}
		// Patch abstract interface implementations.
		XmlNodeRef xmlAbstractInterfaceImplementations = xml->findChild("abstract_interface_implementations");
		if(xmlAbstractInterfaceImplementations)
		{
			for(int32 abstractInterfaceImplementationIdx = 0, abstractInterfaceImplementationCount = xmlAbstractInterfaceImplementations->getChildCount(); abstractInterfaceImplementationIdx < abstractInterfaceImplementationCount; ++ abstractInterfaceImplementationIdx)
			{
				XmlNodeRef xmlAbstractInterfaceImplementation = xmlAbstractInterfaceImplementations->getChild(abstractInterfaceImplementationIdx);
				if(xmlAbstractInterfaceImplementation->isTag("abstract_interface_implementation"))
				{
					if(!xmlAbstractInterfaceImplementation->findChild("origin"))
					{
						XmlNodeRef xmlOrigin = gEnv->pSystem->CreateXmlNode("origin", true);
						xmlOrigin->setAttr("value", "env");
						xmlAbstractInterfaceImplementation->addChild(xmlOrigin);
					}
				}
			}
		}
		// Patch graph enumeration nodes.
		XmlNodeRef xmlGraphs = xml->findChild("graphs");
		if(xmlGraphs)
		{
			for(int32 graphIdx = 0, graphCount = xmlGraphs->getChildCount(); graphIdx < graphCount; ++ graphIdx)
			{
				XmlNodeRef xmlGraph = xmlGraphs->getChild(graphIdx);
				if(xmlGraph->isTag("Element"))
				{
					XmlNodeRef xmlGraphDetail = xmlGraph->findChild("detail");
					if(xmlGraphDetail)
					{
						XmlNodeRef xmlGraphNodes = xmlGraphDetail->findChild("nodes");
						if(xmlGraphNodes)
						{
							for(int32 graphNodeIdx = 0, graphNodeCount = xmlGraphNodes->getChildCount(); graphNodeIdx < graphNodeCount; ++ graphNodeIdx)
							{
								XmlNodeRef	xmlGraphNode = xmlGraphNodes->getChild(graphNodeIdx);
								if(xmlGraphNode->isTag("Element"))
								{
									XmlNodeRef xmlGraphNodeHeader = xmlGraphNode->findChild("header");
									if(xmlGraphNodeHeader)
									{
										XmlNodeRef xmlGraphNodeType = xmlGraphNodeHeader->findChild("type");
										if(xmlGraphNodeType)
										{
											if(strcmp(xmlGraphNodeType->getAttr("value"), "switch") == 0)
											{
												XmlNodeRef xmlGraphNodeDetail = xmlGraphNode->findChild("detail");
												if(xmlGraphNodeDetail)
												{
													XmlNodeRef	xmlTypeInfo = gEnv->pSystem->CreateXmlNode("typeInfo", true);
													xmlGraphNodeDetail->addChild(xmlTypeInfo);
													XmlNodeRef xmlTypeGUID = xmlGraphNodeDetail->findChild("typeGUID");
													if(xmlTypeGUID)
													{
														xmlTypeGUID->setTag("guid");
														xmlGraphNodeDetail->removeChild(xmlTypeGUID);
														xmlTypeInfo->addChild(xmlTypeGUID);
													}
													XmlNodeRef xmlCaseValueCount = xmlGraphNodeDetail->findChild("caseValueCount");
													if(xmlCaseValueCount)
													{
														uint64 caseValueCount = 0;
														if(xmlCaseValueCount->getAttr("value", caseValueCount))
														{
															XmlNodeRef xmlCases = gEnv->pSystem->CreateXmlNode("cases", true);
															xmlGraphNodeDetail->addChild(xmlCases);
															for(uint64 caseValueIdx = 0; caseValueIdx < caseValueCount; ++ caseValueIdx)
															{
																stack_string caseValueTag;
																caseValueTag.Format("caseValue%d", caseValueIdx);
																XmlNodeRef xmlCaseValue = xmlGraphNodeDetail->findChild(caseValueTag.c_str());
																if(xmlCaseValue)
																{
																	XmlNodeRef xmlCase = gEnv->pSystem->CreateXmlNode("Element", true);
																	xmlCases->addChild(xmlCase);
																	xmlCaseValue->setTag("value");
																	xmlGraphNodeDetail->removeChild(xmlCaseValue);
																	xmlCase->addChild(xmlCaseValue);
																}
															}
														}
														xmlGraphNodeDetail->removeChild(xmlCaseValueCount);
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CDocPatcher::Patch103(XmlNodeRef xml, const char* szFileName) const
	{
		// Patch timers.
		XmlNodeRef xmlTimers = xml->findChild("timers");
		if(xmlTimers)
		{
			for(int32 timerIdx = 0, timerCount = xmlTimers->getChildCount(); timerIdx < timerCount; ++ timerIdx)
			{
				XmlNodeRef xmlTimer = xmlTimers->getChild(timerIdx);
				if(xmlTimer->isTag("timer"))
				{
					XmlNodeRef xmlTimerUnits = xmlTimer->findChild("units");
					if(xmlTimerUnits)
					{
						XmlNodeRef xmlTimerDelay = xmlTimer->findChild("delay");
						if(xmlTimerDelay)
						{
							const char* szTimerUnits = xmlTimerUnits->getAttr("value");
							if(strcmp(szTimerUnits, "frames") == 0)
							{
								xmlTimerUnits->setAttr("value", "Frames");
								xmlTimerDelay->setTag("frames");
							}
							else if((strcmp(szTimerUnits, "real") == 0) || (strcmp(szTimerUnits, "milliseconds") == 0))
							{
								xmlTimerUnits->setAttr("value", "Seconds");
								xmlTimerDelay->setTag("seconds");
								float timerDelay = 0.0f;
								xmlTimerDelay->getAttr("value", timerDelay);
								timerDelay /= 1000.0f;
								xmlTimerDelay->setAttr("value", timerDelay);
							}
						}
					}
					XmlNodeRef xmlTimerFlags = xmlTimer->findChild("flags");
					if(xmlTimerFlags)
					{
						for(int32 timerFlagIdx = 0, timerFlagCount = xmlTimerFlags->getChildCount(); timerFlagIdx < timerFlagCount; ++ timerFlagIdx)
						{
							XmlNodeRef xmlTimerFlag = xmlTimerFlags->getChild(timerFlagIdx);
							if(xmlTimerFlag->isTag("auto_start"))
							{
								xmlTimerFlag->setTag("AutoStart");
							}
							else if(xmlTimerFlag->isTag("repeat"))
							{
								xmlTimerFlag->setTag("Repeat");
							}
						}
					}
				}
			}
		}
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CDocPatcher::Patch104(XmlNodeRef xml, const char* szFileName) const
	{
		// Patch type information.
		auto typeInfoPatcher = [] (XmlNodeRef xml)
		{
			if(xml->isTag("typeInfo.origin"))
			{
				xml->setTag("origin");

				XmlNodeRef xmlParent = xml->getParent();
				XmlNodeRef xmlTypeInfo = gEnv->pSystem->CreateXmlNode("typeInfo", true);
				xmlParent->removeChild(xml);
				xmlParent->addChild(xmlTypeInfo);
				xmlTypeInfo->addChild(xml);

				XmlNodeRef xmlTypeGUID = xmlParent->findChild("type_guid");
				if(xmlTypeGUID != nullptr)
				{
					xmlParent->removeChild(xmlTypeGUID);
					xmlTypeInfo->addChild(xmlTypeGUID);
					xmlTypeGUID->setTag("guid");
				}
			}
		};
		DocPatcherUtils::RecurseXml(xml, typeInfoPatcher);
		// Patch graph nodes.
		XmlNodeRef xmlGraphs = xml->findChild("graphs");
		if(xmlGraphs)
		{
			for(int32 graphIdx = 0, graphCount = xmlGraphs->getChildCount(); graphIdx < graphCount; ++ graphIdx)
			{
				XmlNodeRef xmlGraph = xmlGraphs->getChild(graphIdx);
				if(xmlGraph->isTag("Element"))
				{
					XmlNodeRef xmlGraphDetail = xmlGraph->findChild("detail");
					if(xmlGraphDetail)
					{
						XmlNodeRef xmlGraphNodes = xmlGraphDetail->findChild("nodes");
						if(xmlGraphNodes)
						{
							for(int32 graphNodeIdx = 0, graphNodeCount = xmlGraphNodes->getChildCount(); graphNodeIdx < graphNodeCount; ++ graphNodeIdx)
							{
								XmlNodeRef xmlGraphNode = xmlGraphNodes->getChild(graphNodeIdx);
								if(xmlGraphNode->isTag("Element"))
								{
									XmlNodeRef xmlGraphNodeHeader = xmlGraphNode->findChild("header");
									if(xmlGraphNodeHeader)
									{
										XmlNodeRef xmlGraphNodeType = xmlGraphNodeHeader->findChild("type");
										if(xmlGraphNodeType)
										{
											if(strcmp(xmlGraphNodeType->getAttr("value"), "set_variable") == 0)
											{
												xmlGraphNodeType->setAttr("value", "set");
											}
											else if(strcmp(xmlGraphNodeType->getAttr("value"), "get_variable") == 0)
											{
												xmlGraphNodeType->setAttr("value", "get");
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		// Patch public member variables.
		XmlNodeRef xmlProperties = xml->findChild("properties");
		if(!xmlProperties)
		{
			xmlProperties = gEnv->pSystem->CreateXmlNode("properties", true);
			xml->addChild(xmlProperties);
		}
		XmlNodeRef xmlMemberVariables = xml->findChild("member_variables");
		if(xmlMemberVariables)
		{
			xmlMemberVariables->setTag("variables");
			XmlNodeRefVector xmlMemberVariableNodes;
			DocPatcherUtils::CacheXmlChildren(xmlMemberVariables, xmlMemberVariableNodes);
			for(XmlNodeRef& xmlMemberVariable : xmlMemberVariableNodes)
			{
				if(xmlMemberVariable->isTag("member_variable"))
				{
					xmlMemberVariable->setTag("variable");
					XmlNodeRef xmlMemberVariableFlags = xmlMemberVariable->findChild("flags");
					if(xmlMemberVariableFlags)
					{
						XmlNodeRef xmlMemberVariablePublicFlag = xmlMemberVariableFlags->findChild("public");
						if(xmlMemberVariablePublicFlag)
						{
							if(strcmp(xmlMemberVariablePublicFlag->getAttr("value"), "true") == 0)
							{
								xmlMemberVariables->removeChild(xmlMemberVariable);
								xmlProperties->addChild(xmlMemberVariable);
								xmlMemberVariable->setTag("property");
							}
						}
						xmlMemberVariable->removeChild(xmlMemberVariableFlags);
					}
				}
			}
		}
		return true;
	}
}
