// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AIRecorder.cpp
   Description: Detailed event-based AI recorder for visual debugging

   -------------------------------------------------------------------------
   History:
   -01:07:2005 : Created by Kirill Bulatsev
   -19:11:2008 : Separated from simple text CAIRecorder by Matthew

   Notes:        Don't include the header in CAISystem - below it is redundant

 *********************************************************************/

#include "StdAfx.h"
#include "AIRecorder.h"
#include "PipeUser.h"
#include <CryString/StringUtils.h>
#include <CryGame/IGameFramework.h>

#ifdef CRYAISYSTEM_DEBUG

// New AI recorder
	#define AIRECORDER_FOLDER             "Recordings"
	#define AIRECORDER_DEFAULT_FILE       "AIRECORD"
	#define AIRECORDER_EDITOR_AUTO_APPEND "EDITORAUTO"
	#define AIRECORDER_VERSION            4

// We use some magic numbers to help when debugging the save structure
	#define RECORDER_MAGIC 0xAA1234BB
	#define UNIT_MAGIC     0xCC9876DD
	#define EVENT_MAGIC    0xEE0921FF

// Anything larger than this is assumed to be a corrupt file
	#define MAX_NAME_LENGTH         1024
	#define MAX_UNITS               10024
	#define MAX_EVENT_STRING_LENGTH 1024

FILE* CAIRecorder::m_pFile = NULL;  // Hack!

struct RecorderFileHeader
{
	int version;
	int unitsNumber;
	int magic;

	RecorderFileHeader()
		: version(AIRECORDER_VERSION)
		, unitsNumber(0)
		, magic(RECORDER_MAGIC)
	{}

	bool check(void) { return RECORDER_MAGIC == magic && unitsNumber < MAX_UNITS; }

};

struct UnitHeader
{
	int               nameLength;
	TAIRecorderUnitId ID;
	int               magic;
	// And the variable length name follows this
	UnitHeader()
		: nameLength(0)
		, ID(0)
		, magic(UNIT_MAGIC)
	{}

	bool check(void) { return (UNIT_MAGIC == magic && nameLength < MAX_NAME_LENGTH); }
};

struct StreamedEventHeader
{
	TAIRecorderUnitId unitID;
	int               streamID;
	int               magic;

	StreamedEventHeader()
		: unitID(0)
		, streamID(0)
		, magic(EVENT_MAGIC)
	{}

	bool check(void) { return EVENT_MAGIC == magic; }

};

//----------------------------------------------------------------------------------------------
void CRecorderUnit::RecordEvent(IAIRecordable::e_AIDbgEvent event, const IAIRecordable::RecorderEventData* pEventData)
{
	if (!m_pRecorder || !m_pRecorder->IsRunning()) return;

	float time = (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds();

	if (CAIRecorder::m_pFile) //(Hack!)
	{
		bool bSuccess;
		StreamedEventHeader header;
		header.unitID = m_id;
		header.streamID = event;
		bSuccess = (fwrite(&header, sizeof(header), 1, CAIRecorder::m_pFile) == 1);
		// We go through the stream to get the right virtual function

		bSuccess &= m_Streams[event]->WriteValue(pEventData, time);
		if (!bSuccess)
		{
			gEnv->pLog->LogError("[AI Recorder] Failed to write stream event");
		}
	}
	else
	{
		m_Streams[event]->AddValue(pEventData, time);
	}
}

//----------------------------------------------------------------------------------------------
bool CRecorderUnit::LoadEvent(IAIRecordable::e_AIDbgEvent stream)
{
	if (CAIRecorder::m_pFile) //(Hack!)
		return m_Streams[stream]->LoadValue(CAIRecorder::m_pFile);
	return true;
}

//
//----------------------------------------------------------------------------------------------
CRecordable::CRecordable()
	: m_pMyRecord(NULL)
{

}

//
//----------------------------------------------------------------------------------------------
CRecorderUnit* CRecordable::GetOrCreateRecorderUnit(CAIObject* pAIObject, bool bForce)
{
	if (!m_pMyRecord && pAIObject && !gEnv->pSystem->IsSerializingFile())
	{
		m_pMyRecord = gAIEnv.GetAIRecorder()->AddUnit(pAIObject->GetSelfReference(), bForce);
	}

	return m_pMyRecord;
}

//
//----------------------------------------------------------------------------------------------
CRecorderUnit::CRecorderUnit(CAIRecorder* pRecorder, CTimeValue startTime, CWeakRef<CAIObject> refUnit, uint32 lifeIndex)
	: m_startTime(startTime)
	, m_pRecorder(pRecorder)
	, m_id(lifeIndex, refUnit.GetObjectID())
{
	CRY_ASSERT(refUnit.IsSet());
	CRY_ASSERT(m_pRecorder);

	// Get the name of the unit object
	if (refUnit.IsSet())
	{
		m_sName = refUnit.GetAIObject()->GetName();
	}
	else
	{
		m_sName.Format("UNKNOWN_UNIT_%u", (tAIObjectID)m_id);
	}

	m_Streams[IAIRecordable::E_RESET] = new CRecorderUnit::StreamStr("Reset");
	m_Streams[IAIRecordable::E_SIGNALRECIEVED] = new CRecorderUnit::StreamStr("Signal Received", true);
	m_Streams[IAIRecordable::E_SIGNALRECIEVEDAUX] = new CRecorderUnit::StreamStr("Auxilary Signal Received", true);
	m_Streams[IAIRecordable::E_SIGNALEXECUTING] = new CRecorderUnit::StreamStr("Signal Executing", true);
	m_Streams[IAIRecordable::E_GOALPIPESELECTED] = new CRecorderUnit::StreamStr("Goalpipe Selected", true);
	m_Streams[IAIRecordable::E_GOALPIPEINSERTED] = new CRecorderUnit::StreamStr("Goalpipe Inserted", true);
	m_Streams[IAIRecordable::E_GOALPIPERESETED] = new CRecorderUnit::StreamStr("Goalpipe Reset", true);
	m_Streams[IAIRecordable::E_BEHAVIORSELECTED] = new CRecorderUnit::StreamStr("Behavior Selected", true);
	m_Streams[IAIRecordable::E_BEHAVIORDESTRUCTOR] = new CRecorderUnit::StreamStr("Behavior Destructor", true);
	m_Streams[IAIRecordable::E_BEHAVIORCONSTRUCTOR] = new CRecorderUnit::StreamStr("Behavior Constructor", true);
	m_Streams[IAIRecordable::E_ATTENTIONTARGET] = new CRecorderUnit::StreamStr("AttTarget", true);
	m_Streams[IAIRecordable::E_ATTENTIONTARGETPOS] = new CRecorderUnit::StreamVec3("AttTarget Pos", true);
	m_Streams[IAIRecordable::E_REGISTERSTIMULUS] = new CRecorderUnit::StreamStr("Stimulus", true);
	m_Streams[IAIRecordable::E_HANDLERNEVENT] = new CRecorderUnit::StreamStr("Handler Event", true);
	m_Streams[IAIRecordable::E_ACTIONSUSPEND] = new CRecorderUnit::StreamStr("Action Suspend", true);
	m_Streams[IAIRecordable::E_ACTIONRESUME] = new CRecorderUnit::StreamStr("Action Resume", true);
	m_Streams[IAIRecordable::E_ACTIONEND] = new CRecorderUnit::StreamStr("Action End", true);
	m_Streams[IAIRecordable::E_EVENT] = new CRecorderUnit::StreamStr("Event", true);
	m_Streams[IAIRecordable::E_REFPOINTPOS] = new CRecorderUnit::StreamVec3("Ref Point", true);
	m_Streams[IAIRecordable::E_AGENTPOS] = new CRecorderUnit::StreamVec3("Agent Pos", true);
	// (Kevin) Direction should be able to be filtered? But needs a lower threshold. So instead of bool, maybe supply threshold value? (10/08/2009)
	m_Streams[IAIRecordable::E_AGENTDIR] = new CRecorderUnit::StreamVec3("Agent Dir", false);
	m_Streams[IAIRecordable::E_LUACOMMENT] = new CRecorderUnit::StreamStr("Lua Comment", true);
	m_Streams[IAIRecordable::E_PERSONALLOG] = new CRecorderUnit::StreamStr("Personal Log", true);
	m_Streams[IAIRecordable::E_HEALTH] = new CRecorderUnit::StreamFloat("Health", true);
	m_Streams[IAIRecordable::E_HIT_DAMAGE] = new CRecorderUnit::StreamStr("Hit Damage", false);
	m_Streams[IAIRecordable::E_DEATH] = new CRecorderUnit::StreamStr("Death", true);
	m_Streams[IAIRecordable::E_PRESSUREGRAPH] = new CRecorderUnit::StreamFloat("Pressure", true);
	m_Streams[IAIRecordable::E_BOOKMARK] = new CRecorderUnit::StreamFloat("Bookmark", false);
}

//
//----------------------------------------------------------------------------------------------
CRecorderUnit::~CRecorderUnit()
{
	for (TStreamMap::iterator strItr(m_Streams.begin()); strItr != m_Streams.end(); ++strItr)
		delete strItr->second;
	m_Streams.clear();
}

//
//----------------------------------------------------------------------------------------------
void CRecorderUnit::StreamBase::Seek(float whereTo)
{
	m_CurIdx = 0;

	if (whereTo > FLT_EPSILON)
	{
		const int iMaxIndex = max((int)m_Stream.size() - 1, 0);
		while (m_CurIdx < iMaxIndex && m_Stream[m_CurIdx]->m_StartTime <= whereTo)
		{
			++m_CurIdx;
		}
		m_CurIdx = max(m_CurIdx - 1, 0);
	}
}

//
//----------------------------------------------------------------------------------------------
int CRecorderUnit::StreamBase::GetCurrentIdx()
{
	return m_CurIdx;
}

//
//----------------------------------------------------------------------------------------------
int CRecorderUnit::StreamBase::GetSize()
{
	return (int)(m_Stream.size());
}

//
//----------------------------------------------------------------------------------------------
void CRecorderUnit::StreamBase::ClearImpl()
{
	for (size_t i = 0; i < m_Stream.size(); ++i)
		delete m_Stream[i];
	m_Stream.clear();
}

//
//----------------------------------------------------------------------------------------------
float CRecorderUnit::StreamBase::GetStartTime()
{
	if (m_Stream.empty())
		return 0.0f;
	return m_Stream.front()->m_StartTime;
}

//
//----------------------------------------------------------------------------------------------
float CRecorderUnit::StreamBase::GetEndTime()
{
	if (m_Stream.empty())
		return 0.0f;
	return m_Stream.back()->m_StartTime;
}

//
//----------------------------------------------------------------------------------------------
CRecorderUnit::StreamStr::StreamStr(char const* name, bool bUseIndex /*= false*/)
	: StreamBase(name)
	, m_bUseIndex(bUseIndex)
	, m_uIndexGen(INVALID_INDEX)
{
	if (bUseIndex)
	{
		// Index not supported in disk mode currently
	#if !defined(_RELEASE) && !defined(CONSOLE_CONST_CVAR_MODE)
		if (gAIEnv.CVars.Recorder == eAIRM_Disk)
		{
			gEnv->pLog->LogWarning("[AI Recorder] %s is set to use String Index mode but this is not supported in Disk mode yet!", name);
			m_bUseIndex = false;
		}
	#endif
	}
}

//
//----------------------------------------------------------------------------------------------
void* CRecorderUnit::StreamStr::GetCurrent(float& startingFrom)
{
	if (m_CurIdx < 0 || m_CurIdx >= (int)m_Stream.size())
		return NULL;
	startingFrom = m_Stream[m_CurIdx]->m_StartTime;
	return (void*)(static_cast<StreamUnit*>(m_Stream[m_CurIdx])->m_String.c_str());
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamStr::GetCurrentString(string& sOut, float& startingFrom)
{
	sOut = (char*)GetCurrent(startingFrom);
	return (!sOut.empty());
}

//
//----------------------------------------------------------------------------------------------
void* CRecorderUnit::StreamStr::GetNext(float& startingFrom)
{
	if (++m_CurIdx >= (int)m_Stream.size())
	{
		m_CurIdx = (int)m_Stream.size();
		return NULL;
	}
	startingFrom = m_Stream[m_CurIdx]->m_StartTime;
	return (void*)(static_cast<StreamUnit*>(m_Stream[m_CurIdx])->m_String.c_str());
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamStr::LoadStringIndex(FILE* pFile)
{
	// Read number of indexes made
	if (!fread(&m_uIndexGen, sizeof(m_uIndexGen), 1, pFile)) return false;

	for (uint32 uCount = 0; uCount < m_uIndexGen; ++uCount)
	{
		uint32 uIndex = INVALID_INDEX;
		if (!fread(&uIndex, sizeof(uIndex), 1, pFile)) return false;

		int strLen;
		if (!fread(&strLen, sizeof(strLen), 1, pFile)) return false;

		if (strLen < 0 || strLen > MAX_EVENT_STRING_LENGTH) return false;

		string sString;
		if (strLen != 0)
		{
			std::vector<char> buffer;
			buffer.resize(strLen);
			if (fread(&buffer[0], sizeof(char), strLen, pFile) != strLen) return false;
			sString.assign(&buffer[0], strLen);
		}
		else
		{
			sString = "<Empty string>";
		}

		// Add to map
		m_StrIndexLookup.insert(TStrIndexLookup::value_type(sString.c_str(), uIndex));
	}

	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamStr::SaveStringIndex(FILE* pFile)
{
	// Write number of indexes made
	if (!fwrite(&m_uIndexGen, sizeof(m_uIndexGen), 1, pFile)) return false;

	TStrIndexLookup::const_iterator itIter = m_StrIndexLookup.begin();
	TStrIndexLookup::const_iterator itIterEnd = m_StrIndexLookup.end();
	while (itIter != itIterEnd)
	{
		const string& str(itIter->first);
		const uint32& uIndex(itIter->second);
		const int strLen = (str ? str.size() : 0);

		if (!fwrite(&uIndex, sizeof(uIndex), 1, pFile)) return false;
		if (!fwrite(&strLen, sizeof(strLen), 1, pFile)) return false;
		if (!str.empty())
		{
			if (fwrite(str.c_str(), sizeof(char), strLen, pFile) != strLen) return false;
		}

		++itIter;
	}

	return true;
}

//
//----------------------------------------------------------------------------------------------
uint32 CRecorderUnit::StreamStr::GetOrMakeStringIndex(const char* szString)
{
	uint32 uResult = INVALID_INDEX;

	if (m_bUseIndex && szString)
	{
		uResult = stl::find_in_map(m_StrIndexLookup, szString, INVALID_INDEX);
		if (INVALID_INDEX == uResult)
		{
			// Add it and make new one
			uResult = ++m_uIndexGen;
			m_StrIndexLookup.insert(TStrIndexLookup::value_type(szString, uResult));
		}
	}

	return uResult;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamStr::GetStringFromIndex(uint32 uIndex, string& sOut) const
{
	CRY_ASSERT(m_bUseIndex);

	sOut.clear();

	bool bResult = false;
	TStrIndexLookup::const_iterator itIter = m_StrIndexLookup.begin();
	TStrIndexLookup::const_iterator itIterEnd = m_StrIndexLookup.end();
	while (itIter != itIterEnd)
	{
		if (itIter->second == uIndex)
		{
			sOut = itIter->first;
			bResult = true;
			break;
		}
		++itIter;
	}

	return bResult;
}

//
//----------------------------------------------------------------------------------------------
void CRecorderUnit::StreamStr::AddValue(const IAIRecordable::RecorderEventData* pEventData, float t)
{
	// Make an index for it, but don't record it. It'll be saved to file later with the right index
	if (m_bUseIndex)
	{
		GetOrMakeStringIndex(pEventData->pString);
	}

	m_Stream.push_back(new StreamUnit(t, pEventData->pString));
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamStr::WriteValue(const IAIRecordable::RecorderEventData* pEventData, float t)
{
	return WriteValue(t, pEventData->pString, CAIRecorder::m_pFile);
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamStr::WriteValue(float t, const char* str, FILE* pFile)
{
	// When using index, we have the string length be all bits on to represent this fact
	int strLen = ~0;
	if (!m_bUseIndex)
	{
		strLen = (str ? strlen(str) : 0);
	}

	if (!fwrite(&t, sizeof(t), 1, pFile)) return false;
	if (!fwrite(&strLen, sizeof(strLen), 1, pFile)) return false;
	if (m_bUseIndex)
	{
		uint32 uIndex = GetOrMakeStringIndex(str);
		if (uIndex > INVALID_INDEX)
		{
			if (!fwrite(&uIndex, sizeof(uint32), 1, pFile)) return false;
		}
	}
	else if (str)
	{
		if (fwrite(str, sizeof(char), strLen, pFile) != strLen) return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamStr::LoadValue(FILE* pFile)
{
	string name;
	float time;
	if (!LoadValue(time, name, pFile)) return false;

	// Check chronological ordering
	if (!m_Stream.empty() && m_Stream.back()->m_StartTime > time)
	{
		gEnv->pLog->LogError("[AI Recorder] Aborting - events are not recorded in chronological order");
		return false;
	}

	m_Stream.push_back(new StreamUnit(time, name.c_str()));
	return true;
}

//
//----------------------------------------------------------------------------------------------
void CRecorderUnit::StreamStr::Clear()
{
	CRecorderUnit::StreamBase::Clear();

	m_StrIndexLookup.clear();
	m_uIndexGen = INVALID_INDEX;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamStr::SaveStream(FILE* pFile)
{
	int counter(m_Stream.size());
	if (!fwrite(&counter, sizeof(counter), 1, pFile)) return false;
	for (uint32 idx(0); idx < m_Stream.size(); ++idx)
	{
		StreamUnit* pCurUnit = static_cast<StreamUnit*>(m_Stream[idx]);
		float t = pCurUnit->m_StartTime;
		const char* pStr = pCurUnit->m_String.c_str();
		if (!WriteValue(t, pStr, pFile)) return false;
	}
	return true;
}

bool CRecorderUnit::StreamStr::LoadValue(float& t, string& name, FILE* pFile)
{
	int strLen;
	if (!fread(&t, sizeof(t), 1, pFile)) return false;
	if (!fread(&strLen, sizeof(strLen), 1, pFile)) return false;

	// When using index, we have the string length be all bits on to represent this fact
	bool bIsEmpty = true;
	if (strLen == ~0)
	{
		uint32 uIndex = INVALID_INDEX;
		if (!fread(&uIndex, sizeof(uint32), 1, pFile)) return false;
		bIsEmpty = (!GetStringFromIndex(uIndex, name));
	}
	else
	{
		if (strLen < 0 || strLen > MAX_EVENT_STRING_LENGTH) return false;
		if (strLen != 0)
		{
			std::vector<char> buffer;
			buffer.resize(strLen);
			if (fread(&buffer[0], sizeof(char), strLen, pFile) != strLen) return false;
			name.assign(&buffer[0], strLen);
			bIsEmpty = false;
		}
	}

	if (bIsEmpty)
	{
		name = "<Empty string>";
	}

	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamStr::LoadStream(FILE* pFile)
{
	string name;
	int counter;
	fread(&counter, sizeof(counter), 1, pFile);

	for (int idx(0); idx < counter; ++idx)
		if (!LoadValue(pFile)) return false;

	return true;
}

//
//----------------------------------------------------------------------------------------------
void* CRecorderUnit::StreamVec3::GetCurrent(float& startingFrom)
{
	if (m_CurIdx < 0 || m_CurIdx >= (int)m_Stream.size())
		return NULL;
	startingFrom = m_Stream[m_CurIdx]->m_StartTime;
	return (void*)(&static_cast<StreamUnit*>(m_Stream[m_CurIdx])->m_Pos);
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamVec3::GetCurrentString(string& sOut, float& startingFrom)
{
	sOut.clear();

	Vec3* pVec = (Vec3*)GetCurrent(startingFrom);
	if (pVec)
	{
		const Vec3& v(*pVec);
		sOut = CryStringUtils::toString(v);
	}

	return (!sOut.empty());
}

//
//----------------------------------------------------------------------------------------------
void* CRecorderUnit::StreamVec3::GetNext(float& startingFrom)
{
	if (++m_CurIdx >= (int)m_Stream.size())
	{
		m_CurIdx = m_Stream.size();
		return NULL;
	}
	startingFrom = m_Stream[m_CurIdx]->m_StartTime;
	return (void*)(&static_cast<StreamUnit*>(m_Stream[m_CurIdx])->m_Pos);
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamVec3::FilterPoint(const IAIRecordable::RecorderEventData* pEventData) const
{
	bool bResult = true;

	// Simple point filtering.
	if (m_bUseFilter && !m_Stream.empty())
	{
		StreamUnit* pLastUnit(static_cast<StreamUnit*>(m_Stream.back()));
		const Vec3 vDelta = pLastUnit->m_Pos - pEventData->pos;
		bResult = (vDelta.len2() > 0.25f);
	}

	return bResult;
}

//
//----------------------------------------------------------------------------------------------
void CRecorderUnit::StreamVec3::AddValue(const IAIRecordable::RecorderEventData* pEventData, float t)
{
	if (FilterPoint(pEventData))
	{
		m_Stream.push_back(new StreamUnit(t, pEventData->pos));
	}
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamVec3::WriteValue(const IAIRecordable::RecorderEventData* pEventData, float t)
{
	bool bResult = true;

	if (FilterPoint(pEventData))
	{
		bResult = WriteValue(t, pEventData->pos, CAIRecorder::m_pFile);
	}

	return bResult;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamVec3::WriteValue(float t, const Vec3& vec, FILE* pFile)
{
	if (!fwrite(&t, sizeof(t), 1, pFile)) return false;
	if (!fwrite(&vec.x, sizeof(vec.x), 1, pFile)) return false;
	if (!fwrite(&vec.y, sizeof(vec.y), 1, pFile)) return false;
	if (!fwrite(&vec.z, sizeof(vec.z), 1, pFile)) return false;
	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamVec3::LoadValue(float& t, Vec3& vec, FILE* pFile)
{
	if (!fread(&t, sizeof(t), 1, pFile)) return false;
	if (!fread(&vec.x, sizeof(vec.x), 1, pFile)) return false;
	if (!fread(&vec.y, sizeof(vec.y), 1, pFile)) return false;
	if (!fread(&vec.z, sizeof(vec.z), 1, pFile)) return false;
	return true;
}

//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamVec3::LoadValue(FILE* pFile)
{
	Vec3 vec;
	float time;
	if (!LoadValue(time, vec, pFile)) return false;

	// Check chronological ordering
	if (!m_Stream.empty() && m_Stream.back()->m_StartTime > time)
	{
		gEnv->pLog->LogError("[AI Recorder] Aborting - events are not recorded in chronological order");
		return false;
	}

	m_Stream.push_back(new StreamUnit(time, vec));
	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamVec3::LoadStream(FILE* pFile)
{
	int counter;
	fread(&counter, sizeof(counter), 1, pFile);

	for (int idx(0); idx < counter; ++idx)
		if (!LoadValue(pFile)) return false;

	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamVec3::SaveStream(FILE* pFile)
{
	int counter(m_Stream.size());
	if (!fwrite(&counter, sizeof(counter), 1, pFile)) return false;
	for (uint32 idx(0); idx < m_Stream.size(); ++idx)
	{
		StreamUnit* pCurUnit(static_cast<StreamUnit*>(m_Stream[idx]));
		float time = pCurUnit->m_StartTime;
		Vec3& vect = pCurUnit->m_Pos;
		if (!WriteValue(time, vect, pFile)) return false;
	}
	return true;
}

//
//----------------------------------------------------------------------------------------------
void* CRecorderUnit::StreamFloat::GetCurrent(float& startingFrom)
{
	if (m_CurIdx < 0 || m_CurIdx >= (int)m_Stream.size())
		return NULL;
	startingFrom = m_Stream[m_CurIdx]->m_StartTime;
	return (void*)(&static_cast<StreamUnit*>(m_Stream[m_CurIdx])->m_Val);
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamFloat::GetCurrentString(string& sOut, float& startingFrom)
{
	sOut.clear();

	float* pF = (float*)GetCurrent(startingFrom);
	if (pF)
	{
		sOut = CryStringUtils::toString(*pF);
	}

	return (!sOut.empty());
}

//
//----------------------------------------------------------------------------------------------
void* CRecorderUnit::StreamFloat::GetNext(float& startingFrom)
{
	if (++m_CurIdx >= (int)m_Stream.size())
	{
		m_CurIdx = (int)m_Stream.size();
		return NULL;
	}
	startingFrom = m_Stream[m_CurIdx]->m_StartTime;
	return (void*)(&static_cast<StreamUnit*>(m_Stream[m_CurIdx])->m_Val);
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamFloat::FilterPoint(const IAIRecordable::RecorderEventData* pEventData) const
{
	bool bResult = true;

	// Simple point filtering.
	if (m_bUseFilter && !m_Stream.empty())
	{
		StreamUnit* pLastUnit(static_cast<StreamUnit*>(m_Stream.back()));
		float fDelta = pLastUnit->m_Val - pEventData->val;
		bResult = (fabsf(fDelta) > FLT_EPSILON);
	}

	return bResult;
}

//
//----------------------------------------------------------------------------------------------
void CRecorderUnit::StreamFloat::AddValue(const IAIRecordable::RecorderEventData* pEventData, float t)
{
	if (FilterPoint(pEventData))
	{
		m_Stream.push_back(new StreamUnit(t, pEventData->val));
	}
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamFloat::WriteValue(const IAIRecordable::RecorderEventData* pEventData, float t)
{
	bool bResult = true;

	if (FilterPoint(pEventData))
	{
		bResult = WriteValue(t, pEventData->val, CAIRecorder::m_pFile);
	}

	return bResult;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamFloat::WriteValue(float t, float val, FILE* pFile)
{
	if (!fwrite(&t, sizeof(t), 1, pFile)) return false;
	if (!fwrite(&val, sizeof(val), 1, pFile)) return false;
	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamFloat::LoadValue(float& t, float& val, FILE* pFile)
{
	if (!fread(&t, sizeof(t), 1, pFile)) return false;
	if (!fread(&val, sizeof(val), 1, pFile)) return false;
	return true;
}

//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamFloat::LoadValue(FILE* pFile)
{
	float val;
	float time;
	if (!LoadValue(time, val, pFile)) return false;

	// Check chronological ordering
	if (!m_Stream.empty() && m_Stream.back()->m_StartTime > time)
	{
		gEnv->pLog->LogError("[AI Recorder] Aborting - events are not recorded in chronological order");
		return false;
	}

	m_Stream.push_back(new StreamUnit(time, val));
	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamFloat::LoadStream(FILE* pFile)
{
	int counter;
	fread(&counter, sizeof(counter), 1, pFile);

	for (int idx(0); idx < counter; ++idx)
		if (!LoadValue(pFile)) return false;

	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamFloat::SaveStream(FILE* pFile)
{
	int counter(m_Stream.size());
	if (!fwrite(&counter, sizeof(counter), 1, pFile)) return false;
	for (uint32 idx(0); idx < m_Stream.size(); ++idx)
	{
		StreamUnit* pCurUnit(static_cast<StreamUnit*>(m_Stream[idx]));
		float time = pCurUnit->m_StartTime;
		float val = pCurUnit->m_Val;
		if (!WriteValue(time, val, pFile)) return false;
	}
	return true;
}

//
//----------------------------------------------------------------------------------------------
IAIDebugStream* CRecorderUnit::GetStream(IAIRecordable::e_AIDbgEvent streamTag)
{
	return m_Streams[streamTag];
}

//
//----------------------------------------------------------------------------------------------
void CRecorderUnit::ResetStreams(CTimeValue startTime)
{
	for (TStreamMap::iterator strItr(m_Streams.begin()); strItr != m_Streams.end(); ++strItr)
	{
		StreamBase* pStream(strItr->second);
		if (pStream)
			pStream->Clear();
	}
	m_startTime = startTime;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::Save(FILE* pFile)
{
	{
		// Make a list of all that are using string indexes
		TStreamMap stringIndexMap;
		for (TStreamMap::iterator strItr(m_Streams.begin()); strItr != m_Streams.end(); ++strItr)
		{
			StreamBase* pStream(strItr->second);
			if (pStream && pStream->IsUsingStringIndex())
			{
				IAIRecordable::e_AIDbgEvent type = ((strItr->first)); // The kind of stream
				stringIndexMap[type] = pStream;
			}
		}

		// Write the number of streams using string-index lookups
		int numStreams(stringIndexMap.size());
		if (!fwrite(&numStreams, sizeof(numStreams), 1, pFile)) return false;

		for (TStreamMap::iterator strItr(stringIndexMap.begin()); strItr != stringIndexMap.end(); ++strItr)
		{
			StreamBase* pStream(strItr->second);
			const int intType((strItr->first)); // The kind of stream
			if (!fwrite(&intType, sizeof(intType), 1, pFile)) return false;
			if (pStream && !pStream->SaveStringIndex(pFile)) return false;
		}
	}

	{
		// Write the number of streams stored
		int numStreams(m_Streams.size());
		if (!fwrite(&numStreams, sizeof(numStreams), 1, pFile)) return false;

		for (TStreamMap::iterator strItr(m_Streams.begin()); strItr != m_Streams.end(); ++strItr)
		{
			StreamBase* pStream(strItr->second);
			const int intType((strItr->first)); // The kind of stream
			if (!fwrite(&intType, sizeof(intType), 1, pFile)) return false;
			if (pStream && !pStream->SaveStream(pFile)) return false;
		}
	}

	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::Load(FILE* pFile)
{
	for (TStreamMap::iterator strItr(m_Streams.begin()); strItr != m_Streams.end(); ++strItr)
	{
		StreamBase* pStream(strItr->second);
		if (pStream)
			pStream->Clear();
	}

	{
		// Read the number of streams using string-index lookups stored
		int numStreams;
		if (!fread(&numStreams, sizeof(numStreams), 1, pFile)) return false;

		for (int i(0); i < numStreams; ++i)
		{
			IAIRecordable::e_AIDbgEvent intType; // The kind of stream
			if (!fread(&intType, sizeof(intType), 1, pFile)) return false;

			if (!m_Streams[intType]->LoadStringIndex(pFile)) return false;
		}
	}

	{
		// Read the number of streams stored
		int numStreams;
		if (!fread(&numStreams, sizeof(numStreams), 1, pFile)) return false;

		for (int i(0); i < numStreams; ++i)
		{
			IAIRecordable::e_AIDbgEvent intType; // The kind of stream
			if (!fread(&intType, sizeof(intType), 1, pFile)) return false;

			if (!m_Streams[intType]->LoadStream(pFile)) return false;
		}
	}

	return true;
}

//
//----------------------------------------------------------------------------------------------
CAIRecorder::CAIRecorder()
{
	m_recordingMode = eAIRM_Off;
	m_lowLevelFileBufferSize = 1024;
	m_lowLevelFileBuffer = new char[m_lowLevelFileBufferSize];
	m_pLog = NULL;
	m_unitLifeCounter = 0;
}

//
//----------------------------------------------------------------------------------------------
void CAIRecorder::Init(void)
{
	if (!m_pLog)
		m_pLog = gEnv->pLog;

	if (gEnv->pSystem)
	{
		ISystemEventDispatcher* pDispatcher = gEnv->pSystem->GetISystemEventDispatcher();
		if (pDispatcher)
			pDispatcher->RegisterListener(this, "CAIRecorder");
	}
}

//
//----------------------------------------------------------------------------------------------
bool CAIRecorder::IsRunning(void) const
{
	return (m_recordingMode != eAIRM_Off);
}

//
//----------------------------------------------------------------------------------------------
void CAIRecorder::GetCompleteFilename(char const* szFilename, bool bAppendFileCount, string& sOut) const
{
	const bool bIsEditor = gEnv->IsEditor();

	if (!szFilename || !szFilename[0])
	{
		// Use current level
		szFilename = PathUtil::GetFileName(gEnv->pGameFramework->GetLevelName());
	}

	if (!szFilename || !szFilename[0])
	{
		// Use defualt
		szFilename = AIRECORDER_DEFAULT_FILE;
	}

	CRY_ASSERT(szFilename && szFilename[0]);

	string sVersion;
	if (bIsEditor)
	{
		sVersion = AIRECORDER_EDITOR_AUTO_APPEND;
	}
	else
	{
		// Get current date line
		time_t ltime;
		time(&ltime);
		tm* pTm = localtime(&ltime);
		char szDate[128];
		strftime(szDate, 128, "Date(%d %b %Y) Time(%H %M %S)", pTm);

		// Get current version line
		const SFileVersion& fileVersion = gEnv->pSystem->GetFileVersion();

		sVersion.Format("Build(%d) %s", fileVersion[0], szDate);

	}

	string sBaseFile;
	sBaseFile.Format("%s_%s", szFilename, sVersion.c_str());

	sOut = PathUtil::Make(AIRECORDER_FOLDER, sBaseFile.c_str(), "rcd");
	gEnv->pCryPak->MakeDir(AIRECORDER_FOLDER);

	// Check if file already exists
	int iFileCount = 0;
	FILE* pFileChecker = (bAppendFileCount && !bIsEditor ? fopen(sOut.c_str(), "rb") : NULL);
	if (pFileChecker)
	{
		string sNewOut;
		while (pFileChecker)
		{
			fclose(pFileChecker);
			sNewOut.Format("%s(%d)", sBaseFile.c_str(), ++iFileCount);
			sOut = PathUtil::Make(AIRECORDER_FOLDER, sNewOut.c_str(), "rcd");
			pFileChecker = fopen(sOut.c_str(), "rb");
		}
	}
}

//
//----------------------------------------------------------------------------------------------
void CAIRecorder::OnReset(IAISystem::EResetReason reason)
{
	//if (!gEnv->IsEditor())
	//	return;

	if (gAIEnv.CVars.DebugRecordAuto == 0)
		return;

	const bool bIsSerializingFile = 0 != gEnv->pSystem->IsSerializingFile();
	if (!bIsSerializingFile)
	{
		// Handle starting/stoping the recorder
		switch (reason)
		{
		case IAISystem::RESET_INTERNAL:
		case IAISystem::RESET_INTERNAL_LOAD:
			Stop();
			Reset();
			break;

		case IAISystem::RESET_EXIT_GAME:
			Stop();
			break;

		case IAISystem::RESET_ENTER_GAME:
			Reset();
			Start(eAIRM_Memory);
			break;
		}
	}
}

//
//----------------------------------------------------------------------------------------------
bool CAIRecorder::AddListener(IAIRecorderListener* pListener)
{
	return stl::push_back_unique(m_Listeners, pListener);
}

//
//----------------------------------------------------------------------------------------------
bool CAIRecorder::RemoveListener(IAIRecorderListener* pListener)
{
	return stl::find_and_erase(m_Listeners, pListener);
}

//
//----------------------------------------------------------------------------------------------
void CAIRecorder::Start(EAIRecorderMode mode, const char* filename)
{
	if (IsRunning()) return;

	Init(); // make sure

	// Clear any late arriving events from last run
	Reset();

	string sFile;
	GetCompleteFilename(filename, true, sFile);

	switch (mode)
	{
	case eAIRM_Memory:
		{
			m_recordingMode = eAIRM_Memory;

			// No action required to start recording
		}
		break;
	case eAIRM_Disk:
		{
			m_recordingMode = eAIRM_Disk;

			// Redundant check
			if (m_pFile)
			{
				fclose(m_pFile);
				m_pFile = NULL;
			}

			// File is closed, so we have the chance to adjust buffer size
			int newBufferSize = gAIEnv.CVars.DebugRecordBuffer;
			newBufferSize = clamp_tpl(newBufferSize, 128, 1024000);
			if (newBufferSize != m_lowLevelFileBufferSize)
			{
				delete[] m_lowLevelFileBuffer;
				m_lowLevelFileBufferSize = newBufferSize;
				m_lowLevelFileBuffer = new char[newBufferSize];
			}

			// Open for streaming, using static file pointer
			m_pFile = fxopen(sFile.c_str(), "wb");

			if (m_pFile)
			{
				// Note - must use own buffer or memory manager may break!
				PREFAST_SUPPRESS_WARNING(6001)
				setvbuf(m_pFile, m_lowLevelFileBuffer, _IOFBF, m_lowLevelFileBufferSize);

				// Write preamble
				if (!Write(m_pFile))
				{
					fclose(m_pFile);
					m_pFile = NULL;
				}
			}
			// Leave it open
		}
		break;
	default:
		m_recordingMode = eAIRM_Off;

		// In other modes does nothing
		// In mode 0, will quite happily be started and stopped doing nothing
		break;
	}

	// Notify listeners
	TListeners::iterator itNext = m_Listeners.begin();
	while (itNext != m_Listeners.end())
	{
		TListeners::iterator itListener = itNext++;
		IAIRecorderListener* pListener = *itListener;
		CRY_ASSERT(pListener);

		pListener->OnRecordingStart(m_recordingMode, sFile.c_str());
	}
}

//
//----------------------------------------------------------------------------------------------
void CAIRecorder::Stop(const char* filename)
{
	if (!IsRunning()) return;

	EAIRecorderMode mode = m_recordingMode;
	m_recordingMode = eAIRM_Off;
	m_unitLifeCounter = 0;

	// Close the recorder file, should have been streaming
	if (m_pFile)
	{
		fclose(m_pFile);
		m_pFile = NULL;
	}

	switch (mode)
	{
	case eAIRM_Disk:
		{
			// Only required to close the recorder file
		}
		break;
	case eAIRM_Memory:
		{
			// Dump the history to disk
			Save(filename);
		}
		break;
	default:
		break;
	}

	string sFile;
	GetCompleteFilename(filename, true, sFile);

	// Notify listeners
	TListeners::iterator itNext = m_Listeners.begin();
	while (itNext != m_Listeners.end())
	{
		TListeners::iterator itListener = itNext++;
		IAIRecorderListener* pListener = *itListener;
		CRY_ASSERT(pListener);

		pListener->OnRecordingStop(sFile.c_str());
	}
}

//
//----------------------------------------------------------------------------------------------
CAIRecorder::~CAIRecorder()
{
	Shutdown();
}

//
//----------------------------------------------------------------------------------------------
void CAIRecorder::Shutdown(void)
{
	if (IsRunning())
		Stop();

	if (gEnv->pSystem)
	{
		ISystemEventDispatcher* pDispatcher = gEnv->pSystem->GetISystemEventDispatcher();
		if (pDispatcher)
			pDispatcher->RemoveListener(this);
	}

	DestroyDummyObjects();

	for (TUnits::iterator it = m_Units.begin(); it != m_Units.end(); ++it)
		delete it->second;
	m_Units.clear();

	if (m_lowLevelFileBuffer)
		delete[] m_lowLevelFileBuffer;
}

//
//----------------------------------------------------------------------------------------------
void CAIRecorder::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	if (event == ESYSTEM_EVENT_FAST_SHUTDOWN ||
	    event == ESYSTEM_EVENT_LEVEL_UNLOAD)
	{
		if (IsRunning())
			Stop();
	}
}

//
//----------------------------------------------------------------------------------------------
void CAIRecorder::Update()
{
	// This is never called so far
}

//
//----------------------------------------------------------------------------------------------
bool CAIRecorder::Load(const char* filename)
{
	if (IsRunning()) return false; // To avoid undefined behaviour

	// Open the AI recorder dump file
	string sFile = filename;
	if (sFile.empty())
	{
		GetCompleteFilename(filename, false, sFile);
	}
	FILE* pFile = fxopen(sFile.c_str(), "rb");

	bool bSuccess = false;
	if (pFile)
	{
		// Update static file pointer
		m_pFile = pFile;

		bSuccess = Read(pFile);

		m_pFile = NULL;
		fclose(pFile);
	}

	if (bSuccess)
	{
		// Notify listeners
		TListeners::iterator itNext = m_Listeners.begin();
		while (itNext != m_Listeners.end())
		{
			TListeners::iterator itListener = itNext++;
			IAIRecorderListener* pListener = *itListener;
			CRY_ASSERT(pListener);

			pListener->OnRecordingLoaded(sFile.c_str());
		}
	}

	return bSuccess;
}

bool CAIRecorder::Read(FILE* pFile)
{
	CAISystem* pAISystem = GetAISystem();
	CRY_ASSERT(pAISystem);

	// File header
	RecorderFileHeader fileHeader;
	fread(&fileHeader, sizeof(fileHeader), 1, pFile);
	if (!fileHeader.check())
		return false;
	if (fileHeader.version > AIRECORDER_VERSION)
	{
		m_pLog->LogError("[AI Recorder] Saved AI Recorder log is of wrong version number");
		fclose(pFile);
		return false;
	}

	// Clear all units streams
	Reset();

	// String stores name of each unit
	string name;

	//std::vector <tAIObjectID> tempIDs;  // Ids for units we create just during loading, to skip over data
	//std::map <tAIObjectID,tAIObjectID> idMap;   // Mapping the ids recorded in the file into those used in this run

	// For each record
	for (int i = 0; i < fileHeader.unitsNumber; i++)
	{
		UnitHeader header;
		// Read entity name and ID
		if (!fread(&header, sizeof(header), 1, pFile)) return false;
		if (!header.check()) return false;
		if (header.nameLength > 0)
		{
			std::vector<char> buffer;
			buffer.resize(header.nameLength);
			if (fread(&buffer[0], sizeof(char), header.nameLength, pFile) != header.nameLength) return false;
			name.assign(&buffer[0], header.nameLength);
		}
		else
		{
			name.clear();
		}

		string sDummyName;
		uint32 uLifeCounter = (header.ID >> 32);
		sDummyName.Format("%s_%u", name.c_str(), uLifeCounter);

		// Create a dummy object to represent this recording
		TDummyObjects::value_type refDummy;
		gAIEnv.pAIObjectManager->CreateDummyObject(refDummy, sDummyName);
		if (!refDummy.IsNil())
		{
			m_DummyObjects.push_back(refDummy);

			// Create map entry from the old, possibly changed ID to the current one
			//idMap[header.ID] = liveID;
			CAIObject* pLiveObject = refDummy.GetAIObject();
			CRecorderUnit* unit = pLiveObject->CreateAIDebugRecord();

			if (!unit) return false;
			if (!unit->Load(pFile)) return false;
		}
		else
		{
			m_pLog->LogError("[AI Recorder] Failed to create a Recorder Unit for \'%s\'", name.c_str());
		}
	}

	// After the "announced" data in the file, we continue, looking for more
	// A streamed log will typically have 0 length announced data (providing the IDs/names etc)
	// and all data will follow afterwards

	// For each event that follows
	// (Kevin) - What's the purpose of this section? Is it still needed? (24/8/09)
	bool bEndOfFile = false;
	do
	{
		// Read event metadata
		StreamedEventHeader header;
		bEndOfFile = (!fread(&header, sizeof(header), 1, pFile));
		if (!bEndOfFile)
		{
			if (!header.check())
			{
				m_pLog->LogError("[AI Recorder] corrupt event found reading streamed section");
				return false;
			}
			else
			{
				CRY_ASSERT_MESSAGE(false, "Recorder has streamded data. This code needs to be readded.");

				//int liveID = idMap[header.unitID];
				//if (!liveID)
				//{
				//	// New unannounced ID
				//	m_pLog->LogWarning("[AI Recorder] Unknown ID %d found in stream", header.unitID);
				//	// Generate a random ID - unlikely to collide.
				//	liveID = abs( (int)cry_rand()%100000 ) + 10000;
				//	tempIDs.push_back(liveID);
				//	idMap[header.unitID] = liveID;
				//}

				//CRecorderUnit *unit = AddUnit(liveID, true);
				//if (!unit) return false;
				//if (!unit->LoadEvent( (IAIRecordable::e_AIDbgEvent) header.streamID)) return false;
			}
		}
	}
	while (!bEndOfFile);

	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CAIRecorder::Save(const char* filename)
{
	if (IsRunning()) return false; // To avoid undefined behaviour

	// This method should not change state at all
	bool bSuccess = false;

	// Open the AI recorder dump file
	string sFile;
	GetCompleteFilename(filename, true, sFile);
	FILE* pFile = fxopen(sFile.c_str(), "wb");

	if (pFile)
	{
		// Update static file pointer
		m_pFile = pFile;

		// Note - must use own buffer or memory manager may break!
		setvbuf(pFile, m_lowLevelFileBuffer, _IOFBF, m_lowLevelFileBufferSize);
		bSuccess = Write(pFile);

		m_pFile = NULL;
		fclose(pFile);
	}

	if (bSuccess)
	{
		// Notify listeners
		TListeners::iterator itNext = m_Listeners.begin();
		while (itNext != m_Listeners.end())
		{
			TListeners::iterator itListener = itNext++;
			IAIRecorderListener* pListener = *itListener;
			CRY_ASSERT(pListener);

			pListener->OnRecordingSaved(sFile.c_str());
		}
	}
	else
	{
		m_pLog->LogError("[AI Recorder] Save dump failed");
	}

	return bSuccess;
}

bool CAIRecorder::Write(FILE* pFile)
{
	// File header
	RecorderFileHeader fileHeader;
	fileHeader.version = AIRECORDER_VERSION;
	fileHeader.unitsNumber = m_Units.size();
	if (!fwrite(&fileHeader, sizeof(fileHeader), 1, pFile)) return false;

	// For each record
	for (TUnits::iterator unitIter = m_Units.begin(); unitIter != m_Units.end(); ++unitIter)
	{
		CRecorderUnit* pUnit = unitIter->second;
		CRY_ASSERT(pUnit);
		if (!pUnit)
			continue;

		// Record entity name and ID (ID may be not be preserved)
		const string& name = pUnit->GetName();

		UnitHeader header;
		header.nameLength = (int)name.size();
		header.ID = pUnit->GetId();

		if (!fwrite(&header, sizeof(header), 1, pFile)) return false;
		if (fwrite(name.c_str(), sizeof(char), header.nameLength, pFile) != header.nameLength) return false;

		if (!pUnit->Save(pFile)) return false;
	}
	return true;
}

//
//----------------------------------------------------------------------------------------------
CRecorderUnit* CAIRecorder::AddUnit(CWeakRef<CAIObject> refObject, bool force)
{
	CRY_ASSERT(refObject.IsSet());

	CRecorderUnit* pNewUnit = NULL;

	if ((IsRunning() || force) && refObject.IsSet())
	{
		// Create a new unit only if activated or required
		const uint32 lifeIndex = ++m_unitLifeCounter;
		pNewUnit = new CRecorderUnit(this, GetAISystem()->GetFrameStartTime(), refObject, lifeIndex);
		m_Units[pNewUnit->GetId()] = pNewUnit;
	}

	return pNewUnit;
}

//
//----------------------------------------------------------------------------------------------
void CAIRecorder::Reset(void)
{
	DestroyDummyObjects();

	const CTimeValue frameStartTime = gEnv->pTimer->GetFrameStartTime();
	for (TUnits::iterator unitIter = m_Units.begin(); unitIter != m_Units.end(); ++unitIter)
	{
		unitIter->second->ResetStreams(frameStartTime);
	}
}

//
//----------------------------------------------------------------------------------------------
void CAIRecorder::DestroyDummyObjects()
{
	for (TDummyObjects::iterator dummyIter = m_DummyObjects.begin(); dummyIter != m_DummyObjects.end(); ++dummyIter)
	{
		TDummyObjects::value_type refDummy = *dummyIter;
		refDummy.Release();
	}

	m_DummyObjects.clear();
}

#endif //CRYAISYSTEM_DEBUG
