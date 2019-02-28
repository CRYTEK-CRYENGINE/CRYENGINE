// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

	CMonoProperty(MonoInternals::MonoProperty* pProperty, const char* szName);
	CMonoProperty(MonoInternals::MonoReflectionProperty* pProperty, const char* szName);
	virtual ~CMonoProperty() {}

	std::shared_ptr<CMonoObject> Get(MonoInternals::MonoObject* pObject, bool &bEncounteredException) const;
	void Set(MonoInternals::MonoObject* pObject, MonoInternals::MonoObject* pValue, bool &bEncounteredException) const;
	void Set(MonoInternals::MonoObject* pObject, void** pParams, bool &bEncounteredException) const;
	
	MonoInternals::MonoProperty* GetHandle() const { return m_pProperty; }
	void OnDeserialized(MonoInternals::MonoProperty* pNewHandle) { m_pProperty = pNewHandle; }

	CMonoMethod GetGetMethod() const;
	CMonoMethod GetSetMethod() const;

	const char* GetName() const { return m_name.c_str(); }

	std::shared_ptr<CMonoClass> GetUnderlyingClass() const { return m_pUnderlyingClass; }

protected:
	MonoInternals::MonoType* GetUnderlyingType(MonoInternals::MonoReflectionProperty* pReflectionProperty) const;
	MonoInternals::MonoClass* GetUnderlyingClass(MonoInternals::MonoReflectionProperty* pReflectionProperty) const;

protected:
	string m_name;
	MonoInternals::MonoProperty* m_pProperty;
	std::shared_ptr<CMonoClass>  m_pUnderlyingClass;
};