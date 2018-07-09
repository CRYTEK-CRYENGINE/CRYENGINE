// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:CrySizerStats.h
//  Declaration of the CrySizerStats class, which is used to
//  calculate the memory usage by the subsystems and components, to help
//  the artists keep the memory budged low.
//
//
//	History:
//
//////////////////////////////////////////////////////////////////////

#ifndef _CRY_SYSTEM_CRY_SIZER_STATS_HDR_
#define _CRY_SYSTEM_CRY_SIZER_STATS_HDR_

class CrySizerImpl;

//////////////////////////////////////////////////////////////////////////
// This class holds only the necessary statistics data, which can be carried
// over a few frames without significant impact on memory usage
// CrySizerImpl is an implementation of ICrySizer, which is used to collect
// those data; it must be destructed immediately after constructing the Stats
// object to avoid excessive memory usage.
class CrySizerStats
{
public:
	// constructs the statistics based on the given cry sizer
	CrySizerStats(CrySizerImpl* pCrySizer);

	CrySizerStats();

	// this structure describes one component of the memory size statistics
	struct Component
	{
		Component() { clear(); }
		Component(const string& name, unsigned size = 0, unsigned num = 0) :
			strName(name), sizeBytes(size), sizeBytesTotal(0), numObjects(num), nDepth(0) {}
		void clear()
		{
			strName = "";
			sizeBytes = 0;
			sizeBytesTotal = 0;
			numObjects = 0;
			nDepth = 0;
		}

		// the name of the component, as it appeared in the push() call
		string   strName;
		// the total size, in bytes, of objects in the component
		size_t   sizeBytes;
		// the total size including the subcomponents
		size_t   sizeBytesTotal;
		// the number of objects allocated
		size_t   numObjects;
		unsigned nDepth;

		float getSizeKBytes() const      { return sizeBytes / float(1 << 10); }

		float getTotalSizeKBytes() const { return sizeBytesTotal / float(1 << 10); }

		float getSizeMBytes() const      { return sizeBytes / float(1 << 20); }

		float getTotalSizeMBytes() const { return sizeBytesTotal / float(1 << 20); }

		struct NameOrder
		{
			bool operator()(const Component& left, const Component& right) const { return left.strName < right.strName; }
		};

		struct SizeOrder
		{
			bool operator()(const Component& left, const Component& right) const { return left.sizeBytes < right.sizeBytes; }
		};

		struct GenericOrder
		{
			bool operator()(const Component& left, const Component& right) const;
		};
	};

	// returns the number of different subsystems/components used
	unsigned         numComponents() const                   { return (unsigned)m_arrComponents.size(); }
	// returns the name of the i-th component
	const Component& getComponent(unsigned nComponent) const { return m_arrComponents[nComponent]; }

	unsigned         size() const                            { return numComponents(); }
	const Component& operator[](unsigned i) const            { return getComponent(i); }
	const Component& operator[](signed i) const              { return getComponent(i); }

	unsigned         row() const                             { return m_nStartRow; }
	void             updateKeys();

	size_t           getMaxNameLength() const { return m_nMaxNameLength; }
	enum {g_numTimers = 3};

	void  startTimer(unsigned nTimer, ITimer* pTimer);
	void  stopTimer(unsigned nTimer, ITimer* pTimer);
	float getTime(unsigned nTimer) const { assert(nTimer < g_numTimers); return m_fTime[nTimer]; }
	int   getAgeFrames() const           { return m_nAgeFrames; }
	void  incAgeFrames()                 { ++m_nAgeFrames; }
protected:
	// refreshes the statistics built after the component array is built
	void refresh();
protected:
	// the names of the components
	typedef std::vector<Component> ComponentArray;
	ComponentArray m_arrComponents;

	// the maximum length of the component name, in characters
	size_t m_nMaxNameLength;

	// the timer that counts the time spent on statistics gathering
	float m_fTime[g_numTimers];

	// the age of the statistics, in frames
	int m_nAgeFrames;

	//current row offset inc/dec by cursor keys
	unsigned m_nStartRow;

	friend class CrySizerStatsBuilder;
};

//////////////////////////////////////////////////////////////////////////
// this is the constructor for the CrySizerStats
class CrySizerStatsBuilder
{
public:
	CrySizerStatsBuilder(CrySizerImpl* pSizer, int nMinSubcomponentBytes = 0);

	void build(CrySizerStats* pStats);

protected:
	typedef CrySizerStats::Component Component;
	// if there is already such name in the map, then just returns the index
	// of the compoentn in the component array; otherwise adds an entry to themap
	// and to the component array nad returns its index
	Component& mapName(unsigned nName);

	// creates the map of names from old to new, and initializes the components themselves
	void processNames();

	// given the name in the old system, adds the subtree of names to the
	// name map and components. In case all the subtree is empty, returns 0 and
	// adds nothing. Otherwise, returns the total size of objects belonging to the
	// subtree
	size_t addNameSubtree(unsigned nDepth, size_t nName);

protected:
	CrySizerStats* m_pStats;
	CrySizerImpl*  m_pSizer;

	// this is the mapping from the old names into the new componentn indices
	typedef std::vector<size_t> IdToIdMap;
	// from old to new
	IdToIdMap m_mapNames;

	// this is the threshold: if the total number of bytes in the subcomponent
	// is less than this, the subcomponent isn't shown
	unsigned m_nMinSubcomponentBytes;
};

//////////////////////////////////////////////////////////////////////////
// Renders the given usage stats; gets created upon every rendering
class CrySizerStatsRenderer
{
public:
	// constructor
	CrySizerStatsRenderer(ISystem* pSystem, CrySizerStats* pStats, unsigned nMaxDepth = 2, int nMinSubcomponentBytes = -1);
	void render(bool bRefreshMark = false);
	// dumps it to log. uses MB as default
	void dump(bool bUseKB = false);

protected: // -------------------------------------------------

	typedef CrySizerStats::Component Component;

	IRenderer*        m_pRenderer;      //
	ILog*             m_pLog;           //
	CrySizerStats*    m_pStats;         //

	ITextModeConsole* m_pTextModeConsole;

	// this is the threshold: if the total number of bytes in the subcomponent
	// is less than this, the subcomponent isn't shown
	unsigned m_nMinSubcomponentBytes;

	// the max depth of the branch to output
	unsigned m_nMaxSubcomponentDepth;
};

#endif
