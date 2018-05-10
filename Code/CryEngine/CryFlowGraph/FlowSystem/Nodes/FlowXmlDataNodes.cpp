// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowXmlDataNodes.cpp
//  Description: Flowgraph nodes to dealing with data in Xml elements
// -------------------------------------------------------------------------
//  History:
//    - 8/16/08 : File created - Kevin Kirst
//    - 09/06/2011: Added to SDK - Sascha Hoba
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowBaseXmlNode.h"

////////////////////////////////////////////////////
class CFlowXmlNode_SetValue : public CFlowXmlNode_Base
{
	enum EInputs
	{
		EIP_Value = EIP_CustomStart,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_SetValue(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_SetValue(void)
	{

	}

	////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// Define input ports here, in same order as EInputPorts
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS(),
			InputPortConfig_Void("Value",_HELP("Value to set")),
			{ 0 }
		};

		// Define output ports here, in same oreder as EOutputPorts
		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS(),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Set the value of the active element");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_SetValue(pActInfo);
	}

	////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	////////////////////////////////////////////////////
	virtual bool Execute(SActivationInfo* pActInfo)
	{
		bool bResult = false;

		SXmlDocument* doc;
		if (GDM->GetXmlDocument(pActInfo->pGraph, &doc) && doc->active)
		{
			TFlowInputData valueData = GetPortAny(pActInfo, EIP_Value);
			string value;
			if (valueData.GetValueWithConversion(value))
			{
				doc->active->setContent(value.c_str());
				bResult = true;
			}
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_IncValue : public CFlowXmlNode_Base
{
	enum EInputs
	{
		EIP_Amount = EIP_CustomStart,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_IncValue(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_IncValue(void)
	{

	}

	////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// Define input ports here, in same order as EInputPorts
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS(),
			InputPortConfig<float>("Amount",_HELP("Amount to incrememt by")),
			{ 0 }
		};

		// Define output ports here, in same oreder as EOutputPorts
		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS(),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Increments the value of the active element");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_IncValue(pActInfo);
	}

	////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	////////////////////////////////////////////////////
	virtual bool Execute(SActivationInfo* pActInfo)
	{
		bool bResult = false;

		SXmlDocument* doc;
		if (GDM->GetXmlDocument(pActInfo->pGraph, &doc) && doc->active)
		{
			// Read value fromt content
			const float amount = GetPortFloat(pActInfo, EIP_Amount);
			float value = 0.0f;
			const char* content = doc->active->getContent();
			if (sscanf(content, "%f", &value))
			{
				// Increment and set back
				char newContent[32];
				cry_sprintf(newContent, "%f", value + amount);
				doc->active->setContent(newContent);
				bResult = true;
			}
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_GetValue : public CFlowXmlNode_Base
{
	enum EOutputs
	{
		EOP_Value = EOP_CustomStart,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_GetValue(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_GetValue(void)
	{

	}

	////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// Define input ports here, in same order as EInputPorts
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS(),
			{ 0 }
		};

		// Define output ports here, in same oreder as EOutputPorts
		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS(),
			OutputPortConfig_Void("Value",_HELP("Value")),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Get the value of the active element");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_GetValue(pActInfo);
	}

	////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	////////////////////////////////////////////////////
	virtual bool Execute(SActivationInfo* pActInfo)
	{
		bool bResult = false;

		SXmlDocument* doc;
		if (GDM->GetXmlDocument(pActInfo->pGraph, &doc) && doc->active)
		{
			string value = doc->active->getContent();
			ActivateOutput(pActInfo, EOP_Value, value);
			bResult = true;
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_ClearValue : public CFlowXmlNode_Base
{
public:
	////////////////////////////////////////////////////
	CFlowXmlNode_ClearValue(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_ClearValue(void)
	{

	}

	////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// Define input ports here, in same order as EInputPorts
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS(),
			{ 0 }
		};

		// Define output ports here, in same oreder as EOutputPorts
		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS(),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Clears the value of the active element");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_ClearValue(pActInfo);
	}

	////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	////////////////////////////////////////////////////
	virtual bool Execute(SActivationInfo* pActInfo)
	{
		bool bResult = false;

		SXmlDocument* doc;
		if (GDM->GetXmlDocument(pActInfo->pGraph, &doc) && doc->active)
		{
			doc->active->setContent("");
			bResult = true;
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_SetAttribute : public CFlowXmlNode_Base
{
	enum EInputs
	{
		EIP_Name = EIP_CustomStart,
		EIP_Value,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_SetAttribute(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_SetAttribute(void)
	{

	}

	////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// Define input ports here, in same order as EInputPorts
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS(),
			InputPortConfig<string>("Name","",                      _HELP("Name of attribute"), 0, 0),
			InputPortConfig_Void("Value",  _HELP("Value to set")),
			{ 0 }
		};

		// Define output ports here, in same oreder as EOutputPorts
		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS(),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Set an attribute for the active element");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_SetAttribute(pActInfo);
	}

	////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	////////////////////////////////////////////////////
	virtual bool Execute(SActivationInfo* pActInfo)
	{
		bool bResult = false;

		SXmlDocument* doc;
		if (GDM->GetXmlDocument(pActInfo->pGraph, &doc) && doc->active)
		{
			TFlowInputData valueData = GetPortAny(pActInfo, EIP_Value);
			string value;
			if (valueData.GetValueWithConversion(value))
			{
				doc->active->setAttr(GetPortString(pActInfo, EIP_Name), value.c_str());
				bResult = true;
			}
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_IncAttribute : public CFlowXmlNode_Base
{
	enum EInputs
	{
		EIP_Name = EIP_CustomStart,
		EIP_Amount,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_IncAttribute(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_IncAttribute(void)
	{

	}

	////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// Define input ports here, in same order as EInputPorts
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS(),
			InputPortConfig<string>("Name", "",                             _HELP("Name of attribute"), 0, 0),
			InputPortConfig<float>("Amount",_HELP("Amount to increment")),
			{ 0 }
		};

		// Define output ports here, in same oreder as EOutputPorts
		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS(),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Increments an attribute by the given amount for the active element");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_IncAttribute(pActInfo);
	}

	////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	////////////////////////////////////////////////////
	virtual bool Execute(SActivationInfo* pActInfo)
	{
		bool bResult = false;

		SXmlDocument* doc;
		if (GDM->GetXmlDocument(pActInfo->pGraph, &doc) && doc->active)
		{
			// Read value fromt content
			char const* name = GetPortString(pActInfo, EIP_Name);
			const float amount = GetPortFloat(pActInfo, EIP_Amount);
			float value = 0.0f;
			doc->active->getAttr(name, value);

			// Increment and set back
			doc->active->setAttr(name, value + amount);
			bResult = true;
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_GetAttribute : public CFlowXmlNode_Base
{
	enum EInputs
	{
		EIP_Name = EIP_CustomStart,
	};

	enum EOutputs
	{
		EOP_Value = EOP_CustomStart,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_GetAttribute(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_GetAttribute(void)
	{

	}

	////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// Define input ports here, in same order as EInputPorts
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS(),
			InputPortConfig<string>("Name","",  _HELP("Name of the attribute"), 0, 0),
			{ 0 }
		};

		// Define output ports here, in same oreder as EOutputPorts
		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS(),
			OutputPortConfig_Void("Value",_HELP("Value")),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Get the value of an attribute for the active element");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_GetAttribute(pActInfo);
	}

	////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	////////////////////////////////////////////////////
	virtual bool Execute(SActivationInfo* pActInfo)
	{
		bool bResult = false;

		SXmlDocument* doc;
		if (GDM->GetXmlDocument(pActInfo->pGraph, &doc) && doc->active)
		{
			string value = doc->active->getAttr(GetPortString(pActInfo, EIP_Name));
			if (!value.empty())
			{
				ActivateOutput(pActInfo, EOP_Value, value);
				bResult = true;
			}
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_GetAttributeCount : public CFlowXmlNode_Base
{
	enum EOutputs
	{
		EOP_Count = EOP_CustomStart,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_GetAttributeCount(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_GetAttributeCount(void)
	{

	}

	////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// Define input ports here, in same order as EInputPorts
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS(),
			{ 0 }
		};

		// Define output ports here, in same oreder as EOutputPorts
		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS(),
			OutputPortConfig<int>("Count",_HELP("Number of attributes")),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Get the number of attributes for the active element");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_GetAttributeCount(pActInfo);
	}

	////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	////////////////////////////////////////////////////
	virtual bool Execute(SActivationInfo* pActInfo)
	{
		bool bResult = false;

		SXmlDocument* doc;
		if (GDM->GetXmlDocument(pActInfo->pGraph, &doc) && doc->active)
		{
			ActivateOutput(pActInfo, EOP_Count, doc->active->getNumAttributes());
			bResult = true;
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_HasAttribute : public CFlowXmlNode_Base
{
	enum EInputs
	{
		EIP_Name = EIP_CustomStart,
	};

	enum EOutputs
	{
		EOP_Yes = EOP_CustomStart,
		EOP_No,
		EOP_Result,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_HasAttribute(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_HasAttribute(void)
	{

	}

	////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// Define input ports here, in same order as EInputPorts
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS(),
			InputPortConfig<string>("Name","",  _HELP("Name of the attribute"), 0, 0),
			{ 0 }
		};

		// Define output ports here, in same oreder as EOutputPorts
		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS(),
			OutputPortConfig_Void("Yes",    _HELP("Yes - Has attribute")),
			OutputPortConfig_Void("No",     _HELP("No - Does not have attribute")),
			OutputPortConfig<bool>("Result",_HELP("Boolean result")),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Checks if an attribute exists for the active element");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_HasAttribute(pActInfo);
	}

	////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	////////////////////////////////////////////////////
	virtual bool Execute(SActivationInfo* pActInfo)
	{
		bool bResult = false;

		SXmlDocument* doc;
		if (GDM->GetXmlDocument(pActInfo->pGraph, &doc) && doc->active)
		{
			const bool bHas = doc->active->haveAttr(GetPortString(pActInfo, EIP_Name));
			if (bHas) ActivateOutput(pActInfo, EOP_Yes, true);
			else ActivateOutput(pActInfo, EOP_No, true);
			ActivateOutput(pActInfo, EOP_Result, bHas);
			bResult = true;
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_DeleteAttribute : public CFlowXmlNode_Base
{
	enum EInputs
	{
		EIP_Name = EIP_CustomStart,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_DeleteAttribute(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_DeleteAttribute(void)
	{

	}

	////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// Define input ports here, in same order as EInputPorts
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS(),
			InputPortConfig<string>("Name","",  _HELP("Name of the attribute"), 0, 0),
			{ 0 }
		};

		// Define output ports here, in same oreder as EOutputPorts
		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS(),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Deletes an attribute from the active element");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_DeleteAttribute(pActInfo);
	}

	////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	////////////////////////////////////////////////////
	virtual bool Execute(SActivationInfo* pActInfo)
	{
		bool bResult = false;

		SXmlDocument* doc;
		if (GDM->GetXmlDocument(pActInfo->pGraph, &doc) && doc->active)
		{
			const string attrName = GetPortString(pActInfo, EIP_Name);
			const bool bHasAttr = doc->active->haveAttr(attrName);
			if (bHasAttr)
			{
				doc->active->delAttr(attrName);
			}
			bResult = true;
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_DeleteAllAttributes : public CFlowXmlNode_Base
{
public:
	////////////////////////////////////////////////////
	CFlowXmlNode_DeleteAllAttributes(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_DeleteAllAttributes(void)
	{

	}

	////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// Define input ports here, in same order as EInputPorts
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS(),
			{ 0 }
		};

		// Define output ports here, in same oreder as EOutputPorts
		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS(),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Deletes all attributes from the active element");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_DeleteAllAttributes(pActInfo);
	}

	////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	////////////////////////////////////////////////////
	virtual bool Execute(SActivationInfo* pActInfo)
	{
		bool bResult = false;

		SXmlDocument* doc;
		if (GDM->GetXmlDocument(pActInfo->pGraph, &doc) && doc->active)
		{
			doc->active->removeAllAttributes();
			bResult = true;
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
////////////////////////////////////////////////////

REGISTER_FLOW_NODE("Xml:SetValue", CFlowXmlNode_SetValue);
REGISTER_FLOW_NODE("Xml:IncValue", CFlowXmlNode_IncValue);
REGISTER_FLOW_NODE("Xml:GetValue", CFlowXmlNode_GetValue);
REGISTER_FLOW_NODE("Xml:ClearValue", CFlowXmlNode_ClearValue);
REGISTER_FLOW_NODE("Xml:SetAttribute", CFlowXmlNode_SetAttribute);
REGISTER_FLOW_NODE("Xml:IncAttribute", CFlowXmlNode_IncAttribute);
REGISTER_FLOW_NODE("Xml:GetAttribute", CFlowXmlNode_GetAttribute);
REGISTER_FLOW_NODE("Xml:GetAttributeCount", CFlowXmlNode_GetAttributeCount);
REGISTER_FLOW_NODE("Xml:HasAttribute", CFlowXmlNode_HasAttribute);
REGISTER_FLOW_NODE("Xml:DeleteAttribute", CFlowXmlNode_DeleteAttribute);
REGISTER_FLOW_NODE("Xml:DeleteAllAttributes", CFlowXmlNode_DeleteAllAttributes);
