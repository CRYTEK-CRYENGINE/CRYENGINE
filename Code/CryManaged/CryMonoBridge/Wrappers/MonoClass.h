// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CMonoLibrary;
class CMonoObject;
class CMonoMethod;
class CMonoProperty;

#ifndef HAVE_MONO_API
namespace MonoInternals
{
	struct MonoClass;
}
#endif

class CMonoClass
{
	friend class CMonoLibrary;
	friend class CMonoRuntime;
	friend class CAppDomain;

	// Begin public API
public:
	CMonoClass(CMonoLibrary* pLibrary, const char *nameSpace, const char *name);
	CMonoClass(CMonoLibrary* pLibrary, MonoInternals::MonoClass* pClass);
	~CMonoClass() = default;

	const char* GetName() const;
	const char* GetNamespace() const;

	CMonoLibrary* GetAssembly() const;
		// Creates a new instance of this class without calling any constructors
	std::shared_ptr<CMonoObject> CreateUninitializedInstance();
	// Creates a new instance of this class
	std::shared_ptr<CMonoObject> CreateInstance(void** pConstructorParams = nullptr, int numParams = 0);
	std::shared_ptr<CMonoObject> CreateInstanceWithDesc(const char* parameterDesc, void** pConstructorParams);

	std::shared_ptr<CMonoObject> CreateFromMonoObject(MonoInternals::MonoObject* pObject);

	// Searches the specified class for the method
	// Will NOT search in base classes, see FindMethodInInheritedClasses
	std::weak_ptr<CMonoMethod> FindMethod(const char* szName, int numParams = 0);
	// Searches the entire inheritance tree for the specified method
	std::weak_ptr<CMonoMethod> FindMethodInInheritedClasses(const char* szName, int numParams = 0);

	// Searches the specified class for the method by its description
	// Will NOT search in base classes, see FindMethodWithDescInInheritedClasses
	std::weak_ptr<CMonoMethod> FindMethodWithDesc(const char* szMethodDesc);
	// Searches the entire inheritance tree for the specified method by its description
	// Skips searching after encountering pBaseClass if present
	std::weak_ptr<CMonoMethod> FindMethodWithDescInInheritedClasses(const char* szMethodDesc, CMonoClass* pBaseClass);
	
	std::weak_ptr<CMonoProperty> FindProperty(const char* szName);
	std::weak_ptr<CMonoProperty> FindPropertyInInheritedClasses(const char* szName);

	static std::shared_ptr<CMonoProperty> MakeProperty(MonoInternals::MonoProperty* pProperty, const char* szName);
	static std::shared_ptr<CMonoProperty> MakeProperty(MonoInternals::MonoReflectionProperty* pProperty, const char* szName);

	void SetMonoClass(MonoInternals::MonoClass* pMonoClass);
	MonoInternals::MonoClass* GetMonoClass() const { return m_pClass; }
	bool IsValueType() const;

	bool IsVoid() const { return m_pClass == MonoInternals::mono_get_void_class(); }

protected:
	void Serialize(CMonoObject* pSerializer);
	void Deserialize(CMonoObject* pSerializer);

	void ReloadClass();
	void OnAssemblyUnload();
	// Re-query all cached methods, in case they were removed from the class
	void RefreshCachedMembers();

	// Temporary classes are assigned a weak pointer of itself to pass on to created objects
	void SetWeakPointer(std::weak_ptr<CMonoClass> pClass) { m_pThis = pClass; }

	void RegisterObject(std::weak_ptr<CMonoObject> pObject);
	std::weak_ptr<CMonoMethod> FindOrEmplaceMethod(MonoInternals::MonoMethod* pMethod);

protected:
	MonoInternals::MonoClass* m_pClass;
	CMonoLibrary* m_pLibrary;

	// Each class has to contain a weak pointer to itself in order to provide a shared pointer to the objects it creates
	// Otherwise temporary classes may create objects that outlive the class itself
	std::weak_ptr<CMonoClass> m_pThis;

	// Vector containing weak pointers to all created objects
	// We don't actually manage the deletion of these objects, that is up to whoever created the instance.
	std::vector<std::weak_ptr<CMonoObject>> m_objects;

	// Vector containing weak pointers to all queried methods
	// Needed to make sure function pointers are activated after serialization
	std::vector<std::shared_ptr<CMonoMethod>> m_methods;

	std::vector<std::shared_ptr<CMonoProperty>> m_properties;

	string m_namespace;
	string m_name;
};