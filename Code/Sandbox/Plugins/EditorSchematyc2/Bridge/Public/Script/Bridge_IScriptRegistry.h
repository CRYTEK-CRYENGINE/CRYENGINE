// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptRegistry.h>
#include "Bridge_IScriptFile.h"

namespace Schematyc2 {

struct IScriptRegistry;

struct IScriptClass;
struct IScriptEnumeration;
struct IScriptFunction;
struct IScriptModule;

};

namespace Bridge {

typedef TemplateUtils::CDelegate<Schematyc2::EVisitStatus(IScriptFile&)>          ScriptFileVisitor;
typedef TemplateUtils::CDelegate<Schematyc2::EVisitStatus(const IScriptFile&)>    ScriptFileConstVisitor;

struct IScriptRegistry
{
	virtual ~IScriptRegistry() {}

	// Compatibility interface.
	//////////////////////////////////////////////////        
	virtual IScriptFile* LoadFile(const char* szFileName) = 0;
	virtual IScriptFile* CreateFile(const char* szFileName, Schematyc2::EScriptFileFlags flags = Schematyc2::EScriptFileFlags::None) = 0;
	virtual IScriptFile* GetFile(const Schematyc2::SGUID& guid) = 0;
	virtual IScriptFile* GetFile(const char* szFileName) = 0;
	virtual void VisitFiles(const ScriptFileVisitor& visitor, const char* szFilePath = nullptr) = 0;
	virtual void VisitFiles(const ScriptFileConstVisitor& visitor, const char* szFilePath = nullptr) const = 0;
	virtual void RefreshFiles(const Schematyc2::SScriptRefreshParams& params) = 0;
	virtual bool Load() = 0;
	virtual void Save(bool bAlwaysSave = false) = 0;

	// New interface.
	//////////////////////////////////////////////////
	virtual Schematyc2::IScriptModule* AddModule(const char* szName, Schematyc2::IScriptElement* pScope = nullptr) = 0;
	virtual Schematyc2::IScriptEnumeration* AddEnumeration(const char* szName, Schematyc2::IScriptElement* pScope = nullptr) = 0;
	virtual Schematyc2::IScriptFunction* AddFunction(const char* szName, Schematyc2::IScriptElement* pScope = nullptr) = 0;
	virtual Schematyc2::IScriptClass* AddClass(const char* szName, const Schematyc2::SGUID& foundationGUID, Schematyc2::IScriptElement* pScope = nullptr) = 0;
	virtual Schematyc2::IScriptElement* GetElement(const Schematyc2::SGUID& guid) = 0;
	virtual void RemoveElement(const Schematyc2::SGUID& guid) = 0;
	virtual Schematyc2::EVisitStatus VisitElements(const Schematyc2::ScriptElementVisitor& visitor, Schematyc2::IScriptElement* pScope = nullptr, Schematyc2::EVisitFlags flags = Schematyc2::EVisitFlags::None) = 0;
	virtual Schematyc2::EVisitStatus VisitElements(const Schematyc2::ScriptElementConstVisitor& visitor, const Schematyc2::IScriptElement* pScope = nullptr, Schematyc2::EVisitFlags flags = Schematyc2::EVisitFlags::None) const = 0;
	virtual bool IsElementNameUnique(const char* szName, Schematyc2::IScriptElement* pScope = nullptr) const = 0; // #SchematycTODO : Should we also validate the name here?
	virtual Schematyc2::SScriptRegistrySignals& Signals() = 0;
	virtual Bridge::IScriptFile* Wrapfile(Schematyc2::IScriptFile* file) = 0;
};

}
