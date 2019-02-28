// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

////////////////////////////////////////////////////////////////////////////////////////
class CFlowCreateContainerNode : public CFlowBaseNode<eNCT_Instanced>
{
private:
	enum
	{
		EIP_Id,
		EIP_Create,
	};

	enum
	{
		EOP_Error,
		EOP_Success,
		EOP_Id,
	};

public:
	CFlowCreateContainerNode(SActivationInfo* pActInfo)
	{

	}

	virtual ~CFlowCreateContainerNode()
	{

	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowCreateContainerNode(pActInfo);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<int>("Id",     0,                            _HELP("Id for the container")),
			InputPortConfig_Void("Create", _HELP("Create a container")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<int>("Error",      _HELP("the number specifies the error - 1: Container already exists")),
			OutputPortConfig_AnyType("Success", _HELP("Operation successfully completed")),
			OutputPortConfig<int>("Id",         _HELP("Id of the container if successfully created")),
			{ 0 }
		};

		config.sDescription = _HELP("This node is used to create containers");
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Create))
				{
					int id = GetPortInt(pActInfo, EIP_Id);
					if (gEnv->pFlowSystem->CreateContainer(id))
					{
						ActivateOutput(pActInfo, EOP_Id, id);
						ActivateOutput(pActInfo, EOP_Success, 1);
					}
					else
					{
						ActivateOutput(pActInfo, EOP_Error, 1);
					}
				}
			}
			break;
		}
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

////////////////////////////////////////////////////////////////////////////////////////
class CFlowEditContainerNode : public CFlowBaseNode<eNCT_Instanced>
{
private:
	enum
	{
		EIP_Id,
		EIP_Add,
		EIP_AddUnique,
		EIP_Remove,
		EIP_Clear,
		EIP_GetCount,
		EIP_Delete,
	};

	enum
	{
		EOP_Error,
		EOP_Success,
	};

public:
	CFlowEditContainerNode(SActivationInfo* pActInfo)
	{

	}

	virtual ~CFlowEditContainerNode()
	{

	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowEditContainerNode(pActInfo);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<int>("Id",           _HELP("Which container to manipulate")),
			InputPortConfig_AnyType("Add",       _HELP("Add the passed item")),
			InputPortConfig_AnyType("AddUnique", _HELP("Add the passed item if it didnt exist")),
			InputPortConfig_AnyType("Remove",    _HELP("Remove all occurrences of the current item")),
			InputPortConfig_AnyType("Clear",     _HELP("Empty container")),
			InputPortConfig_Void("GetCount",     _HELP("Get number of elements - result send in success port if containerId was valid")),
			InputPortConfig_Void("Delete",       _HELP("Delete container")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<int>("Error",      _HELP("the number specifies the error - 1: Container ID is invalid")),
			OutputPortConfig_AnyType("Success", _HELP("Operation successfully completed")),
			{ 0 }
		};

		config.sDescription = _HELP("This node is used to access and manipulate containers");
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.SetCategory(EFLN_APPROVED);
	}

	IFlowSystemContainerPtr GetContainer(SActivationInfo* pActInfo)
	{
		TFlowSystemContainerId id = GetPortInt(pActInfo, EIP_Id);
		IFlowSystemContainerPtr container = gEnv->pFlowSystem->GetContainer(id);
		TFlowInputData data = GetPortAny(pActInfo, EIP_Add);
		if (container)
		{
			return container;
		}
		else
		{
			ActivateOutput(pActInfo, EOP_Error, 1); // Container doesn't exist
			return IFlowSystemContainerPtr();
		}
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Add))
				{
					if (IFlowSystemContainerPtr container = GetContainer(pActInfo))
					{
						TFlowInputData data = GetPortAny(pActInfo, EIP_Add);
						container->AddItem(data);
						ActivateOutput(pActInfo, EOP_Success, 1);
					}
				}

				if (IsPortActive(pActInfo, EIP_AddUnique))
				{
					if (IFlowSystemContainerPtr container = GetContainer(pActInfo))
					{
						container->AddItemUnique(GetPortAny(pActInfo, EIP_AddUnique));
						ActivateOutput(pActInfo, EOP_Success, 1);
					}
				}

				if (IsPortActive(pActInfo, EIP_Remove))
				{
					if (IFlowSystemContainerPtr container = GetContainer(pActInfo))
					{
						container->RemoveItem(GetPortAny(pActInfo, EIP_Remove));
						ActivateOutput(pActInfo, EOP_Success, 1);
					}
				}

				if (IsPortActive(pActInfo, EIP_Clear))
				{
					if (IFlowSystemContainerPtr container = GetContainer(pActInfo))
					{
						container->Clear();
						ActivateOutput(pActInfo, EOP_Success, 1);
					}
				}

				if (IsPortActive(pActInfo, EIP_GetCount))
				{
					if (IFlowSystemContainerPtr container = GetContainer(pActInfo))
					{
						ActivateOutput(pActInfo, EOP_Success, container->GetItemCount());
					}
				}

				if (IsPortActive(pActInfo, EIP_Delete))
				{
					if (IFlowSystemContainerPtr container = GetContainer(pActInfo))
					{
						gEnv->pFlowSystem->DeleteContainer(GetPortInt(pActInfo, EIP_Id));
						ActivateOutput(pActInfo, EOP_Success, 1);
					}
				}
			}
			break;
		}
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

////////////////////////////////////////////////////////////////////////////////////////
class CFlowIterateContainerNode : public CFlowBaseNode<eNCT_Instanced>
{
private:
	TFlowSystemContainerId m_containerId;
	int                    m_currentIdx;

	enum
	{
		EIP_Id,
		EIP_Start,
	};

	enum
	{
		EOP_Error,
		EOP_Done,
		EOP_Out,
	};

public:
	CFlowIterateContainerNode(SActivationInfo* pActInfo)
	{
		m_containerId = InvalidContainerId;
		m_currentIdx = 0;
	}

	virtual ~CFlowIterateContainerNode()
	{

	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowIterateContainerNode(pActInfo);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<int>("Id",    0,                         _HELP("Id for the container")),
			InputPortConfig_Void("Start", _HELP("Start iterating")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<int>("Error",  _HELP("the number specifies the error - 1: containerId is invalid")),
			OutputPortConfig_Void("Done",   _HELP("Operation successfully completed")),
			OutputPortConfig_AnyType("Out", _HELP("Value from the container")),
			{ 0 }
		};

		config.sDescription = _HELP("This node is used to iterator over containers");
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Start))
				{
					m_currentIdx = 0;
					m_containerId = GetPortInt(pActInfo, EIP_Id);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
				}
			}
			break;
		case eFE_Update:
			{
				IFlowSystemContainerPtr pContainer = gEnv->pFlowSystem->GetContainer(m_containerId);
				if (pContainer)
				{
					if (m_currentIdx < pContainer->GetItemCount())
					{
						ActivateOutput(pActInfo, EOP_Out, pContainer->GetItem(m_currentIdx));
						m_currentIdx++;
					}
					else
					{
						ActivateOutput(pActInfo, EOP_Done, 1);
						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					}
				}
				else
				{
					ActivateOutput(pActInfo, EOP_Error, 1);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				}
			}

			break;
		}
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("System:Container:Create", CFlowCreateContainerNode);
REGISTER_FLOW_NODE("System:Container:Edit", CFlowEditContainerNode);
REGISTER_FLOW_NODE("System:Container:Iterate", CFlowIterateContainerNode);
