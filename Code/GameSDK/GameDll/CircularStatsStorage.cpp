// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/******************************************************************************
** CircularStatsStorage.cpp
** 23/04/10
******************************************************************************/

/*
	classes relating to memory management for game statistics
	statistics are allocated from a circular buffer and will be purged and lost
	if the circular buffer is not sufficiently large enough
*/

#include "StdAfx.h"
#include "CircularStatsStorage.h"
#include "GameCVars.h"
#include "Utility/CryWatch.h"
#include <CryCore/TypeInfo_impl.h>

// this define allows for structured 'newed' from allocators other than the circular buffer to be added to the stats system
// we can allow this to ease the transition to the circular buffer system but it generally defeats the purpose
#define ALSO_SUPPORT_NON_CIRCULAR_BUFFER_ALLOCS			0

const CCircularBufferTimeline *CCircularBufferStatsContainer::GetTimeline(
	size_t		inTimelineId) const
{
	CCircularBufferTimeline	*tl = NULL;

#ifndef _RELEASE
	Validate();
#endif

	if (inTimelineId < m_numTimelines)
	{
		tl = &m_timelines[inTimelineId];
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME,VALIDATOR_ERROR,"Statistics event %" PRISIZE_T " is larger than the max registered of %" PRISIZE_T ", event ignored",inTimelineId,m_numTimelines);
	}

	return tl;
}

CCircularBufferTimeline *CCircularBufferStatsContainer::GetMutableTimeline(
	size_t		inTimelineId)
{
	return const_cast<CCircularBufferTimeline*>(GetTimeline(inTimelineId));
}

bool CCircularBufferStatsContainer::HasAnyTimelineEvents() const
{
	bool		gotEvents=false;

	for (size_t i=0; i<m_numTimelines; i++)
	{
		if (!m_timelines[i].m_list.empty())
		{
			gotEvents=true;
			break;
		}
	}

	return gotEvents;
}

const SStatAnyValue *CCircularBufferStatsContainer::GetState(
	size_t		inStateId) const
{
	const SStatAnyValue		*result=NULL;

	if (inStateId < m_numStates)
	{
		result = &m_states[inStateId];
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME,VALIDATOR_ERROR,"Statistics state %" PRISIZE_T " is larger than the max registered of %" PRISIZE_T ", state ignored",inStateId,m_numStates);
	}

	return result;
}

SStatAnyValue *CCircularBufferStatsContainer::GetMutableState(
	size_t		inStateId)
{
	return const_cast<SStatAnyValue*>(GetState(inStateId));
}

const char *CCircularBufferStatsContainer::GetEventName(
	size_t		inEventId)
{
	IGameStatistics			*gs=g_pGame->GetIGameFramework()->GetIGameStatistics();
	const SGameStatDesc		*dc=gs ? gs->GetEventDesc(inEventId) : NULL;
	return dc ? dc->serializeName.c_str() : NULL;
}

CCircularBufferStatsContainer::CCircularBufferStatsContainer(
	CCircularBufferStatsStorage *inStorage) :
	m_storage(inStorage),
	m_timelines(NULL),
	m_states(NULL),
	m_refCount(0),
	m_numTimelines(0),
	m_numStates(0)
{
}

CCircularBufferStatsContainer::~CCircularBufferStatsContainer()
{
	Clear();			// not strictly necessary as circular buffer entries are all purgable anyway and the states will be freed by the destructors called by the array delete below, but this will allow us to verify the circular buffer is empty at the end of a stats save
	delete [] m_states;
	delete [] m_timelines;
}

void CCircularBufferStatsContainer::Init(size_t numEvents, size_t numStates)
{
	assert(m_states==NULL && m_timelines==NULL);
	m_states=new SStatAnyValue[numStates];
	m_numStates=numStates;
	m_timelines=new CCircularBufferTimeline[numEvents];
	m_numTimelines=numEvents;
}

#ifndef _RELEASE

// checks for memory stomps and fatals out if any have occurred
void CCircularBufferStatsContainer::Validate() const
{
	for (size_t i=0; i<m_numTimelines; i++)
	{
		CCircularBufferTimeline		*pTL=&m_timelines[i];
		CDoubleLinkedElement			*pHead=pTL->m_list.Head();
		CDoubleLinkedElement			*pTail=pTL->m_list.Tail();

		if (pTL->m_type == eRBPT_Invalid)
		{
			if (pHead != pTL->m_list.GetLink() || pTail != pTL->m_list.GetLink())
			{
				CryFatalError("Memory stomp, stats container %p timeline %" PRISIZE_T "has had nothing added to it, yet its head and tail have been altered (head %p , tail %p)", this, i, pHead, pTail);
			}
		}
		else
		{
			// check they're in the expected range
			if (m_storage->IsUsingCircularBuffer() && !(m_storage->ContainsPtr(pHead) && m_storage->ContainsPtr(pTail)) && !(pHead == pTL->m_list.GetLink() && pTail == pTL->m_list.GetLink()))
			{
				CryFatalError("Memory stomp, stats container %p timeline %" PRISIZE_T "has its head or tail out of the accepted range (head %p , tail %p)", this, i, pHead, pTail);
			}
		}
	}
}

#endif

void CCircularBufferStatsContainer::AddEvent(size_t eventID, const CTimeValue& time, const SStatAnyValue& val)
{
#ifndef _RELEASE
	Validate();
#endif

	if (val.type==eSAT_TXML && m_storage->ContainsPtr(val.pSerializable))
	{
		// link into linked list
		CCircularBufferTimeline				*timeline=GetMutableTimeline(eventID);
		if (timeline)
		{
			CCircularBufferTimelineEntry	*timelineEntry=static_cast<CCircularBufferTimelineEntry*>(val.pSerializable);

			timeline->m_list.AddTail(&timelineEntry->m_timelineLink);
			timeline->SetType(eRBPT_TimelineEntry);
			timelineEntry->AddRef();
			timelineEntry->SetTime(time);
#if DEBUG_CIRCULAR_STATS
			m_storage->m_numCircularEvents++;
#endif
		}
	}
	else
	{
		bool		record=true;

		if (val.type==eSAT_TXML && m_storage->IsUsingCircularBuffer())		// if trying to add a serialisable xml class that's not been allocated from the circular buffer, emit a warning
		{
#if ALSO_SUPPORT_NON_CIRCULAR_BUFFER_ALLOCS
			if (val.pSerializable)
			{
				CryWarning(VALIDATOR_MODULE_GAME,VALIDATOR_ERROR,"Statistics event %s needs moving to work with the circular buffer storage, this event IS being recorded but is using memory suboptimally",GetEventName(eventID));
				m_storage->m_numLegacyEvents++;
			}
			else
			{
				record=false;			
			}
#else
			if (val.pSerializable)	// If this is NULL it's probably because we ran out of memory in m_circularBuffer
			{
				CryWarning(VALIDATOR_MODULE_GAME,VALIDATOR_ERROR,"Statistics event %s needs moving to work with the circular buffer storage, this event is being ignored",GetEventName(eventID));
			}
			record=false;
#endif
		}

		if (record)
		{
			CCircularBufferTimeline			*timeline=GetMutableTimeline(eventID);
			if (timeline)
			{
				SCircularStatAnyValue		*stat=new (m_storage) SCircularStatAnyValue;
				if (stat)
				{
					stat->val=val;
					stat->time=time;
					timeline->m_list.AddTail(&stat->link);
					timeline->SetType(eRBPT_StatAnyValue);
#if DEBUG_CIRCULAR_STATS
					m_storage->m_numCircularEvents++;
#endif
				}
			}
		}
	}

#ifndef _RELEASE
	Validate();
#endif
}
	
void CCircularBufferStatsContainer::AddState(size_t stateID, const SStatAnyValue& val)
{
	SStatAnyValue		*state=GetMutableState(stateID);
	if (state)
	{
		*state=val;
	}
}

size_t CCircularBufferStatsContainer::GetEventTrackLength(size_t eventID) const
{
	size_t								result=0;
	const CCircularBufferTimeline		*timeline=GetTimeline(eventID);

	if (timeline)
	{
		result=timeline->m_list.size();
	}

	return result;
}

// FIXME : this is very inefficient as the data model is in a linked list - would be better to change the interface to use an iterator... or for it to just ask the container to convert itself
//		   to XML (preferably into a text buffer rather than xml node hierarchy) - that would be ideal
void CCircularBufferStatsContainer::GetEventInfo(size_t eventID, size_t idx, CTimeValue& outTime, SStatAnyValue& outParam) const
{
	bool							found=false;
	int								maxEvents=-1;
	const CCircularBufferTimeline	*timeline=GetTimeline(eventID);
	int								i=0;

	for (CDoubleLinkedList::const_iterator iter=timeline->m_list.begin(); iter!=timeline->m_list.end(); ++iter)
	{
		if (i==idx)
		{
			switch (timeline->GetType())
			{
				case eRBPT_TimelineEntry:
					{
						CCircularBufferTimelineEntry	*timelineEntry=CCircularBufferTimelineEntry::EntryFromListElement(*iter);

						outParam=SStatAnyValue(timelineEntry);
						outTime=timelineEntry->GetTime();
					}
					break;

				case eRBPT_StatAnyValue:
					{
						SCircularStatAnyValue			*stat=SCircularStatAnyValue::EntryFromListElement(*iter);

						outParam=stat->val;
						outTime=stat->time;
					}
					break;

				case eRBPT_Invalid:
					break;

				default:
					CRY_ASSERT_MESSAGE(0,"Found unrecognised timeline time in stats circular buffer - cannot return stats");
					break;
			}
			found=true;
			break;
		}
		i++;
	}
	maxEvents=i;

	CRY_ASSERT_MESSAGE(found==true, string().Format("Failed to find event info index %d for event %u, max events on timeline is %d",idx,eventID,maxEvents));
}

void CCircularBufferStatsContainer::GetStateInfo(size_t stateID, SStatAnyValue& outValue) const
{
	const SStatAnyValue		*state=GetState(stateID);

	if (state)
	{
		outValue=*state;
	}
}

void CCircularBufferStatsContainer::Clear()
{
	const SStatAnyValue	null(0);

	for (size_t i=0; i<m_numStates; i++)
	{
		SStatAnyValue		*state=GetMutableState(i);

		*state=null;			// would be better if SStatAnyValue had a Clear() function of its own
	}

	for (size_t i=0; i<m_numTimelines; i++)
	{
		CCircularBufferTimeline		*timeline=GetMutableTimeline(i);

		while (!timeline->m_list.empty())
		{
			switch (timeline->GetType())
			{
				case eRBPT_TimelineEntry:
					CCircularBufferTimelineEntry::EntryFromListElement(timeline->m_list.Head())->ForceRelease();	// we don't need to ForceRelease(), could do Unlink() then Release() but
					// i don't want people holding refs to these objects as they can be purged at any point
					// so lets try and catch some here
					break;

				case eRBPT_StatAnyValue:
					{
						SCircularStatAnyValue		*val=SCircularStatAnyValue::EntryFromListElement(timeline->m_list.Head());
						delete val;
					}
					break;

				default:
					CRY_ASSERT_MESSAGE(0,"unknown data type found in timeline, cannot cleanly destruct it - possible memory leak");
					break;
			}
		}
	}
}

bool CCircularBufferStatsContainer::IsEmpty() const
{
	bool		empty=true;

	for (size_t i=0; i<m_numTimelines && empty; i++)
	{
		const CCircularBufferTimeline *ll=GetTimeline(i);
		empty=ll->m_list.empty();
	}
	for (size_t i=0; i<m_numStates && empty; i++)
	{
		const SStatAnyValue		*val=GetState(i);
		empty=(val->type==eSAT_NONE);
	}

	return empty;
}

void CCircularBufferStatsContainer::GetMemoryStatistics(ICrySizer *pSizer)
{
	pSizer->Add(*this);

	for (size_t i=0; i<m_numStates ; i++)
	{
		const SStatAnyValue		*val=GetState(i);
		val->GetMemoryUsage(pSizer);
	}

	for (size_t i=0; i<m_numTimelines; i++)
	{
		const CCircularBufferTimeline	*timeline=GetTimeline(i);

		pSizer->Add(*timeline);

		// why no iterate through timeline and adding of element sizes?
		// i don't think it's correct to sum the amounts used from the circular buffer here - that memory is already allocated as part of the circular buffer storage and
		// shouldn't be counted twice - depends on how you want to use the memory statistics to analyse memory relating to pools i guess
	}
}


void CCircularBufferStatsContainer::AddRef()
{
	++m_refCount;
}

void CCircularBufferStatsContainer::Release()
{
	--m_refCount;
	if (m_refCount<=0)
	{
		delete this;
	}
}

CCircularBufferStatsStorage		*CCircularBufferStatsStorage::s_storage=NULL;

// a value of 0 stops the buffer being allocated allowing both unlimited allocation or for the memory not to be reserved if stats is disabled / unwanted
CCircularBufferStatsStorage::CCircularBufferStatsStorage(
	size_t				inBufferSize) :
#if DEBUG_CIRCULAR_STATS
	CGameMechanismBase("CircularStatsDebug"),
#endif
	m_totalBytesAlloced(0),
	m_totalBytesRequested(0),
	m_peakAlloc(0),
	m_numDiscards(0),
	m_refCount(0),
	m_serializeLocked(false)
{
#if DEBUG_CIRCULAR_STATS
	m_numLegacyEvents=0;
	m_numCircularEvents=0;
#endif

	if (inBufferSize>0)
	{
		m_circularBuffer.Init(inBufferSize,NULL);
		m_circularBuffer.SetPacketDiscardCallback(DiscardCallback,this);
	}
	CRY_ASSERT_MESSAGE(!s_storage,"Creating multiple CCircularBufferStatsStorage instances isn't currently supported, would need to remove default 'new' operator for CCircularBufferTimelineEntry and force the specification of which CCircularBufferStatsStorage should be used");
	s_storage=this;
}

CCircularBufferStatsStorage::~CCircularBufferStatsStorage()
{
	m_circularBuffer.SetPacketDiscardCallback(NULL,NULL);
	if (s_storage==this)
	{
		s_storage=NULL;
	}
}

void CCircularBufferStatsStorage::AddRef()
{
	++m_refCount;
}

void CCircularBufferStatsStorage::Release()
{
	--m_refCount;
	if (m_refCount<=0)
	{
		delete this;
	}
}

void CCircularBufferStatsStorage::LockForSerialization()
{
	CRY_ASSERT_MESSAGE(!m_serializeLocked,"Attempted to lock stats storage for serialization, but it is already locked??");
	m_serializeLocked=true;
}

void CCircularBufferStatsStorage::UnlockFromSerialization()
{
	m_serializeLocked=false;
}

bool CCircularBufferStatsStorage::IsLockedForSerialization()
{
	return m_serializeLocked;
}

#if DEBUG_CIRCULAR_STATS
void CCircularBufferStatsStorage::DebugUpdate()
{
	if (g_pGameCVars->g_telemetry_memory_display)
	{
		if (IsUsingCircularBuffer())
		{
			CryWatch("[MT] Stats circular buffer, using %d / %d bytes, # discards %d, # legacy events %d, # new events %d",m_circularBuffer.size(),m_circularBuffer.capacity(),m_numDiscards,m_numLegacyEvents,m_numCircularEvents);
			CryWatch("[MT] Stats circular buffer, total alloc requests %d bytes, peak %d bytes",m_totalBytesRequested,m_peakAlloc);
		}
		else
		{
			CryWatch("[MT] Stats circular buffer set to UNLIMITED, current alloc %d, peak %d",m_totalBytesRequested,m_peakAlloc);
		}
		if (IsLockedForSerialization())
		{
			CryWatch("[MT] Stats circular buffer is locked for serialization");
		}
	}
}
#endif

// static
CCircularBufferStatsStorage	*CCircularBufferStatsStorage::GetDefaultStorage()
{
	CRY_ASSERT_MESSAGE(s_storage!=NULL,"Tried to access default circular buffer storage, but none exists");
	return s_storage;
}

// static
CCircularBufferStatsStorage	*CCircularBufferStatsStorage::GetDefaultStorageNoCheck()
{
	return s_storage;
}

IStatsContainer *CCircularBufferStatsStorage::CreateContainer()
{
	return new CCircularBufferStatsContainer(this);
}

bool CCircularBufferStatsStorage::ContainsPtr(
	const void		*inPtr)
{
	return m_circularBuffer.ContainsPtr(inPtr);
}

// allocates storage of the specified size
void *CCircularBufferStatsStorage::Alloc(int inSize, uint8 inType)
{
	CRY_ASSERT_MESSAGE(!m_serializeLocked,"Shouldn't be trying to allocate currently, circular storage is locked for serialisation (may corrupt data)");

	CRY_ASSERT_MESSAGE(this==GetDefaultStorage(),"CCircularBufferStatsStorage Alloc and Free need to know which circular buffer allocations belong to if allowing more than one circular buffer");

	void		*result=NULL;
	int			headerSize=sizeof(SRecording_Packet);
	int			totalSize=inSize+headerSize;
	SRecording_Packet	*header;

	if (IsUsingCircularBuffer())
	{
		size_t freeSpace = m_circularBuffer.capacity() - m_circularBuffer.size();		// WARNING: This only works because we never wrap round (i.e. using it as a straight array)
		if (freeSpace >= (size_t)totalSize)
		{
			// Potential optimisation here, we could replace this with a much simpler allocator
			header=m_circularBuffer.AllocEmptyPacket(totalSize,inType);		// PTR ARITH
		}
		else
		{
			// Ran out of space
			m_numDiscards++;
			m_totalBytesRequested+=totalSize;
			return NULL;
		}
	}
	else
	{
		header=(SRecording_Packet*)(new char[totalSize]);
		header->size=totalSize;
		header->type=inType;
	}

	result=((char*)header)+headerSize;

	m_totalBytesAlloced+=totalSize;
	m_peakAlloc=max(m_peakAlloc,m_totalBytesAlloced);
	m_totalBytesRequested+=totalSize;

	return result;
}

// static
// frees storage
void CCircularBufferStatsStorage::Free(void *inPtr)
{
	CCircularBufferStatsStorage		*storage=GetDefaultStorage();

	if (storage)
	{
		CRY_ASSERT_MESSAGE(!storage->m_serializeLocked,"Shouldn't be trying to free currently, circular storage is locked for serialisation (may corrupt data)");

		int						headerSize=sizeof(SRecording_Packet);
		SRecording_Packet		*header=(SRecording_Packet*)(((char*)inPtr)-headerSize);								// PTR ARITH

		header->type=eRBPT_Free;
		storage->m_totalBytesAlloced-=header->size;
		CRY_ASSERT_MESSAGE(storage->m_totalBytesAlloced>=0,"CCircularBufferStatsStorage memory stats aren't adding up, total used is negative");

		if (storage->IsUsingCircularBuffer())
		{
			if (storage->m_totalBytesAlloced<=0)
			{
#ifdef _DEBUG
				// validate that all allocations are freed
				for (CRecordingBuffer::iterator iter=storage->m_circularBuffer.begin(), end=storage->m_circularBuffer.end(); iter!=end; ++iter)
				{
					const SRecording_Packet	&packet=*iter;
					CRY_ASSERT_TRACE(packet.type==eRBPT_Free,("CCircularBufferStatsStorage thinks there should be no allocations in the circular buffer, but one of type %d remains",packet.type));
				}
#endif
				storage->m_circularBuffer.Reset();
			}
		}
		else
		{
			delete [] ((char*)header);
		}
	}
}

// resets the per session counters used to put usage stats into the output
void CCircularBufferStatsStorage::ResetUsageCounters()
{
	m_numDiscards=0;
	m_totalBytesRequested=0;
}

// discard callback
void CCircularBufferStatsStorage::DiscardCallback(SRecording_Packet *ps, float recordedTime, void *inUserData)
{
	CRY_ASSERT_MESSAGE(false, "This should never happen, we no longer add data to the circular buffer if it is full");
	switch (ps->type)
	{
		case eRBPT_Free:
			// nothing to do
			break;

		case eRBPT_TimelineEntry:
			{
				// memory layout is
				//		SRecording_Packet
				//		class storage - vtable if present will be part of the class storage
				int									headerSize=sizeof(SRecording_Packet);
				CCircularBufferTimelineEntry		*entry=(CCircularBufferTimelineEntry*)(((char*)ps)+headerSize);		// PTR ARITH

				entry->ForceRelease();

				CCircularBufferStatsStorage		*storage=static_cast<CCircularBufferStatsStorage*>(inUserData);
				storage->m_numDiscards++;
			}
			break;

		case eRBPT_StatAnyValue:
			{
				int									headerSize=sizeof(SRecording_Packet);
				SCircularStatAnyValue				*entry=(SCircularStatAnyValue*)(((char*)ps)+headerSize);		// PTR ARITH

				delete entry;

				CCircularBufferStatsStorage			*storage=static_cast<CCircularBufferStatsStorage*>(inUserData);
				storage->m_numDiscards++;
			}
			break;

		default:
			CRY_ASSERT_MESSAGE(0,"Unknown packet type in CCircularBufferStatsStorage discard callback");
			break;
	}
}

// static
CCircularBufferTimelineEntry *CCircularBufferTimelineEntry::EntryFromListElement(
	const CDoubleLinkedElement		*inElement)
{
	size_t							linkOffset=offsetof(CCircularBufferTimelineEntry,m_timelineLink);

	CCircularBufferTimelineEntry	*result=(CCircularBufferTimelineEntry*)(((char*)inElement)-linkOffset);			// PTR ARITH

	assert((&result->m_timelineLink)==inElement);

	return result;
}

CCircularBufferTimelineEntry::CCircularBufferTimelineEntry()
{
}

CCircularBufferTimelineEntry::CCircularBufferTimelineEntry(
	const CCircularBufferTimelineEntry		&inCopyMe)
{
	*this=inCopyMe;
}

CCircularBufferTimelineEntry::~CCircularBufferTimelineEntry()
{
	assert(GetRefCount()==0);
}

// static
SCircularStatAnyValue *SCircularStatAnyValue::EntryFromListElement(
	const CDoubleLinkedElement		*inElement)
{
	size_t							linkOffset=offsetof(SCircularStatAnyValue,link);

	SCircularStatAnyValue			*result=(SCircularStatAnyValue*)(((char*)inElement)-linkOffset);			// PTR ARITH

	assert((&result->link)==inElement);

	return result;
}

void CCircularBufferTimelineEntry::ForceRelease()
{
	int			expectedRefcount=(m_timelineLink.IsInList()) ? 1 : 0;

	CRY_ASSERT_MESSAGE(GetRefCount()==expectedRefcount,"Circular buffer stats entry is being purged due to lack of space, but some one some where is still holding a ref to it - this is very likely to crash! Code is not permitted to hold references to these objects");

	if (expectedRefcount==1)
	{
		Release();
	}
	else
	{
		delete this;
	}
}

CCircularXMLSerializer::CCircularXMLSerializer(
	CCircularBufferStatsStorage			*pInStorage) :
	m_pStorage(pInStorage),
	m_state(k_notStartedProducing),
	m_containerIterator(-1),
	m_eventIterator(NULL),
	m_timelineIterator(-1),
	m_stateIterator(-1),
	m_indentLevel(0)
{
	m_pStorage->LockForSerialization();
	m_entries.reserve(256);

	IGameStatistics		*pStat=g_pGame->GetIGameFramework()->GetIGameStatistics();
	if (pStat)
	{
		pStat->RegisterSerializer(this);
	}

	memset(m_indentStr,' ',sizeof(m_indentStr));
}

CCircularXMLSerializer::~CCircularXMLSerializer()
{
	m_pStorage->UnlockFromSerialization();

	IGameStatistics		*pStat=g_pGame->GetIGameFramework()->GetIGameStatistics();
	if (pStat)
	{
		pStat->UnregisterSerializer(this);
	}

	m_entries.clear();			// important, clear all references to serialize entries (which are stored in the storage buffer) before we release our reference to the storage buffer
	m_pStorage=NULL;
}

void CCircularXMLSerializer::IncreaseIndentation(
	int					inDelta)
{
	m_indentLevel+=inDelta;
	m_indentLevel=clamp_tpl<int>(m_indentLevel,0,CRY_ARRAY_COUNT(m_indentStr));
}

// outputs some text into the telemetry buffer with the current indentation level
// returns true if successful, false if it didn't fit
bool CCircularXMLSerializer::Output(
	SWriteState	*pIOState,
	const char	*pInDataToWrite,
	int					inDataLenToWrite,
	bool				inDoIndent)
{
	bool				result=true;
	int					dataWritten=pIOState->dataWritten;
	int					indentLevel=inDoIndent ? m_indentLevel : 0;
	char				*pBuffer=pIOState->pBuffer;

	if ((inDataLenToWrite+indentLevel)<=(pIOState->bufferSize-dataWritten))
	{
		if (indentLevel>0)
		{
			memcpy(pBuffer+dataWritten,m_indentStr,indentLevel);
			dataWritten+=indentLevel;
		}
		memcpy(pBuffer+dataWritten,pInDataToWrite,inDataLenToWrite);
		dataWritten+=inDataLenToWrite;
	}
	else
	{
		// buffer full, return what we've got
		pIOState->full=true;
		result=false;
	}

	pIOState->dataWritten=dataWritten;

	return result;
}

#define OUTPUT(str,len)	Output(pIOState,str,len,true)
#define OUTPUT_NO_INDENT(str,len)	Output(pIOState,str,len,false)

void CCircularXMLSerializer::SerializeContainerTag(
	SWriteState															*pIOState,
	CCircularBufferStatsContainer						*pInCont,
	IGameStatistics													*pInStats)
{
	if (m_stateIterator==-1 && !pIOState->full)
	{
		const size_t						numStates=pInStats->GetStateCount();
		CryFixedStringT<512>		tag;
		const SSerializeEntry		*entry=&m_entries[m_containerIterator];

		switch (entry->action)
		{
			case k_openTag:
				tag.Format("<%s",entry->tagName.c_str());
				for (int i=0; i<int(numStates); i++)
				{
					const SStatAnyValue				*pState=pInCont->GetState(i);
					if (pState != NULL && pState->IsValidType() && pState->type!=eSAT_TXML)			// states which are not xml sub nodes get encoded as attributes in the container's start element tag
					{
						stack_string strValue;
						if(pState->ToString(strValue))
						{
							CryFixedStringT<128>	str2;
							str2.Format(" %s=\"%s\"",pInStats->GetStateDesc(i)->serializeName.c_str(),strValue.c_str());
							tag+=(const char*)str2;
						}
					}
				}
				tag+=">\n";

				if (OUTPUT(tag.c_str(),tag.length()))
				{
					IncreaseIndentation(1);
				}
				break;

			case k_closeTag:
				IncreaseIndentation(-1);
				tag.Format("</%s>\n",entry->tagName.c_str());
				if (!OUTPUT(tag.c_str(),tag.length()))
				{
					IncreaseIndentation(1);
				}
				break;

			default:
				assert(0);
				break;
		}

		if (!pIOState->full)
		{
			++m_stateIterator;
		}
	}
}

void CCircularXMLSerializer::SerializeTimeline(
	SWriteState															*pIOState,
	CCircularBufferStatsContainer						*pInCont,
	IGameStatistics													*pInStats)
{
	const CCircularBufferTimeline	*pTimeline=pInCont->GetTimeline(m_timelineIterator);

	if (m_eventIterator==NULL)
	{
		if (!pTimeline->m_list.empty())
		{
			CryFixedStringT<80>		tlStart;
			tlStart.Format("<timeline name=\"%s\">\n",pInStats->GetEventDesc(m_timelineIterator)->serializeName.c_str());

			if (OUTPUT(tlStart.c_str(),tlStart.length()))
			{
				IncreaseIndentation(1);
			}
		}

		if (!pIOState->full)
		{
			m_eventIterator=pTimeline->m_list.begin();
		}
	}

	CDoubleLinkedList::const_iterator		end=pTimeline->m_list.end();
	uint32 bufferSize=64*1024;
	char* buffer=(char*)malloc(bufferSize);

	while (m_eventIterator!=end && !pIOState->full)
	{
		// currently individual events are still serialised through an xml node,
		// but the entire tree is no longer built up
		// if usage of these xml nodes presents a problem, we need to replace the GetXML() method
		// with a GetXMLString() method instead, which can return the xml text without creating
		// nodes
		// however, as the lifespan of the xml nodes is so short, i don't anticipate these being
		// necessary - they're practically stack objects
		XmlNodeRef										xml;

		switch (pTimeline->GetType())
		{
			case eRBPT_TimelineEntry:
				{
					CCircularBufferTimelineEntry	*pTimelineEntry=CCircularBufferTimelineEntry::EntryFromListElement(*m_eventIterator);
					xml=pInStats->CreateStatXMLNode("val");
					xml->setAttr("time",pTimelineEntry->GetTime().GetMilliSecondsAsInt64());
					XmlNodeRef		child=pTimelineEntry->GetXML(pInStats);
					child->setTag("param");
					xml->addChild(child);
				}
				break;

			case eRBPT_StatAnyValue:
				{
					SCircularStatAnyValue					*pVal=SCircularStatAnyValue::EntryFromListElement(*m_eventIterator);
					xml=pInStats->CreateStatXMLNode("val");
					xml->setAttr("time",pVal->time.GetMilliSecondsAsInt64());
					CRY_ASSERT_MESSAGE(pVal->val.type!=eSAT_TXML,"adding SStatAnyValue() with xmlnodes is no longer supported, add a data type derived from IXMLSerializable instead");
					stack_string strValue;
					if(pVal->val.ToString(strValue))
					{
						xml->setAttr("prm",strValue);
					}
				}
				break;

			default:
				CRY_ASSERT_MESSAGE(0,"unknown data type found in timeline, cannot save data into telemetry");
				break;
		}

		if (xml)
		{
			XmlString											str=xml->getXMLUnsafe(m_indentLevel, buffer, bufferSize);			// use the xml indentation to handle multi line xml nodes correctly

			OUTPUT_NO_INDENT(str.c_str(),str.length());
		}

		if (!pIOState->full)
		{
			++m_eventIterator;
		}
	}

	free(buffer);

	if (m_eventIterator==end && !pTimeline->m_list.empty() && !pIOState->full)
	{
		IncreaseIndentation(-1);
		if (!OUTPUT("</timeline>\n",12))
		{
			IncreaseIndentation(1);
		}
	}
}

void CCircularXMLSerializer::SerializeTimelines(
	SWriteState															*pIOState,
	CCircularBufferStatsContainer						*pInCont,
	IGameStatistics													*pInStats)
{
	const size_t numEventTimelines=pInStats->GetEventCount();

	while (m_timelineIterator<=int(numEventTimelines) && !pIOState->full)
	{
		if (m_timelineIterator==-1)
		{
			if (pInCont->HasAnyTimelineEvents())
			{
				if (OUTPUT("<timelines>\n",12))
				{
					IncreaseIndentation(1);
				}
			}
		}
		else if (m_timelineIterator==int(numEventTimelines))
		{
			if (pInCont->HasAnyTimelineEvents())
			{
				IncreaseIndentation(-1);
				if (!OUTPUT("</timelines>\n",13))
				{
					IncreaseIndentation(1);
				}
			}
		}
		else
		{
			SerializeTimeline(pIOState,pInCont,pInStats);
		}

		if (!pIOState->full)
		{
			++m_timelineIterator;
			m_eventIterator=NULL;
		}
	}
}

void CCircularXMLSerializer::SerializeStates(
	SWriteState															*pIOState,
	CCircularBufferStatsContainer						*pInCont,
	IGameStatistics													*pInStats)
{
	const size_t numStates=pInStats->GetStateCount();

	while (m_stateIterator<int(numStates) && !pIOState->full)
	{
		const SStatAnyValue						*pState=pInCont->GetState(m_stateIterator);

		if (pState != NULL && pState->IsValidType() && pState->type==eSAT_TXML && pState->pSerializable)
		{
			XmlNodeRef xml=pState->pSerializable->GetXML(pInStats);
			xml->setTag(pInStats->GetStateDesc(m_stateIterator)->serializeName.c_str());
			if (xml)
			{
				XmlString									str=xml->getXML(m_indentLevel);
				OUTPUT_NO_INDENT(str.c_str(),str.length());
			}
		}

		if (!pIOState->full)
		{
			++m_stateIterator;
		}
	}
}

ITelemetryProducer::EResult CCircularXMLSerializer::ProduceTelemetry(
	char				*pOutBuffer,
	int					inMinRequired,
	int					inBufferSize,
	int					*pOutWritten)
{
	EResult						result=eTS_EndOfStream;
	IGameStatistics		*pStat=(g_pGame && g_pGame->GetIGameFramework()) ? g_pGame->GetIGameFramework()->GetIGameStatistics() : NULL;
	SWriteState				state;

	state.pBuffer=pOutBuffer;
	state.bufferSize=inBufferSize;
	state.dataWritten=0;
	state.full=false;

	if (pStat)
	{
		if (m_state==k_notStartedProducing)
		{
			m_state=k_producing;
			m_containerIterator=0;
			m_stateIterator=-1;
			m_timelineIterator=-1;
			m_eventIterator=NULL;
		}

		int												containerSize=m_entries.size();

		while (m_containerIterator!=containerSize && !state.full)
		{
			const SSerializeEntry						*entry=&m_entries[m_containerIterator];
			CCircularBufferStatsContainer		*pCont=static_cast<CCircularBufferStatsContainer*>(entry->pContainer.get());

			SerializeContainerTag(&state,pCont,pStat);

			if (entry->action==k_openTag)
			{
				SerializeTimelines(&state,pCont,pStat);

				SerializeStates(&state,pCont,pStat);
			}

			if (!state.full)
			{
				++m_containerIterator;
				m_stateIterator=-1;
				m_timelineIterator=-1;
			}
		}

		if (m_state==k_producing && m_containerIterator!=containerSize)
		{
			assert(state.dataWritten>0);
			result=eTS_Available;
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME,VALIDATOR_ERROR,"Failed to serialise data - IGameStatistics interface removed during serialisation");		// possibly could happen during game tear down
	}

	*pOutWritten=state.dataWritten;

	return result;
}

#undef OUTPUT
#undef OUTPUT_NO_INDENT

void CCircularXMLSerializer::VisitNode(const SNodeLocator& locator, const char* serializeName, IStatsContainer& container, EStatNodeState state)
{
	switch (state)
	{
		case eSNS_Alive:
			CryWarning(VALIDATOR_MODULE_GAME,VALIDATOR_ERROR,"Live stats node encountered during stats serialization, ignoring %s\n",serializeName);
			break;

		case eSNS_Dead:
			{
				m_entries.resize(m_entries.size()+1);
				SSerializeEntry				&entry=m_entries.back();
				entry.action=k_openTag;
				entry.tagName=serializeName;
				entry.pContainer=&container;
				assert(m_state==k_notStartedProducing);		// don't mess with array once iteration has started!
			}
			break;

		default:
			assert(0);
			break;
	}
}

void CCircularXMLSerializer::LeaveNode(const SNodeLocator& locator, const char* serializeName, IStatsContainer& container, EStatNodeState state)
{
	switch (state)
	{
		case eSNS_Alive:
			CryWarning(VALIDATOR_MODULE_GAME,VALIDATOR_ERROR,"Live stats node encountered during stats serialization, ignoring %s\n",serializeName);
			break;

		case eSNS_Dead:
			{
				m_entries.resize(m_entries.size()+1);
				SSerializeEntry				&entry=m_entries.back();
				entry.action=k_closeTag;
				entry.tagName=serializeName;
				entry.pContainer=&container;
				assert(m_state==k_notStartedProducing);		// don't mess with array once iteration has started!
			}
			break;

		default:
			assert(0);
			break;
	}
}














