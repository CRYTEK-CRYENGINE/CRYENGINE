// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowEntityNode.h
//  Version:     v1.00
//  Created:     23/5/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowEntityNode.h"

//////////////////////////////////////////////////////////////////////////
CFlowEntityClass::CFlowEntityClass(IEntityClass* pEntityClass)
{
	//m_classname = pEntityClass->GetName();
	m_pEntityClass = pEntityClass;
}

//////////////////////////////////////////////////////////////////////////
CFlowEntityClass::~CFlowEntityClass()
{
	Reset();
}

//////////////////////////////////////////////////////////////////////////
void CFlowEntityClass::FreeStr(const char* src)
{
	if (src)
		delete[]src;
}

//////////////////////////////////////////////////////////////////////////
void CFlowEntityClass::Reset()
{
	for (std::vector<SInputPortConfig>::iterator it = m_inputs.begin(), itEnd = m_inputs.end(); it != itEnd; ++it)
		FreeStr(it->name);

	for (std::vector<SOutputPortConfig>::iterator it = m_outputs.begin(), itEnd = m_outputs.end(); it != itEnd; ++it)
		FreeStr(it->name);

	stl::free_container(m_inputs);
	stl::free_container(m_outputs);
}

//////////////////////////////////////////////////////////////////////////
const char* CFlowEntityClass::CopyStr(const char* src)
{
	char* dst = 0;
	if (src)
	{
		int len = strlen(src);
		dst = new char[len + 1];
		cry_strcpy(dst, len + 1, src);
	}
	return dst;
}

//////////////////////////////////////////////////////////////////////////
void MakeHumanName(SInputPortConfig& config)
{
	char* name = strchr((char*)config.name, '_');
	if (name != 0)
		config.humanName = name + 1;
}

//////////////////////////////////////////////////////////////////////////
void CFlowEntityClass::GetInputsOutputs(IEntityClass* pEntityClass)
{
	int nEvents = pEntityClass->GetEventCount();
	for (int i = 0; i < nEvents; i++)
	{
		IEntityClass::SEventInfo event = pEntityClass->GetEventInfo(i);
		if (event.bOutput)
		{
			// Output
			switch (event.type)
			{
			case IEntityClass::EVT_BOOL:
				m_outputs.push_back(OutputPortConfig<bool>(CopyStr(event.name)));
				break;
			case IEntityClass::EVT_INT:
				m_outputs.push_back(OutputPortConfig<int>(CopyStr(event.name)));
				break;
			case IEntityClass::EVT_FLOAT:
				m_outputs.push_back(OutputPortConfig<float>(CopyStr(event.name)));
				break;
			case IEntityClass::EVT_VECTOR:
				m_outputs.push_back(OutputPortConfig<Vec3>(CopyStr(event.name)));
				break;
			case IEntityClass::EVT_ENTITY:
				m_outputs.push_back(OutputPortConfig<EntityId>(CopyStr(event.name)));
				break;
			case IEntityClass::EVT_STRING:
				m_outputs.push_back(OutputPortConfig<string>(CopyStr(event.name)));
				break;
			}
		}
		else
		{
			// Input
			switch (event.type)
			{
			case IEntityClass::EVT_BOOL:
				m_inputs.push_back(InputPortConfig<bool>(CopyStr(event.name)));
				MakeHumanName(m_inputs.back());
				break;
			case IEntityClass::EVT_INT:
				m_inputs.push_back(InputPortConfig<int>(CopyStr(event.name)));
				MakeHumanName(m_inputs.back());
				break;
			case IEntityClass::EVT_FLOAT:
				m_inputs.push_back(InputPortConfig<float>(CopyStr(event.name)));
				MakeHumanName(m_inputs.back());
				break;
			case IEntityClass::EVT_VECTOR:
				m_inputs.push_back(InputPortConfig<Vec3>(CopyStr(event.name)));
				MakeHumanName(m_inputs.back());
				break;
			case IEntityClass::EVT_ENTITY:
				m_inputs.push_back(InputPortConfig<EntityId>(CopyStr(event.name)));
				MakeHumanName(m_inputs.back());
				break;
			case IEntityClass::EVT_STRING:
				m_inputs.push_back(InputPortConfig<string>(CopyStr(event.name)));
				MakeHumanName(m_inputs.back());
				break;
			}
		}
	}
	m_outputs.push_back(OutputPortConfig<bool>(0, 0));
	m_inputs.push_back(InputPortConfig<bool>(0, false));
}

//////////////////////////////////////////////////////////////////////////
void CFlowEntityClass::GetConfiguration(SFlowNodeConfig& config)
{
	m_inputs.clear();
	m_outputs.clear();
	GetInputsOutputs(m_pEntityClass);

	//if (m_inputs.empty() && m_outputs.empty())
	//	GetInputsOutputs( m_pEntityClass );

	static const SInputPortConfig in_config[] = {
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		{ 0 }
	};

	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_HIDE_UI;
	config.SetCategory(EFLN_APPROVED); // all Entity flownodes are approved!

	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;

	if (!m_inputs.empty())
		config.pInputPorts = &m_inputs[0];

	if (!m_outputs.empty())
		config.pOutputPorts = &m_outputs[0];

}

//////////////////////////////////////////////////////////////////////////
IFlowNodePtr CFlowEntityClass::Create(IFlowNode::SActivationInfo* pActInfo)
{
	return new CFlowEntityNode(this, pActInfo);
}

//////////////////////////////////////////////////////////////////////////
// CFlowEntityNode implementation.
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CFlowEntityNode::CFlowEntityNode(CFlowEntityClass* pClass, SActivationInfo* pActInfo)
{
	m_event = ENTITY_EVENT_SCRIPT_EVENT;
	m_pClass = pClass;
	//pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
	//	m_entityId = (EntityId)pActInfo->m_pUserData;

	m_nodeID = pActInfo->myID;
	m_pGraph = pActInfo->pGraph;
	m_lastInitializeFrameId = -1;
}

IFlowNodePtr CFlowEntityNode::Clone(SActivationInfo* pActInfo)
{
	IFlowNode* pNode = new CFlowEntityNode(m_pClass, pActInfo);
	return pNode;
};

//////////////////////////////////////////////////////////////////////////
void CFlowEntityNode::GetConfiguration(SFlowNodeConfig& config)
{
	m_pClass->GetConfiguration(config);
}

//////////////////////////////////////////////////////////////////////////
void CFlowEntityNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	CFlowEntityNodeBase::ProcessEvent(event, pActInfo);

	switch (event)
	{
	case eFE_Activate:
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entityId);
			if (!pEntity)
			{
				return;
			}

			if (m_pClass->m_pEntityClass->GetFlags() & ECLF_SEND_SCRIPT_EVENTS_FROM_FLOWGRAPH)
			{
				SendEventToEntity(pActInfo, pEntity);
			}

			IEntityScriptComponent* pScriptProxy = (IEntityScriptComponent*)pEntity->GetProxy(ENTITY_PROXY_SCRIPT);
			if (!pScriptProxy)
				return;

			// Check active ports, and send event to entity.
			for (int i = 0; i < m_pClass->m_inputs.size() - 1; i++)
			{
				if (IsPortActive(pActInfo, i))
				{
					EFlowDataTypes type = GetPortType(pActInfo, i);
					switch (type)
					{
					case eFDT_Int:
						pScriptProxy->CallEvent(m_pClass->m_inputs[i].name, (float)GetPortInt(pActInfo, i));
						break;
					case eFDT_Float:
						pScriptProxy->CallEvent(m_pClass->m_inputs[i].name, GetPortFloat(pActInfo, i));
						break;
					case eFDT_Bool:
						pScriptProxy->CallEvent(m_pClass->m_inputs[i].name, GetPortBool(pActInfo, i));
						break;
					case eFDT_Vec3:
						pScriptProxy->CallEvent(m_pClass->m_inputs[i].name, GetPortVec3(pActInfo, i));
						break;
					case eFDT_EntityId:
						pScriptProxy->CallEvent(m_pClass->m_inputs[i].name, GetPortEntityId(pActInfo, i));
						break;
					case eFDT_String:
						pScriptProxy->CallEvent(m_pClass->m_inputs[i].name, GetPortString(pActInfo, i).c_str());
						break;
					}
				}
			}
		}
		break;
	case eFE_Initialize:
		if (gEnv->IsEditor() && m_entityId)
		{
			UnregisterEvent(m_event);
			RegisterEvent(m_event);
		}
		const int frameId = gEnv->nMainFrameID;
		if (frameId != m_lastInitializeFrameId)
		{
			m_lastInitializeFrameId = frameId;
			for (size_t i = 0; i < m_pClass->GetNumOutputs(); i++)
			{
				ActivateOutput(pActInfo, i, SFlowSystemVoid());
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CFlowEntityNode::SerializeXML(SActivationInfo*, const XmlNodeRef& root, bool reading)
{
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CFlowEntityNode::Serialize(SActivationInfo*, TSerialize ser)
{
	if (ser.IsReading())
		m_lastInitializeFrameId = -1;
}

//////////////////////////////////////////////////////////////////////////
void CFlowEntityNode::SendEventToEntity(SActivationInfo* pActInfo, IEntity* pEntity)
{
	SEntityEvent event(ENTITY_EVENT_SCRIPT_EVENT);
	for (int iInput = 0, inputCount = m_pClass->m_inputs.size() - 1; iInput < inputCount; ++iInput)
	{
		if (IsPortActive(pActInfo, iInput))
		{
			event.nParam[0] = INT_PTR(m_pClass->m_inputs[iInput].name);
			EFlowDataTypes type = GetPortType(pActInfo, iInput);
			switch (type)
			{
			case eFDT_Int:
				{
					event.nParam[1] = INT_PTR(IEntityEventHandler::Int);
					const int value = GetPortInt(pActInfo, iInput);
					event.nParam[2] = INT_PTR(&value);
					pEntity->SendEvent(event);
					break;
				}
			case eFDT_Float:
				{
					event.nParam[1] = INT_PTR(IEntityEventHandler::Float);
					const float value = GetPortFloat(pActInfo, iInput);
					event.nParam[2] = INT_PTR(&value);
					pEntity->SendEvent(event);
					break;
				}
			case eFDT_EntityId:
				{
					event.nParam[1] = INT_PTR(IEntityEventHandler::Entity);
					const EntityId value = GetPortEntityId(pActInfo, iInput);
					event.nParam[2] = INT_PTR(&value);
					pEntity->SendEvent(event);
					break;
				}
			case eFDT_Vec3:
				{
					event.nParam[1] = INT_PTR(IEntityEventHandler::Vector);
					const Vec3 value = GetPortVec3(pActInfo, iInput);
					event.nParam[2] = INT_PTR(&value);
					pEntity->SendEvent(event);
					break;
				}
			case eFDT_String:
				{
					event.nParam[1] = INT_PTR(IEntityEventHandler::String);
					const string value = GetPortString(pActInfo, iInput);
					event.nParam[2] = INT_PTR(value.c_str());
					pEntity->SendEvent(event);
					break;
				}
			case eFDT_Bool:
				{
					event.nParam[1] = INT_PTR(IEntityEventHandler::Bool);
					const bool value = GetPortBool(pActInfo, iInput);
					event.nParam[2] = INT_PTR(&value);
					pEntity->SendEvent(event);
					break;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowEntityNode::OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
{
	if (!m_pGraph->IsEnabled() || m_pGraph->IsSuspended() || !m_pGraph->IsActive())
		return;

	switch (event.event)
	{
	case ENTITY_EVENT_DONE:
		// The entity being destroyed is not always the entity stored in the input of the node (which is what actually is Get/Set by those functions ).
		// In the case of AIWaves, when the node is forwarded to newly spawned entities, this event will be called when those spawned entities are destroyed. But
		//  but the input will still be the original entityId. And should not be zeroed, or the node will stop working after a reset (this only can happen on editor mode)
		if (m_pGraph->GetEntityId(m_nodeID) == pEntity->GetId())
			m_pGraph->SetEntityId(m_nodeID, 0);
		break;

	case ENTITY_EVENT_SCRIPT_EVENT:
		{
			const char* sEvent = (const char*)event.nParam[0];
			// Find output port.
			for (int i = 0; i < m_pClass->m_outputs.size(); i++)
			{
				if (!m_pClass->m_outputs[i].name)
					break;
				if (strcmp(sEvent, m_pClass->m_outputs[i].name) == 0)
				{
					SFlowAddress addr(m_nodeID, i, true);
					switch (event.nParam[1])
					{
					case IEntityClass::EVT_INT:
						assert(event.nParam[2] && "Attempt to activate FlowGraph port of type int without value");
						m_pGraph->ActivatePort(addr, *(int*)event.nParam[2]);
						break;
					case IEntityClass::EVT_FLOAT:
						assert(event.nParam[2] && "Attempt to activate FlowGraph port of type float without value");
						m_pGraph->ActivatePort(addr, *(float*)event.nParam[2]);
						break;
					case IEntityClass::EVT_BOOL:
						assert(event.nParam[2] && "Attempt to activate FlowGraph port of type bool without value");
						m_pGraph->ActivatePort(addr, *(bool*)event.nParam[2]);
						break;
					case IEntityClass::EVT_VECTOR:
						assert(event.nParam[2] && "Attempt to activate FlowGraph port of type Vec3 without value");
						m_pGraph->ActivatePort(addr, *(Vec3*)event.nParam[2]);
						break;
					case IEntityClass::EVT_ENTITY:
						assert(event.nParam[2] && "Attempt to activate FlowGraph port of type EntityId without value");
						m_pGraph->ActivatePort(addr, *(EntityId*)event.nParam[2]);
						break;
					case IEntityClass::EVT_STRING:
						{
							assert(event.nParam[2] && "Attempt to activate FlowGraph port of type string without value");
							const char* str = (const char*)event.nParam[2];
							m_pGraph->ActivatePort(addr, string(str));
						}
						break;
					}
					//m_pGraph->ActivatePort( addr,(bool)true );
					break;
				}
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// Flow node for entity position.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityPos : public CFlowEntityNodeBase
{
public:
	enum EInputs
	{
		IN_POS,
		IN_ROTATE,
		IN_SCALE,
		IN_COORDSYS,
	};
	enum EOutputs
	{
		OUT_POS,
		OUT_ROTATE,
		OUT_SCALE,
		OUT_YDIR,
		OUT_XDIR,
		OUT_ZDIR,
	};
	enum ECoordSys
	{
		CS_PARENT = 0,
		CS_WORLD,
	};

	enum { ALL_VALUES = ENTITY_XFORM_POS | ENTITY_XFORM_ROT | ENTITY_XFORM_SCL };

	CFlowNode_EntityPos(SActivationInfo* pActInfo)
		: CFlowEntityNodeBase()
		, m_pGraph(NULL)
		, m_nodeName(NULL)
	{
		m_event = ENTITY_EVENT_XFORM;
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { pActInfo->m_pUserData = (void*)(UINT_PTR)m_entityId; return new CFlowNode_EntityPos(pActInfo); };
	virtual void         GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("pos", _HELP("Entity position vector")),
			InputPortConfig<Vec3>("rotate", _HELP("Entity rotation angle in degrees")),
			InputPortConfig<Vec3>("scale", _HELP("Entity scale vector")),
			InputPortConfig<int>("CoordSys", 1, _HELP("In which coordinate system the values are expressed"), _HELP("CoordSys"), _UICONFIG("enum_int:Parent=0,World=1")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("pos",      _HELP("Entity position vector")),
			OutputPortConfig<Vec3>("rotate",   _HELP("Entity rotation angle in degrees")),
			OutputPortConfig<Vec3>("scale",    _HELP("Entity scale vector")),
			OutputPortConfig<Vec3>("fwdDir",   _HELP("Entity direction vector - Y axis")),
			OutputPortConfig<Vec3>("rightDir", _HELP("Entity direction vector - X axis")),
			OutputPortConfig<Vec3>("upDir",    _HELP("Entity direction vector - Z axis")),
			{ 0 }
		};
		config.sDescription = _HELP("Entity Position/Rotation/Scale");
		config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
		if (!ShouldShowInEditorList())
			config.nFlags |= EFLN_HIDE_UI;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		CFlowEntityNodeBase::ProcessEvent(event, pActInfo);

		IEntity* pEntity = GetEntity(pActInfo);
		if (!pEntity)
			return;

		switch (event)
		{
		case eFE_Activate:
			{
				ECoordSys coorSys = (ECoordSys)GetPortInt(pActInfo, IN_COORDSYS);

				if (IsPortActive(pActInfo, IN_POS))
				{
					const Vec3 v = GetPortVec3(pActInfo, IN_POS);
					if (coorSys == CS_WORLD)
					{
						Matrix34 tm = pEntity->GetWorldTM();
						tm.SetTranslation(v);
						pEntity->SetWorldTM(tm);
					}
					else
					{
						Matrix34 tm = pEntity->GetLocalTM();
						tm.SetTranslation(v);
						pEntity->SetLocalTM(tm);
					}
				}
				if (IsPortActive(pActInfo, IN_ROTATE))
				{
					const Vec3 v = GetPortVec3(pActInfo, IN_ROTATE);
					Matrix34 tm = Matrix33(Quat::CreateRotationXYZ(Ang3(DEG2RAD(v))));
					if (coorSys == CS_WORLD)
					{
						tm.SetTranslation(pEntity->GetWorldPos());
						pEntity->SetWorldTM(tm);
					}
					else
					{
						tm.SetTranslation(pEntity->GetPos());
						pEntity->SetLocalTM(tm);
					}
				}
				if (IsPortActive(pActInfo, IN_SCALE))
				{
					Vec3 v = GetPortVec3(pActInfo, IN_SCALE);
					if (v.x == 0) v.x = 1.0f;
					if (v.y == 0) v.y = 1.0f;
					if (v.z == 0) v.z = 1.0f;
					pEntity->SetScale(v);
				}
			}
			break;
		case eFE_Initialize:
			{
				m_pGraph = pActInfo->pGraph;
				m_nodeName = m_pGraph->GetNodeName(pActInfo->myID);
				OutputValues(pActInfo, ALL_VALUES);
			}
			break;

		case eFE_SetEntityId:
			OutputValues(pActInfo, ALL_VALUES);
			break;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// IEntityEventListener
	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
	{
		if (!m_pGraph || !m_pGraph->IsEnabled() || m_pGraph->IsSuspended() || !m_pGraph->IsActive())
			return;

		switch (event.event)
		{
		case ENTITY_EVENT_XFORM:
			{
				IFlowNode::SActivationInfo actInfo;
				if (!m_pGraph->GetActivationInfo(m_nodeName, actInfo))
					return;

				OutputValues(&actInfo, uint32(event.nParam[0]));
			}
			break;
		}
	}

	void OutputValues(IFlowNode::SActivationInfo* pActInfo, uint entityFlags)
	{
		ECoordSys coorSys = (ECoordSys)GetPortInt(pActInfo, IN_COORDSYS);

		bool bAny = (entityFlags & (ENTITY_XFORM_POS | ENTITY_XFORM_ROT | ENTITY_XFORM_SCL)) == 0;
		IEntity* pEntity = pActInfo->pEntity;
		if (!pEntity)
			return;

		switch (coorSys)
		{
		case CS_WORLD:
			{
				if (bAny || entityFlags & ENTITY_XFORM_POS)
					ActivateOutput(pActInfo, OUT_POS, pEntity->GetWorldPos());

				if (bAny || entityFlags & ENTITY_XFORM_ROT)
				{
					ActivateOutput(pActInfo, OUT_ROTATE, Vec3(RAD2DEG(Ang3(pEntity->GetWorldAngles()))));
					const Matrix34& mat = pEntity->GetWorldTM();
					ActivateOutput(pActInfo, OUT_YDIR, mat.GetColumn1()); // forward -> mat.TransformVector(Vec3(0,1,0)) );
					ActivateOutput(pActInfo, OUT_XDIR, mat.GetColumn0()); // right   -> mat.TransformVector(Vec3(1,0,0)) );
					ActivateOutput(pActInfo, OUT_ZDIR, mat.GetColumn2()); // up      -> mat.TransformVector(Vec3(0,0,1)) );
				}

				if (bAny || entityFlags & ENTITY_XFORM_SCL)
					ActivateOutput(pActInfo, OUT_SCALE, pEntity->GetScale());
				break;
			}

		case CS_PARENT:
			{
				if (bAny || entityFlags & ENTITY_XFORM_POS)
					ActivateOutput(pActInfo, OUT_POS, pEntity->GetPos());

				if (bAny || entityFlags & ENTITY_XFORM_ROT)
				{
					ActivateOutput(pActInfo, OUT_ROTATE, Vec3(RAD2DEG(Ang3::GetAnglesXYZ(pEntity->GetLocalTM()))));
					const Matrix34& mat = pEntity->GetLocalTM();
					ActivateOutput(pActInfo, OUT_YDIR, mat.GetColumn1()); // forward -> mat.TransformVector(Vec3(0,1,0)) );
					ActivateOutput(pActInfo, OUT_XDIR, mat.GetColumn0()); // right   -> mat.TransformVector(Vec3(1,0,0)) );
					ActivateOutput(pActInfo, OUT_ZDIR, mat.GetColumn2()); // up      -> mat.TransformVector(Vec3(0,0,1)) );
				}

				if (bAny || entityFlags & ENTITY_XFORM_SCL)
					ActivateOutput(pActInfo, OUT_SCALE, pEntity->GetScale());
				break;
			}
		}
	}

	virtual bool ShouldShowInEditorList() { return true; }

	//////////////////////////////////////////////////////////////////////////

protected:
	IFlowGraph* m_pGraph;
	const char* m_nodeName;
};

//////////////////////////////////////////////////////////////////////////
// Flow node for entity tagpoint.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityTagpoint : public CFlowNode_EntityPos
{
public:
	CFlowNode_EntityTagpoint(SActivationInfo* pActInfo) : CFlowNode_EntityPos(pActInfo)
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { pActInfo->m_pUserData = (void*)(UINT_PTR)m_entityId; return new CFlowNode_EntityTagpoint(pActInfo); }

protected:
	virtual bool ShouldShowInEditorList() { return false; }
};

//////////////////////////////////////////////////////////////////////////
// Flow node for entity position.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityGetPos : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_GET,
		IN_COORDSYS,
	};
	enum EOutputs
	{
		OUT_POS = 0,
		OUT_ROTATE,
		OUT_SCALE,
		OUT_YDIR,
		OUT_XDIR,
		OUT_ZDIR,
	};

	enum ECoordSys
	{
		CS_PARENT = 0,
		CS_WORLD,
	};

	CFlowNode_EntityGetPos(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Get", _HELP("Trigger to get current values")),
			InputPortConfig<int>("CoordSys", 1, _HELP("In which coordinate system the values are expressed"), _HELP("CoordSys"), _UICONFIG("enum_int:Parent=0,World=1")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("Pos", _HELP("Entity position vector")),
			OutputPortConfig<Vec3>("Rotate", _HELP("Entity rotation angle in degrees")),
			OutputPortConfig<Vec3>("Scale", _HELP("Entity scale vector always in parent coordinates")),
			OutputPortConfig<Vec3>("FwdDir", _HELP("Entity direction vector - Y axis")),
			OutputPortConfig<Vec3>("RightDir", _HELP("Entity direction vector - X axis")),
			OutputPortConfig<Vec3>("UpDir", _HELP("Entity direction vector - Z axis")),
			{ 0 }
		};
		config.sDescription = _HELP("Get Entity Position/Rotation/Scale");
		config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event != eFE_Activate || IsPortActive(pActInfo, IN_GET) == false)
			return;
		// only if IN_GET is activated.

		IEntity* pEntity = pActInfo->pEntity;
		if (pEntity == 0)
			return;

		ECoordSys coorSys = (ECoordSys)GetPortInt(pActInfo, IN_COORDSYS);

		switch (coorSys)
		{
		case CS_WORLD:
			{
				ActivateOutput(pActInfo, OUT_POS, pEntity->GetWorldPos());
				ActivateOutput(pActInfo, OUT_ROTATE, Vec3(RAD2DEG(Ang3(pEntity->GetWorldAngles()))));
				ActivateOutput(pActInfo, OUT_SCALE, pEntity->GetScale());

				const Matrix34& mat = pEntity->GetWorldTM();
				ActivateOutput(pActInfo, OUT_YDIR, mat.GetColumn1());  // forward -> mat.TransformVector(Vec3(0,1,0)) );
				ActivateOutput(pActInfo, OUT_XDIR, mat.GetColumn0());  // right   -> mat.TransformVector(Vec3(1,0,0)) );
				ActivateOutput(pActInfo, OUT_ZDIR, mat.GetColumn2());  // up      -> mat.TransformVector(Vec3(0,0,1)) );
				break;
			}

		case CS_PARENT:
			{
				ActivateOutput(pActInfo, OUT_POS, pEntity->GetPos());
				ActivateOutput(pActInfo, OUT_ROTATE, Vec3(RAD2DEG(Ang3::GetAnglesXYZ(pEntity->GetRotation()))));
				ActivateOutput(pActInfo, OUT_SCALE, pEntity->GetScale());

				const Matrix34& mat = pEntity->GetLocalTM();
				ActivateOutput(pActInfo, OUT_YDIR, mat.GetColumn1());  // forward -> mat.TransformVector(Vec3(0,1,0)) );
				ActivateOutput(pActInfo, OUT_XDIR, mat.GetColumn0());  // right   -> mat.TransformVector(Vec3(1,0,0)) );
				ActivateOutput(pActInfo, OUT_ZDIR, mat.GetColumn2());  // up      -> mat.TransformVector(Vec3(0,0,1)) );
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
class CFlowNode_EntityCheckDistance : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		INP_CHECK,
		INP_MIN_DIST,
		INP_MAX_DIST,
		INP_ENT00,
		INP_ENT01,
		INP_ENT02,
		INP_ENT03,
		INP_ENT04,
		INP_ENT05,
		INP_ENT06,
		INP_ENT07,
		INP_ENT08,
		INP_ENT09,
		INP_ENT10,
		INP_ENT11,
		INP_ENT12,
		INP_ENT13,
		INP_ENT14,
		INP_ENT15,
	};
	enum EOutputs
	{
		OUT_NEAR_ENT = 0,
		OUT_NEAR_ENT_DIST,
		OUT_FAR_ENT,
		OUT_FAR_ENT_DIST,
		OUT_NOENTITIES_IN_RANGE,
	};

	CFlowNode_EntityCheckDistance(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Check",         _HELP("Trigger to check distances")),
			InputPortConfig<float>("MinDistance", _HELP("Any entity that is nearer than this, will be ignored")),
			InputPortConfig<float>("MaxDistance", _HELP("Any entity that is farther than this, will be ignored (0=no limit)")),
			InputPortConfig<EntityId>("Entity1",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity2",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity3",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity4",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity5",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity6",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity7",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity8",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity9",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity10", _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity11", _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity12", _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity13", _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity14", _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity15", _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity16", _HELP("EntityID")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<EntityId>("NearEntity",  _HELP("Nearest entity")),
			OutputPortConfig<float>("NearEntityDist", _HELP("Nearest entity distance")),
			OutputPortConfig<EntityId>("FarEntity",   _HELP("Farthest entity")),
			OutputPortConfig<float>("FarEntityDist",  _HELP("Farthest entity distance")),
			OutputPortConfig_AnyType("NoEntInRange",  _HELP("Trigered when none of the entities were between Min and Max distance")),
			{ 0 }
		};
		config.sDescription = _HELP("Check distance between the node entity and the entities defined in the inputs");
		config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event != eFE_Activate || !IsPortActive(pActInfo, INP_CHECK))
			return;

		IEntity* pEntityNode = pActInfo->pEntity;
		if (!pEntityNode)
			return;

		float minRangeDist = GetPortFloat(pActInfo, INP_MIN_DIST);
		float maxRangeDist = GetPortFloat(pActInfo, INP_MAX_DIST);
		float minRangeDist2 = minRangeDist * minRangeDist;
		float maxRangeDist2 = maxRangeDist * maxRangeDist;
		if (maxRangeDist2==0)
		{
			maxRangeDist2 = FLT_MAX; // no limit on max distance when the input is 0
		}

		EntityId minEnt = 0;
		EntityId maxEnt = 0;
		float minDist2 = maxRangeDist2;
		float maxDist2 = minRangeDist2;
		bool anyEntityInRange = false;

		for (uint32 i = INP_ENT00; i <= INP_ENT15; ++i)
		{
			EntityId entityIdCheck = GetPortEntityId(pActInfo, i);
			IEntity* pEntityCheck = gEnv->pEntitySystem->GetEntity(entityIdCheck);
			if (pEntityCheck)
			{
				float dist2 = pEntityCheck->GetWorldPos().GetSquaredDistance(pEntityNode->GetWorldPos());
				if (dist2>=minRangeDist2 && dist2<=maxRangeDist2)
				{
					anyEntityInRange = true;
					if (dist2 <= minDist2)
					{
						minDist2 = dist2;
						minEnt = entityIdCheck;
					}
					if (dist2 >= maxDist2)
					{
						maxDist2 = dist2;
						maxEnt = entityIdCheck;
					}
				}
			}
		}

		if (anyEntityInRange)
		{
			ActivateOutput(pActInfo, OUT_NEAR_ENT, minEnt);
			ActivateOutput(pActInfo, OUT_NEAR_ENT_DIST, sqrtf(minDist2));

			ActivateOutput(pActInfo, OUT_FAR_ENT, maxEnt);
			ActivateOutput(pActInfo, OUT_FAR_ENT_DIST, sqrtf(maxDist2));
		}
		else
		{
			ActivateOutput(pActInfo, OUT_NOENTITIES_IN_RANGE, true);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityCheckProjection : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputs
	{
		INP_ENABLED = 0,
		INP_OUT_TYPE,
		INP_MAX_ANGLE_THRESHOLD,
		INP_SORT,
		INP_SHOW_DEBUG,
		INP_ENT01,
		INP_ENT02,
		INP_ENT03,
		INP_ENT04,
		INP_ENT05,
		INP_ENT06,
		INP_ENT07,
		INP_ENT08,
		INP_ENT09,
		INP_ENT10,
		INP_ENT11,
		INP_ENT12,
		INP_ENT13,
		INP_ENT14,
		INP_ENT15,
		INP_ENT16,
		INP_FIRST_ENTITY_IDX = INP_ENT01,
		INP_LAST_ENTITY_IDX  = INP_ENT16,
	};
	enum EOutputs
	{
		OUT_BEST_ENT = 0,
		OUT_BEST_ENT_PROJ_VALUE,
		OUT_BEST_ENT_CHANGED,
		OUT_ENT01,
		OUT_ENT01_PROJ,
		OUT_ENT02,
		OUT_ENT02_PROJ,
		OUT_ENT03,
		OUT_ENT03_PROJ,
		OUT_ENT04,
		OUT_ENT04_PROJ,
		OUT_ENT05,
		OUT_ENT05_PROJ,
		OUT_ENT06,
		OUT_ENT06_PROJ,
		OUT_ENT07,
		OUT_ENT07_PROJ,
		OUT_ENT08,
		OUT_ENT08_PROJ,
		OUT_ENT09,
		OUT_ENT09_PROJ,
		OUT_ENT10,
		OUT_ENT10_PROJ,
		OUT_ENT11,
		OUT_ENT11_PROJ,
		OUT_ENT12,
		OUT_ENT12_PROJ,
		OUT_ENT13,
		OUT_ENT13_PROJ,
		OUT_ENT14,
		OUT_ENT14_PROJ,
		OUT_ENT15,
		OUT_ENT15_PROJ,
		OUT_ENT16,
		OUT_ENT16_PROJ,

		OUT_FIRST_ENTITY_IDX = OUT_ENT01,
		OUT_LAST_ENTITY_IDX  = OUT_ENT16,
	};

	enum EProjectionOutput
	{
		ePO_AngleDegrees = 0,
		ePO_Percentage
	};

	CFlowNode_EntityCheckProjection(SActivationInfo* pActInfo) : m_storedBestEntityId(0)
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_EntityCheckProjection(pActInfo);
	};

	// x1, x2, y1, y2 = pixel coordinates
	void UtilDraw2DLine(float x1, float y1, float x2, float y2, const ColorF& color, float thickness)
	{
		IRenderer* pRenderer = gEnv->pRenderer;
		IRenderAuxGeom* pAux = pRenderer->GetIRenderAuxGeom();
		SAuxGeomRenderFlags flags = pAux->GetRenderFlags();
		SAuxGeomRenderFlags renderFlagsRestore = flags;

		const float dx = 1.0f / pAux->GetCamera().GetViewSurfaceX();
		const float dy = 1.0f / pAux->GetCamera().GetViewSurfaceZ();
		x1 *= dx;
		x2 *= dx;
		y1 *= dy;
		y2 *= dy;

		ColorB col((uint8)(color.r * 255.0f), (uint8)(color.g * 255.0f), (uint8)(color.b * 255.0f), (uint8)(color.a * 255.0f));

		flags.SetMode2D3DFlag(e_Mode2D);
		flags.SetDrawInFrontMode(e_DrawInFrontOn);
		flags.SetDepthTestFlag(e_DepthTestOff);
		flags.SetCullMode(e_CullModeNone);
		flags.SetDepthWriteFlag(e_DepthWriteOff);
		pAux->SetRenderFlags(flags);
		pAux->DrawLine(Vec3(x1, y1, 0), col, Vec3(x2, y2, 0), col, thickness);

		// restore render flags
		pAux->SetRenderFlags(renderFlagsRestore);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("Enable", false,_HELP("Enable the node's update")),
			InputPortConfig<int>("Output", 0,     _HELP("OutputType"), _HELP("Output Type"),_UICONFIG("enum_int:Angle(Degrees)=0,Percentage=1")),
			InputPortConfig<float>("Threshold",10,_HELP("Maximum Projection Angle to consider an entity in range")),
			InputPortConfig<bool>("Sort", false,  _HELP("Sorts the output from best to worst projection")),
			InputPortConfig<bool>("Debug", false, _HELP("Shows debug information")),
			InputPortConfig<EntityId>("Entity1",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity2",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity3",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity4",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity5",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity6",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity7",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity8",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity9",  _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity10", _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity11", _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity12", _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity13", _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity14", _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity15", _HELP("EntityID")),
			InputPortConfig<EntityId>("Entity16", _HELP("EntityID")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<EntityId>("Best Entity", _HELP("EntityID with the best projection or 0 if no entity is in range")),
			OutputPortConfig<float>("Best Projection", _HELP("Best projection (in degrees or percentage). If no entity is in range then 180 degrees or 0 percentage")),
			OutputPortConfig_AnyType("Best Entity Changed", _HELP("Triggered when the best entity changes")),
			OutputPortConfig<EntityId>("Entity1", _HELP("EntityID")),
			OutputPortConfig<float>("Entity1 Projection", _HELP("Entity's projection (in degrees or percentage)")),
			OutputPortConfig<EntityId>("Entity2", _HELP("EntityID")),
			OutputPortConfig<float>("Entity2 Projection", _HELP("Entity's projection (in degrees or percentage)")),
			OutputPortConfig<EntityId>("Entity3", _HELP("EntityID")),
			OutputPortConfig<float>("Entity3 Projection", _HELP("Entity's projection (in degrees or percentage)")),
			OutputPortConfig<EntityId>("Entity4", _HELP("EntityID")),
			OutputPortConfig<float>("Entity4 Projection", _HELP("Entity's projection (in degrees or percentage)")),
			OutputPortConfig<EntityId>("Entity5", _HELP("EntityID")),
			OutputPortConfig<float>("Entity5 Projection", _HELP("Entity's projection (in degrees or percentage)")),
			OutputPortConfig<EntityId>("Entity6", _HELP("EntityID")),
			OutputPortConfig<float>("Entity6 Projection", _HELP("Entity's projection (in degrees or percentage)")),
			OutputPortConfig<EntityId>("Entity7", _HELP("EntityID")),
			OutputPortConfig<float>("Entity7 Projection", _HELP("Entity's projection (in degrees or percentage)")),
			OutputPortConfig<EntityId>("Entity8", _HELP("EntityID")),
			OutputPortConfig<float>("Entity8 Projection", _HELP("Entity's projection (in degrees or percentage)")),
			OutputPortConfig<EntityId>("Entity9", _HELP("EntityID")),
			OutputPortConfig<float>("Entity9 Projection", _HELP("Entity's projection (in degrees or percentage)")),
			OutputPortConfig<EntityId>("Entity10", _HELP("EntityID")),
			OutputPortConfig<float>("Entity10 Projection", _HELP("Entity's projection (in degrees or percentage)")),
			OutputPortConfig<EntityId>("Entity11", _HELP("EntityID")),
			OutputPortConfig<float>("Entity11 Projection", _HELP("Entity's projection (in degrees or percentage)")),
			OutputPortConfig<EntityId>("Entity12", _HELP("EntityID")),
			OutputPortConfig<float>("Entity12 Projection", _HELP("Entity's projection (in degrees or percentage)")),
			OutputPortConfig<EntityId>("Entity13", _HELP("EntityID")),
			OutputPortConfig<float>("Entity13 Projection", _HELP("Entity's projection (in degrees or percentage)")),
			OutputPortConfig<EntityId>("Entity14", _HELP("EntityID")),
			OutputPortConfig<float>("Entity14 Projection", _HELP("Entity's projection (in degrees or percentage)")),
			OutputPortConfig<EntityId>("Entity15", _HELP("EntityID")),
			OutputPortConfig<float>("Entity15 Projection", _HELP("Entity's projection (in degrees or percentage)")),
			OutputPortConfig<EntityId>("Entity16", _HELP("EntityID")),
			OutputPortConfig<float>("Entity16 Projection", _HELP("Entity's projection (in degrees or percentage)")),
			{ 0 }
		};
		config.sDescription = _HELP("Check projection between the node's target entity forward direction (or the camera's view direction if not target entity selected) and the entities defined in the inputs");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	float CalculateOutputValue(int outputType, float projectionAngleDegrees, float maxAngleDegrees)
	{
		float maxAngle = maxAngleDegrees > 0.f ? maxAngleDegrees : 0.0001f;
		return outputType == ePO_AngleDegrees ? projectionAngleDegrees : CLAMP((maxAngleDegrees - projectionAngleDegrees) * 100.f / maxAngle, 0.f, 100.f);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				if (pActInfo->pGraph != nullptr)
				{
					const bool enabled = GetPortBool(pActInfo, INP_ENABLED);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, enabled);
					m_storedBestEntityId = 0;
				}
			}
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, INP_ENABLED))
				{
					const bool enabled = GetPortBool(pActInfo, INP_ENABLED);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, enabled);
					if (enabled)
						m_storedBestEntityId = 0; // reset best entity id just after enabling the node
				}
			}
			break;

		case eFE_Update:
			{
				const float WORST_PROJECTION_ANGLE = 180.f;
				IRenderer* pRenderer = gEnv->pRenderer;
				const IEntity* pEntityNode = pActInfo->pEntity;
				if (pEntityNode == nullptr && pRenderer == nullptr)
					return;

				Vec3 viewDir = pEntityNode ? pEntityNode->GetForwardDir() : Vec3(ZERO);
				Vec3 srcPos = pEntityNode ? pEntityNode->GetWorldPos() : Vec3(ZERO);

				if (pEntityNode == nullptr && pRenderer)
				{
					const CCamera& rCam = GetISystem()->GetViewCamera();
					viewDir = rCam.GetViewdir();
					srcPos = rCam.GetPosition();
				}

#ifndef _RELEASE
				const bool bDebug = GetPortBool(pActInfo, INP_SHOW_DEBUG);

				// Draw screen's centre if projection done from the camera position
				if (bDebug && pEntityNode == nullptr)
				{
					const int w = std::max(IRenderAuxGeom::GetAux()->GetCamera().GetViewSurfaceX(), 1);
					const int h = std::max(IRenderAuxGeom::GetAux()->GetCamera().GetViewSurfaceZ(), 1);
					const float delta = 0.025f * h;
					const float x = 0.5f * w, y = 0.5f * h;
					const ColorF color(1.f, 1.f, 0.f, 1.f);
					UtilDraw2DLine(x - delta, y, x + delta, y, color, 1.f);
					UtilDraw2DLine(x, y - delta, x, y + delta, color, 1.f);
				}
#endif

				struct SEntityProjection
				{
					SEntityProjection() : id(-1), proj(FLT_MAX) {}
					EntityId id;
					float    proj;
				};

				const size_t kNumInputs = INP_LAST_ENTITY_IDX - INP_FIRST_ENTITY_IDX + 1;
				SEntityProjection bestResult;                   // entity info with the best projection
				SEntityProjection sortedEntities[kNumInputs];   // array of sorted entities' info based on projection

				bool bAnyEntityInRange = false;
				const bool bSort = GetPortBool(pActInfo, INP_SORT);
				const int outputType = GetPortInt(pActInfo, INP_OUT_TYPE);                                                         // either degrees or percentage
				const float maxAngleDegrees = CLAMP(GetPortFloat(pActInfo, INP_MAX_ANGLE_THRESHOLD), 0.f, WORST_PROJECTION_ANGLE); // max angle clamped to [0..180]

				for (uint32 i = INP_FIRST_ENTITY_IDX; i <= INP_LAST_ENTITY_IDX; ++i)
				{
					bool bEntityInRange = false;
					const uint32 inEntityIndex = i - INP_FIRST_ENTITY_IDX;
					const uint32 outEntityIndex = OUT_FIRST_ENTITY_IDX + inEntityIndex * 2;
					const EntityId currentEntityId = GetPortEntityId(pActInfo, i);
					const IEntity* pCurrentEntity = gEnv->pEntitySystem->GetEntity(currentEntityId);

					float outEntityProjData = 0.f;
					float projAngle = WORST_PROJECTION_ANGLE;
					if (pCurrentEntity)
					{
						const Vec3 currentEntityPos = pCurrentEntity->GetWorldPos();
						const Vec3 dirToEntity = (currentEntityPos - srcPos).normalized();
						projAngle = RAD2DEG(acos(viewDir.dot(dirToEntity))); // [0..180]

						if (projAngle < maxAngleDegrees)
						{
							// entity is in range
							outEntityProjData = CalculateOutputValue(outputType, projAngle, maxAngleDegrees);

							if (projAngle < bestResult.proj)
							{
								bestResult.proj = projAngle;
								bestResult.id = currentEntityId;
							}

							bAnyEntityInRange = true;
							bEntityInRange = true;

#ifndef _RELEASE
							if (bDebug) IRenderAuxText::DrawLabelF(currentEntityPos, 1.5f, "%1.2f%s Id:%d", outEntityProjData, outputType == ePO_AngleDegrees ? "deg" : "%", currentEntityId);
#endif
						}

#ifndef _RELEASE
						if (bDebug)
						{
							if (IRenderAuxGeom* pAux = gEnv->pRenderer->GetIRenderAuxGeom()) pAux->DrawSphere(currentEntityPos, 0.15f, ColorB(0xff, 0xff, 0x00, 0xff));
						}
#endif
					}

					if (bEntityInRange == false)
					{
						// entity not in range or invalid
						outEntityProjData = outputType == ePO_AngleDegrees ? projAngle : 0.f;
					}

					// if sorting, store info otherwise output directly data for current entity
					if (bSort)
					{
						sortedEntities[inEntityIndex].id = currentEntityId;
						sortedEntities[inEntityIndex].proj = outEntityProjData;
					}
					else
					{
						ActivateOutput(pActInfo, outEntityIndex, currentEntityId);
						ActivateOutput(pActInfo, outEntityIndex + 1, outEntityProjData);
					}
				}

				// Perform sort and output sorted info (if needed)
				if (bSort)
				{
					std::sort(sortedEntities, sortedEntities + kNumInputs,
					          [outputType](const SEntityProjection& a, const SEntityProjection& b) { return (outputType == ePO_AngleDegrees) ? (a.proj < b.proj) : (a.proj > b.proj); }
					          );

					for (size_t i = 0; i < kNumInputs; ++i)
					{
						const size_t outEntityIdx = OUT_FIRST_ENTITY_IDX + i * 2;
						ActivateOutput(pActInfo, outEntityIdx, sortedEntities[i].id);
						ActivateOutput(pActInfo, outEntityIdx + 1, sortedEntities[i].proj);
					}

				}

				// if no entities in projection range make sure we output a best EntityId = 0
				if (!bAnyEntityInRange || bestResult.id <= 0)
				{
					bestResult.id = 0;
					bestResult.proj = WORST_PROJECTION_ANGLE;
				}

				// Output best entity info
				ActivateOutput(pActInfo, OUT_BEST_ENT, bestResult.id);
				ActivateOutput(pActInfo, OUT_BEST_ENT_PROJ_VALUE, CalculateOutputValue(outputType, bestResult.proj, maxAngleDegrees));

				if (m_storedBestEntityId != bestResult.id)
				{
					ActivateOutput(pActInfo, OUT_BEST_ENT_CHANGED, true);
					m_storedBestEntityId = bestResult.id;
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
	EntityId m_storedBestEntityId;

};

//////////////////////////////////////////////////////////////////////////
// Flow node to get the AABB of an entity
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityGetBounds : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_GET,
		IN_COORDSYS
	};

	enum EOutputs
	{
		OUT_MIN,
		OUT_MAX
	};

	enum ECoordSys
	{
		CS_LOCAL,
		CS_WORLD,
	};

	CFlowNode_EntityGetBounds(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_Void("Get", _HELP("")),
			InputPortConfig<int>("CoordSys", 1, _HELP("In which coordinate system the values are expressed"),_HELP("CoordSys"), _UICONFIG("enum_int:Local=0,World=1")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<Vec3>("Min", _HELP("Min position of the AABB")),
			OutputPortConfig<Vec3>("Max", _HELP("Max position of the AABB")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
		config.sDescription = _HELP("Gets the AABB");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, IN_GET) && pActInfo->pEntity)
			{
				AABB aabb(Vec3(0, 0, 0), Vec3(0, 0, 0));

				switch (GetPortInt(pActInfo, IN_COORDSYS))
				{
				case CS_LOCAL:
					pActInfo->pEntity->GetLocalBounds(aabb);
					break;

				case CS_WORLD:
					pActInfo->pEntity->GetWorldBounds(aabb);
					break;
				}

				ActivateOutput(pActInfo, OUT_MIN, aabb.min);
				ActivateOutput(pActInfo, OUT_MAX, aabb.max);
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Flow node for controlling the entity look at position.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityFaceAt : public CFlowEntityNodeBase
{
public:
	enum EInputs
	{
		IN_TARGET = 0,
		IN_VECTOR,
		IN_START,
		IN_STOP,
		IN_FWD_AXIS,
		IN_REFERENCE,
		IN_BLENDSPEED,
	};
	enum EOutputs
	{
		OUT_CURRENT = 0,
	};

	enum EFwdAxis
	{
		eFA_XPlus = 0,
		eFA_XMinus,
		eFA_YPlus,
		eFA_YMinus,
		eFA_ZPlus,
		eFA_ZMinus,
	};

	CFlowNode_EntityFaceAt(SActivationInfo* pActInfo)
		: CFlowEntityNodeBase()
		, m_targetPos(ZERO)
		, m_fwdAxis(eFA_XPlus)
		, m_referenceVector(ZERO)
		, m_targetEntityId(0)
		, m_blendDuration (0.0f)
		, m_bIsBlending(false)
	{}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { pActInfo->m_pUserData = (void*)(UINT_PTR)m_entityId; return new CFlowNode_EntityFaceAt(pActInfo); }

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<EntityId>("target", _HELP("face direction of target [EntityID]"), _HELP("Target")),
			InputPortConfig<Vec3>("pos", Vec3(ZERO), _HELP("target this position [Vec3]")),
			InputPortConfig<SFlowSystemVoid>("Activate", _HELP("start trigger")),
			InputPortConfig<SFlowSystemVoid>("Deactivate", _HELP("stop trigger")),
			InputPortConfig<int>("FwdDir", eFA_XPlus, _HELP("Axis that will be made to point at the target"), _HELP( "FwdDir" ), "enum_int:X+=0,X-=1,Y+=2,Y-=3,Z+=4,Z-=5"),
			InputPortConfig<Vec3>("ReferenceVec", Vec3( 0.0f, 0.0f, 1.0f ), _HELP("This reference vector represents the desired Up (Z+), unless you're using Z+ or Z- as FwdDir, in which case this vector represents the right vector (X+)")),
			InputPortConfig<float>("BlendSpeed",           0,                                            _HELP("degrees per second. If 0, then there is no blending.")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("current", _HELP("the current directional vector")),
			{ 0 }
		};
		config.sDescription = _HELP("Entity Looks At");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void CalcBlendingStartParams(IEntity* pNodeEntity, float blendSpeed)
	{
		m_timeStart = gEnv->pTimer->GetFrameStartTime();
		m_quatEntityNodeStart = pNodeEntity->GetRotation();

		Matrix34 finalMat;
		CalculateLookAtMatrix(pNodeEntity, finalMat);
		const Quat finalQuat(finalMat);

		Quat qDist = finalQuat * !m_quatEntityNodeStart; // calcs quaternion to go from start to end;
		qDist.Normalize();

		float angDiff = RAD2DEG(2 * acosf(qDist.w));   // and then use the amount of rotation on that quat
		if (angDiff > 180.f)
			angDiff = 360.0f - angDiff;
		m_blendDuration = angDiff / blendSpeed; // only can be here if blendSpeed >0
		if (m_blendDuration < 0.01f)
			m_bIsBlending = false;
	}

	void SnapToTarget(SActivationInfo* pActInfo)
	{
		IEntity* pNodeEntity = GetEntity(pActInfo);
		Matrix34 worldMat;

		CalculateLookAtMatrix(pNodeEntity, worldMat);
		pNodeEntity->SetWorldTM(worldMat);
	}

	void BlendToTarget(SActivationInfo* pActInfo)
	{
		const float timeDone = (gEnv->pTimer->GetFrameStartTime() - m_timeStart).GetSeconds();
		const float blendFactor = timeDone / m_blendDuration;
		if (blendFactor >= 1)
		{
			m_bIsBlending = false;
			SnapToTarget(pActInfo);
		}
		else
		{
			IEntity* pNodeEntity = GetEntity(pActInfo);
			Matrix34 finalMat;
			CalculateLookAtMatrix(pNodeEntity, finalMat);

			const Quat finalQuat(finalMat);
			const Quat blendedQuat = Quat::CreateNlerp(m_quatEntityNodeStart, finalQuat, blendFactor);

			pNodeEntity->SetRotation(blendedQuat);
		}
	}

	void CalculateLookAtMatrix(IEntity* pNodeEntity, Matrix34& resMat /*out*/)
	{
		IEntity* pLookAtEntity = gEnv->pEntitySystem->GetEntity(m_targetEntityId);
		if (pLookAtEntity)
			m_targetPos = pLookAtEntity->GetWorldPos();

		const Vec3& worldPos = pNodeEntity->GetWorldPos();
		Vec3 facingDirection = (m_targetPos - pNodeEntity->GetWorldPos()).GetNormalized();

		Vec3 xAxis(1.0f, 0.0f, 0.0f);
		Vec3 yAxis(0.0f, 1.0f, 0.0f);
		Vec3 zAxis(0.0f, 0.0f, 1.0f);

		switch (m_fwdAxis)
		{
		case eFA_XMinus:
			facingDirection = -facingDirection;
		case eFA_XPlus:
			xAxis = facingDirection;
			yAxis = m_referenceVector ^ xAxis;
			zAxis = xAxis ^ yAxis;
			break;
		case eFA_YMinus:
			facingDirection = -facingDirection;
		case eFA_YPlus:
			yAxis = facingDirection;
			xAxis = yAxis ^ m_referenceVector;
			zAxis = xAxis ^ yAxis;
			break;
		case eFA_ZMinus:
			facingDirection = -facingDirection;
		case eFA_ZPlus:
			zAxis = facingDirection;
			yAxis = zAxis ^ m_referenceVector;
			xAxis = yAxis ^ zAxis;
			break;
		}

		resMat.SetFromVectors(xAxis.GetNormalized(), yAxis.GetNormalized(), zAxis.GetNormalized(), worldPos);
	}

	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event) {}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		CFlowEntityNodeBase::ProcessEvent(event, pActInfo);

		IEntity* pEntity = GetEntity(pActInfo);
		if (!pEntity)
			return;

		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, IN_STOP)) // Stop
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				}

				if (IsPortActive(pActInfo, IN_TARGET))
				{
					m_targetEntityId = GetPortEntityId(pActInfo, IN_TARGET);
				}
				else if (IsPortActive(pActInfo, IN_VECTOR))
				{
					m_targetPos = GetPortVec3(pActInfo, IN_VECTOR);
				}

				if (IsPortActive(pActInfo, IN_FWD_AXIS))
				{
					m_fwdAxis = static_cast<EFwdAxis>(GetPortInt(pActInfo, IN_FWD_AXIS));
				}

				if (IsPortActive(pActInfo, IN_REFERENCE))
				{
					m_referenceVector = GetPortVec3(pActInfo, IN_REFERENCE);
					m_referenceVector.Normalize();
				}

				if (pEntity && IsPortActive(pActInfo, IN_START))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
					const float blendSpeed = GetPortFloat(pActInfo, IN_BLENDSPEED);
					m_bIsBlending = blendSpeed > 0;
					if (m_bIsBlending)
					{
						CalcBlendingStartParams(pEntity, blendSpeed);
					}
				}
				break;
			}

		case eFE_Initialize:
			{
				m_targetEntityId = GetPortEntityId(pActInfo, IN_TARGET);
				m_targetPos = GetPortVec3(pActInfo, IN_VECTOR);
				m_fwdAxis = static_cast<EFwdAxis>(GetPortInt(pActInfo, IN_FWD_AXIS));
				m_referenceVector = GetPortVec3(pActInfo, IN_REFERENCE);
				m_referenceVector.Normalize();
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				break;
			}

		case eFE_Update:
			{
				if (pEntity)
				{
					if (m_bIsBlending)
					{
						BlendToTarget(pActInfo);
					}
					else
					{
						SnapToTarget(pActInfo);
					}

					ActivateOutput(pActInfo, OUT_CURRENT, Vec3(RAD2DEG(Ang3(pEntity->GetRotation()))));
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	Vec3       m_targetPos;
	EFwdAxis   m_fwdAxis;
	Vec3       m_referenceVector;
	EntityId   m_targetEntityId;
	CTimeValue m_timeStart;
	float      m_blendDuration;
	Quat       m_quatEntityNodeStart;
	bool       m_bIsBlending;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_BroadcastEntityEvent : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_SEND,
		IN_EVENT,
		IN_NAME,
	};
	CFlowNode_BroadcastEntityEvent(SActivationInfo* pActInfo) {}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("send",  _HELP("When signal recieved on this input entity event is broadcasted")),
			InputPortConfig<string>("event", _HELP("Entity event to be sent")),
			InputPortConfig<string>("name",  _HELP("Any part of the entity name to receive event")),
			{ 0 }
		};
		config.sDescription = _HELP("This node broadcasts specified event to all entities which names contain sub string in parameter name");
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event != eFE_Activate)
			return;

		if (IsPortActive(pActInfo, IN_SEND))
		{
			const string& sEvent = GetPortString(pActInfo, IN_EVENT);
			const string& sSubName = GetPortString(pActInfo, IN_NAME);
			IEntityItPtr pEntityIterator = gEnv->pEntitySystem->GetEntityIterator();
			pEntityIterator->MoveFirst();
			IEntity* pEntity = 0;
			while (pEntity = pEntityIterator->Next())
			{
				if (strstr(pEntity->GetName(), sSubName) != 0)
				{
					IEntityScriptComponent* pScriptProxy = (IEntityScriptComponent*)pEntity->GetProxy(ENTITY_PROXY_SCRIPT);
					if (pScriptProxy)
						pScriptProxy->CallEvent(sEvent);
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Flow node for entity ID.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityId : public CFlowEntityNodeBase
{
public:
	CFlowNode_EntityId(SActivationInfo* pActInfo) : CFlowEntityNodeBase()
	{
		m_entityId = (EntityId)(UINT_PTR)pActInfo->m_pUserData;
	}
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { pActInfo->m_pUserData = (void*)(UINT_PTR)m_entityId; return new CFlowNode_EntityId(pActInfo); };
	virtual void         GetConfiguration(SFlowNodeConfig& config)
	{
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("Id", _HELP("Entity ID")),
			{ 0 }
		};
		config.sDescription = _HELP("Entity ID");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = 0;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void         OnEntityEvent(IEntity* pEntity, const SEntityEvent& event) {}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		CFlowEntityNodeBase::ProcessEvent(event, pActInfo);

		if (event == eFE_SetEntityId)
		{
			EntityId entityId = GetEntityId(pActInfo);
			if (entityId > 0)
				ActivateOutput(pActInfo, 0, entityId);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	//////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
// Flow node for getting parent's entity ID.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_ParentId : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_ParentId(SActivationInfo* pActInfo) {}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("Get",_HELP("Trigger to get the entity's parent ID")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("parentId", _HELP("Entity ID of the parent entity")),
			{ 0 }
		};
		config.sDescription = _HELP("Get the Entity ID of the Parent of an Entity");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate)
		{
			IEntity* pEntity = pActInfo->pEntity;
			if (!pEntity)
				return;

			if (IsPortActive(pActInfo, 0))
			{
				IEntity* pParentEntity = pEntity->GetParent();
				ActivateOutput(pActInfo, 0, pParentEntity? pParentEntity->GetId() : INVALID_ENTITYID);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer * s) const
	{
		s->Add(*this);
	}
};


//////////////////////////////////////////////////////////////////////////
// Flow node for getting an entity's children information
//////////////////////////////////////////////////////////////////////////
class CFlowNode_ChildrenInfo : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_GetCount,
		IN_GetByIndex,
		IN_Index,
	};
	enum EOutputs
	{
		OUT_Count,
		OUT_IndexChildId,
	};

	CFlowNode_ChildrenInfo(SActivationInfo *pActInfo) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("GetChildCount", _HELP("Trigger to get the number of children of the entity")),
			InputPortConfig_Void("GetChildByIndex", _HELP("Trigger to get the ID of a child by index")),
			InputPortConfig<int>("ChildIndex", _HELP("Index to be used when getting a child by ID")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("ChildCount", _HELP("Number of children that this entity has")),
			OutputPortConfig<EntityId>("ChildId", _HELP("ID of the child with the given index")),
			{0}
		};
		config.sDescription = _HELP("Gets information (count and IDs) of an entity's children");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo *pActInfo)
	{
		if (event == eFE_Activate)
		{
			IEntity* pEntity = pActInfo->pEntity;
			if (!pEntity)
				return;

			if (IsPortActive(pActInfo, IN_GetCount))
			{
				ActivateOutput(pActInfo, OUT_Count, pEntity->GetChildCount());
			}

			if (IsPortActive(pActInfo, IN_GetByIndex))
			{
				int index = GetPortInt(pActInfo, IN_Index);
				IEntity* pChild = pEntity->GetChild(index);
				ActivateOutput(pActInfo, OUT_IndexChildId, pChild ? pChild->GetId() : INVALID_ENTITYID);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Flow node for setting an entity property
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntitySetProperty : public CFlowEntityNodeBase
{
public:
	enum EInputs
	{
		IN_ACTIVATE,
		IN_SMARTNAME,
		IN_VALUE,
		IN_PERARCHETYPE,
	};
	CFlowNode_EntitySetProperty(SActivationInfo* pActInfo) : CFlowEntityNodeBase()
	{
		m_entityId = (EntityId)(UINT_PTR)pActInfo->m_pUserData;
	}
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { pActInfo->m_pUserData = (void*)(UINT_PTR)m_entityId; return new CFlowNode_EntitySetProperty(pActInfo); };
	virtual void         GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<SFlowSystemVoid>("Set",_HELP("Trigger it to set the property.")),
			InputPortConfig<string>("entityProperties_Property", _HELP("select entity property"), 0, _UICONFIG("ref_entity=entityId")),
			InputPortConfig<string>("Value",_HELP("Property string Value")),
			InputPortConfig<bool>("perArchetype",true,_HELP("False: property is a per instance property True: property is a per archetype property.")),
			{0}
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Error", _HELP("")),
			{ 0 }
		};

		config.sDescription = _HELP("Change entity property value. WARNING!: THIS PROPERTY CHANGE WONT WORK WITH SAVELOAD");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	SmartScriptTable DoResolveScriptTable(IScriptTable* pTable, const string& key, string& outKey)
	{
		ScriptAnyValue value;

		string token;
		int pos = 0;
		token = key.Tokenize(".", pos);
		pTable->GetValueAny(token, value);

		token = key.Tokenize(".", pos);
		if (token.empty())
			return 0;

		string nextToken = key.Tokenize(".", pos);
		while (nextToken.empty() == false)
		{
			if (value.GetType() != EScriptAnyType::Table)
				return 0;
			ScriptAnyValue temp;
			value.GetScriptTable()->GetValueAny(token, temp);
			value = temp;
			token = nextToken;
			nextToken = token.Tokenize(".", pos);
		}
		outKey = token;
		return value.GetScriptTable();
	}

	SmartScriptTable ResolveScriptTable(IScriptTable* pTable, const char* sKey, bool bPerArchetype, string& outKey)
	{
		string key = bPerArchetype ? "Properties." : "PropertiesInstance.";
		key += sKey;
		return DoResolveScriptTable(pTable, key, outKey);
	}

	void         OnEntityEvent(IEntity* pEntity, const SEntityEvent& event) {}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		CFlowEntityNodeBase::ProcessEvent(event, pActInfo);

		switch (event)
		{
		case eFE_Activate:
			OnActivate(pActInfo);
			break;

		case eFE_Initialize:
			OnInitialize(pActInfo);
			break;
		}
	}

	void OnInitialize(SActivationInfo* pActInfo)
	{
		if (!s_propertyHistoryMap.empty())
		{
			for (const TPropertyHistoryMap::value_type& entry : s_propertyHistoryMap)
			{
				const EntityId entityId = entry.first.entityId;
				if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId))
				{
					if (IScriptTable* pScriptTable = pEntity->GetScriptTable())
					{
						const string& propertyName = entry.first.propertyName;
						const bool bPerArchetype = entry.first.bPerArchetype;
						const ScriptAnyValue& value = entry.second;

						string realPropertyName;
						if (SmartScriptTable smartScriptTable = ResolveScriptTable(pScriptTable, propertyName, bPerArchetype, realPropertyName))
						{
							smartScriptTable->SetValueAny(realPropertyName.c_str(), value);

							if (pScriptTable->HaveValue("OnPropertyChange"))
							{
								Script::CallMethod(pScriptTable, "OnPropertyChange");
							}
						}
					}
				}
			}
			s_propertyHistoryMap.clear();
		}
	}

	void OnActivate(SActivationInfo* pActInfo)
	{
		IEntity* pEntity = GetEntity(pActInfo);
		if (!pEntity)
			return;

		const bool bValuePort = IsPortActive(pActInfo, IN_SMARTNAME);
		const bool bTriggerPort = IsPortActive(pActInfo, IN_ACTIVATE);

		if (bValuePort || bTriggerPort)
		{
			if (IScriptTable* pScriptTable = pEntity->GetScriptTable())
			{
				const string inputPropertyName = GetPortString(pActInfo, IN_SMARTNAME);
				const string inputPropertyValue = GetPortString(pActInfo, IN_VALUE);
				const bool bPerArchetype = GetPortBool(pActInfo, IN_PERARCHETYPE);

				const size_t pos = inputPropertyName.find_first_of(":");
				string plainPropertyName = inputPropertyName.substr(pos + 1, inputPropertyName.length() - pos);
				plainPropertyName.replace(":", ".");

				string realPropertyName;
				SmartScriptTable smartScriptTable = ResolveScriptTable(pScriptTable, plainPropertyName.c_str(), bPerArchetype, realPropertyName);

				if (!smartScriptTable)
				{
					GameWarning("[flow] CFlowNode_EntitySetProperty: Cannot resolve property '%s' in entity '%s'",
					            plainPropertyName.c_str(), pEntity->GetName());
					ActivateOutput(pActInfo, 0, 0);
					return;
				}

				if (gEnv->IsEditor())
				{
					SPropertyKey propertyKey = { pEntity->GetId(), plainPropertyName, bPerArchetype };
					TPropertyHistoryMap::iterator it = s_propertyHistoryMap.find(propertyKey);

					if (it == s_propertyHistoryMap.end())
					{
						ScriptAnyValue anyValue;
						const ScriptVarType scriptVarType = smartScriptTable->GetValueType(realPropertyName.c_str());

						if (scriptVarType == svtObject)
						{
							if (GetColorAnyValue(smartScriptTable, realPropertyName, anyValue))
							{
								s_propertyHistoryMap[propertyKey] = anyValue;
							}
						}
						else if (scriptVarType == svtBool || scriptVarType == svtNumber || scriptVarType == svtString)
						{
							if (smartScriptTable->GetValueAny(realPropertyName.c_str(), anyValue))
							{
								s_propertyHistoryMap[propertyKey] = anyValue;
							}
						}
					}
				}

				ChangeProperty(pActInfo, pEntity, pScriptTable, smartScriptTable, realPropertyName, inputPropertyValue);
			}
		}
	}

	void ChangeProperty(SActivationInfo* pActInfo, IEntity* pEntity, IScriptTable* pScriptTable, SmartScriptTable& smartScriptTable, const string& propertyName, const char* propertyValue)
	{
		const ScriptVarType scriptVarType = smartScriptTable->GetValueType(propertyName.c_str());

		switch (scriptVarType)
		{
		case svtNull:
			{
				GameWarning("[flow] CFlowNode_EntitySetProperty: Cannot resolve property '%s' in entity '%s' -> Creating",
				            propertyName.c_str(), pEntity->GetName());
				ActivateOutput(pActInfo, 0, 0);
				break;
			}

		case svtNumber:
			{
				float fValue = (float)atof(propertyValue);
				smartScriptTable->SetValue(propertyName.c_str(), fValue);
				if (pScriptTable->HaveValue("OnPropertyChange"))
					Script::CallMethod(pScriptTable, "OnPropertyChange");
				break;
			}

		case svtBool:
			{
				float fValue = (float)atof(propertyValue);
				bool bVal = (bool)(fabs(fValue) > 0.001f);
				smartScriptTable->SetValue(propertyName.c_str(), bVal);
				if (pScriptTable->HaveValue("OnPropertyChange"))
					Script::CallMethod(pScriptTable, "OnPropertyChange");
				break;
			}

		case svtString:
			{
				smartScriptTable->SetValue(propertyName.c_str(), propertyValue);
				if (pScriptTable->HaveValue("OnPropertyChange"))
					Script::CallMethod(pScriptTable, "OnPropertyChange");
				break;
			}

		case svtObject:
			{
				if (IsColorType(propertyName))
				{
					Vec3 clrValue(ZERO);
					StringToVec3(propertyValue, clrValue);

					smartScriptTable->SetValue(propertyName.c_str(), clrValue);
					if (pScriptTable->HaveValue("OnPropertyChange"))
						Script::CallMethod(pScriptTable, "OnPropertyChange");
				}
				break;
			}
		}
	}

	inline bool IsColorType(const string& propertyName) const
	{
		return strcmp(propertyName.substr(propertyName.find_last_of(".") + 1, 3), "clr") == 0;
	}

	void StringToVec3(const string& propertyValue, Vec3& clrValue) const
	{
		const size_t commaFirst = propertyValue.find_first_of(",");
		const size_t commaLast = propertyValue.find_last_of(",");

		clrValue.x = (float)atof(propertyValue.substr(0, commaFirst));
		clrValue.y = (float)atof(propertyValue.substr(commaFirst + 1, commaLast));
		clrValue.z = (float)atof(propertyValue.substr(commaLast + 1, propertyValue.length()));

		clrValue.x = fabs(min(clrValue.x, 255.f));
		clrValue.y = fabs(min(clrValue.y, 255.f));
		clrValue.z = fabs(min(clrValue.z, 255.f));

		clrValue = clrValue / 255.f;
	}

	bool GetColorAnyValue(SmartScriptTable& smartScriptTable, const string& propertyName, ScriptAnyValue& outAnyValue) const
	{
		if (IsColorType(propertyName))
		{
			if (smartScriptTable->GetValueAny(propertyName.c_str(), outAnyValue))
			{
				Vec3 temp;
				if (outAnyValue.CopyFromTableToXYZ(temp.x, temp.y, temp.z))
				{
					outAnyValue.SetVector(temp);
					return true;
				}
			}
		}
		return false;
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	struct SPropertyKey
	{
		inline bool operator<(const SPropertyKey& other) const
		{
			return (entityId < other.entityId) || (entityId == other.entityId
			                                       && ((bPerArchetype < other.bPerArchetype)
			                                           || (bPerArchetype == other.bPerArchetype && propertyName < other.propertyName))
			                                       );
		}
		EntityId entityId;
		string   propertyName;
		bool     bPerArchetype;
	};

	typedef std::map<SPropertyKey, ScriptAnyValue> TPropertyHistoryMap;
	static TPropertyHistoryMap s_propertyHistoryMap;

	//////////////////////////////////////////////////////////////////////////
};

CFlowNode_EntitySetProperty::TPropertyHistoryMap CFlowNode_EntitySetProperty::s_propertyHistoryMap;

//////////////////////////////////////////////////////////////////////////
// Flow node for getting an entity property value
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityGetProperty : public CFlowEntityNodeBase
{
public:
	enum EInputs
	{
		IN_ACTIVATE,
		IN_SMARTNAME,
		IN_PER_ARCHETYPE,
	};

	CFlowNode_EntityGetProperty(SActivationInfo* pActInfo) : CFlowEntityNodeBase()
	{
		m_entityId = (EntityId)(UINT_PTR)pActInfo->m_pUserData;
	}
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { pActInfo->m_pUserData = (void*)(UINT_PTR)m_entityId; return new CFlowNode_EntityGetProperty(pActInfo); };
	virtual void         GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<SFlowSystemVoid>("Get", _HELP("Trigger it to get the property!")),
			InputPortConfig<string>("entityProperties_Property", _HELP("select entity property"), 0, _UICONFIG("ref_entity=entityId")),
			InputPortConfig<bool>("perArchetype", true, _HELP("False: property is a per instance property True: property is a per archetype property.")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Value", _HELP("Value of the property")),
			OutputPortConfig_AnyType("Error", _HELP("")),
			{ 0 }
		};
		config.sDescription = _HELP("Retrieve entity property value");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void         OnEntityEvent(IEntity* pEntity, const SEntityEvent& event) {}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		CFlowEntityNodeBase::ProcessEvent(event, pActInfo);

		IEntity* pEntity = GetEntity(pActInfo);
		if (!pEntity)
			return;

		if (event == eFE_Activate)
		{
			if (IsPortActive(pActInfo, IN_SMARTNAME) || IsPortActive(pActInfo, IN_ACTIVATE))
			{
				bool bPerArchetype = GetPortBool(pActInfo, IN_PER_ARCHETYPE);

				IScriptTable* pTable = pEntity->GetScriptTable();
				if (!pTable)
					return;

				string inputValue = GetPortString(pActInfo, IN_SMARTNAME);
				string selectedParameter = inputValue.substr(inputValue.find_first_of(":") + 1, (inputValue.length() - inputValue.find_first_of(":")));

				const char* pKey = (selectedParameter.replace(":", ".")).c_str();
				const char* pPropertiesString = bPerArchetype ? "Properties" : "PropertiesInstance";

				// requested property is a vector?
				if (strcmp(selectedParameter.substr(selectedParameter.find_last_of(".") + 1, 3), "clr") == 0)
				{
					Vec3 vecValue(ZERO);

					for (int i = 0; i < 3; i++)
					{
						ScriptAnyValue value;
						char newKey[32];

						cry_strcpy(newKey, pKey);

						if (i == 0) cry_strcat(newKey, ".x");
						if (i == 1) cry_strcat(newKey, ".y");
						if (i == 2) cry_strcat(newKey, ".z");

						if (ReadScriptValue(pTable, pPropertiesString, newKey, value))
						{
							float fVal;
							value.CopyTo(fVal);

							if (i == 0) vecValue.x = fVal;
							if (i == 1) vecValue.y = fVal;
							if (i == 2) vecValue.z = fVal;
						}
						else
						{
							ActivateOutput(pActInfo, 1, 0);
							return;
						}
					}
					ActivateOutput(pActInfo, 0, vecValue * 255.f);
					return;
				}

				ScriptAnyValue value;
				bool isValid = ReadScriptValue(pTable, pPropertiesString, pKey, value);

				if (!isValid)
				{
					GameWarning("[flow] CFlowNode_EntityGetProperty: Could not find property '%s.%s' in entity '%s'", pPropertiesString, pKey, pEntity->GetName());
					ActivateOutput(pActInfo, 1, isValid);
					return;
				}

				switch (value.GetVarType())
				{
				case svtNumber:
					{
						float fVal;
						value.CopyTo(fVal);

						// TODO: fix wrong number return type for booleans
						if (strcmp(selectedParameter.substr(selectedParameter.find_last_of(".") + 1, 1), "b") == 0)
						{
							bool bVal = (bool)(fabs(fVal) > 0.001f);
							ActivateOutput(pActInfo, 0, bVal);
						}
						else
						{
							ActivateOutput(pActInfo, 0, fVal);
						}
						break;
					}

				case svtBool:
					{
						bool val;
						value.CopyTo(val);
						ActivateOutput(pActInfo, 0, val);
						break;
					}

				case svtString:
					{
						const char* pVal = NULL;
						value.CopyTo(pVal);
						ActivateOutput(pActInfo, 0, string(pVal));
						break;
					}

				default:
					{
						GameWarning("[flow] CFlowNode_EntityGetProperty: property '%s.%s' in entity '%s' is of unexpected type('%d')", pPropertiesString, pKey, pEntity->GetName(), value.GetVarType());
						ActivateOutput(pActInfo, 1, 0);
						return;
					}
				}
			}
		}
	}

	static bool ReadScriptValue(IScriptTable* pTable, const char* pPropertiesString, const char* pKey, ScriptAnyValue& value)
	{
		pTable->GetValueAny(pPropertiesString, value);

		int pos = 0;
		string key = pKey;
		string nextToken = key.Tokenize(".", pos);
		while (!nextToken.empty() && value.GetType() == EScriptAnyType::Table)
		{
			ScriptAnyValue temp;
			value.GetScriptTable()->GetValueAny(nextToken, temp);
			value = temp;
			nextToken = key.Tokenize(".", pos);
		}

		return nextToken.empty() && (value.GetType() == EScriptAnyType::Number || value.GetType() == EScriptAnyType::Boolean || value.GetType() == EScriptAnyType::String);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	//////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
// Flow node for Attaching child to parent.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityAttachChild : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_EntityAttachChild(SActivationInfo* pActInfo) {}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Attach", _HELP("Trigger to attach entity")),
			InputPortConfig<EntityId>("Child", _HELP("Child Entity to Attach")),
			InputPortConfig<bool>("KeepTransform", _HELP("Child entity will remain in the same transformation in world space")),
			InputPortConfig<bool>("DisablePhysics", false, _HELP("Force disable physics of child entity on attaching")),
			{ 0 }
		};
		config.sDescription = _HELP("Attach Child Entity");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event != eFE_Activate)
			return;
		if (!pActInfo->pEntity)
			return;
		if (IsPortActive(pActInfo, 0))
		{
			EntityId nChildId = GetPortEntityId(pActInfo, 1);
			IEntity* pChild = gEnv->pEntitySystem->GetEntity(nChildId);
			if (pChild)
			{
				int nFlags = 0;
				if (GetPortBool(pActInfo, 2))
					nFlags |= IEntity::ATTACHMENT_KEEP_TRANSFORMATION;
				if (GetPortBool(pActInfo, 3))
				{
					pChild->EnablePhysics(false);
				}
				pActInfo->pEntity->AttachChild(pChild, nFlags);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Flow node for detaching child from parent.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityDetachThis : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_EntityDetachThis(SActivationInfo* pActInfo) {}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Detach", _HELP("Trigger to detach entity from parent")),
			InputPortConfig<bool>("KeepTransform", _HELP("When attaching entity will stay in same transformation in world space")),
			InputPortConfig<bool>("EnablePhysics", false, _HELP("Force enable physics of entity after detaching")),
			{ 0 }
		};
		config.sDescription = _HELP("Detach child from its parent");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event != eFE_Activate)
			return;
		if (!pActInfo->pEntity)
			return;
		if (IsPortActive(pActInfo, 0))
		{
			int nFlags = 0;
			if (GetPortBool(pActInfo, 1))
				nFlags |= IEntity::ATTACHMENT_KEEP_TRANSFORMATION;
			if (GetPortBool(pActInfo, 2))
			{
				pActInfo->pEntity->EnablePhysics(true);
			}
			pActInfo->pEntity->DetachThis(nFlags);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Flow node for beaming an entity
//////////////////////////////////////////////////////////////////////////
class CFlowNode_BeamEntity : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_BeamEntity(SActivationInfo* pActInfo) {}

	enum EInputPorts
	{
		EIP_Beam = 0,
		EIP_CoordSys,
		EIP_Pos,
		EIP_Rot,
		EIP_UseZeroRot,
		EIP_Scale,
	};

	enum EOutputPorts
	{
		EOP_Done = 0,
	};

	enum ECoordSys
	{
		CS_PARENT = 0,
		CS_WORLD,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Beam", _HELP("Trigger to beam the Entity")),
			InputPortConfig<int>("CoordSys", 1, _HELP("In which coordinate system the values are expressed"), _HELP("CoordSys"), _UICONFIG("enum_int:Parent=0,World=1")),
			InputPortConfig<Vec3>("Position", _HELP("Position in World Coords")),
			InputPortConfig<Vec3>("Rotation", _HELP("Rotation [Degrees] in World Coords. (0,0,0) leaves Rotation untouched, unless UseZeroRot is set to 1.")),
			InputPortConfig<bool>("UseZeroRot", false, _HELP("If true, rotation is applied even if is (0,0,0)")),
			InputPortConfig<Vec3>("Scale", _HELP("Scale. (0,0,0) leaves Scale untouched.")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Done", _HELP("Triggered when done")),
			{ 0 }
		};

		config.sDescription = _HELP("Beam an Entity to a new Position");
		config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event != eFE_Activate)
			return;
		if (!pActInfo->pEntity)
			return;

		if (IsPortActive(pActInfo, EIP_Beam))
		{
			const char* entityName = pActInfo->pEntity->GetName();
			bool isPlayer = gEnv->pGameFramework->GetClientActorId() == pActInfo->pEntity->GetId();

			const Vec3 vPrevSca = pActInfo->pEntity->GetScale();

			const Vec3 vPos = GetPortVec3(pActInfo, EIP_Pos);
			const Vec3 vRot = GetPortVec3(pActInfo, EIP_Rot);
			const Vec3 vSca = GetPortVec3(pActInfo, EIP_Scale);
			bool bUseZeroRot = GetPortBool(pActInfo, EIP_UseZeroRot);
			ECoordSys coordSys = (ECoordSys)GetPortInt(pActInfo, EIP_CoordSys);

			switch (coordSys)
			{
				// World & parent: initialize tm with the current value. overwrite with the orientation if valid and always overwrite position. scale goes last.
			case CS_WORLD:
				{
					Matrix34 tm = pActInfo->pEntity->GetWorldTM();

					if (!vRot.IsZero() || bUseZeroRot)
					{
						tm = Matrix33(Quat::CreateRotationXYZ(Ang3(DEG2RAD(vRot))));
					}

					tm.SetTranslation(vPos);
					if (vPos.IsZero())
					{
						CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "BeamEntity Teleported %s to vPos zero.", entityName);
					}

					pActInfo->pEntity->SetWorldTM(tm, ENTITY_XFORM_TRACKVIEW);

					break;
				}
			case CS_PARENT:
				{
					Matrix34 tm = pActInfo->pEntity->GetLocalTM();

					if (!vRot.IsZero() || bUseZeroRot)
					{
						tm = Matrix33(Quat::CreateRotationXYZ(Ang3(DEG2RAD(vRot))));
					}

					tm.SetTranslation(vPos);
					// No warning for vPos zero in parent coordinates (it is likely intended).

					pActInfo->pEntity->SetLocalTM(tm, ENTITY_XFORM_TRACKVIEW);

					break;
				}
			}

			if (!vSca.IsZero())
			{
				pActInfo->pEntity->SetScale(vSca, ENTITY_XFORM_TRACKVIEW);
			}
			else
			{
				// if a valid scale is not supplied, reaply the previous value
				pActInfo->pEntity->SetScale(vPrevSca, ENTITY_XFORM_TRACKVIEW);
			}

			// TODO: Maybe add some tweaks/checks wrt. physics/collisions
			ActivateOutput(pActInfo, EOP_Done, true);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Flow node for getting entity info.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityGetInfo : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_EntityGetInfo(SActivationInfo* pActInfo) {}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Get", _HELP("Trigger to get info")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<EntityId>("Id",      _HELP("Entity Id")),
			OutputPortConfig<string>("Name",      _HELP("Entity Name")),
			OutputPortConfig<string>("Class",     _HELP("Entity Class")),
			OutputPortConfig<string>("Archetype", _HELP("Entity Archetype")),
			{ 0 }
		};
		config.sDescription = _HELP("Get Entity Information");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event != eFE_Activate)
			return;
		IEntity* pEntity = pActInfo->pEntity;
		if (pEntity == 0)
			return;
		if (IsPortActive(pActInfo, 0))
		{
			string empty;
			ActivateOutput(pActInfo, 0, pEntity->GetId());
			string temp(pEntity->GetName());
			ActivateOutput(pActInfo, 1, temp);
			temp = pEntity->GetClass()->GetName();
			ActivateOutput(pActInfo, 2, temp);
			IEntityArchetype* pArchetype = pEntity->GetArchetype();
			if (pArchetype)
				temp = pArchetype->GetName();
			ActivateOutput(pActInfo, 3, pArchetype ? temp : empty);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

////////////////////////////////////////////////////////////////////////////////////////
// Flow node for setting entity render parameters (opacity, glow, motionblur, etc)
////////////////////////////////////////////////////////////////////////////////////////
class CFlowNodeRenderParams : public CFlowEntityNodeBase
{
public:
	CFlowNodeRenderParams(SActivationInfo* pActInfo) : CFlowEntityNodeBase()
	{
		m_entityId = (EntityId)(UINT_PTR)pActInfo->m_pUserData;
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { pActInfo->m_pUserData = (void*)(UINT_PTR)m_entityId; return new CFlowNodeRenderParams(pActInfo); };

	enum EInputPorts
	{
		EIP_Trigger,
		EIP_ParamFloatName,
		EIP_ParamFloatValue,
	};

	enum EOutputPorts
	{
		EOP_Success,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Set", _HELP("Trigger to activate the node")),
			InputPortConfig<int>("Param", 0, _HELP("Render parameter to set (of type float)") , NULL, _UICONFIG("enum_int:None=0,Opacity=1")),
			InputPortConfig<float>("Value", 1.0f, _HELP("Value to attribute to the render parameter (type float)")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("Success", _HELP("Triggers when the node is processed, outputing 0 or 1 representing fail or success")),
			{ 0 }
		};

		config.sDescription = _HELP("Set render specific parameters via the entity's render proxy");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		IEntity* pEntity = GetEntity(pActInfo);
		if (!pEntity)
			return;

		if (ser.IsWriting())
		{
			float opacity = pEntity->GetOpacity();
			ser.Value("opacity", opacity);
		}
		else
		{
			float opacity;
			ser.Value("opacity", opacity);
			pEntity->SetOpacity(opacity);
		}
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		CFlowEntityNodeBase::ProcessEvent(event, pActInfo);

		if (event != eFE_Activate)
		{
			return;
		}

		IEntity* pEntity = GetEntity(pActInfo);
		if (!pEntity)
		{
			return;
		}

		if (IsPortActive(pActInfo, EIP_Trigger))
		{
			bool success = false;
			int paramType = GetPortInt(pActInfo, EIP_ParamFloatName);
			float paramValue = GetPortFloat(pActInfo, EIP_ParamFloatValue);
			switch (paramType)
			{
			case 1: // Opacity
				pEntity->SetOpacity(clamp_tpl(paramValue, 0.0f, 1.0f));
				success = true;
				break;
			default: // No valid parameter chosen, do nothing
				break;
			}
			ActivateOutput(pActInfo, EOP_Success, success);
		}
	}

	void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNodeCharAttachmentMaterialShaderParam : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNodeCharAttachmentMaterialShaderParam(SActivationInfo* pActInfo) : m_pMaterial(0), m_bFailedCloningMaterial(false)
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNodeCharAttachmentMaterialShaderParam(pActInfo);
	};

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		if (ser.IsReading())
		{
			// new chance on load
			m_bFailedCloningMaterial = false;
		}
	}

	enum EInputPorts
	{
		EIP_CharSlot = 0,
		EIP_Attachment,
		EIP_SetMaterial,
		EIP_ForcedMaterial,
		EIP_SubMtlId,
		EIP_Get,
		EIP_ParamFloat,
		EIP_Float,
		EIP_ParamColor,
		EIP_Color,
		EIP_StartFromDefaultMaterial,
	};

	enum EOutputPorts
	{
		EOP_Float = 0,
		EOP_Color,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<int>("CharSlot", 0, _HELP("Character Slot within Entity")),
			InputPortConfig<string>("Attachment", _HELP("Attachment"), 0, _UICONFIG("dt=attachment,ref_entity=entityId")),
			InputPortConfig_Void("SetMaterial", _HELP("Trigger to force setting a material [ForcedMaterial]")),
			InputPortConfig<string>("ForcedMaterial", _HELP("Material"), 0, _UICONFIG("dt=mat")),
			InputPortConfig<int>("SubMtlId", 0, _HELP("Sub Material Id")),
			InputPortConfig_Void("Get", _HELP("Trigger to get current value")),
			InputPortConfig<string>("ParamFloat", _HELP("Float Parameter to be set/get"), 0, _UICONFIG("dt=matparamcharatt,charslot_ref=CharSlot,attachment_ref=Attachment,sub_ref=SubMtlId,param=float")),
			InputPortConfig<float>("ValueFloat", 0.0f, _HELP("Trigger to set Float value")),
			InputPortConfig<string>("ParamColor", _HELP("Color Parameter to be set/get"), 0, _UICONFIG("dt=matparamcharatt,charslot_ref=CharSlot,attachment_ref=Attachment,sub_ref=SubMtlId,param=vec")),
			InputPortConfig<Vec3>("color_ValueColor", _HELP("Trigger to set Color value")),
			InputPortConfig<bool>("StartFromDefaultMaterial", true, _HELP("Choose whether to start again from the attachment's default material or apply on top of a possibly already modified material")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("ValueFloat", _HELP("Current Float Value")),
			OutputPortConfig<Vec3>("ValueColor",  _HELP("Current Color Value")),
			{ 0 }
		};

		config.sDescription = _HELP("[CUTSCENE ONLY] Set ShaderParams of Character Attachments.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event != eFE_Activate)
			return;

		const bool bGet = IsPortActive(pActInfo, EIP_Get);
		const bool bSetFloat = IsPortActive(pActInfo, EIP_Float);
		const bool bSetColor = IsPortActive(pActInfo, EIP_Color);
		const bool bSetMaterial = IsPortActive(pActInfo, EIP_SetMaterial);

		if (!bGet && !bSetFloat && !bSetColor && !bSetMaterial)
			return;

		IEntity* pEntity = pActInfo->pEntity;
		if (pEntity == 0)
			return;

		const int charSlot = GetPortInt(pActInfo, EIP_CharSlot);
		ICharacterInstance* pChar = pActInfo->pEntity->GetCharacter(charSlot);
		if (pChar == 0)
			return;

		IAttachmentManager* pAttMgr = pChar->GetIAttachmentManager();
		if (pAttMgr == 0)
			return;

		const string& attachment = GetPortString(pActInfo, EIP_Attachment);
		IAttachment* pAttachment = pAttMgr->GetInterfaceByName(attachment.c_str());
		if (pAttachment == 0)
		{
			GameWarning("[flow] CFlowNodeCharAttachmentMaterialShaderParam: Entity '%s' [%d] Cannot access Attachment '%s' [CharSlot=%d]", pEntity->GetName(), pEntity->GetId(), attachment.c_str(), charSlot);
			return;
		}

		IAttachmentObject* pAttachObj = pAttachment->GetIAttachmentObject();
		if (pAttachObj == 0)
		{
			GameWarning("[flow] CFlowNodeCharAttachmentMaterialShaderParam: Entity '%s' [%d] Cannot access AttachmentObject at Attachment '%s' [CharSlot=%d]", pEntity->GetName(), pEntity->GetId(), attachment.c_str(), charSlot);
			return;
		}

		IMaterialManager* pMatMgr = gEnv->p3DEngine->GetMaterialManager();
		if (bSetMaterial)
		{
			const string& matName = GetPortString(pActInfo, EIP_ForcedMaterial);
			if (true /* m_pMaterial == 0 || matName != m_pMaterial->GetName() */) // always reload the mat atm
			{
				m_pMaterial = pMatMgr->LoadMaterial(matName.c_str());
				if (m_pMaterial)
				{
					m_pMaterial = pMatMgr->CloneMultiMaterial(m_pMaterial);
					m_bFailedCloningMaterial = false;
				}
				else
				{
					m_bFailedCloningMaterial = true;
				}
			}
		}

		if (m_pMaterial == 0 && m_bFailedCloningMaterial == false)
		{
			const bool bStartFromDefaultMaterial = GetPortBool(pActInfo, EIP_StartFromDefaultMaterial);
			IMaterial* pOldMtl = bStartFromDefaultMaterial ? pAttachObj->GetBaseMaterial() : pAttachObj->GetReplacementMaterial();
			if (pOldMtl)
			{
				m_pMaterial = pMatMgr->CloneMultiMaterial(pOldMtl);
				m_bFailedCloningMaterial = false;
			}
			else
			{
				m_bFailedCloningMaterial = true;
			}
		}

		IMaterial* pMtl = m_pMaterial;
		if (pMtl == 0)
		{
			GameWarning("[flow] CFlowNodeCharAttachmentMaterialShaderParam: Entity '%s' [%d] Cannot access material at Attachment '%s' [CharSlot=%d]", pEntity->GetName(), pEntity->GetId(), attachment.c_str(), charSlot);
			return;
		}

		const int& subMtlId = GetPortInt(pActInfo, EIP_SubMtlId);
		pMtl = pMtl->GetSafeSubMtl(subMtlId);
		if (pMtl == 0)
		{
			GameWarning("[flow] CFlowNodeCharAttachmentMaterialShaderParam: Entity '%s' [%d] Material '%s' has no sub-material %d", pEntity->GetName(), pEntity->GetId(), m_pMaterial->GetName(), subMtlId);
			return;
		}

		// set our material
		pAttachObj->SetReplacementMaterial(pMtl);

		const string& paramNameFloat = GetPortString(pActInfo, EIP_ParamFloat);
		const string& paramNameVec3 = GetPortString(pActInfo, EIP_ParamColor);

		const SShaderItem& shaderItem = pMtl->GetShaderItem();
		IRenderShaderResources* pRendRes = shaderItem.m_pShaderResources;
		if (pRendRes == 0)
		{
			GameWarning("[flow] CFlowNodeCharAttachmentMaterialShaderParam: Entity '%s' [%d] Material '%s' (sub=%d) at Attachment '%s' [CharSlot=%d] has no render resources",
			            pEntity->GetName(), pEntity->GetId(), pMtl->GetName(), subMtlId, attachment.c_str(), charSlot);
			return;
		}
		DynArrayRef<SShaderParam>& params = pRendRes->GetParameters(); // pShader->GetPublicParams();

		bool bUpdateShaderConstants = false;
		float floatValue = 0.0f;
		Vec3 vec3Value = Vec3(ZERO);
		if (bSetFloat)
		{
			floatValue = GetPortFloat(pActInfo, EIP_Float);
			if (pMtl->SetGetMaterialParamFloat(paramNameFloat, floatValue, false) == false)
			{
				UParamVal val;
				val.m_Float = floatValue;
				SShaderParam::SetParam(paramNameFloat, &params, val);
				bUpdateShaderConstants = true;
			}
		}
		if (bSetColor)
		{
			vec3Value = GetPortVec3(pActInfo, EIP_Color);
			if (pMtl->SetGetMaterialParamVec3(paramNameVec3, vec3Value, false) == false)
			{
				UParamVal val;
				val.m_Color[0] = vec3Value[0];
				val.m_Color[1] = vec3Value[1];
				val.m_Color[2] = vec3Value[2];
				val.m_Color[3] = 1.0f;
				SShaderParam::SetParam(paramNameVec3, &params, val);
				bUpdateShaderConstants = true;
			}
		}
		if (bUpdateShaderConstants)
		{
			shaderItem.m_pShaderResources->UpdateConstants(shaderItem.m_pShader);
		}
		if (bGet)
		{
			if (pMtl->SetGetMaterialParamFloat(paramNameFloat, floatValue, true))
				ActivateOutput(pActInfo, EOP_Float, floatValue);
			if (pMtl->SetGetMaterialParamVec3(paramNameVec3, vec3Value, true))
				ActivateOutput(pActInfo, EOP_Color, vec3Value);

			for (int i = 0; i < params.size(); ++i)
			{
				SShaderParam& param = params[i];
				if (stricmp(param.m_Name, paramNameFloat) == 0)
				{
					float val = 0.0f;
					switch (param.m_Type)
					{
					case eType_BOOL:
						val = param.m_Value.m_Bool;
						break;
					case eType_SHORT:
						val = param.m_Value.m_Short;
						break;
					case eType_INT:
						val = (float)param.m_Value.m_Int;
						break;
					case eType_FLOAT:
						val = param.m_Value.m_Float;
						break;
					default:
						break;
					}
					ActivateOutput(pActInfo, EOP_Float, val);
				}
				if (stricmp(param.m_Name, paramNameVec3) == 0)
				{
					Vec3 val(ZERO);
					if (param.m_Type == eType_VECTOR)
					{
						val.Set(param.m_Value.m_Vector[0], param.m_Value.m_Vector[1], param.m_Value.m_Vector[2]);
					}
					if (param.m_Type == eType_FCOLOR)
					{
						val.Set(param.m_Value.m_Color[0], param.m_Value.m_Color[1], param.m_Value.m_Color[2]);
					}
					ActivateOutput(pActInfo, EOP_Color, val);
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	_smart_ptr<IMaterial> m_pMaterial;
	bool                  m_bFailedCloningMaterial;
};

class CFlowNode_SpawnEntity : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputPorts
	{
		EIP_Spawn,
		EIP_ClassName,
		EIP_Name,
		EIP_Pos,
		EIP_Rot,
		EIP_Scale,
		EIP_Properties,
		EIP_PropertiesInstance,
	};

	enum EOutputPorts
	{
		EOP_Done,
		EOP_Succeeded,
		EOP_Failed,
	};

public:
	////////////////////////////////////////////////////
	CFlowNode_SpawnEntity(SActivationInfo* pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowNode_SpawnEntity(void)
	{

	}

	////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_Void("Spawn", _HELP("Spawn an entity using the values below")),
			InputPortConfig<string>("Class", "", _HELP("Entity class name i.e., \"BasicEntity\""), 0, 0),
			InputPortConfig<string>("Name", "", _HELP("Entity's name"), 0, 0),
			InputPortConfig<Vec3>("Pos", _HELP("Initial position")),
			InputPortConfig<Vec3>("Rot", _HELP("Initial rotation")),
			InputPortConfig<Vec3>("Scale", Vec3(1,1,1), _HELP("Initial scale")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig_Void("Done", _HELP("Called when job is done")),
			OutputPortConfig<EntityId>("Succeeded", _HELP("Called when entity is spawned")),
			OutputPortConfig_Void("Failed", _HELP("Called when entity fails to spawn")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Spawns an entity with the specified properties");
		config.SetCategory(EFLN_ADVANCED);
	}

	////////////////////////////////////////////////////
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Spawn))
				{
					// Get properties
					string className(GetPortString(pActInfo, EIP_ClassName));
					string name(GetPortString(pActInfo, EIP_Name));
					Vec3 pos(GetPortVec3(pActInfo, EIP_Pos));
					Vec3 rot(GetPortVec3(pActInfo, EIP_Rot));
					Vec3 scale(GetPortVec3(pActInfo, EIP_Scale));

					// Define
					SEntitySpawnParams params;
					params.nFlags = ENTITY_FLAG_SPAWNED;
					params.pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(className.c_str());
					params.sName = name.c_str();
					params.vPosition = pos;
					params.vScale = scale;

					Matrix33 mat;
					Ang3 ang(DEG2RAD(rot.x), DEG2RAD(rot.y), DEG2RAD(rot.z));
					mat.SetRotationXYZ(ang);
					params.qRotation = Quat(mat);

					// Create
					IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(params);
					if (NULL == pEntity)
						ActivateOutput(pActInfo, EOP_Failed, true);
					else
						ActivateOutput(pActInfo, EOP_Succeeded, pEntity->GetId());
					ActivateOutput(pActInfo, EOP_Done, true);
				}
			}
			break;
		case eFE_PrecacheResources:
			{
				if (IGame* pGame = gEnv->pGameFramework->GetIGame())
				{
					if (IGame::IResourcesPreCache* pResourceCache = pGame->GetResourceCache())
					{
						pResourceCache->QueueEntityClass(GetPortString(pActInfo, EIP_ClassName));
					}
				}
			}
			break;
		}
	}

	////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

/////////////////////////////////////////////////////////////////
class CFlowNode_SpawnArchetypeEntity : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputPorts
	{
		EIP_Spawn,
		EIP_ArchetypeName,
		EIP_Name,
		EIP_Pos,
		EIP_Rot,
		EIP_Scale,
	};

	enum EOutputPorts
	{
		EOP_Done,
		EOP_Succeeded,
		EOP_Failed,
	};

public:
	////////////////////////////////////////////////////
	CFlowNode_SpawnArchetypeEntity(SActivationInfo* pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowNode_SpawnArchetypeEntity(void)
	{

	}

	////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_Void("Spawn", _HELP("Spawn an entity using the values below")),
			InputPortConfig<string>("Archetype", "", _HELP("Entity archetype name"), 0, _UICONFIG("enum_global:entity_archetypes")),
			InputPortConfig<string>("Name", "", _HELP("Entity's name"), 0, 0),
			InputPortConfig<Vec3>("Pos", _HELP("Initial position")),
			InputPortConfig<Vec3>("Rot", _HELP("Initial rotation")),
			InputPortConfig<Vec3>("Scale", Vec3(1,1,1), _HELP("Initial scale")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig_Void("Done", _HELP("Called when job is done")),
			OutputPortConfig<EntityId>("Succeeded", _HELP("Called when entity is spawned")),
			OutputPortConfig_Void("Failed", _HELP("Called when entity fails to spawn")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Spawns an archetype entity with the specified properties");
		config.SetCategory(EFLN_ADVANCED);
	}

	////////////////////////////////////////////////////
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Spawn))
				{
					// Get properties
					string archName(GetPortString(pActInfo, EIP_ArchetypeName));
					string name(GetPortString(pActInfo, EIP_Name));
					Vec3 pos(GetPortVec3(pActInfo, EIP_Pos));
					Vec3 rot(GetPortVec3(pActInfo, EIP_Rot));
					Vec3 scale(GetPortVec3(pActInfo, EIP_Scale));

					// Define
					IEntity* pEntity = NULL;
					SEntitySpawnParams params;
					IEntityArchetype* pArchetype = gEnv->pEntitySystem->LoadEntityArchetype(archName.c_str());
					if (NULL != pArchetype)
					{
						params.nFlags = ENTITY_FLAG_SPAWNED;
						params.pArchetype = pArchetype;
						params.sName = name.empty() ? pArchetype->GetName() : name.c_str();
						params.vPosition = pos;
						params.vScale = scale;

						Matrix33 mat;
						Ang3 ang(DEG2RAD(rot.x), DEG2RAD(rot.y), DEG2RAD(rot.z));
						mat.SetRotationXYZ(ang);
						params.qRotation = Quat(mat);

						// Create
						int nCastShadowMinSpec;
						if (XmlNodeRef objectVars = pArchetype->GetObjectVars())
						{
							objectVars->getAttr("CastShadowMinSpec", nCastShadowMinSpec);

							static ICVar* pObjShadowCastSpec = gEnv->pConsole->GetCVar("e_ObjShadowCastSpec");
							if (nCastShadowMinSpec <= pObjShadowCastSpec->GetIVal())
								params.nFlags |= ENTITY_FLAG_CASTSHADOW;

							bool bRecvWind;
							objectVars->getAttr("RecvWind", bRecvWind);
							if (bRecvWind)
								params.nFlags |= ENTITY_FLAG_RECVWIND;

							bool bOutdoorOnly;
							objectVars->getAttr("OutdoorOnly", bOutdoorOnly);
							if (bRecvWind)
								params.nFlags |= ENTITY_FLAG_OUTDOORONLY;

							bool bNoStaticDecals;
							objectVars->getAttr("NoStaticDecals", bNoStaticDecals);
							if (bNoStaticDecals)
								params.nFlags |= ENTITY_FLAG_NO_DECALNODE_DECALS;
						}

						pEntity = gEnv->pEntitySystem->SpawnEntity(params);
						if (pEntity)
						{
							{
								if (XmlNodeRef objectVars = pArchetype->GetObjectVars())
								{
									int nViewDistRatio = 100;
									objectVars->getAttr("ViewDistRatio", nViewDistRatio);
									pEntity->SetViewDistRatio(nViewDistRatio);

									int nLodRatio = 100;
									objectVars->getAttr("lodRatio", nLodRatio);
									pEntity->SetLodRatio(nLodRatio);
								}
							}

							if (XmlNodeRef objectVars = pArchetype->GetObjectVars())
							{
								bool bHiddenInGame = false;
								objectVars->getAttr("HiddenInGame", bHiddenInGame);
								if (bHiddenInGame)
									pEntity->Hide(true);
							}
						}
					}

					if (pEntity == NULL)
						ActivateOutput(pActInfo, EOP_Failed, true);
					else
						ActivateOutput(pActInfo, EOP_Succeeded, pEntity->GetId());
					ActivateOutput(pActInfo, EOP_Done, true);
				}
			}
			break;
		case eFE_PrecacheResources:
			{
				if (IGame* pGame = gEnv->pGameFramework->GetIGame())
				{
					if (IGame::IResourcesPreCache* pResourceCache = pGame->GetResourceCache())
					{
						pResourceCache->QueueEntityArchetype(GetPortString(pActInfo, EIP_ArchetypeName));
					}
				}
			}
			break;
		}
	}

	////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Call a LUA function on an entity
class CFlowNode_CallScriptFunction : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_CALL,
		IN_FUNCNAME,
		IN_ARG1,
		IN_ARG2,
		IN_ARG3,
		IN_ARG4,
		IN_ARG5,
	};
	enum EOutputs
	{
		OUT_SUCCESS,
		OUT_FAILED
	};

	CFlowNode_CallScriptFunction(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		// declare input ports
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("call",         _HELP("Call the function")),
			InputPortConfig<string>("FuncName",  _HELP("Script function name")),
			InputPortConfig_AnyType("Argument1", _HELP("Argument 1")),
			InputPortConfig_AnyType("Argument2", _HELP("Argument 2")),
			InputPortConfig_AnyType("Argument3", _HELP("Argument 3")),
			InputPortConfig_AnyType("Argument4", _HELP("Argument 4")),
			InputPortConfig_AnyType("Argument5", _HELP("Argument 5")),
			{ 0 }
		};
		static const SOutputPortConfig out_ports[] = {
			OutputPortConfig_Void("Success", _HELP("Script function was found and called")),
			OutputPortConfig_Void("Failed",  _HELP("Script function was not found")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Calls a script function on the entity");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (eFE_Activate == event && IsPortActive(pActInfo, IN_CALL))
		{
			if (pActInfo->pEntity)
			{
				//Get entity's scripttable
				IEntityScriptComponent* pScriptProx = (IEntityScriptComponent*)pActInfo->pEntity->GetProxy(ENTITY_PROXY_SCRIPT);
				IScriptTable* pTable = pScriptProx->GetScriptTable();

				if (pTable)
				{
					//Convert all inputports to arguments for lua
					ScriptAnyValue arg1 = FillArgumentFromAnyPort(pActInfo, IN_ARG1);
					ScriptAnyValue arg2 = FillArgumentFromAnyPort(pActInfo, IN_ARG2);
					ScriptAnyValue arg3 = FillArgumentFromAnyPort(pActInfo, IN_ARG3);
					ScriptAnyValue arg4 = FillArgumentFromAnyPort(pActInfo, IN_ARG4);
					ScriptAnyValue arg5 = FillArgumentFromAnyPort(pActInfo, IN_ARG5);

					Script::CallMethod(pTable, GetPortString(pActInfo, IN_FUNCNAME), arg1, arg2, arg3, arg4, arg5);

					ActivateOutput(pActInfo, OUT_SUCCESS, true);
				}
			}

			ActivateOutput(pActInfo, OUT_FAILED, true);
		}
	}

	ScriptAnyValue FillArgumentFromAnyPort(SActivationInfo* pActInfo, int port)
	{
		TFlowInputData inputData = GetPortAny(pActInfo, port);

		switch (inputData.GetType())
		{
		case eFDT_Int:
			return ScriptAnyValue((float)GetPortInt(pActInfo, port));
		case eFDT_EntityId:
			{
				ScriptHandle id;
				id.n = GetPortEntityId(pActInfo, port);
				return ScriptAnyValue(id);
			}
		case eFDT_Bool:
			return ScriptAnyValue(GetPortBool(pActInfo, port));
		case eFDT_Float:
			return ScriptAnyValue(GetPortFloat(pActInfo, port));
		case eFDT_String:
			return ScriptAnyValue(GetPortString(pActInfo, port));
			;
		case eFDT_Vec3:
			return ScriptAnyValue(GetPortVec3(pActInfo, port));
			;
		}

		//Type unknown
		assert(false);

		return ScriptAnyValue();
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Returns the entity ID of the gamerules
class CFlowNode_GetGameRulesEntityId : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_GET
	};
	enum EOutputs
	{
		OUT_ID
	};

	CFlowNode_GetGameRulesEntityId(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		// declare input ports
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Get", _HELP("Retrieve the entityId")),
			{ 0 }
		};
		static const SOutputPortConfig out_ports[] = {
			OutputPortConfig<EntityId>("EntityId", _HELP("The entityId of the Gamerules")),
			{ 0 }
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Calls a script function on the entity");
		config.SetCategory(EFLN_DEBUG);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (eFE_Activate == event && IsPortActive(pActInfo, IN_GET))
		{
			if (IEntity* pGameRules = gEnv->pGameFramework->GetIGameRulesSystem()->GetCurrentGameRulesEntity())
			{
				ActivateOutput(pActInfo, OUT_ID, pGameRules->GetId());
			}
			else
			{
				ActivateOutput(pActInfo, OUT_ID, INVALID_ENTITYID);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_FindEntityByName : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_FindEntityByName(SActivationInfo* pActInfo)
	{
	}

	enum EInPorts
	{
		eIP_Set = 0,
		eIP_Name
	};
	enum EOutPorts
	{
		eOP_EntityId = 0
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig_Void   ("Set", _HELP( "Start search for entity with the specified name." )),
			InputPortConfig<string>("Name", string(), _HELP( "Name of the entity to look for." )),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] =
		{
			OutputPortConfig<EntityId>("EntityId", _HELP("Outputs the entity's ID if found, 0 otherwise.")),
			{ 0 }
		};
		config.sDescription = _HELP("Searches for an entity using its name");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate)
		{
			if (IsPortActive(pActInfo, eIP_Set))
			{
				const string& name = GetPortString(pActInfo, eIP_Name);
				IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(name.c_str());
				ActivateOutput(pActInfo, eOP_EntityId, pEntity ? pEntity->GetId() : EntityId(0));
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};

template<class T>
class CAutoRegFlowNodeWithAllowOverride : public CAutoRegFlowNode<T>
{
public:
	CAutoRegFlowNodeWithAllowOverride(const char* sClassName) : CAutoRegFlowNode<T>(sClassName) {}
	virtual bool         AllowOverride() const override { return true; }
};

CAutoRegFlowNodeWithAllowOverride<CFlowNode_EntityTagpoint> g_AutoRegCFlowNode_EntityTagpoint("entity:TagPoint"); 
CRY_EXPORT_STATIC_LINK_VARIABLE(g_AutoRegCFlowNode_EntityTagpoint);

REGISTER_FLOW_NODE("Entity:EntityPos", CFlowNode_EntityPos)
REGISTER_FLOW_NODE("Entity:GetPos", CFlowNode_EntityGetPos)
REGISTER_FLOW_NODE("Entity:BroadcastEvent", CFlowNode_BroadcastEntityEvent)
REGISTER_FLOW_NODE("Entity:EntityId", CFlowNode_EntityId)
REGISTER_FLOW_NODE("Entity:EntityInfo", CFlowNode_EntityGetInfo)
REGISTER_FLOW_NODE("Entity:ParentId", CFlowNode_ParentId)
REGISTER_FLOW_NODE("Entity:GetChildrenInfo", CFlowNode_ChildrenInfo)
REGISTER_FLOW_NODE("Entity:PropertySet", CFlowNode_EntitySetProperty)
REGISTER_FLOW_NODE("Entity:PropertyGet", CFlowNode_EntityGetProperty)
REGISTER_FLOW_NODE("Entity:ChildAttach", CFlowNode_EntityAttachChild)
REGISTER_FLOW_NODE("Entity:ChildDetach", CFlowNode_EntityDetachThis)
REGISTER_FLOW_NODE("Entity:BeamEntity", CFlowNode_BeamEntity)
REGISTER_FLOW_NODE("Entity:RenderParams", CFlowNodeRenderParams)
//REGISTER_FLOW_NODE("entity:TagPoint", CFlowNode_EntityTagpoint)
REGISTER_FLOW_NODE("Entity:CheckDistance", CFlowNode_EntityCheckDistance)
REGISTER_FLOW_NODE("Entity:CheckProjection", CFlowNode_EntityCheckProjection)
REGISTER_FLOW_NODE("Entity:GetBounds", CFlowNode_EntityGetBounds)

REGISTER_FLOW_NODE("Entity:CharAttachmentMaterialParam", CFlowNodeCharAttachmentMaterialShaderParam)

REGISTER_FLOW_NODE("Entity:EntityFaceAt", CFlowNode_EntityFaceAt)

REGISTER_FLOW_NODE("Entity:FindEntityByName", CFlowNode_FindEntityByName);

//spawn nodes
REGISTER_FLOW_NODE("Entity:Spawn", CFlowNode_SpawnEntity);
REGISTER_FLOW_NODE("Entity:SpawnArchetype", CFlowNode_SpawnArchetypeEntity);

//Call lua functions
REGISTER_FLOW_NODE("Entity:CallScriptFunction", CFlowNode_CallScriptFunction);
REGISTER_FLOW_NODE("Game:GetGameRulesEntityId", CFlowNode_GetGameRulesEntityId);

