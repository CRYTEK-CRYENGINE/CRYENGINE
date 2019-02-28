// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if ENABLE_NET_DEBUG_INFO
	#if NET_PROFILE_ENABLE

		#include <CrySystem/ISystem.h>
		#include <CryEntitySystem/IEntitySystem.h>
		#include "NetDebugProfileViewer.h"
		#include "NetCVars.h"

CNetDebugProfileViewer::CNetDebugProfileViewer(uint32 type)
	: CNetDebugInfoSection(type, 0.0F, 0.0F)
{
}

CNetDebugProfileViewer::~CNetDebugProfileViewer()
{
}

void CNetDebugProfileViewer::Tick(const SNetworkProfilingStats* const pProfilingStats)
{
}

		#define PROFILE_ENTRY_STRING_DISPLAY_LENGTH (35)

void CNetDebugProfileViewer::DrawProfileEntry(const SNetProfileStackEntry* entry, int depth, int* line)
{
	CryFixedStringT<1024> stringBufferTemp;
	CryFixedStringT<1024> stringBuffer;
	const SNetProfileCount* counts = &entry->counts;

	switch (m_page)
	{
	case CNetDebugInfo::eP_ProfileViewOnEntities:
		{
			IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(entry->m_name.c_str());
			if (pEntity)
			{
				Vec3 pos = pEntity->GetWorldPos();
				Draw3dLine(pos, 0, s_green, entry->m_name.c_str());

				const float* pColor = s_white;
				if ((entry->m_budget > NO_BUDGET) && (entry->counts.m_worst > entry->m_budget))
				{
					pColor = s_red;
				}

				stringBufferTemp.Format("%u calls, %u/%u bits, %.2f Kbits, %.2f Kbits, %.2f Kbits",
				                        counts->m_callsAv.GetLast(), counts->m_seqbitsAv.GetLast(), counts->m_rmibitsAv.GetLast(), counts->m_payloadAv.GetLast(), counts->m_onWireAv.GetLast(), counts->m_worst);
				Draw3dLine(pos, 1, pColor, stringBufferTemp.c_str());

				pColor = s_lgrey;
				if ((entry->m_budget > NO_BUDGET) && (entry->counts.m_worstAv > entry->m_budget))
				{
					pColor = s_red;
				}

				stringBufferTemp.Format("%.0f calls, %.0f/%.0f bits, %.2f Kbits, %.2f Kbits, %.2f Kbits",
				                        counts->m_callsAv.GetAverage(), counts->m_seqbitsAv.GetAverage(), counts->m_rmibitsAv.GetAverage(), counts->m_payloadAv.GetAverage(), counts->m_onWireAv.GetAverage(), counts->m_worstAv);
				Draw3dLine(pos, 2, pColor, stringBufferTemp.c_str());
			}
		}
		break;

	case CNetDebugInfo::eP_ProfileViewLastSecond:
	case CNetDebugInfo::eP_ProfileViewLast5Seconds:
		{
			const float* pColor = s_white;
			if (*line & 1)
			{
				pColor = s_lgrey;
			}

			for (int i = 0; i < depth; i++)
			{
				stringBuffer.append(".");
			}

			stringBuffer += entry->m_name.substr(0, PROFILE_ENTRY_STRING_DISPLAY_LENGTH - depth).c_str();

			int extraSpace = PROFILE_ENTRY_STRING_DISPLAY_LENGTH - stringBuffer.length();
			for (int j = 0; j < extraSpace; j++)
			{
				stringBuffer.append(" ");
			}

			if (m_page == CNetDebugInfo::eP_ProfileViewLastSecond)
			{
				stringBufferTemp.Format("%7u\t%7u/%7u\t%15.2f\t%15.2f\t%15.2f",
				                        counts->m_callsAv.GetLast(), counts->m_seqbitsAv.GetLast(), counts->m_rmibitsAv.GetLast(), counts->m_payloadAv.GetLast(), counts->m_onWireAv.GetLast(), counts->m_worst);

				if ((entry->m_budget > NO_BUDGET) && (entry->counts.m_worst > entry->m_budget))
				{
					pColor = s_red;
				}
			}
			else
			{
				stringBufferTemp.Format("%7.0f\t%7.0f/%7.0f\t%15.2f\t%15.2f\t%15.2f",
				                        counts->m_callsAv.GetAverage(), counts->m_seqbitsAv.GetAverage(), counts->m_rmibitsAv.GetAverage(), counts->m_payloadAv.GetAverage(), counts->m_onWireAv.GetAverage(), counts->m_worstAv);

				if ((entry->m_budget > NO_BUDGET) && (entry->counts.m_worstAv > entry->m_budget))
				{
					pColor = s_red;
				}
			}
			stringBuffer.append(stringBufferTemp);

			DrawLine(*line, pColor, stringBuffer.c_str());

			(*line)++;
		}
		break;

	default:
		break;
	}
}

void CNetDebugProfileViewer::DrawTree(const SNetProfileStackEntry* root, int depth, int* line)
{
	SNetProfileStackEntry* entry = NULL;

	DrawProfileEntry(root, depth, line);

	for (entry = root->m_child; entry; entry = entry->m_next)
	{
		DrawTree(entry, depth + 1, line);
	}
}

void CNetDebugProfileViewer::Draw(const SNetworkProfilingStats* const pProfilingStats)
{
	switch (m_page)
	{
	case CNetDebugInfo::eP_ProfileViewLastSecond:
	case CNetDebugInfo::eP_ProfileViewLast5Seconds:
		{
			if (m_page == CNetDebugInfo::eP_ProfileViewLastSecond)
			{
				DrawHeader("TRAFFIC PROFILER - LAST SECOND");
			}
			else
			{
				DrawHeader("TRAFFIC PROFILER - AVERAGE LAST 5 SECONDS");
			}

			CryFixedStringT<1024> stringBufferTemp;
			for (int i = 0; i < PROFILE_ENTRY_STRING_DISPLAY_LENGTH; i++)
			{
				stringBufferTemp.append(" ");
			}
			stringBufferTemp.append("  Calls\tSeqbits/RMIbits\tPayload(Kbits)\tOnWire(Kbits)\tWorst(Kbits)");
			DrawLine(1, s_green, "%s", stringBufferTemp.c_str());
		}
		break;

	default:
		break;
	}

	int depth = 0;
	int line = 2;

	const SNetProfileStackEntry* root = NET_PROFILE_GET_TREE_ROOT();
	if (root)
	{
		DrawTree(root, depth, &line);
	}
}

	#endif
#endif
