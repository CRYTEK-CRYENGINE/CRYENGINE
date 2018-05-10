// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __I_CRY_MANNEQUIN_USER_PARAMS__H__
#define __I_CRY_MANNEQUIN_USER_PARAMS__H__

struct IMannequinUserParams
{
	virtual ~IMannequinUserParams() {}
	virtual void Init(const SControllerDef& controllerDef) = 0;
};

class CMannequinUserParamId
{
public:
	CMannequinUserParamId(const SControllerDef& controllerDef, const uint32 paramType)
		: m_controllerDefPathCrc(0)
		, m_paramType(paramType)
	{
		m_controllerDefPathCrc = controllerDef.m_filename.crc;
	}

	bool operator==(const CMannequinUserParamId& rhs) const
	{
		return ((m_controllerDefPathCrc == rhs.m_controllerDefPathCrc) && (m_paramType == rhs.m_paramType));
	}

	bool operator<(const CMannequinUserParamId& rhs) const
	{
		if (m_controllerDefPathCrc < rhs.m_controllerDefPathCrc)
		{
			return true;
		}
		else if (m_controllerDefPathCrc == rhs.m_controllerDefPathCrc)
		{
			return (m_paramType < rhs.m_paramType);
		}

		return false;
	}

	uint32 GetControllerDefCrc() const { return m_controllerDefPathCrc; }

private:
	uint32 m_controllerDefPathCrc;
	uint32 m_paramType;
};

class CMannequinUserParamsManager
{
public:
	CMannequinUserParamsManager()
		: m_autoIncValue(0)
	{}

	~CMannequinUserParamsManager()
	{
		Clear();
	}

	void Clear()
	{
		for (TMannequinOwnedUserParams::iterator it = m_ownedUserParams.begin(); it != m_ownedUserParams.end(); ++it)
		{
			IMannequinUserParams* pParams = *it;
			delete pParams;
		}

		m_userParams.clear();
		m_ownedUserParams.clear();
	}

	void ReloadAll(IAnimationDatabaseManager& animationDatabaseManager)
	{
		static CTagDefinition s_dummyTagDefinition;
		static SControllerDef s_dummyControllerDef(s_dummyTagDefinition, s_dummyTagDefinition, s_dummyTagDefinition);

		for (TMannequinUserParams::iterator it = m_userParams.begin(); it != m_userParams.end(); ++it)
		{
			SMannequinParamEntry& entry = *it;
			const CMannequinUserParamId& mannequinUserParamId = entry.paramId;
			const uint32 controllerDefCrc = mannequinUserParamId.GetControllerDefCrc();
			const SControllerDef* pControllerDef = animationDatabaseManager.FindControllerDef(controllerDefCrc);
			CRY_ASSERT(pControllerDef != NULL);
			pControllerDef = pControllerDef ? pControllerDef : &s_dummyControllerDef;

			IMannequinUserParams* pParams = entry.pParams;
			pParams->Init(*pControllerDef);
		}
	}

	template<typename T>
	const T* FindOrCreateParams(const IActionController* pActionController)
	{
		if (pActionController == NULL)
		{
			return NULL;
		}

		const SAnimationContext& animationContext = pActionController->GetContext();
		const SControllerDef& controllerDef = animationContext.controllerDef;

		return FindOrCreateParams<T>(controllerDef);
	}

	template<typename T>
	const T* FindOrCreateParams(const SControllerDef& controllerDef)
	{
		static uint32 s_mannequinParamType = GetAutoIncParamTypeValue();
		const CMannequinUserParamId mannequinUserParamId(controllerDef, s_mannequinParamType);

		{
			const IMannequinUserParams* pParams = FindParams(mannequinUserParamId);
			if (pParams != NULL)
			{
				return static_cast<const T*>(pParams);
			}
		}

		T* pParams = new T();

		pParams->Init(controllerDef);
		AddParams(mannequinUserParamId, pParams, eMPO_ManagerOwnership);

		return pParams;
	}

	template<typename T>
	void RegisterParams(const IActionController* pActionController, IMannequinUserParams* pParams)
	{
		if (pActionController == NULL)
		{
			return;
		}

		const SAnimationContext& animationContext = pActionController->GetContext();
		const SControllerDef& controllerDef = animationContext.controllerDef;

		RegisterParams<T>(controllerDef, pParams);
	}

	template<typename T>
	void RegisterParams(const SControllerDef& controllerDef, IMannequinUserParams* pParams)
	{
		if (pParams == NULL)
		{
			return;
		}

		static uint32 s_mannequinParamType = GetAutoIncParamTypeValue();
		const CMannequinUserParamId mannequinUserParamId(controllerDef, s_mannequinParamType);

		const IMannequinUserParams* pParamsOther = FindParams(mannequinUserParamId);
		if (pParamsOther != NULL)
		{
			CRY_ASSERT(pParams == pParamsOther);
			return;
		}

		pParams->Init(controllerDef);
		AddParams(mannequinUserParamId, pParams, eMPO_ExternalOwnership);
	}

protected:
	const IMannequinUserParams* FindParams(const CMannequinUserParamId& paramId) const
	{
		TMannequinUserParams::const_iterator cit = std::lower_bound(m_userParams.begin(), m_userParams.end(), SMannequinParamEntry(paramId));
		const bool found = (cit != m_userParams.end()) && (cit->paramId == paramId);
		return (found) ? cit->pParams : NULL;
	}

	enum EMannequinParamsOwnership
	{
		eMPO_ExternalOwnership,
		eMPO_ManagerOwnership
	};
	void AddParams(const CMannequinUserParamId& paramId, IMannequinUserParams* pParams, const EMannequinParamsOwnership ownership)
	{
		const SMannequinParamEntry entry(paramId, pParams);
		m_userParams.insert(std::lower_bound(m_userParams.begin(), m_userParams.end(), entry), entry);

		if (ownership == eMPO_ManagerOwnership)
		{
			m_ownedUserParams.push_back(pParams);
		}
	}

private:
	uint32 GetAutoIncParamTypeValue()
	{
		m_autoIncValue++;
		return m_autoIncValue;
	}

private:
	struct SMannequinParamEntry
	{
		CMannequinUserParamId paramId;
		IMannequinUserParams* pParams;

		SMannequinParamEntry(const CMannequinUserParamId& paramId_, IMannequinUserParams* pParams_ = NULL)
			: paramId(paramId_)
			, pParams(pParams_)
		{
		}

		bool operator<(const SMannequinParamEntry& rhs) const
		{
			return (paramId < rhs.paramId);
		}
	};

	typedef DynArray<SMannequinParamEntry>  TMannequinUserParams;
	typedef DynArray<IMannequinUserParams*> TMannequinOwnedUserParams;

	TMannequinUserParams      m_userParams;
	TMannequinOwnedUserParams m_ownedUserParams;
	uint32                    m_autoIncValue;
};

template<typename T, T invalidValue>
struct TMannequinAutoInit
{
public:
	TMannequinAutoInit()
		: m_value(invalidValue)
	{
	}

	const TMannequinAutoInit<T, invalidValue>& operator=(const T value)
	{
		m_value = value;
		return *this;
	}

	operator const T() const
	{
		return m_value;
	}

	bool IsValid() const
	{
		return (m_value != invalidValue);
	}

private:
	T m_value;
};

typedef TMannequinAutoInit<TagID, TAG_ID_INVALID>           TagIDAutoInit;
typedef TMannequinAutoInit<TagGroupID, GROUP_ID_NONE>       TagGroupIDAutoInit;
typedef TMannequinAutoInit<FragmentID, FRAGMENT_ID_INVALID> FragmentIDAutoInit;
typedef TMannequinAutoInit<TagID, SCOPE_ID_INVALID>         ScopeIDAutoInit;
typedef TMannequinAutoInit<TagID, SCOPE_CONTEXT_ID_INVALID> ContextIDAutoInit;

#define MANNEQUIN_USER_PARAMS__IMPL_IMPL_GET_THIRD_ARG(arg1, arg2, arg3, ...) arg3
#define MANNEQUIN_USER_PARAMS__IMPL_GET_THIRD_ARG(x)                          MANNEQUIN_USER_PARAMS__IMPL_IMPL_GET_THIRD_ARG x
#define MANNEQUIN_USER_PARAMS__GET_THIRD_ARG(...)                             MANNEQUIN_USER_PARAMS__IMPL_GET_THIRD_ARG((__VA_ARGS__))

#define MANNEQUIN_USER_PARAMS__NOTHING

#define MANNEQUIN_USER_PARAMS__ADD_ONE(x, ...)                   + 1
#define MANNEQUIN_USER_PARAMS__OPTIONAL_ARGUMENT(...)            , ## __VA_ARGS__
#define MANNEQUIN_USER_PARAMS__OPTIONAL_ARGUMENT_FROM_LIST(list) MANNEQUIN_USER_PARAMS__OPTIONAL_ARGUMENT(list(MANNEQUIN_USER_PARAMS__ADD_ONE))

#define MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(list, stuff)    MANNEQUIN_USER_PARAMS__GET_THIRD_ARG(1 MANNEQUIN_USER_PARAMS__OPTIONAL_ARGUMENT_FROM_LIST(list), stuff, MANNEQUIN_USER_PARAMS__NOTHING)

#define MANNEQUIN_USER_PARAMS__EMPTY_LIST(x)

#define MANNEQUIN_USER_PARAMS__DECLARE_FRAGMENT(elem) FragmentIDAutoInit elem;
#define MANNEQUIN_USER_PARAMS__INIT_FRAGMENT(elem)    elem = tagDefs.Find( # elem);

#define MANNEQUIN_USER_PARAMS__FRAGMENT_STRUCT(elementsName, elementList) \
  struct elementsName                                                     \
  {                                                                       \
    elementList(MANNEQUIN_USER_PARAMS__DECLARE_FRAGMENT)                  \
                                                                          \
    void Init(const CTagDefinition &tagDefs)                              \
    {                                                                     \
      elementList(MANNEQUIN_USER_PARAMS__INIT_FRAGMENT)                   \
    }                                                                     \
  };

#define MANNEQUIN_USER_PARAMS__DECLARE_TAG(elem) TagIDAutoInit elem;
#define MANNEQUIN_USER_PARAMS__INIT_TAG(elem)    elem = tagDefs.Find( # elem);

#define MANNEQUIN_USER_PARAMS__TAG_STRUCT(elementsName, elementList) \
  struct elementsName                                                \
  {                                                                  \
    elementList(MANNEQUIN_USER_PARAMS__DECLARE_TAG)                  \
                                                                     \
    void Init(const CTagDefinition &tagDefs)                         \
    {                                                                \
      elementList(MANNEQUIN_USER_PARAMS__INIT_TAG)                   \
    }                                                                \
  };

#define MANNEQUIN_USER_PARAMS__DECLARE_TAGGROUP(groupElem) TagGroupIDAutoInit groupElem;
#define MANNEQUIN_USER_PARAMS__INIT_TAGGROUP(groupElem)    groupElem = tagDefs.FindGroup( # groupElem);

#define MANNEQUIN_USER_PARAMS__TAGGROUP_STRUCT(elementsName, elementList) \
  struct elementsName                                                     \
  {                                                                       \
    elementList(MANNEQUIN_USER_PARAMS__DECLARE_TAGGROUP)                  \
                                                                          \
    void Init(const CTagDefinition &tagDefs)                              \
    {                                                                     \
      elementList(MANNEQUIN_USER_PARAMS__INIT_TAGGROUP)                   \
    }                                                                     \
  };

#define MANNEQUIN_USER_PARAMS__DECLARE_SCOPE(elem) ScopeIDAutoInit elem;
#define MANNEQUIN_USER_PARAMS__INIT_SCOPE(elem)    elem = tagDefs.Find( # elem);

#define MANNEQUIN_USER_PARAMS__SCOPE_STRUCT(elementsName, elementList) \
  struct elementsName                                                  \
  {                                                                    \
    elementList(MANNEQUIN_USER_PARAMS__DECLARE_SCOPE)                  \
                                                                       \
    void Init(const CTagDefinition &tagDefs)                           \
    {                                                                  \
      elementList(MANNEQUIN_USER_PARAMS__INIT_SCOPE)                   \
    }                                                                  \
  };

#define MANNEQUIN_USER_PARAMS__FRAGTAG_STRUCT(fragmentName, fragTagTagsList, fragTagTagGroupsList)                                                     \
  struct S ## fragmentName                                                                                                                             \
  {                                                                                                                                                    \
    S ## fragmentName()                                                                                                                                \
      : pTagDefinition(NULL)                                                                                                                           \
    {}                                                                                                                                                 \
                                                                                                                                                       \
                                                                                                                                                       \
    void Init(const SControllerDef &controllerDef)                                                                                                     \
    {                                                                                                                                                  \
      pTagDefinition = NULL;                                                                                                                           \
      fragmentID = controllerDef.m_fragmentIDs.Find( # fragmentName);                                                                                  \
      if (fragmentID != FRAGMENT_ID_INVALID)                                                                                                           \
      {                                                                                                                                                \
        pTagDefinition = controllerDef.GetFragmentTagDef(fragmentID);                                                                                  \
      }                                                                                                                                                \
                                                                                                                                                       \
      static CTagDefinition s_dummyTagDefinition;                                                                                                      \
      pTagDefinition = pTagDefinition ? pTagDefinition : &s_dummyTagDefinition;                                                                        \
      MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(fragTagTagsList, fragmentTagIDs.Init(*pTagDefinition));                                                 \
      MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(fragTagTagGroupsList, fragmentTagGroupIDs.Init(*pTagDefinition));                                       \
    }                                                                                                                                                  \
                                                                                                                                                       \
    MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(fragTagTagsList, MANNEQUIN_USER_PARAMS__FRAGMENT_STRUCT(FragmentTagIDs, fragTagTagsList));                \
    MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(fragTagTagGroupsList, MANNEQUIN_USER_PARAMS__TAGGROUP_STRUCT(FragmentTagGroupIDs, fragTagTagGroupsList)); \
                                                                                                                                                       \
    TagIDAutoInit fragmentID;                                                                                                                          \
    MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(fragTagTagsList, FragmentTagIDs fragmentTagIDs);                                                          \
    MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(fragTagTagGroupsList, FragmentTagGroupIDs fragmentTagGroupIDs);                                           \
    const CTagDefinition* pTagDefinition;                                                                                                              \
  };                                                                                                                                                   \
                                                                                                                                                       \
  S ## fragmentName fragmentName;

#define MANNEQUIN_USER_PARAMS__FRAGTAG_STRUCT_INIT(fragmentName, ...) fragmentName.Init(controllerDef);

#define MANNEQUIN_USER_PARAMS__FRAGTAG_HOLDER_STRUCT(elementsName, elementList) \
  struct elementsName                                                           \
  {                                                                             \
    elementList(MANNEQUIN_USER_PARAMS__FRAGTAG_STRUCT)                          \
                                                                                \
    void Init(const SControllerDef &controllerDef)                              \
    {                                                                           \
      elementList(MANNEQUIN_USER_PARAMS__FRAGTAG_STRUCT_INIT)                   \
    }                                                                           \
                                                                                \
  };

#define MANNEQUIN_USER_PARAMS(structName, fragmentList, tagList, tagGroupList, scopeList, contextList, fragmentTagsList)                   \
  struct structName                                                                                                                        \
    : public IMannequinUserParams                                                                                                          \
  {                                                                                                                                        \
    virtual void Init(const SControllerDef &controllerDef)                                                                                 \
    {                                                                                                                                      \
      MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(fragmentList, fragmentIDs.Init(controllerDef.m_fragmentIDs));                               \
      MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(tagList, tagIDs.Init(controllerDef.m_tags));                                                \
      MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(tagGroupList, tagGroupIDs.Init(controllerDef.m_tags));                                      \
      MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(scopeList, scopeIDs.Init(controllerDef.m_scopeIDs));                                        \
      MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(contextList, contextIDs.Init(controllerDef.m_scopeContexts));                               \
      MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(fragmentTagsList, fragments.Init(controllerDef));                                           \
    }                                                                                                                                      \
                                                                                                                                           \
    MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(fragmentList, MANNEQUIN_USER_PARAMS__FRAGMENT_STRUCT(FragmentIDs, fragmentList));             \
    MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(tagList, MANNEQUIN_USER_PARAMS__TAG_STRUCT(TagIDs, tagList));                                 \
    MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(tagGroupList, MANNEQUIN_USER_PARAMS__TAGGROUP_STRUCT(TagGroupIDs, tagGroupList));             \
    MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(scopeList, MANNEQUIN_USER_PARAMS__SCOPE_STRUCT(ScopeIDs, scopeList));                         \
    MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(contextList, MANNEQUIN_USER_PARAMS__TAG_STRUCT(ContextIDs, contextList));                     \
    MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(fragmentTagsList, MANNEQUIN_USER_PARAMS__FRAGTAG_HOLDER_STRUCT(Fragments, fragmentTagsList)); \
                                                                                                                                           \
    MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(fragmentList, FragmentIDs fragmentIDs);                                                       \
    MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(tagList, TagIDs tagIDs);                                                                      \
    MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(tagGroupList, TagGroupIDs tagGroupIDs);                                                       \
    MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(scopeList, ScopeIDs scopeIDs);                                                                \
    MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(contextList, ContextIDs contextIDs);                                                          \
    MANNEQUIN_USER_PARAMS__IF_LIST_NOT_EMPTY(fragmentTagsList, Fragments fragments);                                                       \
  };

#define MANNEQUIN_AUTO_CRC__DECLARE_ELEMENT(elem) uint32 elem;
#define MANNEQUIN_AUTO_CRC__INIT_ELEMENT(elem)    elem = CCrc32::ComputeLowercase( # elem);

#define MANNEQUIN_AUTO_CRC(structName, crcList)   \
  struct structName                               \
  {                                               \
    structName()                                  \
      : m_initialised(false)                      \
    {}                                            \
                                                  \
    void Init()                                   \
    {                                             \
      if (!m_initialised)                         \
      {                                           \
        crcList(MANNEQUIN_AUTO_CRC__INIT_ELEMENT) \
        m_initialised = true;                     \
      }                                           \
    }                                             \
                                                  \
    crcList(MANNEQUIN_AUTO_CRC__DECLARE_ELEMENT); \
    bool m_initialised;                           \
  };

#endif
