// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowMouseInfo.cpp
//  Version:     v1.00
//  Created:     08/26/2013 - Sascha Hoba
//  Description: Mouse Info Flownode
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include <CryInput/IHardwareMouse.h>
#include <CryFlowGraph/IFlowBaseNode.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CFlowMouseCoordNode : public CFlowBaseNode<eNCT_Instanced>, public IHardwareMouseEventListener, public IInputEventListener
{
private:
	SActivationInfo m_actInfo;
	bool            m_bOutputWorldCoords;
	bool            m_bOutputScreenCoords;
	bool            m_bOutputDeltaCoords;
	bool            m_enabled;

public:

	~CFlowMouseCoordNode()
	{
		if (gEnv->pHardwareMouse)
			gEnv->pHardwareMouse->RemoveListener(this);

		if (GetISystem() && GetISystem()->GetIInput())
			GetISystem()->GetIInput()->RemoveEventListener(this);
	}

	CFlowMouseCoordNode(SActivationInfo* pActInfo)
	{
		m_actInfo = *pActInfo;
		m_bOutputWorldCoords = false;
		m_bOutputScreenCoords = false;
		m_bOutputDeltaCoords = false;
		m_enabled = false;
	}

	enum EInputPorts
	{
		EIP_Enable = 0,
		EIP_Disable,
		EIP_World,
		EIP_Screen,
		EIP_DeltaScreen,
	};

	enum EOutputPorts
	{
		EOP_World = 0,
		EOP_ScreenX,
		EOP_ScreenY,
		EOP_DeltaScreenX,
		EOP_DeltaScreenY,
	};

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("enabled", m_enabled);

		// register listeners if this node was saved as enabled
		if (ser.IsReading() && m_enabled)
		{
			if (gEnv->pHardwareMouse)
				gEnv->pHardwareMouse->AddListener(this);

			if (GetISystem() && GetISystem()->GetIInput())
				GetISystem()->GetIInput()->AddEventListener(this);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Enable",  _HELP("Enable mouse coord info")),
			InputPortConfig_Void("Disable", _HELP("Disable mouse coord info")),
			InputPortConfig<bool>("World",  false,                             _HELP("Output the world x,y,z")),
			InputPortConfig<bool>("Screen", false,                             _HELP("Output the screen x y")),
			InputPortConfig<bool>("Delta",  false,                             _HELP("Output the delta x and y")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<Vec3>("World",       _HELP("Unprojected mouse pos")),
			OutputPortConfig<int>("ScreenX",      _HELP("Screen X coord")),
			OutputPortConfig<int>("ScreenY",      _HELP("Screen Y coord")),
			OutputPortConfig<int>("DeltaScreenX", _HELP("delta screen X coord")),
			OutputPortConfig<int>("DeltaScreenY", _HELP("Delta screen Y coord")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Returns several mouse information.");
		config.SetCategory(EFLN_APPROVED);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowMouseCoordNode(pActInfo);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		m_actInfo = *pActInfo;
		switch (event)
		{
		case eFE_Initialize:
			{
				m_bOutputScreenCoords = GetPortBool(pActInfo, EIP_Screen);
				m_bOutputWorldCoords = GetPortBool(pActInfo, EIP_World);
				m_bOutputDeltaCoords = GetPortBool(pActInfo, EIP_DeltaScreen);

				break;
			}
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Enable))
				{
					if (gEnv->pHardwareMouse)
						gEnv->pHardwareMouse->AddListener(this);

					if (GetISystem() && GetISystem()->GetIInput())
						GetISystem()->GetIInput()->AddEventListener(this);

					m_bOutputScreenCoords = GetPortBool(pActInfo, EIP_Screen);
					m_bOutputWorldCoords = GetPortBool(pActInfo, EIP_World);
					m_bOutputDeltaCoords = GetPortBool(pActInfo, EIP_DeltaScreen);

					m_enabled = true;
				}

				if (IsPortActive(pActInfo, EIP_Disable))
				{
					if (gEnv->pHardwareMouse)
						gEnv->pHardwareMouse->RemoveListener(this);

					if (GetISystem() && GetISystem()->GetIInput())
						GetISystem()->GetIInput()->RemoveEventListener(this);

					m_enabled = false;
				}

				if (IsPortActive(pActInfo, EIP_World))
				{
					m_bOutputWorldCoords = GetPortBool(pActInfo, EIP_World);
				}

				if (IsPortActive(pActInfo, EIP_Screen))
				{
					m_bOutputScreenCoords = GetPortBool(pActInfo, EIP_Screen);
				}

				if (IsPortActive(pActInfo, EIP_DeltaScreen))
				{
					m_bOutputDeltaCoords = GetPortBool(pActInfo, EIP_DeltaScreen);
				}

				break;
			}
		}
	}

	virtual void OnHardwareMouseEvent(int iX, int iY, EHARDWAREMOUSEEVENT eHardwareMouseEvent, int wheelDelta = 0)
	{
		if (m_bOutputScreenCoords)
		{
			ActivateOutput(&m_actInfo, EOP_ScreenX, iX);
			ActivateOutput(&m_actInfo, EOP_ScreenY, iY);
		}

		if (m_bOutputWorldCoords)
		{
			const CCamera& camera = GetISystem()->GetViewCamera();

			Vec3 vPos;
			int mouseX, mouseY;
			mouseX = iX;
			mouseY = camera.GetViewSurfaceZ() - iY;
			gEnv->pRenderer->UnProjectFromScreen((float)mouseX, (float)mouseY, 1, &vPos.x, &vPos.y, &vPos.z);

			ActivateOutput(&m_actInfo, EOP_World, vPos);
		}
	}

	virtual bool OnInputEvent(const SInputEvent& event)
	{
		if (m_bOutputDeltaCoords == false)
			return false;

		switch (event.keyId)
		{
		case eKI_MouseX:
			{
				ActivateOutput(&m_actInfo, EOP_DeltaScreenX, event.value);
				break;
			}
		case eKI_MouseY:
			{
				ActivateOutput(&m_actInfo, EOP_DeltaScreenY, event.value);
				break;
			}
		}

		return false;
	}

};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CFlowMouseButtonNode : public CFlowBaseNode<eNCT_Instanced>, public IInputEventListener
{
private:
	SActivationInfo m_actInfo;
	bool            m_bOutputMouseButton;
	bool            m_bOutputMouseWheel;
	bool            m_enabled;

public:

	CFlowMouseButtonNode(SActivationInfo* pActInfo)
	{
		m_actInfo = *pActInfo;
		m_bOutputMouseButton = false;
		m_bOutputMouseWheel = false;
		m_enabled = false;
	}

	~CFlowMouseButtonNode()
	{
		if (GetISystem() && GetISystem()->GetIInput())
			GetISystem()->GetIInput()->RemoveEventListener(this);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("enabled", m_enabled);

		// register listeners if this node was saved as enabled
		if (ser.IsReading() && m_enabled)
		{
			if (GetISystem() && GetISystem()->GetIInput())
				GetISystem()->GetIInput()->AddEventListener(this);
		}
	}

	enum EInputPorts
	{
		EIP_Enable = 0,
		EIP_Disable,
		EIP_OuputMouseButtons,
		EIP_OuputMouseWheel,
	};

	enum EOutputPorts
	{
		EOP_MousePressed = 0,
		EOP_MouseReleased,
		EOP_MouseWheel
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Enable",       _HELP("Enable mouse button info")),
			InputPortConfig_Void("Disable",      _HELP("Disable mouse button info")),
			InputPortConfig<bool>("MouseButton", false,                              _HELP("Output the mouse button info")),
			InputPortConfig<bool>("MouseWheel",  false,                              _HELP("Output the mouse wheel info")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<int>("MousePressed",  _HELP("The int represents the mouse button that was pressed")),
			OutputPortConfig<int>("MouseReleased", _HELP("The int represents the mouse button that was released")),
			OutputPortConfig<float>("MouseWheel",  _HELP("Positive is wheel up, negative is wheel down")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Mouse button information");
		config.SetCategory(EFLN_APPROVED);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowMouseButtonNode(pActInfo);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		m_actInfo = *pActInfo;
		switch (event)
		{
		case eFE_Initialize:
			{
				m_bOutputMouseButton = GetPortBool(pActInfo, EIP_OuputMouseButtons);
				m_bOutputMouseWheel = GetPortBool(pActInfo, EIP_OuputMouseWheel);

				break;
			}
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Enable))
				{
					if (GetISystem() && GetISystem()->GetIInput())
						GetISystem()->GetIInput()->AddEventListener(this);

					m_bOutputMouseButton = GetPortBool(pActInfo, EIP_OuputMouseButtons);
					m_bOutputMouseWheel = GetPortBool(pActInfo, EIP_OuputMouseWheel);

					m_enabled = true;
				}

				if (IsPortActive(pActInfo, EIP_Disable))
				{
					if (GetISystem() && GetISystem()->GetIInput())
						GetISystem()->GetIInput()->RemoveEventListener(this);

					m_enabled = false;
				}

				if (IsPortActive(pActInfo, EIP_OuputMouseButtons))
				{
					m_bOutputMouseButton = GetPortBool(pActInfo, EIP_OuputMouseButtons);
				}

				if (IsPortActive(pActInfo, EIP_OuputMouseWheel))
				{
					m_bOutputMouseWheel = GetPortBool(pActInfo, EIP_OuputMouseWheel);
				}

				break;
			}
		}
	}

	virtual bool OnInputEvent(const SInputEvent& event)
	{
		switch (event.keyId)
		{
		case eKI_Mouse1:
		case eKI_Mouse2:
		case eKI_Mouse3:
		case eKI_Mouse4:
		case eKI_Mouse5:
		case eKI_Mouse6:
		case eKI_Mouse7:
		case eKI_Mouse8:
			if (m_bOutputMouseButton)
			{
				if (event.state == eIS_Pressed)
				{
					ActivateOutput(&m_actInfo, EOP_MousePressed, (event.keyId - KI_MOUSE_BASE) + 1); // mouse1 should be 1, etc
				}
				if (event.state == eIS_Released)
				{
					ActivateOutput(&m_actInfo, EOP_MouseReleased, (event.keyId - KI_MOUSE_BASE) + 1); // mouse1 should be 1, etc
				}
			}
			break;
		case eKI_MouseWheelDown:
		case eKI_MouseWheelUp:
			// pressed: started scrolling, down: repeating event while scrolling at a speed, released: finished (delta 0)
			// for every pressed event there is at least 1 down event, and we only want to forward the down event to the user (shows the speed and length of a scroll)
			if (m_bOutputMouseWheel && event.state == eIS_Down)
			{
				ActivateOutput(&m_actInfo, EOP_MouseWheel, event.value);
			}
			break;
		}

		return false;
	}

};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CFlowMouseRayCast : public CFlowBaseNode<eNCT_Instanced>, public IHardwareMouseEventListener, public IGameFrameworkListener
{
private:
	SActivationInfo    m_actInfo;
	int                m_mouseX, m_mouseY;
	entity_query_flags m_rayTypeFilter;
	bool               m_enabled;

public:

	CFlowMouseRayCast(SActivationInfo* pActInfo)
	{
		m_actInfo = *pActInfo;
		m_mouseX = 0;
		m_mouseY = 0;
		m_rayTypeFilter = ent_all;
		m_enabled = false;
	}

	~CFlowMouseRayCast()
	{
		if (gEnv->pHardwareMouse)
			gEnv->pHardwareMouse->RemoveListener(this);

		if (gEnv->pGameFramework)
			gEnv->pGameFramework->UnregisterListener(this);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("enabled", m_enabled);

		// register listeners if this node was saved as enabled
		if (ser.IsReading() && m_enabled)
		{
			if (gEnv->pHardwareMouse)
				gEnv->pHardwareMouse->AddListener(this);

			if (gEnv->pGameFramework)
				gEnv->pGameFramework->RegisterListener(this, "MouseRayInfoNode", FRAMEWORKLISTENERPRIORITY_DEFAULT);
		}
	}

	enum EInputPorts
	{
		EIP_Enable = 0,
		EIP_Disable,
		EIP_RayType,
		EIP_EntitiesToIgnore,
	};

	enum EOutputPorts
	{
		EOP_HitPos = 0,
		EOP_HitNormal,
		EOP_EntityId,
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Enable",           _HELP("Enable mouse button info")),
			InputPortConfig_Void("Disable",          _HELP("Disable mouse button info")),
			InputPortConfig<int>("RayType",          0,                                                         _HELP("What should cause a ray hit?"),"All", _UICONFIG("enum_int: All=0, Terrain=1, Rigid=2, Static=3, Water=4, Living=5")),
			InputPortConfig<int>("EntitiesToIgnore", _HELP("Container with Entities to ignore during raycast")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<Vec3>("HitPos",       _HELP("The int represents the mouse button that was pressed")),
			OutputPortConfig<Vec3>("HitNormal",    _HELP("The int represents the mouse button that was released")),
			OutputPortConfig<EntityId>("EntityId", _HELP("Entity that got hit by raycast")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Mouse ray information");
		config.SetCategory(EFLN_APPROVED);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowMouseRayCast(pActInfo);
	}

	void StoreRayType()
	{
		int ray_type = GetPortInt(&m_actInfo, EIP_RayType);
		switch (ray_type)
		{
		case 0:
			m_rayTypeFilter = ent_all;
			break;
		case 1:
			m_rayTypeFilter = ent_terrain;
			break;
		case 2:
			m_rayTypeFilter = ent_rigid;
			break;
		case 3:
			m_rayTypeFilter = ent_static;
			break;
		case 4:
			m_rayTypeFilter = ent_water;
			break;
		case 5:
			m_rayTypeFilter = ent_living;
			break;
		}
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		m_actInfo = *pActInfo;
		switch (event)
		{
		case eFE_Initialize:
			{
				StoreRayType();
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Enable))
				{
					if (gEnv->pHardwareMouse)
						gEnv->pHardwareMouse->AddListener(this);

					if (gEnv->pGameFramework)
						gEnv->pGameFramework->RegisterListener(this, "MouseRayInfoNode", FRAMEWORKLISTENERPRIORITY_DEFAULT);

					StoreRayType();
					m_enabled = true;
				}

				if (IsPortActive(pActInfo, EIP_Disable))
				{
					if (gEnv->pHardwareMouse)
						gEnv->pHardwareMouse->RemoveListener(this);

					if (gEnv->pGameFramework)
						gEnv->pGameFramework->UnregisterListener(this);

					m_enabled = false;
				}

				if (IsPortActive(pActInfo, EIP_RayType))
				{
					StoreRayType();
				}

				break;
			}
		}
	}

	// IHardwareMouseEventListener
	virtual void OnHardwareMouseEvent(int iX, int iY, EHARDWAREMOUSEEVENT eHardwareMouseEvent, int wheelDelta = 0)
	{
		m_mouseX = iX;
		m_mouseY = iY;
	}
	// ~IHardwareMouseEventListener

	// IGameFrameworkListener
	virtual void OnSaveGame(ISaveGame* pSaveGame)         {}
	virtual void OnLoadGame(ILoadGame* pLoadGame)         {}
	virtual void OnLevelEnd(const char* nextLevel)        {}
	virtual void OnActionEvent(const SActionEvent& event) {}

	// Since the modelviewmatrix is updated in the update, and flowgraph is updated in the preupdate, we need this postupdate
	virtual void OnPostUpdate(float fDeltaTime)
	{
		const CCamera& camera = GetISystem()->GetViewCamera();

		int invMouseY = camera.GetViewSurfaceZ() - m_mouseY;

		Vec3 vPos0(0, 0, 0);
		gEnv->pRenderer->UnProjectFromScreen((float)m_mouseX, (float)invMouseY, 0, &vPos0.x, &vPos0.y, &vPos0.z);

		Vec3 vPos1(0, 0, 0);
		gEnv->pRenderer->UnProjectFromScreen((float)m_mouseX, (float)invMouseY, 1, &vPos1.x, &vPos1.y, &vPos1.z);

		Vec3 vDir = vPos1 - vPos0;
		vDir.Normalize();

		ray_hit hit;
		const unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;

		IPhysicalEntity** pSkipEnts = NULL;
		int numSkipped = 0;

		int containerId = GetPortInt(&m_actInfo, EIP_EntitiesToIgnore);
		if (containerId != 0)
		{
			IFlowSystemContainerPtr container = gEnv->pFlowSystem->GetContainer(containerId);
			numSkipped = container->GetItemCount();
			pSkipEnts = new IPhysicalEntity*[numSkipped];
			for (int i = 0; i < numSkipped; i++)
			{
				EntityId id;
				container->GetItem(i).GetValueWithConversion(id);
				pSkipEnts[i] = gEnv->pEntitySystem->GetEntity(id)->GetPhysics();
			}
		}

		if (gEnv->pPhysicalWorld && gEnv->pPhysicalWorld->RayWorldIntersection(vPos0, vDir * gEnv->p3DEngine->GetMaxViewDistance(), m_rayTypeFilter, flags, &hit, 1, pSkipEnts, numSkipped))
		{
			ActivateOutput(&m_actInfo, EOP_HitPos, hit.pt);
			ActivateOutput(&m_actInfo, EOP_HitNormal, hit.n);

			if (hit.pCollider)
			{
				if (IEntity* pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(hit.pCollider))
				{
					ActivateOutput(&m_actInfo, EOP_EntityId, pEntity->GetId());
				}
			}
		}

		if (pSkipEnts)
			delete[] pSkipEnts;
	}
	// ~IGameFrameworkListener
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CFlowMouseCursor : public CFlowBaseNode<eNCT_Instanced>
{
private:
	bool m_isShowing;

public:

	CFlowMouseCursor(SActivationInfo* pActInfo)
	{
		m_isShowing = false;
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("isShowing", m_isShowing);

		if (ser.IsReading())
		{
			if (m_isShowing)
			{
				gEnv->pHardwareMouse->IncrementCounter();
			}
		}
	}

	~CFlowMouseCursor()
	{
		// make sure to put the cursor at the correct counter before deleting this
		if (m_isShowing)
		{
			gEnv->pHardwareMouse->DecrementCounter();
		}
	}

	enum EInputPorts
	{
		EIP_Show = 0,
		EIP_Hide,
	};

	enum EOutputPorts
	{
		EOP_Done = 0,
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Show", _HELP("Show mouse cursor")),
			InputPortConfig_Void("Hide", _HELP("Hide the mouse cursor")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig_Void("Done", _HELP("Done")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Node to show or hide the mouse cursor");
		config.SetCategory(EFLN_APPROVED);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowMouseCursor(pActInfo);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				// hide cursor from start (default behavior)
				if (m_isShowing)
				{
					gEnv->pHardwareMouse->DecrementCounter();
					m_isShowing = false;
				}
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Show))
				{
					if (!m_isShowing)
					{
						gEnv->pHardwareMouse->IncrementCounter();
						m_isShowing = true;
					}

					ActivateOutput(pActInfo, EOP_Done, 1);
				}

				if (IsPortActive(pActInfo, EIP_Hide))
				{
					if (m_isShowing)
					{
						gEnv->pHardwareMouse->DecrementCounter();
						m_isShowing = false;
					}

					ActivateOutput(pActInfo, EOP_Done, 1);
				}

				break;
			}
		}
	}

};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CFlowMouseEntitiesInBox : public CFlowBaseNode<eNCT_Instanced>, public IGameFrameworkListener
{
private:
	int             m_screenX, m_screenY, m_screenX2, m_screenY2;
	SActivationInfo m_actInfo;
	bool            m_get;

public:

	CFlowMouseEntitiesInBox(SActivationInfo* pActInfo)
	{
		m_screenX = 0;
		m_screenX2 = 0;
		m_screenY = 0;
		m_screenY2 = 0;
		m_get = false;
	}

	~CFlowMouseEntitiesInBox()
	{
		if (gEnv->pGameFramework)
			gEnv->pGameFramework->UnregisterListener(this);
	}

	enum EInputPorts
	{
		EIP_Get = 0,
		EIP_ContainerId,
		EIP_ScreenX,
		EIP_ScreenY,
		EIP_ScreenX2,
		EIP_ScreenY2,
	};

	enum EOutputPorts
	{
		EOP_Done = 0,
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Get",         _HELP("Show mouse cursor")),
			InputPortConfig<int>("ContainerId", _HELP("The container to store the entitieIds in, user is responsible for creating/deleting this")),
			InputPortConfig<int>("ScreenX",     _HELP("Hide the mouse cursor")),
			InputPortConfig<int>("ScreenY",     _HELP("Hide the mouse cursor")),
			InputPortConfig<int>("ScreenX2",    _HELP("Hide the mouse cursor")),
			InputPortConfig<int>("ScreenY2",    _HELP("Hide the mouse cursor")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig_Void("Done", _HELP("Done")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Node to show or hide the mouse cursor");
		config.SetCategory(EFLN_APPROVED);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowMouseEntitiesInBox(pActInfo);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Update:
			{

			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Get))
				{
					gEnv->pGameFramework->RegisterListener(this, "FlowMouseEntitiesInBox", FRAMEWORKLISTENERPRIORITY_DEFAULT);

					m_get = true;
					m_actInfo = *pActInfo;
					m_screenX = GetPortInt(pActInfo, EIP_ScreenX);
					m_screenY = GetPortInt(pActInfo, EIP_ScreenY);
					m_screenX2 = GetPortInt(pActInfo, EIP_ScreenX2);
					m_screenY2 = GetPortInt(pActInfo, EIP_ScreenY2);
				}
				break;
			}
		}
	}

	// IGameFrameworkListener
	virtual void OnSaveGame(ISaveGame* pSaveGame)         {}
	virtual void OnLoadGame(ILoadGame* pLoadGame)         {}
	virtual void OnLevelEnd(const char* nextLevel)        {}
	virtual void OnActionEvent(const SActionEvent& event) {}
	virtual void OnPostUpdate(float fDeltaTime)
	{
		// Get it once, then unregister to prevent unnescessary updates
		if (!m_get)
		{
			gEnv->pGameFramework->UnregisterListener(this);
			return;
		}
		m_get = false;

		// Grab the container, and make sure its valid (user has to create it for us and pass the valid ID)
		IFlowSystemContainerPtr pContainer = gEnv->pFlowSystem->GetContainer(GetPortInt(&m_actInfo, EIP_ContainerId));

		if (!pContainer)
			return;

		IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();

		while (!pIt->IsEnd())
		{
			if (IEntity* pEntity = pIt->Next())
			{
				//skip Local player
				if (IPhysicalEntity* physEnt = pEntity->GetPhysics())
				{
					IActor* pClientActor = gEnv->pGameFramework->GetClientActor();

					if (!pClientActor)
						return;

					//skip the client actor entity
					if (physEnt == pClientActor->GetEntity()->GetPhysics())
						continue;

					AABB worldBounds;
					pEntity->GetWorldBounds(worldBounds);

					//skip further calculations if the entity is not visible at all...
					if (gEnv->pSystem->GetViewCamera().IsAABBVisible_F(worldBounds) == CULL_EXCLUSION)
						continue;

					Vec3 wpos = pEntity->GetWorldPos();
					Quat rot = pEntity->GetWorldRotation();
					AABB localBounds;

					pEntity->GetLocalBounds(localBounds);

					//get min and max values of the entity bounding box (local positions)
					Vec3 points[2];
					points[0] = wpos + rot * localBounds.min;
					points[1] = wpos + rot * localBounds.max;

					Vec3 pointsProjected[2];

					//project the bounding box min max values to screen positions
					for (int i = 0; i < 2; ++i)
					{
						gEnv->pRenderer->ProjectToScreen(points[i].x, points[i].y, points[i].z, &pointsProjected[i].x, &pointsProjected[i].y, &pointsProjected[i].z);

						const int w = IRenderAuxGeom::GetAux()->GetCamera().GetViewSurfaceX();
						const int h = IRenderAuxGeom::GetAux()->GetCamera().GetViewSurfaceZ();

						//scale projected values to the actual screen resolution
						pointsProjected[i].x *= 0.01f * w;
						pointsProjected[i].y *= 0.01f * h;
					}

					//check if the projected bounding box min max values are fully or partly inside the screen selection
					if ((m_screenX <= pointsProjected[0].x && pointsProjected[0].x <= m_screenX2) ||
					    (m_screenX >= pointsProjected[0].x && m_screenX2 <= pointsProjected[1].x) ||
					    (m_screenX <= pointsProjected[1].x && m_screenX2 >= pointsProjected[1].x) ||
					    (m_screenX <= pointsProjected[0].x && m_screenX2 >= pointsProjected[1].x))
					{
						if ((m_screenY <= pointsProjected[0].y && m_screenY2 >= pointsProjected[0].y) ||
						    (m_screenY <= pointsProjected[1].y && m_screenY2 >= pointsProjected[0].y) ||
						    (m_screenY <= pointsProjected[1].y && m_screenY2 >= pointsProjected[1].y))
						{
							// Add entity to container
							pContainer->AddItem(TFlowInputData(pEntity->GetId()));
						}
					}
				}

			}
		}
	}
	// ~IGameFrameworkListener

};

REGISTER_FLOW_NODE("Input:MouseCoords", CFlowMouseCoordNode);
REGISTER_FLOW_NODE("Input:MouseButtonInfo", CFlowMouseButtonNode);
REGISTER_FLOW_NODE("Input:MouseRayCast", CFlowMouseRayCast);
REGISTER_FLOW_NODE("Input:MouseCursor", CFlowMouseCursor);
REGISTER_FLOW_NODE("Input:MouseEntitiesInBox", CFlowMouseEntitiesInBox);
