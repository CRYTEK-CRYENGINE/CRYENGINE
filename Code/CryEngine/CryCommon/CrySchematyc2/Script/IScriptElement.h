// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CrySerialization/Forward.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>

#include "CrySchematyc2/BasicTypes.h"
#include "CrySchematyc2/GUID.h"

namespace Schematyc2
{
	struct IGUIDRemapper;
	struct IScriptExtensionMap;
	struct IScriptFile;

	//////////////////////////////////////////////////

	struct IScriptElement;

	struct INewScriptFile // #SchematycTODO : Move to separate file?
	{
		virtual ~INewScriptFile() {}

		virtual SGUID GetGUID() const = 0;
		virtual const char* GetName() const = 0;
		virtual const CTimeValue& GetTimeStamp() const = 0; // #SchematycTODO : Should we be getting the timestamp directly from the file itself?
		virtual void AddElement(IScriptElement& element) = 0;
		virtual void RemoveElement(IScriptElement& element) = 0;
		virtual uint32 GetElementCount() const = 0;
	};

	//////////////////////////////////////////////////

	enum class EScriptElementType
	{
		None,
		Root,
		Module,
		Include,
		Group,
		Enumeration,
		Structure,
		Signal,
		Function,
		AbstractInterface,
		AbstractInterfaceFunction,
		AbstractInterfaceTask,
		Class,
		ClassBase,
		StateMachine,
		State,
		Variable,
		Property,
		Container,
		Timer,
		AbstractInterfaceImplementation,
		ComponentInstance,
		ActionInstance,
		Graph
	};

	enum class EScriptElementFlags
	{
		None     = 0,
		System   = BIT(0), // Element was created by system and therefore can't be removed by users.
		FromFile = BIT(1), // Element was loaded from file.
		Discard  = BIT(2)  // #SchematycTODO : Either remove or replace with flag indicating element 'can' be discarded.
		// #SchematycTODO : Use flags to specify whether element can be renamed, copied, removed etc?
	};

	DECLARE_ENUM_CLASS_FLAGS(EScriptElementFlags)

	typedef TemplateUtils::CDelegate<void (const SGUID&)> ScriptDependancyEnumerator;

	enum class EScriptRefreshReason
	{
		Load,                     // Sent immediately after all files have been loaded.
		EditorAdd,                // Editor only. Sent immediately after element is first added to file.
		EditorFixUp,              // Editor only. Sent after editor plug-in has been initialized to fix-up files and resolve broken/deprecated dependencies.
		EditorPaste,              // Editor only. Sent immediately after one or more elements have been pasted to file.
		EditorEnvModified,        // Editor only. Sent when environment has been modified.
		EditorDependencyModified, // Editor only. Sent when a dependency has been modified.
		EditorSelect,             // Editor only. Sent when an element is selected.
		Other
	};

	struct SScriptRefreshParams
	{
		explicit inline SScriptRefreshParams(EScriptRefreshReason _reason, const SGUID& _guid = SGUID())
			: reason(_reason)
			, guid(_guid)
		{}

		EScriptRefreshReason reason;
		SGUID                guid;
	};

	struct IScriptElement
	{
		virtual ~IScriptElement() {}

		virtual EScriptElementType GetElementType() const = 0;

		virtual IScriptFile& GetFile() = 0;
		virtual const IScriptFile& GetFile() const = 0;
		virtual SGUID GetGUID() const = 0;
		virtual SGUID GetScopeGUID() const = 0; // #SchematycTODO : Remove?

		virtual bool SetName(const char* szName) = 0;
		virtual const char* GetName() const = 0;
		//virtual void SetAccessor(EAccessor accessor) = 0;
		virtual EAccessor GetAccessor() const = 0;
		virtual void SetElementFlags(EScriptElementFlags flags) = 0;
		virtual EScriptElementFlags GetElementFlags() const = 0;
		virtual void SetNewFile(INewScriptFile* pNewFile) = 0;
		virtual INewScriptFile* GetNewFile() = 0;
		virtual const INewScriptFile* GetNewFile() const = 0;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// #SchematycTODO : Can these functions be made private/protected?
		virtual void AttachChild(IScriptElement& child) = 0;
		virtual void DetachChild(IScriptElement& child) = 0;
		virtual void SetParent(IScriptElement* pParent) = 0;
		virtual void SetPrevSibling(IScriptElement* pPrevSibling) = 0;
		virtual void SetNextSibling(IScriptElement* pNextSibling) = 0;
		////////////////////////////////////////////////////////////////////////////////////////////////////

		virtual IScriptElement* GetParent() = 0;
		virtual const IScriptElement* GetParent() const = 0;
		virtual IScriptElement* GetFirstChild() = 0;
		virtual const IScriptElement* GetFirstChild() const = 0;
		virtual IScriptElement* GetLastChild() = 0;
		virtual const IScriptElement* GetLastChild() const = 0;
		virtual IScriptElement* GetPrevSibling() = 0;
		virtual const IScriptElement* GetPrevSibling() const = 0;
		virtual IScriptElement* GetNextSibling() = 0;
		virtual const IScriptElement* GetNextSibling() const = 0;

		virtual IScriptExtensionMap& GetExtensions() = 0;
		virtual const IScriptExtensionMap& GetExtensions() const = 0;

		virtual void EnumerateDependencies(const ScriptDependancyEnumerator& enumerator) const = 0;
		virtual void Refresh(const SScriptRefreshParams& params) = 0;
		virtual void Serialize(Serialization::IArchive& archive) = 0;
		virtual void RemapGUIDs(IGUIDRemapper& guidRemapper) = 0;
	};
}
