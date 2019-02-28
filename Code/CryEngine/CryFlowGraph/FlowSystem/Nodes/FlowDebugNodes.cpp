// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryGame/IGameFramework.h>
#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowNode_DrawSphere : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_DrawSphere(SActivationInfo* pActInfo)
	{
	}

	enum EInputPorts
	{
		eIP_Draw = 0,
		eIP_Pos,
		eIP_Radius,
		eIP_Color,
		eIP_Time,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig<SFlowSystemVoid>("Draw", _HELP("Trigger to draw the Sphere")),
			InputPortConfig<Vec3>("Pos",             _HELP("Position of Sphere")),
			InputPortConfig<float>("Radius",         _HELP("Radius of Sphere")),
			InputPortConfig<Vec3>("Color",           _HELP("Color of the Sphere"),                                               NULL,_UICONFIG("dt=color")),
			InputPortConfig<float>("Time",           _HELP("Duration of the display in seconds. Set to -1 for unlimited time.")),
			{ 0 }
		};

		config.sDescription = _HELP("Draws a sphere for debugging purposes");
		config.pInputPorts = in_config;
		config.SetCategory(EFLN_DEBUG);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
#if !defined(_RELEASE)
		if (event == eFE_Activate)
		{
			if (IsPortActive(pActInfo, eIP_Draw))
			{
				IPersistantDebug* pPersistentDebug = gEnv->pGameFramework->GetIPersistantDebug();
				if (pPersistentDebug)
				{
					const Vec3 pos = GetPortVec3(pActInfo, eIP_Pos);
					const float radius = GetPortFloat(pActInfo, eIP_Radius);
					const float time = GetPortFloat(pActInfo, eIP_Time);
					const ColorF color = GetPortVec3(pActInfo, eIP_Color);

					pPersistentDebug->Begin("FG_Sphere", false);
					pPersistentDebug->AddSphere(pos, radius, color, time);
				}
			}
		}
#endif
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(this);
	}
};

class CFlowNode_DrawLine : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_DrawLine(SActivationInfo* pActInfo)
	{
	}

	enum EInputPorts
	{
		eIP_Draw = 0,
		eIP_Pos1,
		eIP_Pos2,
		eIP_Dir,
		eIP_Length,
		eIP_Color,
		eIP_Time,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig<SFlowSystemVoid>("Draw", _HELP("Trigger to draw the Line")),
			InputPortConfig<Vec3>("Pos1",            _HELP("Start position of Line")),
			InputPortConfig<Vec3>("Pos2",            _HELP("End position of Line")),
			InputPortConfig<Vec3>("Dir",             _HELP("Direction of Line")),
			InputPortConfig<float>("Length",         _HELP("Length of Line")),
			InputPortConfig<Vec3>("Color",           _HELP("Color of the Line"),                                                 NULL,_UICONFIG("dt=color")),
			InputPortConfig<float>("Time",           _HELP("Duration of the display in seconds. Set to -1 for unlimited time.")),
			{ 0 }
		};

		config.sDescription = _HELP("Draws a line for debugging purposes. Use either Pos1, Dir and Length or Pos1 and Pos2");
		config.pInputPorts = in_config;
		config.SetCategory(EFLN_DEBUG);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
#if !defined(_RELEASE)
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIP_Draw))
				{
					IPersistantDebug* pPersistentDebug = gEnv->pGameFramework->GetIPersistantDebug();
					if (pPersistentDebug)
					{
						const Vec3 pos1 = GetPortVec3(pActInfo, eIP_Pos1);
						const Vec3 pos2 = GetPortVec3(pActInfo, eIP_Pos2);
						const float time = GetPortFloat(pActInfo, eIP_Time);
						const ColorF color = GetPortVec3(pActInfo, eIP_Color);

						Vec3 offset(pos2);

						if (pos2.IsZero())
						{
							offset = pos1 + (GetPortVec3(pActInfo, eIP_Dir) * GetPortFloat(pActInfo, eIP_Length));
						}

						pPersistentDebug->Begin("FG_Line", false);
						pPersistentDebug->AddLine(pos1, offset, color, time);
					}
				}
				break;
			}
		}
#endif
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(this);
	}
};

class CFlowNode_DrawCylinder : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_DrawCylinder(SActivationInfo* pActInfo)
	{
	}

	enum EInputPorts
	{
		eIP_Draw = 0,
		eIP_Pos,
		eIP_Dir,
		eIP_Radius,
		eIP_Height,
		eIP_Color,
		eIP_Time,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig<SFlowSystemVoid>("Draw", _HELP("Trigger to draw the Cylinder")),
			InputPortConfig<Vec3>("Pos",             _HELP("Centre position of Cylinder")),
			InputPortConfig<Vec3>("Dir",             _HELP("Direction of Cylinder")),
			InputPortConfig<float>("Radius",         _HELP("Radius of Cylinder")),
			InputPortConfig<float>("Height",         _HELP("Height of Cylinder")),
			InputPortConfig<Vec3>("Color",           _HELP("Color of the Cylinder"),                                             NULL,_UICONFIG("dt=color")),
			InputPortConfig<float>("Time",           _HELP("Duration of the display in seconds. Set to -1 for unlimited time.")),
			{ 0 }
		};

		config.sDescription = _HELP("Draws a cylinder for debugging purposes");
		config.pInputPorts = in_config;
		config.SetCategory(EFLN_DEBUG);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
#if !defined(_RELEASE)
		if (event == eFE_Activate)
		{
			if (IsPortActive(pActInfo, eIP_Draw))
			{
				IPersistantDebug* pPersistentDebug = gEnv->pGameFramework->GetIPersistantDebug();
				if (pPersistentDebug)
				{
					const Vec3 pos = GetPortVec3(pActInfo, eIP_Pos);
					const Vec3 dir = GetPortVec3(pActInfo, eIP_Dir);
					const float radius = GetPortFloat(pActInfo, eIP_Radius);
					const float height = GetPortFloat(pActInfo, eIP_Height);
					const float time = GetPortFloat(pActInfo, eIP_Time);
					const ColorF color = GetPortVec3(pActInfo, eIP_Color);

					pPersistentDebug->Begin("FG_Cylinder", false);
					pPersistentDebug->AddCylinder(pos, dir, radius, height, color, time);
				}
			}
		}
#endif
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(this);
	}
};

class CFlowNode_DrawDirection : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_DrawDirection(SActivationInfo* pActInfo)
	{
	}

	enum EInputPorts
	{
		eIP_Draw = 0,
		eIP_Pos,
		eIP_Dir,
		eIP_Radius,
		eIP_Color,
		eIP_Time,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig<SFlowSystemVoid>("Draw", _HELP("Trigger to draw the Direction (arrow)")),
			InputPortConfig<Vec3>("Pos",             _HELP("Centre position of Direction (arrow)")),
			InputPortConfig<Vec3>("Dir",             _HELP("Direction of Direction (arrow)")),
			InputPortConfig<float>("Radius",         _HELP("Radius of Direction (arrow)")),
			InputPortConfig<Vec3>("Color",           _HELP("Color of the Direction (arrow)"),                                    NULL,_UICONFIG("dt=color")),
			InputPortConfig<float>("Time",           _HELP("Duration of the display in seconds. Set to -1 for unlimited time.")),
			{ 0 }
		};

		config.sDescription = _HELP("Draws an arrow for debugging purposes");
		config.pInputPorts = in_config;
		config.SetCategory(EFLN_DEBUG);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
#if !defined(_RELEASE)
		if (event == eFE_Activate)
		{
			if (IsPortActive(pActInfo, eIP_Draw))
			{
				IPersistantDebug* pPersistentDebug = gEnv->pGameFramework->GetIPersistantDebug();
				if (pPersistentDebug)
				{
					const Vec3 pos = GetPortVec3(pActInfo, eIP_Pos);
					const Vec3 dir = GetPortVec3(pActInfo, eIP_Dir);
					const float radius = GetPortFloat(pActInfo, eIP_Radius);
					const float time = GetPortFloat(pActInfo, eIP_Time);
					const ColorF color = GetPortVec3(pActInfo, eIP_Color);

					pPersistentDebug->Begin("FG_Direction", false);
					pPersistentDebug->AddDirection(pos, radius, dir, color, time);
				}
			}
		}
#endif
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(this);
	}
};

class CFlowNode_DrawCone : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_DrawCone(SActivationInfo* pActInfo)
	{
	}

	enum EInputPorts
	{
		eIP_Draw = 0,
		eIP_Pos,
		eIP_Dir,
		eIP_Radius,
		eIP_Height,
		eIP_Color,
		eIP_Time,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig<SFlowSystemVoid>("Draw", _HELP("Trigger to draw the Cone")),
			InputPortConfig<Vec3>("Pos",             _HELP("Centre position of Cone")),
			InputPortConfig<Vec3>("Dir",             _HELP("Direction of Cone")),
			InputPortConfig<float>("Radius",         _HELP("Radius of Cone base")),
			InputPortConfig<float>("Height",         _HELP("Height of Cone")),
			InputPortConfig<Vec3>("Color",           _HELP("Color of the Cone"),                                                 NULL,_UICONFIG("dt=color")),
			InputPortConfig<float>("Time",           _HELP("Duration of the display in seconds. Set to -1 for unlimited time.")),
			{ 0 }
		};

		config.sDescription = _HELP("Draws a cone for debugging purposes");
		config.pInputPorts = in_config;
		config.SetCategory(EFLN_DEBUG);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
#if !defined(_RELEASE)
		if (event == eFE_Activate)
		{
			if (IsPortActive(pActInfo, eIP_Draw))
			{
				IPersistantDebug* pPersistentDebug = gEnv->pGameFramework->GetIPersistantDebug();
				if (pPersistentDebug)
				{
					const Vec3 pos = GetPortVec3(pActInfo, eIP_Pos);
					const Vec3 dir = GetPortVec3(pActInfo, eIP_Dir);
					const float radius = GetPortFloat(pActInfo, eIP_Radius);
					const float height = GetPortFloat(pActInfo, eIP_Height);
					const float time = GetPortFloat(pActInfo, eIP_Time);
					const ColorF color = GetPortVec3(pActInfo, eIP_Color);

					pPersistentDebug->Begin("FG_Cone", false);
					pPersistentDebug->AddCone(pos, dir, radius, height, color, time);
				}
			}
		}
#endif
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(this);
	}
};

class CFlowNode_DrawAABB : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_DrawAABB(SActivationInfo* pActInfo)
	{
	}

	enum EInputPorts
	{
		eIP_Draw = 0,
		eIP_MinPos,
		eIP_MaxPos,
		eIP_Color,
		eIP_Time,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig<SFlowSystemVoid>("Draw", _HELP("Trigger to draw the AABB")),
			InputPortConfig<Vec3>("MinPos",          _HELP("Minimum position of AABB")),
			InputPortConfig<Vec3>("MaxPos",          _HELP("Maximum position of AABB")),
			InputPortConfig<Vec3>("Color",           _HELP("Color of the AABB"),                                                 NULL,_UICONFIG("dt=color")),
			InputPortConfig<float>("Time",           _HELP("Duration of the display in seconds. Set to -1 for unlimited time.")),
			{ 0 }
		};

		config.sDescription = _HELP("Draws an AABB for debugging purposes");
		config.pInputPorts = in_config;
		config.SetCategory(EFLN_DEBUG);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
#if !defined(_RELEASE)
		if (event == eFE_Activate)
		{
			if (IsPortActive(pActInfo, eIP_Draw))
			{
				IPersistantDebug* pPersistentDebug = gEnv->pGameFramework->GetIPersistantDebug();
				if (pPersistentDebug)
				{
					const Vec3 minPos = GetPortVec3(pActInfo, eIP_MinPos);
					const Vec3 maxPos = GetPortVec3(pActInfo, eIP_MaxPos);
					const float time = GetPortFloat(pActInfo, eIP_Time);
					const ColorF color = GetPortVec3(pActInfo, eIP_Color);

					pPersistentDebug->Begin("FG_AABB", false);
					pPersistentDebug->AddAABB(minPos, maxPos, color, time);
				}
			}
		}
#endif
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(this);
	}
};

class CFlowNode_DrawPlanarDisc : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_DrawPlanarDisc(SActivationInfo* pActInfo)
	{
	}

	enum EInputPorts
	{
		eIP_Draw = 0,
		eIP_Pos,
		eIP_InnerRadius,
		eIP_OuterRadius,
		eIP_Color,
		eIP_Time,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig<SFlowSystemVoid>("Draw", _HELP("Trigger to draw the Planar Disc")),
			InputPortConfig<Vec3>("Pos",             _HELP("Centre position of Planar Disc")),
			InputPortConfig<float>("InnerRadius",    _HELP("Inner radius of Planar Disc")),
			InputPortConfig<float>("OuterRadius",    _HELP("Outer radius of Planar Disc")),
			InputPortConfig<Vec3>("Color",           _HELP("Color of the Planar Disc"),                                          NULL,_UICONFIG("dt=color")),
			InputPortConfig<float>("Time",           _HELP("Duration of the display in seconds. Set to -1 for unlimited time.")),
			{ 0 }
		};

		config.sDescription = _HELP("Draws a Planar Disc for debugging purposes");
		config.pInputPorts = in_config;
		config.SetCategory(EFLN_DEBUG);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
#if !defined(_RELEASE)
		if (event == eFE_Activate)
		{
			if (IsPortActive(pActInfo, eIP_Draw))
			{
				IPersistantDebug* pPersistentDebug = gEnv->pGameFramework->GetIPersistantDebug();
				if (pPersistentDebug)
				{
					const Vec3 pos = GetPortVec3(pActInfo, eIP_Pos);
					const float innerRadius = GetPortFloat(pActInfo, eIP_InnerRadius);
					const float outerRadius = GetPortFloat(pActInfo, eIP_OuterRadius);
					const float time = GetPortFloat(pActInfo, eIP_Time);
					const ColorF color = GetPortVec3(pActInfo, eIP_Color);

					pPersistentDebug->Begin("FG_PlanarDisc", false);
					pPersistentDebug->AddPlanarDisc(pos, innerRadius, outerRadius, color, time);
				}
			}
		}
#endif
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(this);
	}
};

class CFlowNode_DrawEntityTag : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputPorts
	{
		eIP_Draw = 0,
		eIP_Message,
		eIP_FontSize,
		eIP_Color,
		eIP_Time,
	};

	enum EOutputPorts
	{
		eOP_Done
	};

	CFlowNode_DrawEntityTag(SActivationInfo* pActInfo)
		: m_waitTime(0.0f)
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_DrawEntityTag(pActInfo);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SEntityTagParams defaultTag;

		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Draw",       _HELP("Draw Entity Tag")),
			InputPortConfig<string>("Message", _HELP("The message to display")),
			InputPortConfig<float>("FontSize", defaultTag.size,                 _HELP("Input font size")),
			InputPortConfig<Vec3>("Color",     Vec3(defaultTag.color.r,         defaultTag.color.g,                                                          defaultTag.color.b),_HELP("Text color"), NULL, _UICONFIG("dt=color")),
			InputPortConfig<float>("Time",     defaultTag.fadeTime,             _HELP("Duration of the display in seconds. Set to -1 for unlimited time.")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_AnyType("Done", _HELP("Tag has finished being displayed")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Draws a text message above an entity");
		config.SetCategory(EFLN_DEBUG);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
#if !defined(_RELEASE)
		if (event == eFE_Initialize && IsPortActive(pActInfo, eIP_Draw))
		{
			m_waitTime = 0.0f;
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
		}
		else if (event == eFE_Activate && IsPortActive(pActInfo, eIP_Draw))
		{
			IEntity* pEntity = pActInfo->pEntity;
			if (pEntity)
			{
				IPersistantDebug* pPersistentDebug = gEnv->pGameFramework->GetIPersistantDebug();
				if (pPersistentDebug)
				{
					SEntityTagParams params;
					params.entity = pEntity->GetId();
					params.text = GetPortString(pActInfo, eIP_Message);
					params.size = GetPortFloat(pActInfo, eIP_FontSize);
					params.color = ColorF(GetPortVec3(pActInfo, eIP_Color), 1.0f);
					// the time input from this node goes to fade time but if it is unlimited time we need to set that in the visibleTime, not fade
					if (GetPortFloat(pActInfo, eIP_Time) < 0.0f)
					{
						params.fadeTime = 0.0f;
						params.visibleTime = -1.0f;
						m_waitTime = -1.0f;
					}
					else
					{
						params.fadeTime = GetPortFloat(pActInfo, eIP_Time);
						params.visibleTime = 0.0f;
					}
					params.tagContext = "FG_DrawEntityTag";

					pPersistentDebug->AddEntityTag(params);

					if (m_waitTime != -1.0f)
					{
						m_waitTime = gEnv->pTimer->GetFrameStartTime() + (params.fadeTime + params.visibleTime);
						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
					}
				}
			}
		}
		else if (event == eFE_Update)
		{
			// unlimited time does not update the node, no need to test for that
			if (m_waitTime < gEnv->pTimer->GetFrameStartTime())
			{
				m_waitTime.SetSeconds(0.0f);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				ActivateOutput(pActInfo, eOP_Done, GetPortAny(pActInfo, eIP_Draw));
			}
		}
#endif
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	CTimeValue m_waitTime;
};

class CFlowNode_DrawEntityTagAdvanced : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputPorts
	{
		eIP_Draw = 0,
		eIP_Message,
		eIP_FadeTime,
		eIP_FontSize,
		eIP_ViewDistance,
		eIP_StaticID,
		eIP_Column,
		eIP_Color,
		eIP_Time
	};

	enum EOutputPorts
	{
		eOP_Done
	};

	CFlowNode_DrawEntityTagAdvanced(SActivationInfo* pActInfo)
		: m_waitTime(0.0f)
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_DrawEntityTagAdvanced(pActInfo);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SEntityTagParams defaultTag;

		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Draw",           _HELP("Draw advanced Entity Tag")),
			InputPortConfig<string>("Message",     _HELP("The message to display")),
			InputPortConfig<float>("FadeTime",     defaultTag.fadeTime,                            _HELP("Seconds for fade out")),
			InputPortConfig<float>("FontSize",     defaultTag.size,                                _HELP("Input font size")),
			InputPortConfig<float>("ViewDistance", defaultTag.viewDistance,                        _HELP("Distance from camera entity must be within")),
			InputPortConfig<string>("StaticID",    _HELP("Identifier for displaying static tags")),
			InputPortConfig<int>("ColumnNum",      defaultTag.column,                              _HELP("Which column to display on (usually 1)")),
			InputPortConfig<Vec3>("Color",         Vec3(defaultTag.color.r,                        defaultTag.color.g,                                                                        defaultTag.color.b),_HELP("Text color"), NULL, _UICONFIG("dt=color")),
			InputPortConfig<float>("Time",         defaultTag.visibleTime,                         _HELP("Duration of the display in full alpha in seconds. Set to -1 for unlimited time.")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_AnyType("Done", _HELP("Tag has finished being displayed")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Draws a text message above an entity");
		config.SetCategory(EFLN_DEBUG);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
#if !defined(_RELEASE)
		if (event == eFE_Initialize && IsPortActive(pActInfo, eIP_Draw))
		{
			m_waitTime = 0.0f;
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
		}
		else if (event == eFE_Activate && IsPortActive(pActInfo, eIP_Draw))
		{
			IEntity* pEntity = pActInfo->pEntity;
			if (pEntity)
			{
				IPersistantDebug* pPersistentDebug = gEnv->pGameFramework->GetIPersistantDebug();
				if (pPersistentDebug)
				{
					SEntityTagParams params;
					params.entity = pEntity->GetId();
					params.text = GetPortString(pActInfo, eIP_Message);
					params.size = GetPortFloat(pActInfo, eIP_FontSize);
					params.color = ColorF(GetPortVec3(pActInfo, eIP_Color), 1.0f);
					params.visibleTime = GetPortFloat(pActInfo, eIP_Time);
					params.fadeTime = GetPortFloat(pActInfo, eIP_FadeTime);
					params.viewDistance = GetPortFloat(pActInfo, eIP_ViewDistance);
					params.staticId = GetPortString(pActInfo, eIP_StaticID);
					params.column = GetPortInt(pActInfo, eIP_Column);
					params.tagContext = "FG_DrawEntityTagAdvanced";

					pPersistentDebug->AddEntityTag(params);

					m_waitTime = gEnv->pTimer->GetFrameStartTime() + (params.fadeTime + params.visibleTime);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
				}
			}
		}
		else if (event == eFE_Update)
		{
			if (m_waitTime < gEnv->pTimer->GetFrameStartTime())
			{
				m_waitTime.SetSeconds(0.0f);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				ActivateOutput(pActInfo, eOP_Done, GetPortAny(pActInfo, eIP_Draw));
			}
		}
#endif
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	CTimeValue m_waitTime;
};

REGISTER_FLOW_NODE("Debug:Draw:Direction", CFlowNode_DrawDirection);
REGISTER_FLOW_NODE("Debug:Draw:Cone", CFlowNode_DrawCone);
REGISTER_FLOW_NODE("Debug:Draw:Cylinder", CFlowNode_DrawCylinder);
REGISTER_FLOW_NODE("Debug:Draw:Line", CFlowNode_DrawLine);
REGISTER_FLOW_NODE("Debug:Draw:Sphere", CFlowNode_DrawSphere);
REGISTER_FLOW_NODE("Debug:Draw:AABB", CFlowNode_DrawAABB);
REGISTER_FLOW_NODE("Debug:Draw:PlanarDisc", CFlowNode_DrawPlanarDisc);
REGISTER_FLOW_NODE("Debug:Draw:EntityTag", CFlowNode_DrawEntityTag);
REGISTER_FLOW_NODE("Debug:Draw:EntityTagAdvanced", CFlowNode_DrawEntityTagAdvanced);
