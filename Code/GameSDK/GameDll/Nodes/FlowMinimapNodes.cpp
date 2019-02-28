// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <CryGame/IGameFramework.h>
#include <ILevelSystem.h>
#include <CryEntitySystem/IEntitySystem.h>

//---------------------------------------------------------------------------------------
//------------------------------------- MinimapInfo -------------------------------------
//---------------------------------------------------------------------------------------
class CMiniMapInfo
{
public:
	static CMiniMapInfo* GetInstance()
	{
		static CMiniMapInfo inst;
		return &inst;
	}

	void UpdateLevelInfo()
	{
		ILevelInfo* pLevel = gEnv->pGameFramework->GetILevelSystem()->GetCurrentLevel();
		if (pLevel)
			m_minimapInfo = pLevel->GetMinimapInfo();
	}

	const ILevelInfo::SMinimapInfo& GetMinimapInfo()
	{
		UpdateLevelInfo();
		return m_minimapInfo;
	}

	void GetPlayerData(IEntity* pEntity, float& px, float& py, int& rot) const
	{
		if (pEntity)
		{
			px = clamp_tpl((pEntity->GetWorldPos().x - m_minimapInfo.fStartX) / m_minimapInfo.fDimX, 0.0f, 1.0f);
			py = clamp_tpl((pEntity->GetWorldPos().y - m_minimapInfo.fStartY) / m_minimapInfo.fDimY, 0.0f, 1.0f);
			rot = (int) (pEntity->GetWorldAngles().z * 180.0f / gf_PI - 90.0f);
		}
	}

private:
	CMiniMapInfo() {}
	CMiniMapInfo(const CMiniMapInfo&) {}
	CMiniMapInfo& operator=(const CMiniMapInfo&) { return *this; }
	~CMiniMapInfo() {}

	ILevelInfo::SMinimapInfo m_minimapInfo;
};

//----------------------------------- MiniMap info node ---------------------------------
class CFlowMiniMapInfoNode
	: public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowMiniMapInfoNode(SActivationInfo* pActInfo)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Get", _HELP("Get minimap info")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("OnGet",          _HELP("Tirggers of port <Get> is activeated")),
			OutputPortConfig<string>("MapFile",     _HELP("Name of minimap dds file")),
			OutputPortConfig<int>("Width",          _HELP("Minimap width")),
			OutputPortConfig<int>("Height",         _HELP("Minimap height")),
			OutputPortConfig_Void("InvalidMiniMap", _HELP("Triggered when minimap path was empty")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Info about minimap");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate && IsPortActive(pActInfo, eI_Get))
		{
			const ILevelInfo::SMinimapInfo& mapInfo = CMiniMapInfo::GetInstance()->GetMinimapInfo();
			if (!mapInfo.sMinimapName.empty())
			{
				ActivateOutput(pActInfo, eO_OnGet, true);
				ActivateOutput(pActInfo, eO_MapName, mapInfo.sMinimapName);
				ActivateOutput(pActInfo, eO_MapWidth, mapInfo.iWidth);
				ActivateOutput(pActInfo, eO_MapHeight, mapInfo.iHeight);
			}
			else
			{
				ActivateOutput(pActInfo, eO_InvalidPath, true);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	enum EInputPorts
	{
		eI_Get,
	};

	enum EOutputPorts
	{
		eO_OnGet = 0,
		eO_MapName,
		eO_MapWidth,
		eO_MapHeight,
		eO_InvalidPath,
	};

};

//----------------------------------- Entity pos info --------------------------------------
class CFlowMiniMapEntityPosInfo
	: public CFlowBaseNode<eNCT_Instanced>
	  , public IEntityEventListener
{
public:
	CFlowMiniMapEntityPosInfo(SActivationInfo* pActInfo)
	{
	}

	virtual ~CFlowMiniMapEntityPosInfo()
	{
		std::set<EntityId> copyOfEntityIds = m_entityIds;
		for (EntityId entityId : copyOfEntityIds)
		{
			UnRegisterEntity(entityId);
		}
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_Void("Get",     _HELP("Trigger outputs.")),
			InputPortConfig_Void("Enable",  _HELP("Trigger to enable")),
			InputPortConfig_Void("Disable", _HELP("Trigger to enable")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("OnPosChange", _HELP("Triggers if position has changed")),
			OutputPortConfig<float>("PosX",      _HELP("X pos on minimap")),
			OutputPortConfig<float>("PosY",      _HELP("Y pos on minimap")),
			OutputPortConfig<int>("Rotation",    _HELP("Minimap rotation")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.SetCategory(EFLN_ADVANCED);

		SetFlagsAndDescriton(config);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_actInfo = *pActInfo;
			CMiniMapInfo::GetInstance()->UpdateLevelInfo();
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, eI_Trigger))
				TriggerPorts();

			if (IsPortActive(pActInfo, eI_Disable))
			{
				const EntityId entityId = GetEntityId(pActInfo);
				UnRegisterEntity(entityId);
			}

			if (IsPortActive(pActInfo, eI_Enable))
			{
				const EntityId entityId = GetEntityId(pActInfo);
				RegisterEntity(entityId);
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowMiniMapEntityPosInfo(pActInfo);
	}

	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
	{
		if (event.event == ENTITY_EVENT_XFORM)
		{
			float px, py;
			int r;
			CMiniMapInfo::GetInstance()->GetPlayerData(pEntity, px, py, r);
			ActivateOutput(&m_actInfo, eO_OnPosChange, true);
			ActivateOutput(&m_actInfo, eO_PosX, px);
			ActivateOutput(&m_actInfo, eO_PosY, py);
			ActivateOutput(&m_actInfo, eO_Rotation, r);
		}
	}

protected:
	virtual EntityId GetEntityId(SActivationInfo* pActInfo)
	{
		return pActInfo->pEntity != 0 ? pActInfo->pEntity->GetId() : 0;
	}

	virtual void SetFlagsAndDescriton(SFlowNodeConfig& config)
	{
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Info about entity position on minimap");
	}

private:
	void RegisterEntity(EntityId entityId)
	{
		//Check if the entity actual exists so the listener can be added
		if (gEnv->pEntitySystem->GetEntity(entityId) != nullptr)
		{
			auto insertResult = m_entityIds.insert(entityId);

			// ensure to subscribe to given entity only once
			if (insertResult.second)
			{
				gEnv->pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_XFORM, this);
			}
		}
	}

	void UnRegisterEntity(EntityId entityId)
	{
		gEnv->pEntitySystem->RemoveEntityEventListener(entityId, ENTITY_EVENT_XFORM, this);
		m_entityIds.erase(entityId);
	}

	void TriggerPorts()
	{
		for (EntityId entityId : m_entityIds)
		{
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId))
			{
				float px, py;
				int r;
				CMiniMapInfo::GetInstance()->GetPlayerData(pEntity, px, py, r);
				ActivateOutput(&m_actInfo, eO_OnPosChange, true);
				ActivateOutput(&m_actInfo, eO_PosX, px);
				ActivateOutput(&m_actInfo, eO_PosY, py);
				ActivateOutput(&m_actInfo, eO_Rotation, r);
			}
		}
	}

private:
	std::set<EntityId> m_entityIds;
	SActivationInfo    m_actInfo;

	enum EInputPorts
	{
		eI_Trigger = 0,
		eI_Enable,
		eI_Disable,
	};

	enum EOutputPorts
	{
		eO_OnPosChange,
		eO_PosX,
		eO_PosY,
		eO_Rotation,
	};
};

//----------------------------------- Player pos info --------------------------------------
class CFlowMiniMapPlayerPosInfo
	: public CFlowMiniMapEntityPosInfo
{
public:
	CFlowMiniMapPlayerPosInfo(SActivationInfo* pActInfo)
		: CFlowMiniMapEntityPosInfo(pActInfo)
	{
	}

	virtual ~CFlowMiniMapPlayerPosInfo()
	{
	}

protected:
	virtual EntityId GetEntityId(SActivationInfo* pActInfo)
	{
		return gEnv->pGameFramework->GetClientActorId();
	}

	virtual void SetFlagsAndDescriton(SFlowNodeConfig& config)
	{
		config.sDescription = _HELP("Info about player position on minimap");
	}

};

REGISTER_FLOW_NODE("Minimap:MapInfo", CFlowMiniMapInfoNode);
REGISTER_FLOW_NODE("Minimap:PlayerPos", CFlowMiniMapPlayerPosInfo);
REGISTER_FLOW_NODE("Minimap:EntityPos", CFlowMiniMapEntityPosInfo);
