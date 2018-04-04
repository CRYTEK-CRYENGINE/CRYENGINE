// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "MonoObject.h"

class CMonoProperty
{
	// Hacky workaround for no API access for getting MonoProperty from reflection data
	// Post-5.3 we should expose this to Mono and send the change back.
	struct InternalMonoReflectionType
	{
		MonoInternals::MonoObject object;
		MonoInternals::MonoClass *klass;
		MonoInternals::MonoProperty *property;
	};

public:
	CMonoProperty(MonoInternals::MonoProperty* pProperty);
	CMonoProperty(MonoInternals::MonoReflectionProperty* pProperty);
	virtual ~CMonoProperty() {}

	std::shared_ptr<CMonoObject> Get(const CMonoObject* pObject, bool &bEncounteredException) const;
	void Set(const CMonoObject* pObject, const CMonoObject* pValue, bool &bEncounteredException) const;

	void Set(const class CMonoObject* pObject, void** pParams, bool &bEncounteredException) const;

	MonoInternals::MonoProperty* GetHandle() const { return m_pProperty; }
	MonoInternals::MonoTypeEnum GetType(MonoInternals::MonoReflectionProperty* pReflectionProperty) const;

	MonoInternals::MonoType* GetUnderlyingType(MonoInternals::MonoReflectionProperty* pReflectionProperty) const;
	MonoInternals::MonoClass* GetUnderlyingClass(MonoInternals::MonoReflectionProperty* pReflectionProperty) const;

protected:
	MonoInternals::MonoProperty* m_pProperty;
};