// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "FlowInspectorDefault.h"
#include <FlowSystem/FlowSystem.h>
#include <FlowSystem/FlowGraph.h>
#include <FlowSystem/FlowData.h>
#include <CryAISystem/IAIAction.h>

#include <CryEntitySystem/IEntitySystem.h>
#include <CryRenderer/IRenderer.h>

// TODO: find a better place for them

// Address equality operator
inline bool operator==(const SFlowAddress& lhs, const SFlowAddress& rhs)
{
	return lhs.node == rhs.node && lhs.port == rhs.port && lhs.isOutput == rhs.isOutput;
}

namespace
{
static const int MAX_ROWS = 23;             // max number of one-time-flow lines
static const int MAX_ROWS_CONT = 12;        // max number of rows for continuous flows

static const float MAX_AGE = 10.0f;         // max age of one-time-flows updates before fade out
static const float MAX_AGE_CONT = 2.5f;     // max age of continuous updates before fade out

// visuals
static const float BASE_Y = 50.0f;
static const float ROW_SIZE = 9.5f;
static const float COL_SIZE = 5.0f;

static const ColorF DRAW_COLORS[CFlowInspectorDefault::eRTLast] = {
	ColorF(0.5f, 0.5f, 1.0f, 1.0f),     // eRTUnknown blue-ish
	ColorF(1.0f, 1.0f, 1.0f, 1.0f),     // eRTEntity white
	ColorF(1.0f, 0.9f, 0.3f, 1.0f),     // eRTAction yellow-ish
};

float smoothstep(float t)   // it's graphics after all ;-)
{
	return t * t * (3.0f - 2.0f * t);
}

// TODO: better description (currently it's more debug...)
void GetGraphNameAndType(IFlowGraph* pGraph, string& name, CFlowInspectorDefault::ERecType& type)
{
	name = "<noname>";
	type = CFlowInspectorDefault::eRTUnknown;

	IAIAction* pAction = pGraph->GetAIAction();
	EntityId id = pGraph->GetGraphEntity(0);
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
	if (pEntity != 0)
	{
		name = pEntity->GetName();
		type = CFlowInspectorDefault::eRTEntity;
	}
	else
	{
		id = pGraph->GetGraphEntity(1);
		pEntity = gEnv->pEntitySystem->GetEntity(id);
		if (pEntity != 0)
		{
			name = pEntity->GetName();
			type = CFlowInspectorDefault::eRTEntity;
		}
	}
	if (pAction != 0)
	{
		type = CFlowInspectorDefault::eRTAction;
		name = pAction->GetName();
		if (!name.empty())
			name += " ";
		name += "User=";
		pEntity = pAction->GetUserEntity();
		if (pEntity)
		{
			name += pEntity->GetName();
		}
		else name += "<none>";

		name += " Obj=";
		pEntity = pAction->GetObjectEntity();
		if (pEntity)
		{
			name += pEntity->GetName();
		}
		else name += "<none>";
	}
}

const char* GetPortName(IFlowGraph* pGraph, const SFlowAddress addr)
{
	CFlowData* data = static_cast<CFlowData*>(pGraph->GetNodeData(addr.node));
	return data->GetPortName(addr.port, addr.isOutput);
}

string GetNodeName(IFlowGraph* pGraph, const SFlowAddress addr)
{
	const char* typeName = pGraph->GetNodeTypeName(addr.node);
	const char* nodeName = pGraph->GetNodeName(addr.node);
	string human;
	human += typeName;
	human += " Node:";

	if (nodeName == 0)
	{
		IEntity* pEntity = ((CFlowGraph*)pGraph)->GetIEntityForNode(addr.node);
		if (pEntity)
			human += pEntity->GetName();
	}
	else
		human += nodeName;
	return human;
}
};

CFlowInspectorDefault::CFlowInspectorDefault(IFlowSystem* pFlowSystem)
	: m_pRenderer(gEnv->pRenderer)
	, m_pFlowSystem(pFlowSystem)
	, m_refs(0)
	, m_newOneTime(0)
	, m_newCont(0)
	, m_bProcessing(false)
	, m_bPaused(false)
{
}

CFlowInspectorDefault::~CFlowInspectorDefault()
{
}

void
CFlowInspectorDefault::AddRef()
{
	++m_refs;
}

void
CFlowInspectorDefault::Release()
{
	if (--m_refs <= 0)
		delete this;
}

/* virtual */ void
CFlowInspectorDefault::PreUpdate(IFlowGraph* pGraph)
{
	m_bProcessing = true;
	m_currentTime = gEnv->pTimer->GetFrameStartTime();
}

/* virtual */ void
CFlowInspectorDefault::PostUpdate(IFlowGraph* pGraph)
{
	ColorF headColor(1.0f, 0.5f, 0.5f, 1.0f);
	ColorF filterColor(1.0f, 0.1f, 0.1f, 1.0f);
	if (!m_bPaused)
	{
		UpdateRecords();
	}

#if 1
	if (m_newCont <= MAX_ROWS_CONT)
		DrawLabel(2, 0, headColor, 0.2f, "Continuous Flows:");
	else
		DrawLabel(2, 0, headColor, 0.2f, "Continuous Flows [truncated]:");

	DrawRecords(m_contRecords, 1, m_bPaused ? 0.0f : MAX_AGE_CONT);
	if (m_newOneTime <= MAX_ROWS)
		DrawLabel(2, 1 + MAX_ROWS_CONT, headColor, 0.2f, "Flows:");
	else
		DrawLabel(2, 1 + MAX_ROWS_CONT, headColor, 0.2f, "Flows [truncated]:");

	if (m_bPaused)
		DrawLabel(2, -1, headColor, 0.5f, "Paused:");

	if (m_filters.empty() == false)
		DrawLabel(11, -1, filterColor, 0.0, "Filter Active");

	DrawRecords(m_oneTimeRecords, 1 + 1 + MAX_ROWS_CONT, m_bPaused ? 0.0f : MAX_AGE);
#endif

	m_bProcessing = true; // FIXME: actually we should set it to 'false' now, but then we don't get EntityEvents, which occur not during the FG update phase
	m_curRecords.resize(0);

	if (gEnv->pInput)
	{
		static TKeyName scrollKey("scrolllock");
		if (gEnv->pInput->InputState(scrollKey, eIS_Pressed))
			m_bPaused = !m_bPaused;
	}
}

bool
CFlowInspectorDefault::RunFilters(IFlowGraph* pGraph, const SFlowAddress from, const SFlowAddress to)
{
	if (m_filters.empty())
		return true;

	bool gotBlock = false;
	bool gotPass = false;
	IFilter::EFilterResult res;
	IFilter_AutoArray::iterator iter(m_filters.begin());
	IFilter_AutoArray::iterator end(m_filters.end());

	while (iter != end)
	{
		res = (*iter)->Apply(pGraph, from, to);
		gotBlock |= res == IFilter::eFR_Block;
		gotPass |= res == IFilter::eFR_Pass;
		++iter;
	}

	return (!gotBlock || gotPass);
	/*
	   if (gotBlock && !gotPass)
	   return false;
	   return true;
	 */
}

/* virtual */ void
CFlowInspectorDefault::NotifyFlow(IFlowGraph* pGraph, const SFlowAddress from, const SFlowAddress to)
{
	static char msg[256];

	if (!m_bProcessing)
		return;
	if (m_bPaused)
		return;

	if (RunFilters(pGraph, from, to) == false)
		return;

	string name;
	TFlowRecord rec;
	GetGraphNameAndType(pGraph, name, rec.m_type);
	rec.m_from = from;
	rec.m_to = to;
	rec.m_pGraph = pGraph;
	rec.m_tstamp = m_currentTime;

	const TFlowInputData* data = pGraph->GetInputValue(to.node, to.port);
	if (0 != data)
	{
		rec.m_data = *data;
	}
	string val;
	rec.m_data.GetValueWithConversion(val);

	cry_sprintf(msg, "0x%p %s [%s:%s] -> [%s:%s] Val=%s",
	            (const IFlowGraph*) rec.m_pGraph,
	            name.c_str(),
	            GetNodeName(rec.m_pGraph, rec.m_from).c_str(),
	            GetPortName(rec.m_pGraph, rec.m_from),
	            GetNodeName(rec.m_pGraph, rec.m_to).c_str(),
	            GetPortName(rec.m_pGraph, rec.m_to), val.c_str());
	rec.m_message = msg;

	if (CFlowSystemCVars::Get().m_inspectorLog != 0)
	{
		CryLogAlways("[fgi] %s", msg);
	}

	m_curRecords.push_back(rec);
}

/* virtual */ void
CFlowInspectorDefault::NotifyProcessEvent(IFlowNode::EFlowEvent event, IFlowNode::SActivationInfo* pActInfo, IFlowNode* pImpl)
{
}

/* virtual */ void
CFlowInspectorDefault::UpdateRecords()
{
	m_newOneTime = 0;
	m_newCont = 0;

	// if we find a record equal to one in the m_oneTimeRecords, we put it into the m_contRecords instead (replacing one which is already there)
	std::vector<TFlowRecord>::const_iterator iter(m_curRecords.begin());

	while (iter != m_curRecords.end())
	{
		const TFlowRecord& rec(*iter);
		std::deque<TFlowRecord>::const_iterator iterInOneTimeRecords = std::find(m_oneTimeRecords.begin(), m_oneTimeRecords.end(), rec);
		std::deque<TFlowRecord>::iterator iterInContRecords = std::find(m_contRecords.begin(), m_contRecords.end(), rec);

		if (iterInOneTimeRecords != m_oneTimeRecords.end())
		{
			// if found in oneTimeRecords put it into the contList or replace copy
			if (iterInContRecords != m_contRecords.end())
				*iterInContRecords = rec;
			else
			{
				m_contRecords.push_back(rec);
				++m_newCont;
			}
		}
		else
		{
			// not found oneTimeRecords. replace copy in cont if it's there, otherwise put it into oneTimeRecords
			if (iterInContRecords != m_contRecords.end())
				*iterInContRecords = rec;
			else
			{
				m_oneTimeRecords.push_back(rec);
				++m_newOneTime;
			}
		}
		++iter;
	}

	// clear outdated
	while (!m_oneTimeRecords.empty())
	{
		float age = (m_currentTime - m_oneTimeRecords.front().m_tstamp).GetSeconds();
		if (age <= MAX_AGE) break;
		m_oneTimeRecords.pop_front();
	}

	// erase too old ones
	while (m_oneTimeRecords.size() > MAX_ROWS)
	{
		m_oneTimeRecords.pop_front();
	}

	// clear outdated in cont
	while (!m_contRecords.empty())
	{
		float age = (m_currentTime - m_contRecords.front().m_tstamp).GetSeconds();
		if (age <= MAX_AGE_CONT) break;
		m_contRecords.pop_front();
	}

	// erase too old ones in cont
	while (m_contRecords.size() > MAX_ROWS_CONT)
	{
		m_contRecords.pop_front();
	}

}

void
CFlowInspectorDefault::DrawLabel(float col, float row, const ColorF& color, float glow, const char* szText, float fScale) const
{
	const float ColumnSize = COL_SIZE;
	const float RowSize = ROW_SIZE;

	CryFixedStringT<128> msg;
	msg.Format("%s", szText ? szText : "No message");

	if (glow > 0.1f)
	{
		ColorF glowColor(color[0], color[1], color[2], glow);
		IRenderAuxText::Draw2dLabel((float)(ColumnSize * col + 1), (float)(BASE_Y + RowSize * row + 1), fScale * 1.2f, &glowColor[0], false, "%s", msg.c_str());
	}
	ColorF tmp(color);
	IRenderAuxText::Draw2dLabel((float)(ColumnSize * col), (float)(BASE_Y + RowSize * row), fScale * 1.2f, &tmp[0], false, "%s", msg.c_str());
}

/* virtual */ void
CFlowInspectorDefault::DrawRecords(const std::deque<TFlowRecord>& inRecords, int inBaseRow, float inMaxAge) const
{
	// records are pushed back, but we want to draw new records on top
	// so we draw from bottom to top, ensuring linear mem access
	int row = inRecords.size() - 1 + inBaseRow;
	string val;
	ColorF color;

	std::deque<TFlowRecord>::const_iterator iter(inRecords.begin());
	while (iter != inRecords.end())
	{
		const TFlowRecord& rec(*iter);
		float age = (m_currentTime - rec.m_tstamp).GetSeconds() - 0.5f;  // -0.5 grace time
		float ageFactor = inMaxAge > 0 ? age / inMaxAge : 0;
		if (ageFactor < 0) ageFactor = 0;
		if (ageFactor > 1) ageFactor = 1;
		ageFactor = smoothstep(1.0f - ageFactor);
		color = DRAW_COLORS[rec.m_type];
		color.a *= ageFactor;
		DrawLabel(2.f, (float)row, color, 0.0f, rec.m_message.c_str());
		--row;
		++iter;
	}
}

/* virtual */ void
CFlowInspectorDefault::AddFilter(IFlowGraphInspector::IFilterPtr pFilter)
{
	stl::push_back_unique(m_filters, pFilter);
}

/* virtual */ void
CFlowInspectorDefault::RemoveFilter(IFlowGraphInspector::IFilterPtr pFilter)
{
	stl::find_and_erase(m_filters, pFilter);
}

template<class T>
static void AddFlowRecordsTo(const T& cont, ICrySizer* s)
{
	s->AddObject(cont);
	for (typename T::const_iterator it = cont.begin(); it != cont.end(); ++it)
	{
		it->m_data.GetMemoryStatistics(s);
		s->Add(it->m_message);
	}
}

void CFlowInspectorDefault::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
	AddFlowRecordsTo(m_curRecords, s);
	AddFlowRecordsTo(m_oneTimeRecords, s);
	AddFlowRecordsTo(m_contRecords, s);
}
