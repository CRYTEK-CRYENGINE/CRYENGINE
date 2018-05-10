// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

template<class T> class CAutoRegister;

struct CRuntimeClass;

//! System class IDs
enum ESystemClassID
{
	ESYSTEM_CLASS_OBJECT                  = 0x0001,
	ESYSTEM_CLASS_EDITTOOL                = 0x0002,
	ESYSTEM_CLASS_PREFERENCE_PAGE         = 0x0020,
	ESYSTEM_CLASS_VIEWPANE                = 0x0021,
	//! Source/Asset Control Management Provider
	ESYSTEM_CLASS_SCM_PROVIDER            = 0x0022,
	ESYSTEM_CLASS_ASSET_TYPE			  = 0x0023,
	ESYSTEM_CLASS_ASSET_IMPORTER = 0x0024,
	ESYSTEM_CLASS_UITOOLS                 = 0x0050, //Still used by UI Emulator
	ESYSTEM_CLASS_USER                    = 0x1000
};

//! This interface describes a class created by a plugin
struct IClassDesc
{
	//////////////////////////////////////////////////////////////////////////
	// Class description.
	//////////////////////////////////////////////////////////////////////////
	//! This method returns an Editor defined GUID describing the class this plugin class is associated with.
	virtual ESystemClassID SystemClassID() = 0;
	//! This method returns the human readable name of the class.
	virtual const char*    ClassName() = 0;

	//! This method returns the UI name of the class.
	virtual const char* UIName() { return ClassName(); };

	//! Creates the object associated with the description. Default implementation returns nullptr as this may not be used by all class types.
	//! Use the templated CreateObject overload for convenience
	virtual void* CreateObject() { return nullptr; }
	
	template<typename T>
	T* CreateObject() { return static_cast<T*>(CreateObject()); }

	//////////////////////////////////////////////////////////////////////////
	//Legacy methods from MFC and object system
	//TODO: refactor by making a specialized ObjectClassDesc

	//! This method returns Category of this class, Category is specifying where this plugin class fits best in
	//! create panel.
	virtual const char*    Category() { return nullptr; }
	//! Get MFC runtime class of the object, created by this ClassDesc.
	virtual CRuntimeClass* GetRuntimeClass() { return nullptr; }
};

struct IEditorClassFactory
{
public:
	//! Register new class to the factory.
	virtual void        RegisterClass(IClassDesc* pClassDesc) = 0;
	//! Unregister the class from the factory.
	virtual void        UnregisterClass(IClassDesc* pClassDesc) = 0;
	//! Find class in the factory by class name.
	virtual IClassDesc* FindClass(const char* pClassName) const = 0;
	//! Get classes that matching specific requirements.
	virtual void        GetClassesBySystemID(ESystemClassID aSystemClassID, std::vector<IClassDesc*>& rOutClasses) = 0;
	virtual void        GetClassesByCategory(const char* pCategory, std::vector<IClassDesc*>& rOutClasses) = 0;
};


//! Auto registration for classes
typedef CAutoRegister<IClassDesc> CAutoRegisterClassHelper;

// Use this define to automatically register a new class description.
#define REGISTER_CLASS_DESC(ClassDesc)                                                                                       \
  namespace Private_Plugin                                                                                                   \
  {                                                                                                                          \
  ClassDesc g_classDesc##ClassDesc;																							 \
  CAutoRegisterClassHelper g_AutoRegHelper ## ClassDesc(																	 \
  [](){																														 \
	  GetIEditor()->GetClassFactory()->RegisterClass(&g_classDesc##ClassDesc);												 \
  },																														 \
  [](){																														 \
	  GetIEditor()->GetClassFactory()->UnregisterClass(&g_classDesc##ClassDesc);											 \
  });																														 \
  }


