// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Signalv2.h>

#include "CrySchematyc2/BasicTypes.h"
#include "CrySchematyc2/GUID.h"
#include "CrySchematyc2/Script/IScriptFile.h"

namespace Schematyc2
{
	struct IScriptClass;
	struct IScriptEnumeration;
	struct IScriptFunction;
	struct IScriptModule;

	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptFile&)>          ScriptFileVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptFile&)>    ScriptFileConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptElement&)>       ScriptElementVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptElement&)> ScriptElementConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (INewScriptFile&)>       NewScriptFileVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const INewScriptFile&)> NewScriptFileConstVisitor;

	enum class EScriptRegistryChange
	{
		ElementAdded,
		ElementRemoved
	};

	typedef TemplateUtils::CSignalv2<void (EScriptRegistryChange, IScriptElement*)> ScriptRegistryChangeSignal;

	struct SScriptRegistrySignals
	{
		ScriptRegistryChangeSignal change;
	};

	struct IScriptRegistry
	{
		virtual ~IScriptRegistry() {}

		// Compatibility interface.
		//////////////////////////////////////////////////
		virtual IScriptFile* LoadFile(const char* szFileName) = 0;
		virtual IScriptFile* CreateFile(const char* szFileName, EScriptFileFlags flags = EScriptFileFlags::None) = 0;
		virtual IScriptFile* GetFile(const SGUID& guid) = 0;
		virtual IScriptFile* GetFile(const char* szFileName) = 0;
		virtual void VisitFiles(const ScriptFileVisitor& visitor, const char* szFilePath = nullptr) = 0;
		virtual void VisitFiles(const ScriptFileConstVisitor& visitor, const char* szFilePath = nullptr) const = 0;
		virtual void RefreshFiles(const SScriptRefreshParams& params) = 0;
		virtual bool Load() = 0;
		virtual void Save(bool bAlwaysSave = false) = 0;

		// New interface.
		//////////////////////////////////////////////////
		virtual IScriptModule* AddModule(const char* szName, IScriptElement* pScope = nullptr) = 0;
		virtual IScriptEnumeration* AddEnumeration(const char* szName, IScriptElement* pScope = nullptr) = 0;
		virtual IScriptFunction* AddFunction(const char* szName, IScriptElement* pScope = nullptr) = 0;
		virtual IScriptClass* AddClass(const char* szName, const SGUID& foundationGUID, IScriptElement* pScope = nullptr) = 0;
		virtual IScriptElement* GetElement(const SGUID& guid) = 0;
		virtual void RemoveElement(const SGUID& guid) = 0;
		virtual EVisitStatus VisitElements(const ScriptElementVisitor& visitor, IScriptElement* pScope = nullptr, EVisitFlags flags = EVisitFlags::None) = 0;
		virtual EVisitStatus VisitElements(const ScriptElementConstVisitor& visitor, const IScriptElement* pScope = nullptr, EVisitFlags flags = EVisitFlags::None) const = 0;
		virtual bool IsElementNameUnique(const char* szName, IScriptElement* pScope = nullptr) const = 0; // #SchematycTODO : Should we also validate the name here?
		virtual SScriptRegistrySignals& Signals() = 0;
	};
}
