// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowStringNodes.cpp
//  Version:     v1.00
//  Created:     4/10/2006 by AlexL.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <CryString/StringUtils.h>

//////////////////////////////////////////////////////////////////////////
// String nodes.
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CFlowNode_CompareStrings : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_CompareStrings(SActivationInfo* pActInfo) {};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Compare",     _HELP("Trigger to compare strings [A] and [B]")),
			InputPortConfig<string>("A",        _HELP("String A to compare")),
			InputPortConfig<string>("B",        _HELP("String B to compare")),
			InputPortConfig<bool>("IgnoreCase", true),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("Result", _HELP("Outputs -1 if A < B, 0 if A == B, 1 if A > B")),
			OutputPortConfig_Void("False",  _HELP("Triggered if A != B")),
			OutputPortConfig_Void("True",   _HELP("Triggered if A == B")),
			{ 0 }
		};
		config.sDescription = _HELP("Compare two strings");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent evt, SActivationInfo* pActInfo)
	{
		switch (evt)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				const string& a = GetPortString(pActInfo, 1);
				const string& b = GetPortString(pActInfo, 2);
				const bool bIgnoreCase = GetPortBool(pActInfo, 3);
				const int result = bIgnoreCase ? stricmp(a.c_str(), b.c_str()) : a.compare(b);
				ActivateOutput(pActInfo, 0, result);
				ActivateOutput(pActInfo, result != 0 ? 1 : 2, 0);
			}
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_SetString : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_SetString(SActivationInfo* pActInfo) {};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Set",   _HELP("Send string [In] to [Out]")),
			InputPortConfig<string>("In", _HELP("String to be set on [Out].\nWill also be set in Initialize")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<string>("Out"),
			{ 0 }
		};
		config.sDescription = _HELP("Set String Value");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent evt, SActivationInfo* pActInfo)
	{
		switch (evt)
		{
		case eFE_Activate:
		case eFE_Initialize:
			if (IsPortActive(pActInfo, 0))
			{
				ActivateOutput(pActInfo, 0, GetPortString(pActInfo, 1));
			}
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_StringConcat : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_StringConcat(SActivationInfo* pActInfo) {};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Set",        _HELP("Send string [String1+String2] to [Out]")),
			InputPortConfig<string>("String1", _HELP("First string to be concat with second string")),
			InputPortConfig<string>("String2", _HELP("Second string")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<string>("Out"),
			{ 0 }
		};
		config.sDescription = _HELP("Concat two string values");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent evt, SActivationInfo* pActInfo)
	{
		switch (evt)
		{
		case eFE_Activate:
		case eFE_Initialize:
			if (IsPortActive(pActInfo, 0))
			{
				string str1 = GetPortString(pActInfo, 1);
				string str2 = GetPortString(pActInfo, 2);
				ActivateOutput(pActInfo, 0, str1 + str2);
			}
			break;
		}
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_StringWildcardExtract : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_StringWildcardExtract(SActivationInfo* pActInfo) {}
	virtual IFlowNodePtr Clone(SActivationInfo *pActInfo) override
	{
		return new CFlowNode_StringWildcardExtract(pActInfo);
	}
	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}

	enum
	{
		eMaxSupportedWildCards = 2,
	};

	enum
	{
		eIP_Extract = 0,
		eIP_Wildcard,
		eIP_Text,
		eIP_IgnoreCase,
	};

	enum
	{
		eOP_Succeeded = 0,
		eOP_Failed,
		eOP_FirstWildCardIndex,
	};

	typedef CryFixedStringT<16> TPortName;

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Extract", _HELP("Perform extraction of the wildcards")),
			InputPortConfig<string>("Pattern", _HELP("Pattern containing the wildcards to extract")),
			InputPortConfig<string>("Text", _HELP("Text to be split according to the pattern")),
			InputPortConfig<bool>("IgnoreCase", false, _HELP("Tick to ignore the case")),
			{ 0 }
		};
		static std::vector<SOutputPortConfig> s_outConfig;
		if (s_outConfig.empty())
		{
			static std::vector<TPortName> portNames;
			portNames.resize(eMaxSupportedWildCards);
			s_outConfig.reserve(eMaxSupportedWildCards + 3);

			s_outConfig.push_back(OutputPortConfig_Void("Succeeded"));
			s_outConfig.push_back(OutputPortConfig_Void("Failed"));

			for (int i = 0; i < eMaxSupportedWildCards; ++i)
			{
				portNames[i].Format("WildCard%02d", i + 1);
				s_outConfig.push_back(OutputPortConfig<string>(portNames[i].c_str()));
			}

			s_outConfig.push_back(SOutputPortConfig { 0 });
		}
		config.sDescription = _HELP("Extract strings matching a pattern in another string.");
		config.pInputPorts = in_config;
		config.pOutputPorts = &s_outConfig[0];
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent evt, SActivationInfo* pActInfo) override
	{
		switch (evt)
		{
		case eFE_EditorInputPortDataSet:
			RefreshProcessor(pActInfo);
			break;
		case eFE_Initialize:
			RefreshProcessor(pActInfo);
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, eIP_Wildcard))
			{
				RefreshProcessor(pActInfo);
			}
			if (IsPortActive(pActInfo, eIP_Extract))
			{
				PerformExtraction(pActInfo);
			}
			break;
		}
	}

	void PerformExtraction(SActivationInfo* pActInfo)
	{
		const string& text = GetPortString(pActInfo, eIP_Text);
		const bool bCaseSensitive = !GetPortBool(pActInfo, eIP_IgnoreCase);

		const bool bSuccess = m_wildcardProcessor.Process(m_wildcardResult, text.c_str(), bCaseSensitive);
		if (bSuccess)
		{
			ActivateOutput(pActInfo, eOP_Succeeded, true);
			int wildcardIndex = 0;
			m_wildcardProcessor.ForEachWildCardResult(m_wildcardResult, [&wildcardIndex, pActInfo](const CryStringUtils::Wildcards::SConstraintMatch& match)
			{
				if (wildcardIndex < eMaxSupportedWildCards)
				{
				  int portId = eOP_FirstWildCardIndex + wildcardIndex;
				  string tempStr(match.szStart, match.size);
				  ActivateOutput(pActInfo, portId, tempStr);

				  ++wildcardIndex;
				}
			});
		}
		else
		{
			ActivateOutput(pActInfo, eOP_Failed, true);
		}
	}

	void RefreshProcessor(SActivationInfo* pActInfo)
	{
		const string& wildcardStr = GetPortString(pActInfo, eIP_Wildcard).c_str();
		m_wildcardProcessor.Reset(wildcardStr.c_str());
		if (m_wildcardProcessor.GetDescriptor().GetWildCardCount() > eMaxSupportedWildCards)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "FG node [String:WildcardExtract] pattern \"%s\" has too many wildcard characters. %d is the maximum supported.", wildcardStr.c_str(), eMaxSupportedWildCards);
		}
	}

	typedef CryStringUtils::Wildcards::CProcessor<CryStackStringT<char, 32>> TWildcardProcessor;
	typedef CryStringUtils::Wildcards::CEvaluationResult                     TWildcardResult;

	TWildcardProcessor m_wildcardProcessor;
	TWildcardResult    m_wildcardResult;
};

REGISTER_FLOW_NODE("String:SetString", CFlowNode_SetString);
REGISTER_FLOW_NODE("String:Compare", CFlowNode_CompareStrings);
REGISTER_FLOW_NODE("String:Concat", CFlowNode_StringConcat);
REGISTER_FLOW_NODE("String:WildcardExtract", CFlowNode_StringWildcardExtract);
