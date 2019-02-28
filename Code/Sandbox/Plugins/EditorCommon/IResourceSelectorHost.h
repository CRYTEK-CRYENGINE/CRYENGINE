// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
// The aim of IResourceSelectorHost is to unify resource selection dialogs in a one
// API that can be reused with plugins. It also makes possible to register new
// resource selectors dynamically, e.g. inside plugins.
//
// Here is how new selectors are created. In your implementation file you add handler function:
//
//   #include "IResourceSelectorHost.h"
//
//   dll_string SoundFileSelector(const SResourceSelectorContext& x, const char* previousValue)
//   {
//     CMyModalDialog dialog(CWnd::FromHandle(x.parentWindow));
//     ...
//     return previousValue;
//   }
//   REGISTER_RESOURCE_SELECTOR("Sound", SoundFileSelector, "Editor/icons/sound_16x16.png")
//
// To expose it to serialization:
//
//   #include "Serialization/Decorators/Resources.h"
//	 template<class TString>
//	 ResourceSelector<TString> SoundName(TString& s) { return ResourceSelector<TString>(s, "Sound"); }
//
// To use in serialization:
//
//   ar(Serialization::SoundName(soundString), "soundString", "Sound String");
//
// Here is how it can be invoked directly:
//
//   SResourceSelectorContext x;
//   x.parentWindow = parent.GetSafeHwnd();
//   x.typeName = "Sound";
//   string newValue = GetIEditor()->GetResourceSelector()->SelectResource(x, previousValue).c_str();
//
// If you have your own resource selectors in the plugin you will need to run
//
//	  RegisterModuleResourceSelectors(GetIEditor()->GetResourceSelector())
//
// during plugin initialization.
//
// If you want to be able to pass some custom context to the selector (e.g. source of the information for the
// list of items or something similar) then you can add a poitner argument to your selector function, i.e.:
//
//   dll_string SoundFileSelector(const SResourceSelectorContext& x, const char* previousValue,
//			                          SoundFileList* list) // your context argument
//
// And provide this value through serialization context:
//
// struct SourceFileList
// {
//   void Serialize(IArchive& ar)
//   {
//     Serialization::SContext<SourceFileList> context(ar, this);
//     ...
//   }
// }

#include "dll_string.h"

#include <CrySerialization/TypeID.h>
#include <CrySerialization/Decorators/ResourceSelector.h>
#include <EditorCommonAPI.h>
#include "AutoRegister.h"

enum class EAssetResourcePickerState
{
	// Order must be consistent with comment of cvar ed_enableAssetPickers in CAssetManager.cpp
	Disable,
	EnableRecommended,
	EnableAll
};

struct HWND__;
typedef HWND__* HWND;
struct ICharacterInstance;
struct SStaticResourceSelectorEntry;
class QWidget;

struct IResourceSelectionCallback
{
	virtual void SetValue(const char* newValue) = 0;
};

struct SResourceSelectorContext
{
	const char* typeName;
	const SStaticResourceSelectorEntry* resourceSelectorEntry;

	// when null the main window is used as parent.
	QWidget*              parentWidget;
	bool                  useLegacyPicker;
	unsigned int          entityId;
	void*                 contextObject;
	Serialization::TypeID contextObjectType;

	//! The callback can be used to set intermediate values. This variable might be nullptr.
	//! Typically the callback is used for live-updating a resource when a user changes a selection.
	//! This does not overwrite the value returned by the TResourceSelectionFunction, which is always the resource's final value.
	IResourceSelectionCallback* callback;

	Serialization::ICustomResourceParamsPtr pCustomParams;

	SResourceSelectorContext()
		: typeName(0)
		, resourceSelectorEntry(0)
		, parentWidget(0)
		, useLegacyPicker(false)
		, entityId(0)
		, contextObject()
		, contextObjectType(Serialization::TypeID())
		, callback(nullptr)
	{
	}
};

// TResourceSelecitonFunction is used to declare handlers for specific types.
//
// For canceled dialogs previousValue should be returned.
typedef dll_string (* TResourceSelectionFunction)(const SResourceSelectorContext& selectorContext, const char* previousValue);
typedef void (*       TResourceEditFunction)(const SResourceSelectorContext& selectorContext, const char* previousValue);
typedef dll_string (* TResourceSelectionFunctionWithContext)(const SResourceSelectorContext& selectorContext, const char* previousValue, void* contextObject);
typedef dll_string (* TResourceValidationFunction)(const SResourceSelectorContext& selectorContext, const char* newValue, const char* previousValue);
typedef dll_string (* TResourceValidationFunctionWithContext)(const SResourceSelectorContext& selectorContext, const char* newValue, const char* previousValue, void* contextObject);



// See note at the beginning of the file.
struct IResourceSelectorHost
{
	virtual const SStaticResourceSelectorEntry* GetSelector(const char* typeName) const = 0;

	//Select a resource without having to build a context
	virtual dll_string SelectResource(const char* typeName, const char* previousValue, QWidget* parentWidget = nullptr, void* contextObject = nullptr) const = 0;
	
	virtual void RegisterResourceSelector(const SStaticResourceSelectorEntry* entry) = 0;

	// secondary responsibility of this class is to store global selections
	// for example this is used to transfer animations from Character Tool to Mannequin
	// TODO: this a very bad design and should be removed, do not use or expand upon this notion
	virtual void        SetGlobalSelection(const char* resourceType, const char* value) = 0;
	virtual const char* GetGlobalSelection(const char* resourceType) const = 0;
};

// ---------------------------------------------------------------------------

#define INTERNAL_RSH_COMBINE_UTIL(A, B) A ## B
#define INTERNAL_RSH_COMBINE(A, B)      INTERNAL_RSH_COMBINE_UTIL(A, B)

#define INTERNAL_REGISTER_RESOURCE_SELECTOR(name) \
  CAutoRegisterResourceSelector INTERNAL_RSH_COMBINE(g_AutoRegHelper, name)([](){ GetIEditor()->GetResourceSelectorHost()->RegisterResourceSelector(& name); });

#define REGISTER_RESOURCE_SELECTOR(name, function, icon) \
 namespace Private_ResourceSelector { SStaticResourceSelectorEntry INTERNAL_RSH_COMBINE(selector_ ## function, __LINE__)((name), (function), (icon)); \
  INTERNAL_REGISTER_RESOURCE_SELECTOR(INTERNAL_RSH_COMBINE(selector_ ## function, __LINE__)) }

#define REGISTER_RESOURCE_VALIDATING_SELECTOR(name, function, validation, icon) \
 namespace Private_ResourceSelector { SStaticResourceSelectorEntry INTERNAL_RSH_COMBINE(selector_ ## function, __LINE__)((name), (function), (validation), (icon)); \
INTERNAL_REGISTER_RESOURCE_SELECTOR(INTERNAL_RSH_COMBINE(selector_ ## function, __LINE__)) }

#define REGISTER_RESOURCE_EDITING_SELECTOR(name, function, validation, editing, icon) \
 namespace Private_ResourceSelector { SStaticResourceSelectorEntry INTERNAL_RSH_COMBINE(selector_ ## function, __LINE__)((name), (function), (validation), (editing), (icon)); \
INTERNAL_REGISTER_RESOURCE_SELECTOR(INTERNAL_RSH_COMBINE(selector_ ## function, __LINE__)) }

//Note that this is used only once and this is for an asset type
#define REGISTER_RESOURCE_EDITING_SELECTOR_WITH_LEGACY_SUPPORT(name, function, validation, editing, icon) \
 namespace Private_ResourceSelector { SStaticResourceSelectorEntry INTERNAL_RSH_COMBINE(selector_ ## function, __LINE__)((name), (function), (validation), (editing), (icon), true); \
INTERNAL_REGISTER_RESOURCE_SELECTOR(INTERNAL_RSH_COMBINE(selector_ ## function, __LINE__)) }



//! Describes how a particular resource type should be handled by generic UI such as pickers in a property tree
struct EDITOR_COMMON_API SStaticResourceSelectorEntry
{
protected:
	const char*                            typeName;
	TResourceSelectionFunction             function;
	TResourceSelectionFunctionWithContext  functionWithContext;
	TResourceValidationFunction            validate;
	TResourceEditFunction                  edit;
	TResourceValidationFunctionWithContext validateWithContext;
	const char*                            iconPath;
	Serialization::TypeID                  contextType;
	bool                                   addLegacyPicker;
	bool								   isAsset;
	

//Public API part
public:

	const char*				GetTypeName() const { return typeName; }
	const char*				GetIconPath() const { return iconPath; }
	Serialization::TypeID	GetContextType() const { return contextType; }

	//! The field can only have free-form input if validators were specified, otherwise only the picker allows to choose a new value
	bool					UsesInputField() const; 

	bool					HasLegacyPicker() const { return addLegacyPicker; }
	bool					CanEdit() const { return edit != nullptr; }
	void					EditResource(const SResourceSelectorContext& context, const char* value) const;
	dll_string				ValidateValue(const SResourceSelectorContext& context, const char* newValue, const char* previousValue) const;
	dll_string				SelectResource(const SResourceSelectorContext& context, const char* previousValue) const;

	virtual bool			ShowTooltip(const SResourceSelectorContext& context, const char* value) const;
	virtual void			HideTooltip(const SResourceSelectorContext& context, const char* value) const;
	bool					IsAssetSelector() const { return isAsset; }


//Constructors are only meant to be used through declarative macros
public:
	SStaticResourceSelectorEntry(const char* typeName, TResourceSelectionFunction function, const char* icon, const bool addLegacyPicker = false)
		: typeName(typeName)
		, function(function)
		, functionWithContext(nullptr)
		, validate(nullptr)
		, edit(nullptr)
		, validateWithContext(nullptr)
		, iconPath(icon)
		, addLegacyPicker(addLegacyPicker)
		, isAsset(false)
	{
	}

	SStaticResourceSelectorEntry(const char* typeName, TResourceSelectionFunction function, TResourceValidationFunction validation, TResourceEditFunction editing, char* icon, const bool addLegacyPicker = false)
		: typeName(typeName)
		, function(function)
		, functionWithContext(nullptr)
		, validate(validation)
		, edit(editing)
		, validateWithContext(nullptr)
		, iconPath(icon)
		, addLegacyPicker(addLegacyPicker)
		, isAsset(false)
	{
	}

	SStaticResourceSelectorEntry(const char* typeName, TResourceSelectionFunction function, TResourceValidationFunction validation, char* icon, const bool addLegacyPicker = false)
		: typeName(typeName)
		, function(function)
		, functionWithContext(nullptr)
		, validate(validation)
		, edit(nullptr)
		, validateWithContext(nullptr)
		, iconPath(icon)
		, addLegacyPicker(addLegacyPicker)
		, isAsset(false)
	{
	}

	template<class T>
	SStaticResourceSelectorEntry(const char* typeName
	                             , dll_string(*function)(const SResourceSelectorContext &, const char* previousValue, T * context)
	                             , const char* icon
	                             , const bool addLegacyPicker = false)
		: typeName(typeName)
		, function(nullptr)
		, functionWithContext(TResourceSelectionFunctionWithContext(function))
		, validate(nullptr)
		, edit(nullptr)
		, validateWithContext(nullptr)
		, iconPath(icon)
		, addLegacyPicker(addLegacyPicker)
		, isAsset(false)
	{
		contextType = Serialization::TypeID::get<T>();
	}

	template<class T>
	SStaticResourceSelectorEntry(const char* typeName
	                             , dll_string(*function)(const SResourceSelectorContext &, const char* previousValue, T * context)
	                             , dll_string(*validation)(const SResourceSelectorContext &, const char* newValue, const char* previousValue, T * context)
	                             , const char* icon
	                             , const bool addLegacyPicker = false)
		: typeName(typeName)
		, function(nullptr)
		, functionWithContext(TResourceSelectionFunctionWithContext(function))
		, validate(nullptr)
		, edit(nullptr)
		, validateWithContext(TResourceValidationFunctionWithContext(validation))
		, iconPath(icon)
		, addLegacyPicker(addLegacyPicker)
		, isAsset(false)
	{
		contextType = Serialization::TypeID::get<T>();
	}
};

typedef CAutoRegister<SStaticResourceSelectorEntry> CAutoRegisterResourceSelector;

