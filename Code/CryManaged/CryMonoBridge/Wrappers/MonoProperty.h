// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "MonoObject.h"
#include "MonoMethod.h"

class CMonoProperty
{
public:
	// Hacky workaround for no API access for getting MonoProperty from reflection data
	// Post-5.3 we should expose this to Mono and send the change back.
	struct InternalMonoReflectionType
	{
		MonoInternals::MonoObject object;
		MonoInternals::MonoClass *klass;
		MonoInternals::MonoProperty *property;
	};

	CMonoProperty(MonoInternals::MonoProperty* pProperty);
	CMonoProperty(MonoInternals::MonoReflectionProperty* pProperty);
	virtual ~CMonoProperty() {}

	std::shared_ptr<CMonoObject> Get(MonoInternals::MonoObject* pObject, bool &bEncounteredException) const;
	void Set(MonoInternals::MonoObject* pObject, MonoInternals::MonoObject* pValue, bool &bEncounteredException) const;
	void Set(MonoInternals::MonoObject* pObject, void** pParams, bool &bEncounteredException) const;
	
	MonoInternals::MonoProperty* GetHandle() const { return m_pProperty; }

	CMonoMethod GetGetMethod() const;
	CMonoMethod GetSetMethod() const;

	MonoInternals::MonoType* GetUnderlyingType(MonoInternals::MonoReflectionProperty* pReflectionProperty) const;
	MonoInternals::MonoClass* GetUnderlyingClass(MonoInternals::MonoReflectionProperty* pReflectionProperty) const;

protected:
	MonoInternals::MonoProperty* m_pProperty;
};