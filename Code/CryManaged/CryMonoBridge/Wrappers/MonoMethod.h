// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef HAVE_MONO_API
namespace MonoInternals
{
	struct MonoMethod;
}
#endif

class CMonoObject;

class CMonoMethod
{
	friend class CMonoClass;

	// Begin public API
public:
	CMonoMethod(MonoInternals::MonoMethod* pMethod);

	std::shared_ptr<CMonoObject> Invoke(const CMonoObject* pObject, void** pParameters, bool &bEncounteredException) const;
	std::shared_ptr<CMonoObject> Invoke(const CMonoObject* pObject = nullptr, void** pParameters = nullptr) const;
	
	inline std::shared_ptr<CMonoObject> InvokeStatic(void** pParameters) const { return Invoke(nullptr, pParameters); }

	uint32 GetParameterCount() const;
	string GetSignatureDescription(bool bIncludeNamespace = true) const;
	const char* GetName() const { return MonoInternals::mono_method_get_name(m_pMethod); }
	
	MonoInternals::MonoMethod* GetHandle() const { return m_pMethod; }

protected:
	std::shared_ptr<CMonoObject> InvokeInternal(MonoInternals::MonoObject* pMonoObject, void** pParameters, bool &bEncounteredException) const;

	void PrepareForSerialization();
	const char* GetSerializedDescription() const { return m_description; }

	void OnDeserialized(MonoInternals::MonoMethod* pMethod) { m_pMethod = pMethod; }

protected:
	MonoInternals::MonoMethod* m_pMethod;
	
	string m_description;
};