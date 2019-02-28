// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/IScriptView.h>
#include <CrySchematyc/Script/IScriptElement.h>

namespace Schematyc
{
class CScriptView : public IScriptView
{
public:

	CScriptView(const CryGUID& scopeGUID);

	// IScriptView

	virtual const CryGUID&                    GetScopeGUID() const override;
	virtual const IEnvClass*                GetEnvClass() const override;
	virtual const IScriptClass*             GetScriptClass() const override;

	virtual void                            VisitEnvDataTypes(const EnvDataTypeConstVisitor& visitor) const override;
	virtual void                            VisitEnvSignals(const EnvSignalConstVisitor& visitor) const override;
	virtual void                            VisitEnvFunctions(const EnvFunctionConstVisitor& visitor) const override;
	virtual void                            VisitEnvClasses(const EnvClassConstVisitor& visitor) const override;
	virtual void                            VisitEnvInterfaces(const EnvInterfaceConstVisitor& visitor) const override;
	virtual void                            VisitEnvComponents(const EnvComponentConstVisitor& visitor) const override;
	virtual void                            VisitEnvActions(const EnvActionConstVisitor& visitor) const override;

	virtual void                            VisitScriptModules(const ScriptModuleConstVisitor& visitor) const override;
	virtual void                            VisitScriptFunctions(const ScriptFunctionConstVisitor& visitor) const override;
	virtual void                            VisitScriptClasses(const ScriptClassConstVisitor& visitor, EDomainScope scope) const override;
	//virtual void                            VisitScriptStateMachines(const ScriptStateMachineConstVisitor& visitor, EDomainScope scope) const override;
	virtual void                            VisitScriptVariables(const ScriptVariableConstVisitor& visitor, EDomainScope scope) const override;
	//virtual void                            VisitInterfaceImpls(const ScriptTimerConstVisitor& visitor, EDomainScope scope) const override;
	virtual void                            VisitScriptComponentInstances(const ScriptComponentInstanceConstVisitor& visitor, EDomainScope scope) const override;
	virtual void                            VisitScriptActionInstances(const ScriptActionInstanceConstVisitor& visitor, EDomainScope scope) const override;

	virtual void                            VisitScriptModuleFunctions(const ScriptModuleFunctionsConstVisitor& visitor) const;
	virtual void                            VisitScriptModuleVariables(const ScriptModuleVariablesConstVisitor& visitor) const;
	virtual void                            VisitScriptModuleSignals(const ScriptModuleSignalsConstVisitor& visitor) const;

	virtual void                            VisitEnclosedEnums(const ScriptEnumConstVisitor& visitor) const override;
	virtual void                            VisitAccesibleEnums(const ScriptEnumConstVisitor& visitor) const override;
	virtual void                            VisitEnclosedStructs(const ScriptStructConstVisitor& visitor) const override;
	virtual void                            VisitAccesibleStructs(const ScriptStructConstVisitor& visitor) const override;
	virtual void                            VisitEnclosedSignals(const ScriptSignalConstVisitor& visitor) const override;
	virtual void                            VisitAccesibleSignals(const ScriptSignalConstVisitor& visitor) const override;
	virtual void                            VisitEnclosedStates(const ScriptStateConstVisitor& visitor) const override;
	virtual void                            VisitAccesibleStates(const ScriptStateConstVisitor& visitor) const override;
	virtual void                            VisitEnclosedTimers(const ScriptTimerConstVisitor& visitor) const override;
	virtual void                            VisitAccesibleTimers(const ScriptTimerConstVisitor& visitor) const override;

	virtual const IScriptStateMachine*      GetScriptStateMachine(const CryGUID& guid) const override;
	virtual const IScriptComponentInstance* GetScriptComponentInstance(const CryGUID& guid) const override;
	virtual const IScriptActionInstance*    GetScriptActionInstance(const CryGUID& guid) const override;
	virtual const IScriptElement*           GetScriptElement(const CryGUID& guid) const override;

	virtual bool                            QualifyName(const IScriptComponentInstance& scriptComponentInstance, const IEnvFunction& envFunction, EDomainQualifier qualifier, IString& output) const override;
	virtual bool                            QualifyName(const IEnvInterface& envInterface, IString& output) const override;
	virtual bool                            QualifyName(const IEnvClass& envClass, IString& output) const override;
	virtual bool                            QualifyName(const IEnvInterfaceFunction& envInterfaceFunction, IString& output) const override;
	virtual bool                            QualifyName(const IEnvComponent& envComponent, IString& output) const override;
	virtual void                            QualifyName(const IEnvElement& envElement, IString& output) const override;
	virtual bool                            QualifyName(const IScriptElement& scriptElement, EDomainQualifier qualifier, IString& output) const override;

	//virtual bool                            QualifyName(const IScriptElement& scriptElement, EScriptElementAccessor accessor, IString& output) const override;

	// ~IScriptView

private:

	CryGUID m_scopeGUID;
};

typedef std::vector<const IScriptElement*> ScriptAncestors;
inline void GetScriptAncestors(const IScriptElement& scriptElement, ScriptAncestors& ancestors)
{
	ancestors.reserve(16);
	ancestors.push_back(&scriptElement);
	for (const IScriptElement* pScriptScope = scriptElement.GetParent(); pScriptScope; pScriptScope = pScriptScope->GetParent())
	{
		ancestors.push_back(pScriptScope);
	}
}

inline const IScriptElement* FindFirstCommonScriptAncestor(const IScriptElement& lhsScriptElement, const IScriptElement& rhsScriptElement)
{
	ScriptAncestors lhsScriptAncestors;
	ScriptAncestors rhsScriptAncestors;
	GetScriptAncestors(lhsScriptElement, lhsScriptAncestors);
	GetScriptAncestors(rhsScriptElement, rhsScriptAncestors);
	for (const IScriptElement* pScriptAncestor : lhsScriptAncestors)
	{
		if (std::find(rhsScriptAncestors.begin(), rhsScriptAncestors.end(), pScriptAncestor) != rhsScriptAncestors.end())
		{
			return pScriptAncestor;
		}
	}
	return nullptr;
}

inline bool QualifyScriptElementName(const IScriptElement& scriptScope, const IScriptElement& scriptElement, EDomainQualifier qualifier, IString& output)
{
	output.clear();
	switch (qualifier)
	{
	case EDomainQualifier::Global:
		{
			output.assign(scriptElement.GetName());

			for (const IScriptElement* pScriptScope = scriptElement.GetParent(); pScriptScope && (pScriptScope->GetType() != EScriptElementType::Root); pScriptScope = pScriptScope->GetParent())
			{
				output.insert(0, "::");
				output.insert(0, pScriptScope->GetName());
			}
			return true;
		}
	case EDomainQualifier::Local:
		{
			if (&scriptScope != &scriptElement)
			{
				output.assign(scriptElement.GetName());

				const IScriptElement* pFirstCommonScriptAncestor = FindFirstCommonScriptAncestor(scriptScope, scriptElement);
				for (const IScriptElement* pScriptScope = scriptElement.GetParent(); (pScriptScope != pFirstCommonScriptAncestor) && (pScriptScope->GetType() != EScriptElementType::Root); pScriptScope = pScriptScope->GetParent())
				{
					output.insert(0, "::");
					output.insert(0, pScriptScope->GetName());
				}
			}
			return true;
		}
	default:
		{
			return false;
		}
	}
}

} // Schematyc
