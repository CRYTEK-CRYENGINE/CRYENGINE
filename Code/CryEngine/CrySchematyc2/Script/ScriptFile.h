// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Script/IScriptFile.h>

#include "Script/ScriptUserDocumentation.h"
#include "Script/ScriptVariableDeclaration.h"
#include "Script/Elements/ScriptAbstractInterface.h"
#include "Script/Elements/ScriptAbstractInterfaceFunction.h"
#include "Script/Elements/ScriptAbstractInterfaceImplementation.h"
#include "Script/Elements/ScriptAbstractInterfaceTask.h"
#include "Script/Elements/ScriptActionInstance.h"
#include "Script/Elements/ScriptClass.h"
#include "Script/Elements/ScriptClassBase.h"
#include "Script/Elements/ScriptComponentInstance.h"
#include "Script/Elements/ScriptContainer.h"
#include "Script/Elements/ScriptEnumeration.h"
#include "Script/Elements/ScriptGroup.h"
#include "Script/Elements/ScriptInclude.h"
#include "Script/Elements/ScriptProperty.h"
#include "Script/Elements/ScriptSignal.h"
#include "Script/Elements/ScriptState.h"
#include "Script/Elements/ScriptStateMachine.h"
#include "Script/Elements/ScriptStructure.h"
#include "Script/Elements/ScriptTimer.h"
#include "Script/Elements/ScriptVariable.h"

namespace Schematyc2
{
	class CScriptRoot;

	DECLARE_SHARED_POINTERS(IScriptElement)

	class CScriptFile : public IScriptFile
	{
	private:

		enum class EElementOrigin
		{
			File,
			Clipboard
		};

		typedef std::unordered_map<SGUID, IScriptElementPtr> Elements;
		typedef std::multimap<SGUID, IScriptElement*>        ElementsScopeGuidMap;
		typedef std::map<string, ElementsScopeGuidMap>       ElementsToSave;

		struct SNewElement
		{
			SNewElement(const IScriptElementPtr& _pElement, const XmlNodeRef& _xml);

			IScriptElementPtr pElement;
			XmlNodeRef        xml;
		};

		typedef std::vector<SNewElement> NewElements;

	public:

		CScriptFile(const char* szFileName, const SGUID& guid, EScriptFileFlags flags);

		// IScriptFile
		virtual const char* GetFileName() const override;
		virtual SGUID GetGUID() const override;
		virtual void SetFlags(EScriptFileFlags flags) override;
		virtual EScriptFileFlags GetFlags() const override;
		virtual IScriptInclude* AddInclude(const SGUID& scopeGUID, const char* szFileName, const SGUID& refGUID) override;
		virtual IScriptInclude* GetInclude(const SGUID& guid) override;
		virtual const IScriptInclude* GetInclude(const SGUID& guid) const override;
		virtual EVisitStatus VisitIncludes(const ScriptIncludeVisitor& visitor) override;
		virtual EVisitStatus VisitIncludes(const ScriptIncludeVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitIncludes(const ScriptIncludeConstVisitor& visitor) const override;
		virtual EVisitStatus VisitIncludes(const ScriptIncludeConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptGroup* AddGroup(const SGUID& scopeGUID, const char* szName) override;
		virtual IScriptGroup* GetGroup(const SGUID& guid) override;
		virtual const IScriptGroup* GetGroup(const SGUID& guid) const override;
		virtual EVisitStatus VisitGroups(const ScriptGroupVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitGroups(const ScriptGroupConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptEnumeration* AddEnumeration(const SGUID& scopeGUID, const char* szName) override;
		virtual IScriptEnumeration* GetEnumeration(const SGUID& guid) override;
		virtual const IScriptEnumeration* GetEnumeration(const SGUID& guid) const override;
		virtual EVisitStatus VisitEnumerations(const ScriptEnumerationVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitEnumerations(const ScriptEnumerationConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptStructure* AddStructure(const SGUID& scopeGUID, const char* szName) override;
		virtual IScriptStructure* GetStructure(const SGUID& guid) override;
		virtual const IScriptStructure* GetStructure(const SGUID& guid) const override;
		virtual EVisitStatus VisitStructures(const ScriptStructureVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitStructures(const ScriptStructureConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptSignal* AddSignal(const SGUID& scopeGUID, const char* szName) override;
		virtual IScriptSignal* GetSignal(const SGUID& guid) override;
		virtual const IScriptSignal* GetSignal(const SGUID& guid) const override;
		virtual EVisitStatus VisitSignals(const ScriptSignalVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitSignals(const ScriptSignalConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptAbstractInterface* AddAbstractInterface(const SGUID& scopeGUID, const char* szName) override;
		virtual IScriptAbstractInterface* GetAbstractInterface(const SGUID& guid) override;
		virtual const IScriptAbstractInterface* GetAbstractInterface(const SGUID& guid) const override;
		virtual EVisitStatus VisitAbstractInterfaces(const ScriptAbstractInterfaceVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitAbstractInterfaces(const ScriptAbstractInterfaceConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptAbstractInterfaceFunction* AddAbstractInterfaceFunction(const SGUID& scopeGUID, const char* szName) override;
		virtual IScriptAbstractInterfaceFunction* GetAbstractInterfaceFunction(const SGUID& guid) override;
		virtual const IScriptAbstractInterfaceFunction* GetAbstractInterfaceFunction(const SGUID& guid) const override;
		virtual EVisitStatus VisitAbstractInterfaceFunctions(const ScriptAbstractInterfaceFunctionVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitAbstractInterfaceFunctions(const ScriptAbstractInterfaceFunctionConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptAbstractInterfaceTask* AddAbstractInterfaceTask(const SGUID& scopeGUID, const char* szName) override;
		virtual IScriptAbstractInterfaceTask* GetAbstractInterfaceTask(const SGUID& guid) override;
		virtual const IScriptAbstractInterfaceTask* GetAbstractInterfaceTask(const SGUID& guid) const override;
		virtual EVisitStatus VisitAbstractInterfaceTasks(const ScriptAbstractInterfaceTaskVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitAbstractInterfaceTasks(const ScriptAbstractInterfaceTaskConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptClass* AddClass(const SGUID& scopeGUID, const char* szName, const SGUID& foundationGUID) override;
		virtual IScriptClass* GetClass(const SGUID& guid) override;
		virtual const IScriptClass* GetClass(const SGUID& guid) const override;
		virtual EVisitStatus VisitClasses(const ScriptClassVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitClasses(const ScriptClassConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptClassBase* AddClassBase(const SGUID& scopeGUID, const SGUID& refGUID) override;
		virtual IScriptClassBase* GetClassBase(const SGUID& guid) override;
		virtual const IScriptClassBase* GetClassBase(const SGUID& guid) const override;
		virtual EVisitStatus VisitClassBases(const ScriptClassBaseVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitClassBases(const ScriptClassBaseConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptStateMachine* AddStateMachine(const SGUID& scopeGUID, const char* szName, EScriptStateMachineLifetime lifetime, const SGUID& contextGUID, const SGUID& partnerGUID) override;
		virtual IScriptStateMachine* GetStateMachine(const SGUID& guid) override;
		virtual const IScriptStateMachine* GetStateMachine(const SGUID& guid) const override;
		virtual EVisitStatus VisitStateMachines(const ScriptStateMachineVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitStateMachines(const ScriptStateMachineConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptState* AddState(const SGUID& scopeGUID, const char* szName, const SGUID& partnerGUID) override;
		virtual IScriptState* GetState(const SGUID& guid) override;
		virtual const IScriptState* GetState(const SGUID& guid) const override;
		virtual EVisitStatus VisitStates(const ScriptStateVisitor& visitor) override;
		virtual EVisitStatus VisitStates(const ScriptStateVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitStates(const ScriptStateConstVisitor& visitor) const override;
		virtual EVisitStatus VisitStates(const ScriptStateConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptVariable* AddVariable(const SGUID& scopeGUID, const char* szName, const CAggregateTypeId& typeId) override;
		virtual IScriptVariable* GetVariable(const SGUID& guid) override;
		virtual const IScriptVariable* GetVariable(const SGUID& guid) const override;
		virtual EVisitStatus VisitVariables(const ScriptVariableVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitVariables(const ScriptVariableConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptProperty* AddProperty(const SGUID& scopeGUID, const char* szName, const SGUID& refGUID, const CAggregateTypeId& typeId) override;
		virtual IScriptProperty* GetProperty(const SGUID& guid) override;
		virtual const IScriptProperty* GetProperty(const SGUID& guid) const override;
		virtual EVisitStatus VisitProperties(const ScriptPropertyVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitProperties(const ScriptPropertyConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptContainer* AddContainer(const SGUID& scopeGUID, const char* szName, const SGUID& typeGUID) override;
		virtual IScriptContainer* GetContainer(const SGUID& guid) override;
		virtual const IScriptContainer* GetContainer(const SGUID& guid) const override;
		virtual EVisitStatus VisitContainers(const ScriptContainerVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitContainers(const ScriptContainerConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptTimer* AddTimer(const SGUID& scopeGUID, const char* szName) override;
		virtual IScriptTimer* GetTimer(const SGUID& guid) override;
		virtual const IScriptTimer* GetTimer(const SGUID& guid) const override;
		virtual EVisitStatus VisitTimers(const ScriptTimerVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitTimers(const ScriptTimerConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptAbstractInterfaceImplementation* AddAbstractInterfaceImplementation(const SGUID& scopeGUID, EDomain domain, const SGUID& refGUID) override;
		virtual IScriptAbstractInterfaceImplementation* GetAbstractInterfaceImplementation(const SGUID& guid) override;
		virtual const IScriptAbstractInterfaceImplementation* GetAbstractInterfaceImplementation(const SGUID& guid) const override;
		virtual EVisitStatus VisitAbstractInterfaceImplementations(const ScriptAbstractInterfaceImplementationVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitAbstractInterfaceImplementations(const ScriptAbstractInterfaceImplementationConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptComponentInstance* AddComponentInstance(const SGUID& scopeGUID, const char* szName, const SGUID& componentGUID, EScriptComponentInstanceFlags flags) override;
		virtual IScriptComponentInstance* GetComponentInstance(const SGUID& guid) override;
		virtual const IScriptComponentInstance* GetComponentInstance(const SGUID& guid) const override;
		virtual EVisitStatus VisitComponentInstances(const ScriptComponentInstanceVisitor& visitor) override;
		virtual EVisitStatus VisitComponentInstances(const ScriptComponentInstanceVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitComponentInstances(const ScriptComponentInstanceConstVisitor& visitor) const override;
		virtual EVisitStatus VisitComponentInstances(const ScriptComponentInstanceConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IScriptActionInstance* AddActionInstance(const SGUID& scopeGUID, const char* szName, const SGUID& actionGUID, const SGUID& componentInstanceGUID) override;
		virtual IScriptActionInstance* GetActionInstance(const SGUID& guid) override;
		virtual const IScriptActionInstance* GetActionInstance(const SGUID& guid) const override;
		virtual EVisitStatus VisitActionInstances(const ScriptActionInstanceVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitActionInstances(const ScriptActionInstanceConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual IDocGraph* AddGraph(const SScriptGraphParams& params) override;
		virtual IDocGraph* GetGraph(const SGUID& guid) override;
		virtual const IDocGraph* GetGraph(const SGUID& guid) const override;
		virtual EVisitStatus VisitGraphs(const DocGraphVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitGraphs(const DocGraphConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual void RemoveElement(const SGUID& guid, bool clearScope) override;
		virtual IScriptElement* GetElement(const SGUID& guid) override;
		virtual const IScriptElement* GetElement(const SGUID& guid) const override;
		virtual IScriptElement* GetElement(const SGUID& guid, EScriptElementType elementType) override;
		virtual const IScriptElement* GetElement(const SGUID& guid, EScriptElementType elementType) const override;
		virtual EVisitStatus VisitElements(const ScriptElementVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) override;
		virtual EVisitStatus VisitElements(const ScriptElementConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const override;
		virtual bool IsElementNameUnique(const SGUID& scopeGUID, const char* szName) const override; // #SchematycTODO : Move to separate utils class!!!
		virtual XmlNodeRef CopyElementsToXml(const SGUID& guid, bool bRecurseHierarchy) const override;
		virtual void PasteElementsFromXml(const SGUID& scopeGUID, const XmlNodeRef& xml) override;
		virtual void Load() override;
		virtual void Save() override;
		virtual void Refresh(const SScriptRefreshParams& params) override;
		virtual bool GetClipboardInfo(const XmlNodeRef& xml, SScriptElementClipboardInfo& clipboardInfo) const override; // #SchematycTODO : Move to separate utils class!!!
		// ~IScriptFile

	private:

		void AttachElement(IScriptElement& element, const SGUID& scopeGUID);
		template <typename TYPE> TYPE* GetElement(EScriptElementType type, const SGUID& guid);
		template <typename TYPE> const TYPE* GetElement(EScriptElementType type, const SGUID& guid) const;
		EVisitStatus VisitElements(const ScriptElementVisitor& visitor, IScriptElement& parentElement, bool bRecurseHierarchy);
		template <typename TYPE> EVisitStatus VisitElements(EScriptElementType type, const TemplateUtils::CDelegate<EVisitStatus (TYPE&)>& visitor);
		template <typename TYPE> EVisitStatus VisitElements(EScriptElementType type, const TemplateUtils::CDelegate<EVisitStatus (TYPE&)>& visitor, IScriptElement& parentElement, bool bRecurseHierarchy);
		EVisitStatus VisitElements(const ScriptElementConstVisitor& visitor, const IScriptElement& parentElement, bool bRecurseHierarchy) const;
		template <typename TYPE> EVisitStatus VisitElements(EScriptElementType type, const TemplateUtils::CDelegate<EVisitStatus (const TYPE&)>& visitor) const;
		template <typename TYPE> EVisitStatus VisitElements(EScriptElementType type, const TemplateUtils::CDelegate<EVisitStatus (const TYPE&)>& visitor, const IScriptElement& parentElement, bool bRecurseHierarchy) const;
		void CopyElementsToXmlRecursive(const IScriptElement& element, const XmlNodeRef& xmlElements, bool bRecurseHierarchy) const;
		void CreateElementsFromXml(const SGUID& scopeGUID, const XmlNodeRef& xml, EElementOrigin origin);
		void GatherElementsToSave(EScriptElementType type, ElementsToSave& elementsToSave);
		XmlNodeRef SaveElements(EScriptElementType type, const char* szTag);
		XmlNodeRef SaveGraphs();

		void MakeElementNameUnique(const SGUID& scopeGUID, stack_string& name) const; // #SchematycTODO : Move to separate utils class!!!
		const char* GetElementTypeName(EScriptElementType elementType) const; // #SchematycTODO : Move to separate utils class!!!
		EScriptElementType GetElementType(const char* szElementTypeName) const; // #SchematycTODO : Move to separate utils class!!!
		void ClearScope(const SGUID& scopeGUID);

		string                       m_fileName;
		SGUID                        m_guid;
		EScriptFileFlags             m_flags;
		Elements                     m_elements;
		std::unique_ptr<CScriptRoot> m_pRoot;
	};

	DECLARE_SHARED_POINTERS(CScriptFile)
}
