// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EntityObjectDebugger.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CryString/StringUtils.h>

#include "Entity.h"

#include  <CrySchematyc/Runtime/IRuntimeClass.h>
#include  <CrySchematyc/IObject.h>
#include  <CrySchematyc/Services/ITimerSystem.h>

namespace
{
using CStackString = Schematyc::CStackString;

class CObjectDump : public Schematyc::IObjectDump
{
public:

	explicit inline CObjectDump(CStackString& debugText)
		: m_debugText(debugText)
	{}

	// IObjectDump

	virtual void operator()(const SStateMachine& stateMachine) override
	{
		m_debugText.append("[s] ");
		m_debugText.append(stateMachine.szName);
		m_debugText.append(" : ");
		m_debugText.append(stateMachine.state.szName);
		m_debugText.append("\n");
	}

	virtual void operator()(const SVariable& variable) override
	{
		m_debugText.append("[v] ");
		m_debugText.append(variable.szName);
		m_debugText.append(" : ");
		{
			CStackString temp;
			Schematyc::Any::ToString(temp, variable.value);
			m_debugText.append(temp);
		}
		m_debugText.append("\n");
	}

	virtual void operator()(const STimer& timer) override
	{
		m_debugText.append("[t] ");
		m_debugText.append(timer.szName);
		m_debugText.append(" : ");
		{
			CStackString temp;
			timer.timeRemaining.ToString(temp);
			m_debugText.append(temp);
		}
		m_debugText.append("\n");
	}

	// ~IObjectDump

private:

	CStackString& m_debugText;
};
} // Anonymous

int sc_EntityDebugConfig = 0;
ICVar* sc_EntityDebugFilter = nullptr;
ICVar* sc_EntityDebugTextPos = nullptr;

const char* szEntityDebugConfigDescription = "Schematyc - Configure entity debugging:\n"
                                             "\ts = draw states\n"
                                             "\tv = draw variables\n"
                                             "\tt = draw timers";

CEntityObjectDebugger::CEntityObjectDebugger()
{
	REGISTER_CVAR(sc_EntityDebugConfig, sc_EntityDebugConfig, VF_BITFIELD, szEntityDebugConfigDescription);
	sc_EntityDebugFilter = REGISTER_STRING("sc_EntityDebugFilter", "", VF_NULL, "Schematyc - Entity debug filter");
	sc_EntityDebugTextPos = REGISTER_STRING("sc_EntityDebugTextPos", "20, 10", VF_NULL, "Schematyc - Entity debug text position");
}

CEntityObjectDebugger::~CEntityObjectDebugger()
{
	gEnv->pConsole->UnregisterVariable("sc_EntityDebugConfig");
	gEnv->pConsole->UnregisterVariable("sc_EntityDebugFilter");
	gEnv->pConsole->UnregisterVariable("sc_EntityDebugTextPos");
}

void CEntityObjectDebugger::Update()
{
	if (sc_EntityDebugConfig == 0)
	{
		return;
	}

	Schematyc::ObjectDumpFlags objectDumpFlags;
	if (sc_EntityDebugConfig == 1)
	{
		objectDumpFlags.Add(Schematyc::EObjectDumpFlags::All);
	}
	else
	{
		if ((sc_EntityDebugConfig & AlphaBit('s')) != 0)
		{
			objectDumpFlags.Add(Schematyc::EObjectDumpFlags::States);
		}
		if ((sc_EntityDebugConfig & AlphaBit('v')) != 0)
		{
			objectDumpFlags.Add(Schematyc::EObjectDumpFlags::Variables);
		}
		if ((sc_EntityDebugConfig & AlphaBit('t')) != 0)
		{
			objectDumpFlags.Add(Schematyc::EObjectDumpFlags::Timers);
		}
	}

	const char* szFilter = sc_EntityDebugFilter->GetString();

	Vec2 textPos(ZERO);
	sscanf(sc_EntityDebugTextPos->GetString(), "%f, %f", &textPos.x, &textPos.y);

	auto visitEntityObject = [objectDumpFlags, szFilter, &textPos](EntityId entityId, Schematyc::ObjectId objectId) -> Schematyc::EVisitStatus
	{
		Schematyc::IObject* pObject = gEnv->pSchematyc->GetObject(objectId);
		if (pObject && pObject->GetEntity())
		{
			if (pObject->GetSimulationMode() == Schematyc::ESimulationMode::Game)
			{
				const CEntity& entity = static_cast<const CEntity&>(*pObject->GetEntity());
				const char* szName = entity.GetName();
				if ((szFilter[0] == '\0') || CryStringUtils::stristr(szName, szFilter))
				{
					const Schematyc::IRuntimeClass& objectClass = pObject->GetClass();

					CStackString debugText = szName;
					debugText.append(" : class=\"");
					debugText.append(objectClass.GetName());
					debugText.append("\", id=");
					{
						CStackString temp;
						ToString(temp, pObject->GetId());
						debugText.append(temp.c_str());
					}
					debugText.append("\n");

					CObjectDump objectDump(debugText);
					pObject->Dump(objectDump, objectDumpFlags);

					if (!debugText.empty())
					{
						const EDrawTextFlags drawTextFlags = static_cast<EDrawTextFlags>(eDrawText_2D | eDrawText_FixedSize | eDrawText_800x600 | eDrawText_Monospace);
						IRenderAuxText::Draw2dLabelEx(textPos.x, textPos.y, 1.8f, Col_White, drawTextFlags, "%s", debugText.c_str());

						uint32 lineCount = 1;
						for (const char* pPos = debugText.c_str(), * pEnd = pPos + debugText.length(); pPos < pEnd; ++pPos)
						{
							if (*pPos == '\n')
							{
								++lineCount;
							}
						}

						const float approxLineHeight = 15.0f;
						textPos.y += approxLineHeight * (lineCount + 1);
					}
				}
			}
		}
		return Schematyc::EVisitStatus::Continue;
	};

	IEntityItPtr pIt = g_pIEntitySystem->GetEntityIterator();
	//////////////////////////////////////////////////////////////////////////
	pIt->MoveFirst();
	while (!pIt->IsEnd())
	{
		CEntity* pEntity = static_cast<CEntity*>(pIt->Next());
		if (pEntity->GetSchematycObject())
		{
			visitEntityObject(pEntity->GetId(), pEntity->GetSchematycObject()->GetId());
		}
	}
}
