// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/IObject.h"
#include "CrySchematyc/Utils/Assert.h"
#include "CrySchematyc/Utils/GUID.h"

namespace Schematyc
{

// Forward declare interfaces.
struct IObject;

struct SActionParams
{
	inline SActionParams(const CryGUID& _guid, IObject& _object)
		: guid(_guid)
		, object(_object)
	{}

	const CryGUID& guid;
	IObject&     object;
	IEntityComponent*  pComponent = nullptr;
};

class CAction
{
public:

	virtual bool Init()
	{
		return true;
	}

	virtual void Start()    {}
	virtual void Stop()     {}
	virtual void Shutdown() {}

	inline void  PreInit(const SActionParams& params)
	{
		m_guid = params.guid;
		m_pObject = &params.object;
		m_pComponent = params.pComponent;
	}

	template<typename SIGNAL> inline void OutputSignal(const SIGNAL& signal)
	{
		GetObject().ProcessSignal(signal, m_guid);
	}

	inline const CryGUID& GetGUID() const
	{
		return m_guid;
	}

	inline IObject& GetObject() const
	{
		SCHEMATYC_CORE_ASSERT(m_pObject);
		return *m_pObject;
	}

	inline IEntityComponent* GetComponent() const
	{
		return m_pComponent;
	}

private:

	CryGUID       m_guid;
	IObject*    m_pObject = nullptr;
	IEntityComponent* m_pComponent = nullptr;
};

DECLARE_SHARED_POINTERS(CAction)

} // Schematyc
