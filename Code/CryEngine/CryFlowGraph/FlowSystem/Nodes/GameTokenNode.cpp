// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"


#include <CryGame/IGameTokens.h>
#include <CryFlowGraph/IFlowBaseNode.h>

namespace
{
//! Get game token from the system or warn if not found
IGameToken* GetGameToken(const char* callingNodeName, IFlowNode::SActivationInfo* pActInfo, int tokenPort, bool bForceCreate = false)
{
	const string& tokenName = GetPortString(pActInfo, tokenPort);
	if (tokenName.empty())
	{
		return nullptr;
	}

	IGameTokenSystem* pGTS = GetIGameTokenSystem();
	IGameToken* pToken = pGTS->FindToken(tokenName.c_str());
	if (!pToken && pActInfo->pGraph != nullptr)
	{
		// try graph token instead:
		const char* name = pActInfo->pGraph->GetGlobalNameForGraphToken(tokenName.c_str());

		pToken = pGTS->FindToken(name);
		assert(!pToken || pToken->GetFlags() & EGAME_TOKEN_GRAPHVARIABLE);

		if (!pToken && bForceCreate)
			pToken = pGTS->SetOrCreateToken(tokenName.c_str(), TFlowInputData(string("<undefined>"), false));
	}

	if (!pToken)
	{
		CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_ERROR,
			"[FG] Cannot find GameToken: '%s' Node: %s Graph: %s",
			tokenName.c_str(),
			callingNodeName, pActInfo->pGraph != nullptr ? pActInfo->pGraph->GetDebugName() : "Unknown!"
		);
	}

	return pToken;
}

//! Check if the value was inserted correctly in the token or if there was a type coercion
void WarningIfGameTokenUsedWithWrongType(const char* callingNodeName, IFlowNode::SActivationInfo* pActInfo, const char* tokenName, TFlowInputData& data, const string& valueStr)
{
	// give designers a warning that they're probably using a token incorrectly
	// don't warn for empty value strings as that is a common use case to just listen to a token without actually wanting a comparison
	if (!valueStr.empty())
	{
		if (data.CheckIfForcedConversionOfCurrentValueWithString(valueStr))
		{
			CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_ERROR,
				"[FG] Forced conversion of GameToken '%s' of type '%s' with value >%s<. Node: %s Graph: %s",
				tokenName,
				FlowTypeToName(data.GetType()), valueStr.c_str(),
				callingNodeName, pActInfo->pGraph->GetDebugName()
			);
		}
	}
}

//! Check if the value is compatible with the token or if there was a type coercion by setting a temporary token
void DryRunAndWarningIfGameTokenUsedWithWrongType(const char* place, IFlowNode::SActivationInfo* pActInfo, int tokenPort, int valuePort)
{
	IGameToken* tempGT = GetGameToken(place, pActInfo, tokenPort);
	if (tempGT)
	{
		const string& valueStr = GetPortString(pActInfo, valuePort);
		if (!valueStr.empty())
		{
			TFlowInputData tempFDRef;
			tempGT->GetValue(tempFDRef); // get variant with correct type
			TFlowInputData tempFD(tempFDRef); // copy
			tempFD.SetValueWithConversion(valueStr); // set without affecting the real GT

			WarningIfGameTokenUsedWithWrongType(place, pActInfo, GetPortString(pActInfo, tokenPort), tempFD, valueStr);
		}
	}
}

//! Check if token and value FD are the same
bool CompareTokenWithValue(TFlowInputData& tokenData, TFlowInputData& valueData)
{
	CRY_ASSERT(tokenData.GetType() == valueData.GetType());

	// value is always kept in sync with the token data type, so no check here

	bool bEquals = false;
	if (tokenData.GetType() == eFDT_String && valueData.GetType() == eFDT_String)
	{
		// Treat the strings separately as we want them to be case-insensitive comparisons
		const string& dataString = *tokenData.GetPtr<string>();
		const string& comparisonString = *valueData.GetPtr<string>();
		bEquals = (dataString.compareNoCase(comparisonString) == 0);
	}
	else
	{
		bEquals = (tokenData == valueData);
	}
	return bEquals;
}
};



class CSetGameTokenFlowNode : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputs
	{
		eIN_Trigger,
		eIN_GameToken,
		eIN_Value,
	};

	enum EOutputs
	{
		eOUT_GametokenValue,
	};

	CSetGameTokenFlowNode(SActivationInfo* pActInfo) : m_pCachedToken(nullptr)
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CSetGameTokenFlowNode(pActInfo);
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		if (ser.IsReading()) // forces re-caching
			m_pCachedToken = nullptr;
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Trigger",            _HELP("Trigger this input to actually set the game token value"), _HELP("Trigger")),
			InputPortConfig<string>("gametoken_Token", _HELP("Game token to set"),                                       _HELP("Token")),
			InputPortConfig<string>("Value",           _HELP("Value to set")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("OutValue", _HELP("Value which was set (debug)")),
			{ 0 }
		};
		config.sDescription = _HELP("Set the value of a game token");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_pCachedToken = nullptr;
			if (gEnv->IsEditor())
			{
				// this is all temporary data and not necessary at runtime, it is here to give a visible warning for designers
				DryRunAndWarningIfGameTokenUsedWithWrongType(s_sNodeName, pActInfo, eIN_GameToken, eIN_Value);
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIN_GameToken))
				{
					CacheToken(pActInfo);
				}

				if (IsPortActive(pActInfo, eIN_Trigger))
				{
					if (!m_pCachedToken)
					{
						CacheToken(pActInfo);
					}

					if (m_pCachedToken)
					{
						m_pCachedToken->SetValueFromString(GetPortString(pActInfo, eIN_Value));

						TFlowInputData data;
						m_pCachedToken->GetValue(data);
						ActivateOutput(pActInfo, eOUT_GametokenValue, data);
					}
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	void CacheToken(IFlowNode::SActivationInfo* pActInfo)
	{
		m_pCachedToken = GetGameToken(s_sNodeName, pActInfo, eIN_GameToken);
	}

	IGameToken* m_pCachedToken;
	static constexpr const char* s_sNodeName = "GameTokenSet";
};


class CGetGameTokenFlowNode : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputs
	{
		eIN_Trigger,
		eIN_GameToken,
	};

	enum EOutputs
	{
		eOUT_GametokenValue,
	};

	CGetGameTokenFlowNode(SActivationInfo* pActInfo) : m_pCachedToken(nullptr)
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CGetGameTokenFlowNode(pActInfo);
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		if (ser.IsReading()) // forces re-caching
			m_pCachedToken = nullptr;
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Trigger",            _HELP("Trigger this input to actually get the game token value"), _HELP("Trigger")),
			InputPortConfig<string>("gametoken_Token", _HELP("Game token to set"),                                       _HELP("Token")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("OutValue", _HELP("Value of the game token")),
			{ 0 }
		};
		config.sDescription = _HELP("Get the value of a game token");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_pCachedToken = nullptr;
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIN_GameToken))
				{
					CacheToken(pActInfo);
				}

				if (IsPortActive(pActInfo, eIN_Trigger))
				{
					if (!m_pCachedToken)
					{
						CacheToken(pActInfo);
					}

					if (m_pCachedToken)
					{
						TFlowInputData data;
						m_pCachedToken->GetValue(data);
						ActivateOutput(pActInfo, eOUT_GametokenValue, data);
					}
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	void CacheToken(SActivationInfo* pActInfo)
	{
		m_pCachedToken = GetGameToken(s_sNodeName, pActInfo, eIN_GameToken);
	}

	IGameToken* m_pCachedToken;
	static constexpr const char* s_sNodeName = "GameTokenGet";
};


class CCheckGameTokenFlowNode : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputs
	{
		eIN_Trigger,
		eIN_GameToken,
		eIN_Value,
	};

	enum EOutputs
	{
		eOUT_GametokenValue,
		eOUT_Equals,
		eOUT_EqualsTrue,
		eOUT_EqualsFalse,
	};

	CCheckGameTokenFlowNode(SActivationInfo* pActInfo) : m_pCachedToken(nullptr)
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CCheckGameTokenFlowNode(pActInfo);
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		if (ser.IsReading()) // forces re-caching
			m_pCachedToken = nullptr;
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Trigger",            _HELP("Trigger this input to actually get the game token value"), _HELP("Trigger")),
			InputPortConfig<string>("gametoken_Token", _HELP("Game token to set"),                                       _HELP("Token")),
			InputPortConfig<string>("CheckValue",      _HELP("Value to compare the token with")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("OutValue", _HELP("Value of the token")),
			OutputPortConfig<bool>("Result",     _HELP("TRUE if equals, FALSE otherwise")),
			OutputPortConfig_Void("True",        _HELP("Triggered if equal")),
			OutputPortConfig_Void("False",       _HELP("Triggered if not equal")),
			{ 0 }
		};
		config.sDescription = _HELP("Check if the value of a game token equals to a value");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_pCachedToken = nullptr;
			if (gEnv->IsEditor())
			{
				// this is all temporary data and not necessary at runtime, it is here to give a visible warning for designers
				DryRunAndWarningIfGameTokenUsedWithWrongType(s_sNodeName, pActInfo, eIN_GameToken, eIN_Value);
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIN_GameToken)) // update token if it changed
				{
					CacheTokenAndValue(pActInfo);
				}
				else if (IsPortActive(pActInfo, eIN_Value)) // if token is the same, but value changed, update just that
				{
					if (m_pCachedToken)
					{
						UpdateValue(pActInfo);
					}
				}

				if (IsPortActive(pActInfo, eIN_Trigger))
				{
					if (!m_pCachedToken)
					{
						CacheTokenAndValue(pActInfo);
					}

					if (m_pCachedToken)
					{
						#if (!defined(_RELEASE) && !defined(PERFORMANCE_BUILD))
						DryRunAndWarningIfGameTokenUsedWithWrongType(s_sNodeName, pActInfo, eIN_GameToken, eIN_Value);
						#endif

						TFlowInputData tokenData;
						m_pCachedToken->GetValue(tokenData);
						ActivateOutput(pActInfo, eOUT_GametokenValue, tokenData);

						bool bEquals = false;
						const string& valueStr = GetPortString(pActInfo, eIN_Value);
						// if value port is empty and the token is not string, don't make normal comparison. return that the token does not equal
						if (!(valueStr.empty() && m_pCachedToken->GetType() != eFDT_String))
						{
							bEquals = CompareTokenWithValue(tokenData, m_cachedValue);
						}

						ActivateOutput(pActInfo, eOUT_Equals, bEquals);
						ActivateOutput(pActInfo, bEquals? eOUT_EqualsTrue : eOUT_EqualsFalse, true);
					}
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	void UpdateValue(SActivationInfo* pActInfo)
	{
		const string& valueStr = GetPortString(pActInfo, eIN_Value);
		m_cachedValue.SetValueWithConversion(valueStr); // set even if string is empty
		#if (!defined(_RELEASE) && !defined(PERFORMANCE_BUILD))
		WarningIfGameTokenUsedWithWrongType(s_sNodeName, pActInfo, m_pCachedToken->GetName(), m_cachedValue, valueStr);
		#endif
	}

	void CacheTokenAndValue(SActivationInfo* pActInfo)
	{
		m_pCachedToken = GetGameToken(s_sNodeName, pActInfo, eIN_GameToken);

		if (m_pCachedToken)
		{
			m_cachedValue.SetUnlocked();
			m_pCachedToken->GetValue(m_cachedValue); // setting the value FlowData to the correct type
			m_cachedValue.SetLocked();
			UpdateValue(pActInfo);
		}
	}

private:
	IGameToken* m_pCachedToken;
	TFlowInputData m_cachedValue;
	static constexpr const char* s_sNodeName = "GameTokenCheck";
};


class CGameTokenCheckMultiFlowNode : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CGameTokenCheckMultiFlowNode(SActivationInfo* pActInfo) : m_pCachedToken(nullptr)
	{
	}

	enum
	{
		eIN_Trigger = 0,
		eIN_GameToken,
		eIN_FirstValue,
		eIN_NumValues = 8
	};

	enum
	{
		eOUT_GametokenValue,
		eOUT_OneTrue,
		eOUT_AllFalse,
	};

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CGameTokenCheckMultiFlowNode(pActInfo);
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		if (ser.IsReading()) // forces re-caching
			m_pCachedToken = nullptr;
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Trigger",            _HELP("Trigger this input to perform the check"), _HELP("Trigger")),
			InputPortConfig<string>("gametoken_Token", _HELP("Game token to check"),                     _HELP("Token")),
			InputPortConfig<string>("Value1",          _HELP("Value to compare the token with")),
			InputPortConfig<string>("Value2",          _HELP("Value to compare the token with")),
			InputPortConfig<string>("Value3",          _HELP("Value to compare the token with")),
			InputPortConfig<string>("Value4",          _HELP("Value to compare the token with")),
			InputPortConfig<string>("Value5",          _HELP("Value to compare the token with")),
			InputPortConfig<string>("Value6",          _HELP("Value to compare the token with")),
			InputPortConfig<string>("Value7",          _HELP("Value to compare the token with")),
			InputPortConfig<string>("Value8",          _HELP("Value to compare the token with")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("OutValue", _HELP("Value of the token")),
			OutputPortConfig_Void("One_True",    _HELP("Triggered if the token is equal to at least one of the values")),
			OutputPortConfig_Void("All_False",   _HELP("Triggered if no given value is equal to the token")),
			{ 0 }
		};
		config.sDescription = _HELP("Check if a game token equals to any of a list of values");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_pCachedToken = nullptr;
			if (gEnv->IsEditor())
			{
				// this is all temporary data and not necessary at runtime, it is here to give a visible warning for designers
				for (uint i = 0; i < eIN_NumValues; ++i)
				{
					DryRunAndWarningIfGameTokenUsedWithWrongType(s_sNodeName, pActInfo, eIN_GameToken, eIN_FirstValue + i);
				}
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIN_GameToken))
				{
					CacheTokenAndValues(pActInfo);
				}
				else if (m_pCachedToken)
				{
					// if token is the same, but any value changed, update just that
					for (uint i = 0; i < eIN_NumValues; ++i)
					{
						if (IsPortActive(pActInfo, eIN_FirstValue + i))
						{
							UpdateValue(pActInfo, i);
						}
					}
				}

				if (IsPortActive(pActInfo, eIN_Trigger))
				{
					if (!m_pCachedToken)
					{
						CacheTokenAndValues(pActInfo);
					}

					if (m_pCachedToken)
					{
						#if (!defined(_RELEASE) && !defined(PERFORMANCE_BUILD))
						for (uint i = 0; i < eIN_NumValues; ++i)
						{
							DryRunAndWarningIfGameTokenUsedWithWrongType(s_sNodeName, pActInfo, eIN_GameToken, eIN_FirstValue + i);
						}
						#endif

						TFlowInputData tokenData;
						m_pCachedToken->GetValue(tokenData);
						ActivateOutput(pActInfo, eOUT_GametokenValue, tokenData);

						bool bAnyTrue = false;
						for (uint i = 0; i < eIN_NumValues; ++i)
						{
							const string& valueStr = GetPortString(pActInfo, eIN_FirstValue + i);
							// if value port is empty and the token is not string, don't make normal comparison. return that the token does not equal
							if (!(valueStr.empty() && m_pCachedToken->GetType() != eFDT_String))
							{
								if (CompareTokenWithValue(tokenData, m_cachedValues[i]))
								{
									bAnyTrue = true;
									break;
								}
							}
						}
						ActivateOutput(pActInfo, bAnyTrue ? eOUT_OneTrue : eOUT_AllFalse, true);
					}
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	void UpdateValue(SActivationInfo* pActInfo, uint i)
	{
		const string& valueStr = GetPortString(pActInfo, eIN_FirstValue + i);
		m_cachedValues[i].SetValueWithConversion(valueStr); // set even if string is empty
		#if (!defined(_RELEASE) && !defined(PERFORMANCE_BUILD))
		WarningIfGameTokenUsedWithWrongType(s_sNodeName, pActInfo, m_pCachedToken->GetName(), m_cachedValues[i], valueStr);
		#endif
	}

	void CacheTokenAndValues(SActivationInfo* pActInfo)
	{
		m_pCachedToken = GetGameToken(s_sNodeName, pActInfo, eIN_GameToken);

		if (m_pCachedToken)
		{
			for (uint i = 0; i < eIN_NumValues; ++i)
			{
				m_cachedValues[i].SetUnlocked();
				m_pCachedToken->GetValue(m_cachedValues[i]); // setting the value FlowData to the correct type
				m_cachedValues[i].SetLocked();
				UpdateValue(pActInfo, i);
			}
		}
	}

private:
	IGameToken* m_pCachedToken;
	TFlowInputData m_cachedValues[eIN_NumValues];
	static constexpr const char* s_sNodeName = "GameTokenCheckMulti";
};


class CGameTokenFlowNode : public CFlowBaseNode<eNCT_Instanced>, public IGameTokenEventListener
{
public:
	enum EInputs
	{
		eIN_GameToken,
		eIN_Value,
		eIn_FireOnStart,
		eIn_TriggerOnlyOnResultChange,
	};

	enum EOutputs
	{
		eOUT_GametokenValue,
		eOUT_EqualsTrue,
		eOUT_EqualsFalse,
		eOUT_Equals,
	};

	CGameTokenFlowNode(SActivationInfo* pActInfo) : m_pCachedToken(nullptr), m_actInfo(*pActInfo)
	{
	}

	~CGameTokenFlowNode()
	{
		Unregister();
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CGameTokenFlowNode(pActInfo);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		if (ser.IsReading())
		{
			// recache and register at token
			CacheTokenAndValue(pActInfo);
		}
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<string>("gametoken_Token", _HELP("GameToken to set/get"), _HELP("Token")),
			InputPortConfig<string>("compare_Value",   _HELP("Value to compare to"),  _HELP("CompareTo")),
			InputPortConfig<bool>("FireOnStart", false, _HELP("If this node should trigger the output on game start"),  _HELP("FireOnStart")),
			InputPortConfig<bool>("FireOnlyOnResultChange", false,
				_HELP("The value port will always trigger when the token value changes, regardless of this setting."
				" Set this to TRUE to trigger the 'Equal..' ports only if the result of the comparison changed."
				" Set to FALSE to also trigger the ports on token value change, even if the comparison result is the same.")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Out",         _HELP("Outputs the token's value on every change"),                      _HELP("Output")),
			OutputPortConfig_AnyType("Equal True",  _HELP("Outputs when the token's value equals the checked input."),       _HELP("Equal")),
			OutputPortConfig_AnyType("Equal False", _HELP("Outputs when the token's value is not equal the checked input."), _HELP("Not Equal")),
			OutputPortConfig<bool>("Equals",        _HELP("Outputs the result of the equality check."),                      _HELP("Equals")),
			{ 0 }
		};
		config.sDescription = _HELP("Listener for when a GameToken gets changed with an optional comparison value");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
				m_prevEqualResult = -1; // token was not checked yet
				CacheTokenAndValue(pActInfo);
				if (m_pCachedToken && !gEnv->IsEditing() && IsPortActive(pActInfo, eIn_FireOnStart) && GetPortBool(pActInfo, eIn_FireOnStart))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
				}
			}
			break;
		case eFE_Update:
			{
				assert(m_pCachedToken);
				TriggerOutputs();
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			}
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIN_GameToken))
				{
					CacheTokenAndValue(pActInfo);
					if (m_pCachedToken)
					{
						TFlowInputData tokenData;
						m_pCachedToken->GetValue(tokenData);
						ActivateOutput(pActInfo, eOUT_GametokenValue, tokenData);
					}
				}

				if (IsPortActive(pActInfo, eIN_Value))
				{
					RefreshComparisonDataAndType();
				}
			}
			break;
		}
	}

	void RefreshComparisonDataAndType()
	{
		if (m_pCachedToken)
		{
			TFlowInputData tokenData;
			m_pCachedToken->GetValue(tokenData);
			if (m_cachedValue.GetType() != tokenData.GetType())
			{
				m_cachedValue.SetUnlocked();
				m_cachedValue = tokenData;
				m_cachedValue.SetLocked();
			}

			const string& comparisonString = GetPortString(&m_actInfo, eIN_Value);
			m_cachedValue.SetValueWithConversion(comparisonString);

			#if (!defined(_RELEASE) && !defined(PERFORMANCE_BUILD))
			WarningIfGameTokenUsedWithWrongType(s_sNodeName, &m_actInfo, m_pCachedToken->GetName(), m_cachedValue, comparisonString);
			#endif
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	void TriggerOutputs()
	{
		TFlowInputData tokenData;
		m_pCachedToken->GetValue(tokenData);
		ActivateOutput(&m_actInfo, eOUT_GametokenValue, tokenData);

		// If, for some reason, the GameToken's data type changed and we didn't get the notification, make sure we update it properly.
		if (tokenData.GetType() != m_cachedValue.GetType())
		{
			CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_ERROR, "[FG] %s: GameToken %s changed its mind from %s to %s.",
				s_sNodeName,
				m_pCachedToken->GetName(),
				FlowTypeToName(m_cachedValue.GetType()),
				FlowTypeToName(tokenData.GetType())
			);
			RefreshComparisonDataAndType();
		}

		bool bEquals = false;
		const string& valueStr = GetPortString(&m_actInfo, eIN_Value);
		// if value port is empty and the token is not string, don't make normal comparison. return that the token does not equal
		if (!(valueStr.empty() && m_pCachedToken->GetType() != eFDT_String))
		{
			bEquals = CompareTokenWithValue(tokenData, m_cachedValue);
		}
		if (!GetPortBool(&m_actInfo, eIn_TriggerOnlyOnResultChange) || (int)bEquals != m_prevEqualResult)
		{
			ActivateOutput(&m_actInfo, bEquals ? eOUT_EqualsTrue : eOUT_EqualsFalse, true);
			ActivateOutput(&m_actInfo, eOUT_Equals, bEquals);
			m_prevEqualResult = bEquals;
		}
	}

	void OnGameTokenEvent(EGameTokenEvent event, IGameToken* pGameToken)
	{
		assert(pGameToken == m_pCachedToken);
		if (event == EGAMETOKEN_EVENT_CHANGE)
		{
			TriggerOutputs();
		}
		else if (event == EGAMETOKEN_EVENT_DELETE)
		{
			// no need to unregister
			m_pCachedToken = nullptr;
		}
	}

	void Register()
	{
		IGameTokenSystem* pGTSys = GetIGameTokenSystem();
		if (m_pCachedToken && pGTSys)
		{
			RefreshComparisonDataAndType();

			pGTSys->RegisterListener(m_pCachedToken->GetName(), this);
		}
	}

	void Unregister()
	{
		IGameTokenSystem* pGTSys = GetIGameTokenSystem();
		if (m_pCachedToken && pGTSys)
		{
			pGTSys->UnregisterListener(m_pCachedToken->GetName(), this);
			m_pCachedToken = nullptr;
		}
	}

	void CacheTokenAndValue(SActivationInfo* pActInfo)
	{
		Unregister();

		m_pCachedToken = GetGameToken(s_sNodeName, pActInfo, eIN_GameToken);

		if (m_pCachedToken)
		{
			Register();
		}
	}

private:
	SActivationInfo m_actInfo;
	IGameToken*     m_pCachedToken;
	TFlowInputData  m_cachedValue;
	int             m_prevEqualResult;
	static constexpr const char* s_sNodeName = "GameToken (listener)";
};


class CModifyGameTokenFlowNode : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputs
	{
		eIN_Trigger,
		eIN_GameToken,
		eIN_Op,
		eIN_Type,
		eIN_Value,
	};

	enum EOutputs
	{
		eOUT_GametokenValue,
	};

	CModifyGameTokenFlowNode(SActivationInfo* pActInfo) : m_pCachedToken(nullptr)
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CModifyGameTokenFlowNode(pActInfo);
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		if (ser.IsReading()) // forces re-caching
			m_pCachedToken = nullptr;
	}

	enum EOperation
	{
		EOP_SET = 0,
		EOP_ADD,
		EOP_SUB,
		EOP_MUL,
		EOP_DIV
	};

	enum EType
	{
		ET_BOOL = 0,
		ET_INT,
		ET_FLOAT,
		ET_VEC3,
		ET_STRING
	};

	template<class Type>
	struct Helper
	{
		Helper(TFlowInputData& value, const TFlowInputData& operand)
		{
			Init(value, operand);
		}

		bool Init(TFlowInputData& value, const TFlowInputData& operand)
		{
			m_ok = value.GetValueWithConversion(m_value);
			m_ok &= operand.GetValueWithConversion(m_operand);
			return m_ok;
		}

		bool m_ok;
		Type m_value;
		Type m_operand;

	};

	template<class Type> bool not_zero(Type& val)          { return true; }
	bool                      not_zero(int& val)           { return val != 0; }
	bool                      not_zero(float& val)         { return val != 0.0f; }

	template<class Type> void op_set(Helper<Type>& data)   { if (data.m_ok) data.m_value = data.m_operand; }
	template<class Type> void op_add(Helper<Type>& data)   { if (data.m_ok) data.m_value += data.m_operand; }
	template<class Type> void op_sub(Helper<Type>& data)   { if (data.m_ok) data.m_value -= data.m_operand; }
	template<class Type> void op_mul(Helper<Type>& data)   { if (data.m_ok) data.m_value *= data.m_operand; }
	template<class Type> void op_div(Helper<Type>& data)   { if (data.m_ok && not_zero(data.m_operand)) data.m_value /= data.m_operand; }

	void                      op_div(Helper<string>& data) {};
	void                      op_mul(Helper<string>& data) {};
	void                      op_sub(Helper<string>& data) {};
	void                      op_mul(Helper<Vec3>& data)   {};
	void                      op_div(Helper<Vec3>& data)   {};

	void                      op_add(Helper<bool>& data)   {};
	void                      op_sub(Helper<bool>& data)   {};
	void                      op_div(Helper<bool>& data)   {};
	void                      op_mul(Helper<bool>& data)   {};

	template<class Type> void DoOp(EOperation op, Helper<Type>& data)
	{
		switch (op)
		{
		case EOP_SET:
			op_set(data);
			break;
		case EOP_ADD:
			op_add(data);
			break;
		case EOP_SUB:
			op_sub(data);
			break;
		case EOP_MUL:
			op_mul(data);
			break;
		case EOP_DIV:
			op_div(data);
			break;
		}
	}

	void WarnIfWrongOperationType(SActivationInfo* pActInfo)
	{
		bool bOpTypeMatchesToken = false;
		int opType = GetPortInt(pActInfo, eIN_Type);
		IGameToken* tempGT = GetGameToken(s_sNodeName, pActInfo, eIN_GameToken);
		if (tempGT)
		{
			switch (tempGT->GetType())
			{
			case eFDT_Bool:   bOpTypeMatchesToken = (opType == ET_BOOL); break;
			case eFDT_Int:    bOpTypeMatchesToken = (opType == ET_INT); break;
			case eFDT_Float:  bOpTypeMatchesToken = (opType == ET_FLOAT); break;
			case eFDT_Vec3:   bOpTypeMatchesToken = (opType == ET_VEC3); break;
			case eFDT_String: bOpTypeMatchesToken = (opType == ET_STRING); break;
			}
			if (!bOpTypeMatchesToken)
			{
				CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_ERROR, "[FG] %s: Using wrong operation type for GameToken '%s'", s_sNodeName, tempGT->GetName());
			}
		}
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Trigger",            _HELP("Trigger this input to actually get the game token value"), _HELP("Trigger")),
			InputPortConfig<string>("gametoken_Token", _HELP("Game token to set"), _HELP("Token")),
			InputPortConfig<int>("Op",     EOP_SET,    _HELP("Operation token = token OP value"), _HELP("Operation"), _UICONFIG("enum_int:Set=0,Add=1,Sub=2,Mul=3,Div=4")),
			InputPortConfig<int>("Type",   ET_STRING,  _HELP("Type"), _HELP("Type"), _UICONFIG("enum_int:Bool=0,Int=1,Float=2,Vec3=3,String=4")),
			InputPortConfig<string>("Value",           _HELP("Value to operate with")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("OutValue", _HELP("Value of the token")),
			{ 0 }
		};
		config.sDescription = _HELP("Operate on a GameToken");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_pCachedToken = nullptr;
			if (gEnv->IsEditor())
			{
				// this is all temporary data and not necessary at runtime, it is here to give a visible warning for designers
				DryRunAndWarningIfGameTokenUsedWithWrongType(s_sNodeName, pActInfo, eIN_GameToken, eIN_Value);
				WarnIfWrongOperationType(pActInfo);
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIN_GameToken))
				{
					CacheTokenAndValue(pActInfo);
				}
				else if (IsPortActive(pActInfo, eIN_Value)) // if token is the same, but value changed, update just that
				{
					if (m_pCachedToken)
					{
						UpdateValue(pActInfo);
					}
				}

				if (IsPortActive(pActInfo, eIN_Trigger))
				{
					if (!m_pCachedToken)
					{
						CacheTokenAndValue(pActInfo);
					}

					if (m_pCachedToken)
					{
						#if (!defined(_RELEASE) && !defined(PERFORMANCE_BUILD))
						DryRunAndWarningIfGameTokenUsedWithWrongType(s_sNodeName, pActInfo, eIN_GameToken, eIN_Value);
						WarnIfWrongOperationType(pActInfo);
						#endif

						TFlowInputData tokenData;
						m_pCachedToken->GetValue(tokenData);

						EOperation op = static_cast<EOperation>(GetPortInt(pActInfo, eIN_Op));
						EType type = static_cast<EType>(GetPortInt(pActInfo, eIN_Type));
						switch (type)
						{
						case ET_BOOL:
							{ Helper<bool> h(tokenData, m_cachedValue); DoOp(op, h); if (h.m_ok) m_pCachedToken->SetValue(TFlowInputData(h.m_value)); }   break;
						case ET_INT:
							{ Helper<int> h(tokenData, m_cachedValue); DoOp(op, h); if (h.m_ok) m_pCachedToken->SetValue(TFlowInputData(h.m_value)); }    break;
						case ET_FLOAT:
							{ Helper<float> h(tokenData, m_cachedValue); DoOp(op, h); if (h.m_ok) m_pCachedToken->SetValue(TFlowInputData(h.m_value)); }  break;
						case ET_VEC3:
							{ Helper<Vec3> h(tokenData, m_cachedValue); DoOp(op, h); if (h.m_ok) m_pCachedToken->SetValue(TFlowInputData(h.m_value)); }   break;
						case ET_STRING:
							{ Helper<string> h(tokenData, m_cachedValue); DoOp(op, h); if (h.m_ok) m_pCachedToken->SetValue(TFlowInputData(h.m_value)); } break;
						}

						m_pCachedToken->GetValue(tokenData);
						ActivateOutput(pActInfo, eOUT_GametokenValue, tokenData);
					}
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	void UpdateValue(SActivationInfo* pActInfo)
	{
		const string& valueStr = GetPortString(pActInfo, eIN_Value);
		m_cachedValue.SetValueWithConversion(valueStr); // set even if string is empty
		#if (!defined(_RELEASE) && !defined(PERFORMANCE_BUILD))
		WarningIfGameTokenUsedWithWrongType(s_sNodeName, pActInfo, m_pCachedToken->GetName(), m_cachedValue, valueStr);
		#endif
	}

	void CacheTokenAndValue(SActivationInfo* pActInfo)
	{
		m_pCachedToken = GetGameToken(s_sNodeName, pActInfo, eIN_GameToken);

		if (m_pCachedToken)
		{
			m_cachedValue.SetUnlocked();
			m_pCachedToken->GetValue(m_cachedValue); // setting the value FlowData to the correct type
			m_cachedValue.SetLocked();
			UpdateValue(pActInfo);
		}
	}

	IGameToken* m_pCachedToken;
	TFlowInputData m_cachedValue;
	static constexpr const char* s_sNodeName = "GameTokenModify";
};

//////////////////////////////////////////////////////////////////////////

class CFlowNodeGameTokensLevelToLevelStore : public CFlowBaseNode<eNCT_Singleton>
{
	enum
	{
		INP_TRIGGER = 0,
		INP_TOKEN1,
		INP_TOKEN2,
		INP_TOKEN3,
		INP_TOKEN4,
		INP_TOKEN5,
		INP_TOKEN6,
		INP_TOKEN7,
		INP_TOKEN8,
		INP_TOKEN9,
		INP_TOKEN10
	};

public:
	CFlowNodeGameTokensLevelToLevelStore(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Trigger",              _HELP("When activated, the values of the specified gametokens will be internally stored so that they can be restored in next level")),
			InputPortConfig<string>("gametoken_Token1",  _HELP("GameToken to be stored"),                                                                                                      _HELP("Token1")),
			InputPortConfig<string>("gametoken_Token2",  _HELP("GameToken to be stored"),                                                                                                      _HELP("Token2")),
			InputPortConfig<string>("gametoken_Token3",  _HELP("GameToken to be stored"),                                                                                                      _HELP("Token3")),
			InputPortConfig<string>("gametoken_Token4",  _HELP("GameToken to be stored"),                                                                                                      _HELP("Token4")),
			InputPortConfig<string>("gametoken_Token5",  _HELP("GameToken to be stored"),                                                                                                      _HELP("Token5")),
			InputPortConfig<string>("gametoken_Token6",  _HELP("GameToken to be stored"),                                                                                                      _HELP("Token6")),
			InputPortConfig<string>("gametoken_Token7",  _HELP("GameToken to be stored"),                                                                                                      _HELP("Token7")),
			InputPortConfig<string>("gametoken_Token8",  _HELP("GameToken to be stored"),                                                                                                      _HELP("Token8")),
			InputPortConfig<string>("gametoken_Token9",  _HELP("GameToken to be stored"),                                                                                                      _HELP("Token9")),
			InputPortConfig<string>("gametoken_Token10", _HELP("GameToken to be stored"),                                                                                                      _HELP("Token10")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.sDescription = _HELP("When triggered, the value of all specified gametokens will be internally stored. Those values can be restored by using the 'restore' flow node in next level");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, INP_TRIGGER))
				{
					std::vector<const char*> gameTokensList;
					for (uint32 i = INP_TOKEN1; i <= INP_TOKEN10; ++i)
					{
						const string& gameTokenName = GetPortString(pActInfo, i);
						if (!gameTokenName.empty())
							gameTokensList.push_back(gameTokenName.c_str());
					}
					const char** ppGameTokensList = &(gameTokensList[0]);
					GetIGameTokenSystem()->SerializeSaveLevelToLevel(ppGameTokensList, (uint32)gameTokensList.size());
				}
				break;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////

class CFlowNodeGameTokensLevelToLevelRestore : public CFlowBaseNode<eNCT_Singleton>
{
	enum
	{
		INP_TRIGGER = 0,
	};

public:
	CFlowNodeGameTokensLevelToLevelRestore(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Trigger", _HELP("When activated, the gametokens previously saved with a 'store' flownode (suposedly at the end of the previous level) will recover their stored value")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.sDescription = _HELP("When 'Trigger' is activated, the gametokens previously saved with a 'store' flownode (supposedly at the end of the previous level) will recover their stored value");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, INP_TRIGGER))
				{
					GetIGameTokenSystem()->SerializeReadLevelToLevel();
				}
				break;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("Mission:GameTokenModify", CModifyGameTokenFlowNode);
REGISTER_FLOW_NODE("Mission:GameTokenSet", CSetGameTokenFlowNode);
REGISTER_FLOW_NODE("Mission:GameTokenGet", CGetGameTokenFlowNode);
REGISTER_FLOW_NODE("Mission:GameTokenCheck", CCheckGameTokenFlowNode);
REGISTER_FLOW_NODE("Mission:GameTokenCheckMulti", CGameTokenCheckMultiFlowNode);
REGISTER_FLOW_NODE("Mission:GameToken", CGameTokenFlowNode);
REGISTER_FLOW_NODE("Mission:GameTokensLevelToLevelStore", CFlowNodeGameTokensLevelToLevelStore);
REGISTER_FLOW_NODE("Mission:GameTokensLevelToLevelRestore", CFlowNodeGameTokensLevelToLevelRestore);
