// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Containers/VectorMap.h>
#include <CryString/NameCRCHelper.h>
#include <CryCore/Containers/CryListenerSet.h>

enum
{
	CAF_File,
	AIM_File,
	LMG_File,
};

//////////////////////////////////////////////////////////////////////////
// Custom hash map class.
//////////////////////////////////////////////////////////////////////////
struct CHashMap_AmimNameCRC
{
	typedef std::map<uint32, size_t> NameHashMap;

	//----------------------------------------------------------------------------------
	// Returns the index of the animation from crc value
	//----------------------------------------------------------------------------------
	size_t GetValueCRC(uint32 crc) const
	{
		NameHashMap::const_iterator it = m_HashMap.find(crc);
		if (it == m_HashMap.end())
			return -1;
		return it->second;
	}

	// Returns the index of the animation from name. Name converted in lower case in this function
	size_t GetValue(const char* name) const
	{
		uint32 crc32 = CCrc32::ComputeLowercase(name);
		return GetValueCRC(crc32);
	}

	bool InsertValue(uint32 crc32, size_t num)
	{
		bool res = m_HashMap.find(crc32) == m_HashMap.end();
		m_HashMap[crc32] = num;
		return res;
	}

	size_t GetAllocMemSize() const
	{
		return m_HashMap.size() * (sizeof(uint32) + sizeof(size_t));
	}

	size_t GetMapSize() const
	{
		return m_HashMap.size();
	}

	void GetMemoryUsage(class ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_HashMap);
	}

	void Clear()
	{
		m_HashMap.clear();
	}
protected:
	NameHashMap m_HashMap;
};

//! this structure contains info about loaded animations
struct ModelAnimationHeader
{
#ifdef STORE_ANIMATION_NAMES
	string m_Name;    // the name of the animation (not the name of the file) - unique per-model
#endif
	uint32 m_CRC32Name;     //hash value for searching animations
	int16  m_nGlobalAnimId;
	int16  m_nAssetType;

	size_t SizeOfAnimationHeader() const
	{
		size_t size = 0;//sizeof(ModelAnimationHeader);
#ifdef STORE_ANIMATION_NAMES
		size += m_Name.length();
#endif
		return size;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
#ifdef STORE_ANIMATION_NAMES
		pSizer->AddObject(m_Name);
#endif
	}

	ModelAnimationHeader() : m_nGlobalAnimId(-1), m_nAssetType(CAF_File){};
	~ModelAnimationHeader() {}

	// the name of the animation when STORE_ANIMATION_NAMES; otherwise a shortened filepath
	// (should only be used for debugging)
	const char* GetAnimName() const
	{
#ifdef STORE_ANIMATION_NAMES
		return m_Name.c_str();
#else
		const char* filePath = GetFilePath();
		if (filePath)
		{
			const char* lastSeparator = strrchr(filePath, '/');
			const char* fileName = lastSeparator ? lastSeparator + 1 : filePath;
			return fileName;
		}
		else
		{
			extern const char* strEmpty;
			return strEmpty;
		}
#endif
	};

	const char* GetFilePath() const;

	void        SetAnimName(const char* name)
	{
#ifdef STORE_ANIMATION_NAMES
		m_Name = name;
#endif
		m_CRC32Name = CCrc32::ComputeLowercase(name);
	}

};

//////////////////////////////////////////////////////////////////////////
// Implementation of IAnimationSet, holding the information about animations
// and bones for a single model. Animations also include the subclass of morph targets
//////////////////////////////////////////////////////////////////////////
class CAnimationSet final : public IAnimationSet
{
public:
	CAnimationSet(const char* pSkeletonFilePath);

	////////////////////////////////////////////////////////////////////////////
	// IAnimationSet implementation
	virtual ~CAnimationSet() override;

	virtual void AddRef() override
	{
		++m_nRefCounter;
	}
	virtual void Release() override
	{
		--m_nRefCounter;
		if (m_nRefCounter == 0)
			delete this;
	}

	virtual uint32 GetAnimationCount() const override
	{
		return m_arrAnimations.size();
	}

	virtual int           GetAnimIDByName(const char* szAnimationName) const override;
	virtual const char*   GetNameByAnimID(int nAnimationId) const override;
	virtual int           GetAnimIDByCRC(uint32 animationCRC) const override;
	virtual uint32        GetCRCByAnimID(int nAnimationId) const override;
	virtual uint32        GetFilePathCRCByAnimID(int nAnimationId) const override;
	virtual const char*   GetFilePathByName(const char* szAnimationName) const override;
	virtual const char*   GetFilePathByID(int nAnimationId) const override;
	virtual f32           GetDuration_sec(int nAnimationId) const override;
	virtual uint32        GetAnimationFlags(int nAnimationId) const override;
	virtual uint32        GetAnimationSize(const uint32 nAnimationId) const override;
	virtual bool          IsAnimLoaded(int nAnimationId) const override;
	virtual bool          IsAimPose(int nAnimationId, const IDefaultSkeleton& defaultSkeleton) const override;
	virtual bool          IsLookPose(int nAnimationId, const IDefaultSkeleton& defaultSkeleton) const override;
	virtual void          AddRef(const int32 nAnimationId) const override;
	virtual void          Release(const int32 nAnimationId) const override;
	virtual bool          GetAnimationDCCWorldSpaceLocation(const char* szAnimationName, QuatT& startLocation) const override;
	virtual bool          GetAnimationDCCWorldSpaceLocation(int32 AnimID, QuatT& startLocation) const override;
	virtual bool          GetAnimationDCCWorldSpaceLocation(const CAnimation* pAnim, QuatT& startLocation, uint32 nControllerID) const override;
	virtual ESampleResult SampleAnimation(int32 animationId, float animationNormalizedTime, uint32 controllerId, QuatT& relativeLocationOutput) const override;

#ifdef EDITOR_PCDEBUGCODE
	virtual void        GetSubAnimations(DynArray<int>& animIdsOut, int animId) const override;
	virtual int         GetNumFacialAnimations() const override;
	virtual const char* GetFacialAnimationPathByName(const char* szName) const override;
	virtual const char* GetFacialAnimationName(int index) const override;
	virtual int32       GetGlobalIDByName(const char* szAnimationName) const override;
	virtual int32       GetGlobalIDByAnimID(int nAnimationId) const override;
	virtual const char* GetAnimationStatus(int nAnimationId) const override;
	virtual uint32      GetTotalPosKeys(const uint32 nAnimationId) const override;
	virtual uint32      GetTotalRotKeys(const uint32 nAnimationId) const override;
	virtual const char* GetDBAFilePath(const uint32 nAnimationId) const override;
	virtual int         AddAnimationByPath(const char* animationName, const char* animationPath, const IDefaultSkeleton* pIDefaultSkeleton) override;
	virtual void        RebuildAimHeader(const char* szAnimationName, const IDefaultSkeleton* pIDefaultSkeleton) override;
	virtual void        RegisterListener(IAnimationSetListener* pListener) override;
	virtual void        UnregisterListener(IAnimationSetListener* pListener) override;
	virtual bool        GetMotionParameters(uint32 nAnimationId, int32 parameterIndex, IDefaultSkeleton* pDefaultSkeleton, Vec4& outParameters) const override;
	virtual bool        GetMotionParameterRange(uint32 nAnimationId, EMotionParamID paramId, float& outMin, float& outMax) const override;
	virtual bool        IsBlendSpace(int animId) const override;
	virtual bool        IsCombinedBlendSpace(int animId) const override;
#endif
	////////////////////////////////////////////////////////////////////////////

	// gets called when the given animation (by global id) is unloaded.
	// the animation controls need to be unbound to free up the memory
	void ReleaseAnimationData();
	// prepares to load the specified number of CAFs by reserving the space for the controller pointers
	void prepareLoadCAFs(unsigned nReserveAnimations);
	void prepareLoadANMs(unsigned nReserveAnimations);

	// when the animinfo is given, it's used to set the values of the global animation as if the animation has already been loaded once -
	// so that the next time the anim info is available and there's no need to load the animation synchronously
	// Returns the global anim id of the file, or -1 if error
	int  LoadFileCAF(const char* szFileName, const char* szAnimName);
	int  LoadFileAIM(const char* szFileName, const char* szAnimName, const IDefaultSkeleton* pIDefaultSkeleton);
	int  LoadFileANM(const char* szFileName, const char* szAnimName, DynArray<CControllerTCB>& m_LoadCurrAnimation, CryCGALoader* pCGA, const IDefaultSkeleton* pIDefaultSkeleton);
	int  LoadFileLMG(const char* szFileName, const char* szAnimName);
	// Reuse an animation that is already loaded in the global animation set for this model
	void PrepareToReuseAnimations(size_t amount);
	void ReuseAnimation(const ModelAnimationHeader& header);

	// tries to load the animation info if isn't present in memory; returns NULL if can't load
	const ModelAnimationHeader* GetModelAnimationHeader(int32 i) const;

	int32                       FindAimposeByGlobalID(uint32 nGlobalIDAimPose)
	{
		uint32 numAnims = GetAnimationCount();
		for (uint32 nMID = 0; nMID < numAnims; nMID++)
		{
			if (m_arrAnimations[nMID].m_nAssetType != AIM_File)
				continue;
			if (m_arrAnimations[nMID].m_nGlobalAnimId == nGlobalIDAimPose)
				return 1;
		}
		return -1;
	}

	size_t SizeOfAnimationSet() const;
	void   GetMemoryUsage(ICrySizer* pSizer) const;

	struct FacialAnimationEntry
	{
		FacialAnimationEntry(const string& name, const string& path) : name(name), path(path) {}
		string name;
		string path;
		uint32 crc;
		operator const char*() const { return name.c_str(); }

		void GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(name);
			pSizer->AddObject(path);
		}
	};
	typedef VectorSet<FacialAnimationEntry, stl::less_stricmp<FacialAnimationEntry>> FacialAnimationSet;
	FacialAnimationSet& GetFacialAnimations() { return m_facialAnimations; }
#ifndef EDITOR_PCDEBUGCODE
	int                 GetNumFacialAnimations() const;
	const char*         GetFacialAnimationPathByName(const char* szName) const;
	const char*         GetFacialAnimationName(int index) const;
#endif
	const char*         GetFacialAnimationPath(int index) const;

	//---------------------------------------------------------------------
	//---------------------------------------------------------------------
	//---------------------------------------------------------------------

	bool IsVEGLoaded(int nGlobalVEGId) const;

#ifndef EDITOR_PCDEBUGCODE
	const char* GetAnimationStatus(int nAnimationId) const;
#endif

	// for internal use only

	GlobalAnimationHeaderCAF* GetGAH_CAF(int nAnimationId) const;
	GlobalAnimationHeaderCAF* GetGAH_CAF(const char* AnimationName) const;
	GlobalAnimationHeaderAIM* GetGAH_AIM(int nAnimationId) const;
	GlobalAnimationHeaderAIM* GetGAH_AIM(const char* AnimationName) const;
	GlobalAnimationHeaderLMG* GetGAH_LMG(int nAnimationId) const;
	GlobalAnimationHeaderLMG* GetGAH_LMG(const char* AnimationName) const;

#ifndef EDITOR_PCDEBUGCODE
	int32        GetGlobalIDByAnimID(int nAnimationId) const;
	int32        GetGlobalIDByName(const char* szAnimationName) const;
#endif
	ILINE uint32 GetGlobalIDByAnimID_Fast(int nAnimationId) const
	{
		assert(nAnimationId >= 0);
		assert(nAnimationId < m_arrAnimations.size());
		return m_arrAnimations[nAnimationId].m_nGlobalAnimId;
	};

	// Not safe method. Just direct access to m_arrAnimations
	const ModelAnimationHeader& GetModelAnimationHeaderRef(int i)
	{
		return m_arrAnimations[i];
	}

	const char* GetSkeletonFilePathDebug() const { return m_strSkeletonFilePath.c_str(); }

	void        VerifyLMGs();

#ifdef EDITOR_PCDEBUGCODE
	void NotifyListenersAboutReloadStarted();
	void NotifyListenersAboutReload();
#endif
private:

#ifdef STORE_ANIMATION_NAMES
	void VerifyAliases_Debug();
#endif

	void StoreAnimName(uint32 nameCRC, const char* name)
	{
		if (Console::GetInst().ca_StoreAnimNamesOnLoad)
			m_hashToNameMap[nameCRC] = name;
	}

private:
	void InsertHash(uint32 crc, size_t id)
	{
		DynArray<uint32>::iterator it = std::lower_bound(m_AnimationHashMapKey.begin(), m_AnimationHashMapKey.end(), crc);
		ptrdiff_t index = it - m_AnimationHashMapKey.begin();
		m_AnimationHashMapKey.insert(it, crc);

		DynArray<uint16>::iterator itValue = m_AnimationHashMapValue.begin();
		m_AnimationHashMapValue.insert(itValue + index, static_cast<uint16>(id));
	}

	size_t GetValueCRC(uint32 crc) const
	{
		DynArray<uint32>::const_iterator itBegin = m_AnimationHashMapKey.begin();
		DynArray<uint32>::const_iterator itEnd = m_AnimationHashMapKey.end();
		DynArray<uint32>::const_iterator it = std::lower_bound(itBegin, itEnd, crc);
		if (it != itEnd && *it == crc)
		{
			ptrdiff_t index = it - itBegin;
			return m_AnimationHashMapValue[static_cast<int>(index)];
		}
		return -1;
	}

	// Returns the index of the animation from name. Name converted in lower case in this function
	size_t GetValue(const char* name) const
	{
		uint32 crc32 = CCrc32::ComputeLowercase(name);
		return GetValueCRC(crc32);
	}

private:
	// No more direct access to vector. No more LocalIDs!!!! This is just vector of registered animations!!!
	// When animation started we need build remap controllers\bones
	DynArray<ModelAnimationHeader> m_arrAnimations;
	FacialAnimationSet             m_facialAnimations;

	DynArray<uint32>               m_AnimationHashMapKey;   //< Array of crc32 animation names (i.e. ModelAnimationHeader::m_CRC32Name), sorted by hash values.
	DynArray<uint16>               m_AnimationHashMapValue; //< Mapping from indices of m_AnimationHashMapKey to indices of m_arrAnimations.

	typedef std::map<uint32, string> HashToNameMap;
	HashToNameMap                        m_hashToNameMap; // Simple optional array of names that maps to m_arrAnimations. Filled if ca_StoreAnimNamesOnLoad == 1.
	int                                  m_nRefCounter;
	//Just for debugging...Just for debugging...Just for debugging
	const string                         m_strSkeletonFilePath; //This was the original skeleton the animation set was created for. But the set might also work on an extended version of that skeleton
	CListenerSet<IAnimationSetListener*> m_listeners;
};
