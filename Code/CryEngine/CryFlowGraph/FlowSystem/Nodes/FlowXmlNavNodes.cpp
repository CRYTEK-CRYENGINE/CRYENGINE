// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowXmlNavNodes.cpp
//  Description: Flowgraph nodes to read/write Xml files
// -------------------------------------------------------------------------
//  History:
//    - 8/16/08 : File created - Kevin Kirst
//    - 09/06/2011: Added to SDK - Sascha Hoba
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowBaseXmlNode.h"

////////////////////////////////////////////////////
class CFlowXmlNode_NewChild : public CFlowXmlNode_Base
{
	enum EInputs
	{
		EIP_Name = EIP_CustomStart,
		EIP_Active,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_NewChild(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_NewChild(void)
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
			InputPortConfig<string>("Name","",    _HELP("Name of child node"),                      0, 0),
			InputPortConfig<bool>("Active",true,  _HELP("Make new child node the active element")),
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
		config.sDescription = _HELP("Creates a new child node at end of parent's sibling list.");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_NewChild(pActInfo);
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
			XmlNodeRef ref = doc->active->newChild(GetPortString(pActInfo, EIP_Name));
			if (ref)
			{
				if (GetPortBool(pActInfo, EIP_Active))
				{
					doc->active = ref;
				}
				bResult = true;
			}
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_GetChild : public CFlowXmlNode_Base
{
	enum EInputs
	{
		EIP_Name = EIP_CustomStart,
		EIP_Create,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_GetChild(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_GetChild(void)
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
			InputPortConfig<string>("Name","",     _HELP("Name of child node"),             0, 0),
			InputPortConfig<bool>("Create",false,  _HELP("Create child node if not found"), 0, 0),
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
		config.sDescription = _HELP("Navigates into the first child node with the given name.");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_GetChild(pActInfo);
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
			char const* name = GetPortString(pActInfo, EIP_Name);
			XmlNodeRef ref = doc->active->findChild(name);
			if (ref)
			{
				doc->active = ref;
				bResult = true;
			}
			else if (GetPortBool(pActInfo, EIP_Create))
			{
				ref = doc->active->newChild(name);
				if (ref)
				{
					doc->active = ref;
					bResult = true;
				}
			}
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_GetChildAt : public CFlowXmlNode_Base
{
	enum EInputs
	{
		EIP_Name = EIP_CustomStart,
		EIP_Index,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_GetChildAt(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_GetChildAt(void)
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
			InputPortConfig<string>("Name","",  _HELP("Name of child node"),                  0, 0),
			InputPortConfig<int>("Index",  1,   _HELP("Location of child in list (1-based)"), 0, 0),
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
		config.sDescription = _HELP("Navigates into the Nth child node with the given name.");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_GetChildAt(pActInfo);
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
			const string childName = GetPortString(pActInfo, EIP_Name);
			const int childIndex = GetPortInt(pActInfo, EIP_Index);
			const int childCount = doc->active->getChildCount();
			XmlNodeRef ref = NULL;
			for (int i = 0, realCount = 0; i < childCount; ++i)
			{
				ref = doc->active->getChild(i);
				if (ref && strcmp(ref->getTag(), childName) == 0)
				{
					if (++realCount >= childIndex)
					{
						doc->active = ref;
						bResult = true;
						break;
					}
				}
			}
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_GetChildCount : public CFlowXmlNode_Base
{
	enum EOutputs
	{
		EOP_Count = EOP_CustomStart,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_GetChildCount(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_GetChildCount(void)
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
			OutputPortConfig<int>("Count",_HELP("Number of children")),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Returns number of children of the active element.");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_GetChildCount(pActInfo);
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
			const int count = doc->active->getChildCount();
			ActivateOutput(pActInfo, EOP_Count, count);
			bResult = true;
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_DeleteChild : public CFlowXmlNode_Base
{
	enum EInputs
	{
		EIP_Name = EIP_CustomStart,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_DeleteChild(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_DeleteChild(void)
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
			InputPortConfig<string>("Name","",  _HELP("Name of child node"), 0, 0),
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
		config.sDescription = _HELP("Deletes the first child node with the given name.");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_DeleteChild(pActInfo);
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
			const string childName = GetPortString(pActInfo, EIP_Name);
			const int childCount = doc->active->getChildCount();
			XmlNodeRef ref = NULL;
			for (int i = 0; i < childCount; ++i)
			{
				ref = doc->active->getChild(i);
				if (ref && strcmp(ref->getTag(), childName) == 0)
				{
					doc->active->deleteChildAt(i);
					bResult = true;
					break;
				}
			}
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_DeleteChildAt : public CFlowXmlNode_Base
{
	enum EInputs
	{
		EIP_Name = EIP_CustomStart,
		EIP_Index,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_DeleteChildAt(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_DeleteChildAt(void)
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
			InputPortConfig<string>("Name","",  _HELP("Name of child node"),                  0, 0),
			InputPortConfig<int>("Index",  1,   _HELP("Location of child in list (1-based)"), 0, 0),
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
		config.sDescription = _HELP("Deletes the Nth child node with the given name.");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_DeleteChildAt(pActInfo);
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
			const string childName = GetPortString(pActInfo, EIP_Name);
			const int childIndex = GetPortInt(pActInfo, EIP_Index);
			const int childCount = doc->active->getChildCount();
			XmlNodeRef ref = NULL;
			for (int i = 0, realCount = 0; i < childCount; ++i)
			{
				ref = doc->active->getChild(i);
				if (ref && strcmp(ref->getTag(), childName) == 0)
				{
					if (++realCount >= childIndex)
					{
						doc->active->deleteChildAt(i);
						bResult = true;
						break;
					}
				}
			}
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_DeleteAllChildren : public CFlowXmlNode_Base
{
	enum EInputs
	{
		EIP_Name = EIP_CustomStart,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_DeleteAllChildren(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_DeleteAllChildren(void)
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
			InputPortConfig<string>("Name","",  _HELP("Optional child name - Specify to delete all children with matching name only"), 0, 0),
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
		config.sDescription = _HELP("Deletes all children of the active element.");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_DeleteAllChildren(pActInfo);
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
			char const* name = GetPortString(pActInfo, EIP_Name);
			if (!name || !name[0])
			{
				doc->active->removeAllChilds();
			}
			else
			{
				const int childCount = doc->active->getChildCount();
				XmlNodeRef ref = NULL;
				for (int i = 0; i < childCount; ++i)
				{
					ref = doc->active->getChild(i);
					if (ref && strcmp(ref->getTag(), name) == 0)
					{
						doc->active->deleteChildAt(i);
					}
				}
			}
			bResult = true;
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_GetParent : public CFlowXmlNode_Base
{
public:
	////////////////////////////////////////////////////
	CFlowXmlNode_GetParent(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_GetParent(void)
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
		config.sDescription = _HELP("Sets the active element to the current active element's parent (move one up).");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_GetParent(pActInfo);
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
			XmlNodeRef ref = doc->active->getParent();
			if (ref)
			{
				doc->active = ref;
				bResult = true;
			}
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_GetRoot : public CFlowXmlNode_Base
{
public:
	////////////////////////////////////////////////////
	CFlowXmlNode_GetRoot(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_GetRoot(void)
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
		config.sDescription = _HELP("Sets the active element to the root node (move to top).");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_GetRoot(pActInfo);
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
		if (GDM->GetXmlDocument(pActInfo->pGraph, &doc) && doc->root)
		{
			doc->active = doc->root;
			bResult = true;
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
////////////////////////////////////////////////////

REGISTER_FLOW_NODE("Xml:NewChild", CFlowXmlNode_NewChild);
REGISTER_FLOW_NODE("Xml:GetChild", CFlowXmlNode_GetChild);
REGISTER_FLOW_NODE("Xml:GetChildAt", CFlowXmlNode_GetChildAt);
REGISTER_FLOW_NODE("Xml:GetChildCount", CFlowXmlNode_GetChildCount);
REGISTER_FLOW_NODE("Xml:DeleteChild", CFlowXmlNode_DeleteChild);
REGISTER_FLOW_NODE("Xml:DeleteChildAt", CFlowXmlNode_DeleteChildAt);
REGISTER_FLOW_NODE("Xml:DeleteAllChildren", CFlowXmlNode_DeleteAllChildren);
REGISTER_FLOW_NODE("Xml:GetParent", CFlowXmlNode_GetParent);
REGISTER_FLOW_NODE("Xml:GetRoot", CFlowXmlNode_GetRoot);
