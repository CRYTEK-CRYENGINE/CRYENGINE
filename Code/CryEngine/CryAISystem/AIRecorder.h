// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AIRecorder.h
   Description: Detailed event-based AI recorder for visual debugging

   -------------------------------------------------------------------------
   History:
   -01:07:2005 : Created by Kirill Bulatsev
   -19:11:2008 : Separated from simple text CAIRecorder by Matthew

 *********************************************************************/

#ifndef __AIRECORDER_H__
#define __AIRECORDER_H__

#pragma once

#ifdef CRYAISYSTEM_DEBUG

	#include <CryAISystem/IAIRecorder.h>
	#include <CryCore/StlUtils.h>
	#include "ObjectContainer.h"

typedef uint64 TAIRecorderUnitId;
struct SAIRecorderUnitId
{
	explicit SAIRecorderUnitId(uint32 lifeIndex, tAIObjectID aiObjectId) : id(0) { Set(lifeIndex, aiObjectId); }
	SAIRecorderUnitId(TAIRecorderUnitId _id) : id(_id) {}

	operator tAIObjectID() const { return tAIObjectID(id & 0xFFFFFFFF); }
	operator TAIRecorderUnitId() const { return id; }

	inline void Set(uint32 lifeIndex, tAIObjectID aiObjectId)
	{
		id = (uint64(lifeIndex) << 32) | aiObjectId;
	}

	TAIRecorderUnitId id;
};

class CAIRecorder;

class CRecorderUnit :
	public IAIDebugRecord
{
public:
	CRecorderUnit(CAIRecorder* pRecorder, CTimeValue startTime, CWeakRef<CAIObject> refUnit, uint32 uLifeIndex);
	virtual ~CRecorderUnit();

	SAIRecorderUnitId GetId() const { return m_id; }

	IAIDebugStream*   GetStream(IAIRecordable::e_AIDbgEvent streamTag);
	void              ResetStreams(CTimeValue startTime);

	void              RecordEvent(IAIRecordable::e_AIDbgEvent event, const IAIRecordable::RecorderEventData* pEventData);
	bool              LoadEvent(IAIRecordable::e_AIDbgEvent stream);

	bool              Save(FILE* pFile);
	bool              Load(FILE* pFile);

	void              SetName(const char* szName) { m_sName = szName; }
	const char*       GetName() const             { return m_sName.c_str(); }

protected:

	struct StreamBase :
		public IAIDebugStream
	{
		struct StreamUnitBase
		{
			float m_StartTime;
			StreamUnitBase(float time) : m_StartTime(time){}
		};
		typedef std::vector<StreamUnitBase*> TStream;

		StreamBase(char const* name) : m_CurIdx(0), m_name(name) {}
		virtual ~StreamBase() { ClearImpl(); }
		virtual bool SaveStream(FILE* pFile) = 0;
		virtual bool LoadStream(FILE* pFile) = 0;
		virtual void AddValue(const IAIRecordable::RecorderEventData* pEventData, float t) = 0;
		virtual bool WriteValue(const IAIRecordable::RecorderEventData* pEventData, float t) = 0;
		virtual bool LoadValue(FILE* pFile) = 0;
		virtual void Clear() { ClearImpl(); }
		void         Seek(float whereTo);
		int          GetCurrentIdx();
		int          GetSize();
		float        GetStartTime();
		float        GetEndTime();
		bool         IsEmpty()       { return m_Stream.empty(); }

		char const*  GetName() const { return m_name; }

		// Needed for IO usage with string streams that use index lookups
		virtual bool LoadStringIndex(FILE* pFile) { return false; }
		virtual bool SaveStringIndex(FILE* pFile) { return false; }
		virtual bool IsUsingStringIndex() const   { return false; }

		TStream     m_Stream;
		int         m_CurIdx;
		char const* m_name;

	private:
		void ClearImpl();

	};

	struct StreamStr :
		public StreamBase
	{
		struct StreamUnit : public StreamBase::StreamUnitBase
		{
			string m_String;
			StreamUnit(float time, const char* pStr) : StreamUnitBase(time), m_String(pStr){}
		};
		StreamStr(char const* name, bool bUseIndex = false);
		bool  SaveStream(FILE* pFile);
		bool  LoadStream(FILE* pFile);
		void  AddValue(const IAIRecordable::RecorderEventData* pEventData, float t);
		bool  WriteValue(float t, const char* str, FILE* pFile);
		bool  WriteValue(const IAIRecordable::RecorderEventData* pEventData, float t);
		bool  LoadValue(float& t, string& name, FILE* pFile);
		bool  LoadValue(FILE* pFile);
		void  Clear();
		void* GetCurrent(float& startingFrom);
		bool  GetCurrentString(string& sOut, float& startingFrom);
		void* GetNext(float& startingFrom);

		// Index usage for optimizing disk write usage
		virtual bool LoadStringIndex(FILE* pFile);
		virtual bool SaveStringIndex(FILE* pFile);
		virtual bool IsUsingStringIndex() const { return m_bUseIndex; }
		uint32       GetOrMakeStringIndex(const char* szString);
		bool         GetStringFromIndex(uint32 uIndex, string& sOut) const;

		typedef std::unordered_map<string, uint32, stl::hash_strcmp<string>, stl::hash_strcmp<string>> TStrIndexLookup;
		TStrIndexLookup m_StrIndexLookup;
		uint32          m_uIndexGen;
		enum { INVALID_INDEX = 0 };
		bool            m_bUseIndex;
	};

	struct StreamVec3 :
		public StreamBase
	{
		struct StreamUnit : public StreamBase::StreamUnitBase
		{
			StreamUnit(float time, const Vec3& pos) : StreamUnitBase(time), m_Pos(pos) {}
			Vec3 m_Pos;
		};
		StreamVec3(char const* name, bool bUseFilter = false) : StreamBase(name), m_bUseFilter(bUseFilter) {}
		bool  SaveStream(FILE* pFile);
		bool  LoadStream(FILE* pFile);
		void  AddValue(const IAIRecordable::RecorderEventData* pEventData, float t);
		bool  WriteValue(float t, const Vec3& vec, FILE* pFile);
		bool  WriteValue(const IAIRecordable::RecorderEventData* pEventData, float t);
		bool  LoadValue(float& t, Vec3& vec, FILE* pFile);
		bool  LoadValue(FILE* pFile);
		void* GetCurrent(float& startingFrom);
		bool  GetCurrentString(string& sOut, float& startingFrom);
		void* GetNext(float& startingFrom);

		// Returns TRUE if the point should be recorded
		bool FilterPoint(const IAIRecordable::RecorderEventData* pEventData) const;
		bool m_bUseFilter;
	};

	struct StreamFloat :
		public StreamBase
	{
		struct StreamUnit : public StreamBase::StreamUnitBase
		{
			StreamUnit(float time, float val) : StreamUnitBase(time), m_Val(val) {}
			float m_Val;
		};
		StreamFloat(char const* name, bool bUseFilter = false) : StreamBase(name), m_bUseFilter(bUseFilter) {}
		bool  SaveStream(FILE* pFile);
		bool  LoadStream(FILE* pFile);
		void  AddValue(const IAIRecordable::RecorderEventData* pEventData, float t);
		bool  WriteValue(float t, float val, FILE* pFile);
		bool  WriteValue(const IAIRecordable::RecorderEventData* pEventData, float t);
		bool  LoadValue(float& t, float& val, FILE* pFile);
		bool  LoadValue(FILE* pFile);
		void* GetCurrent(float& startingFrom);
		bool  GetCurrentString(string& sOut, float& startingFrom);
		void* GetNext(float& startingFrom);

		// Returns TRUE if the point should be recorded
		bool FilterPoint(const IAIRecordable::RecorderEventData* pEventData) const;
		bool m_bUseFilter;
	};

	typedef std::map<IAIRecordable::e_AIDbgEvent, StreamBase*> TStreamMap;

	TStreamMap        m_Streams;
	CTimeValue        m_startTime;
	string            m_sName;
	CAIRecorder*      m_pRecorder;
	SAIRecorderUnitId m_id;
};

class CRecordable
{
public:
	CRecordable();
	virtual ~CRecordable() {}

	void ResetRecorderUnit()
	{
		m_pMyRecord = NULL;
	}

protected:
	CRecorderUnit* GetOrCreateRecorderUnit(CAIObject* pAIObject, bool bForce = false);

protected:
	CRecorderUnit*         m_pMyRecord;
	CCountedRef<CAIObject> m_refUnit;
};

class CAIRecorder : public IAIRecorder, public ISystemEventListener
{
public:
	CAIRecorder();
	~CAIRecorder();

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	// ~ISystemEventListener

	bool IsRunning(void) const;

	// Initialise after construction
	void Init(void);
	void Shutdown(void);

	void Update();

	// Ignored while m_bStarted
	bool Save(const char* filename = NULL);

	// Ignored while m_bStarted
	bool Load(const char* filename = NULL);

	// Prepare to record events
	void Start(EAIRecorderMode mode, const char* filename = NULL);

	// Finalise recording, stop recording events
	void Stop(const char* filename = NULL);

	// Clear any recording in memory
	void Reset(void);

	// Called from AI System when it is reset
	void           OnReset(IAISystem::EResetReason reason);

	bool           AddListener(IAIRecorderListener* pListener);
	bool           RemoveListener(IAIRecorderListener* pListener);

	CRecorderUnit* AddUnit(CWeakRef<CAIObject> refObject, bool force = false);

	//	void	ChangeOwnerName(const char* pOldName, const char* pNewName);

	static FILE* m_pFile; // Hack!

protected:

	// Get the complete filename
	void GetCompleteFilename(char const* szFilename, bool bAppendFileCount, string& sOut) const;

	bool Read(FILE* pFile);

	bool Write(FILE* pFile);

	// Clear out any dummy objects previously created
	void DestroyDummyObjects();

	EAIRecorderMode m_recordingMode;

	typedef std::map<TAIRecorderUnitId, CRecorderUnit*> TUnits;
	TUnits m_Units;

	typedef std::vector<CCountedRef<CAIObject>> TDummyObjects;
	TDummyObjects m_DummyObjects;

	typedef std::vector<IAIRecorderListener*> TListeners;
	TListeners m_Listeners;

	ILog*      m_pLog;

	char*      m_lowLevelFileBuffer;
	uint32     m_unitLifeCounter;
	int        m_lowLevelFileBufferSize;
};

#endif //CRYAISYSTEM_DEBUG

#endif //__AIRECORDER_H__
