// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SchematycEntityDrsComponent.h"
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CSchematycEntityDrsComponent::Initialize()
{
	IEntity& entity = *GetEntity();
	m_pDrsEntityComp = entity.GetOrCreateComponent<IEntityDynamicResponseComponent>();
	if (!m_nameOverride.empty() || !m_globalVariableCollectionToUse.empty())  //the default DrsComponent will create a non-global drs actor, named like the entity. if this is not what we want, we need to re-init.
	{
		m_pDrsEntityComp->ReInit((m_nameOverride.empty()) ? nullptr : m_nameOverride.c_str(), (m_globalVariableCollectionToUse.empty()) ? nullptr : m_globalVariableCollectionToUse.c_str());
	}

	gEnv->pDynamicResponseSystem->GetSpeakerManager()->AddListener(this);
}

void CSchematycEntityDrsComponent::OnShutDown()
{
	gEnv->pDynamicResponseSystem->GetSpeakerManager()->RemoveListener(this);

	if (m_pDrsEntityComp)
	{
		GetEntity()->RemoveComponent(m_pDrsEntityComp);  //we assume no one else needs it anymore
		m_pDrsEntityComp = nullptr;
	}
}

void CSchematycEntityDrsComponent::ReflectType(Schematyc::CTypeDesc<CSchematycEntityDrsComponent>& desc)
{
	desc.SetGUID("25854445-cd59-4257-827d-aef984790598"_cry_guid);
	desc.SetLabel("DRS");
	desc.SetDescription("Dynamic Response System component");
	desc.SetIcon("icons:Dialogs/notification_text.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Singleton, IEntityComponent::EFlags::HideFromInspector });
	desc.AddMember(&CSchematycEntityDrsComponent::m_nameOverride, 'name', "actorNameOverride", "ActorNameOverride", "Override for the DRS actor name. If empty, entity name will be used. Remark: This name has to be unique, therefore the DRS will alter it, if there is already an actor with that name.", "");
	desc.AddMember(&CSchematycEntityDrsComponent::m_globalVariableCollectionToUse, 'glob', "globalCollectionToUse", "GlobalCollectionToUse", "Normally each actor has it`s own local variable collection, that is not accessible (via name) from the outside and is also not serialized .With this property you can change this behavior so that the actor instead uses a global collection.", "");
}

void CSchematycEntityDrsComponent::Register(Schematyc::IEnvRegistrar& registrar)
{
	Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
	{
		Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CSchematycEntityDrsComponent));
		// Functions

		//TODO: Send Signal (With context variables) (for now just hardcoded string/hash/float/int)
		//	  SetVariable (Global/Local, CreateIfNotExisting)
		//	  Idea: CreateContextVariableCollection -> SetVariable -> SendSignal (Send signal frees the collection)
		//	Idea: SetVariable outputs the usedVariableCollection, so that it can be used as an input for SendSignal

		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSchematycEntityDrsComponent::SendSignal, "3f00d849-3a9e-4fdf-b322-30ce546005ed"_cry_guid, "SendSignal");
			pFunction->SetDescription("Sends a DRS signal");
			pFunction->BindInput(1, 'sig', "SignalName");
			pFunction->BindInput(2, 'cfn', "ContextFloatName", "(optional) The name of a float-variable that can be passed along with this signal");
			pFunction->BindInput(3, 'cfv', "ContextFloatValue", "(optional) The value of the float-variable that can be passed along with this signal");
			pFunction->BindInput(4, 'csn', "ContextStringName", "(optional) The name of a (hashed) string-variable that can be passed along with this signal");
			pFunction->BindInput(5, 'csv', "ContextStringValue", "(optional) The value of the (hashed) string-variable that can be passed along with this signal");
			//pFunction->BindInput(6, 'userData', "UserData", "(optional) a string that is passed along with this signal");
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSchematycEntityDrsComponent::SetFloatVariable, "322c7c23-f4bd-4582-a55b-7ccce84aa6c1"_cry_guid, "SetFloatVariable");
			pFunction->SetDescription("Sets a float variable in a variable collection");
			pFunction->BindInput(1, 'col', "CollectionName");
			pFunction->BindInput(2, 'var', "VariableName");
			pFunction->BindInput(3, 'val', "Value");
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSchematycEntityDrsComponent::SetStringVariable, "47b46015-0bb9-4c15-bc40-50914746cfd3"_cry_guid, "SetStringVariable");
			pFunction->SetDescription("Sets a string variable in a variable collection");
			pFunction->BindInput(1, 'col', "CollectionName");
			pFunction->BindInput(2, 'var', "VariableName");
			pFunction->BindInput(3, 'val', "Value");
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSchematycEntityDrsComponent::SetIntVariable, "a0119cb0-f4c3-4c53-959b-4fe06f5d0691"_cry_guid, "SetIntVariable");
			pFunction->SetDescription("Sets a int variable in a variable collection");
			pFunction->BindInput(1, 'col', "CollectionName");
			pFunction->BindInput(2, 'var', "VariableName");
			pFunction->BindInput(3, 'val', "Value");
			componentScope.Register(pFunction);
		}
		// Signals
		{
			componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SResponseFinishedSignal));
			componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SResponseStartedSignal));
			componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SLineStartedSignal));
			componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SLineEndedSignal));
		}
	}
}

void CSchematycEntityDrsComponent::SendSignal(const Schematyc::CSharedString& signalName, const Schematyc::CSharedString& contextFloatName, float contextFloatValue, const Schematyc::CSharedString& contextStringName, const Schematyc::CSharedString& contextStringValue)
{
	DRS::IVariableCollectionSharedPtr pCollection = nullptr;
	if (!contextFloatName.empty() || !contextStringName.empty())
	{
		pCollection = gEnv->pDynamicResponseSystem->CreateContextCollection();
		if (contextFloatName.empty())
		{
			pCollection->CreateVariable(contextFloatName.c_str(), contextFloatValue);
		}
		if (contextStringName.empty())
		{
			pCollection->CreateVariable(contextStringName.c_str(), CHashedString(contextStringValue.c_str()));
		}
	}

	m_pDrsEntityComp->GetResponseActor()->QueueSignal(signalName.c_str(), pCollection, this);
}

void CSchematycEntityDrsComponent::SetFloatVariable(const Schematyc::CSharedString& collectionName, const Schematyc::CSharedString& variableName, float value)
{
	DRS::IVariableCollection* pCollection = GetVariableCollection(collectionName);

	if (pCollection)
	{
		pCollection->SetVariableValue(variableName.c_str(), value);
	}
}

void CSchematycEntityDrsComponent::SetStringVariable(const Schematyc::CSharedString& collectionName, const Schematyc::CSharedString& variableName, const Schematyc::CSharedString& value)
{
	DRS::IVariableCollection* pCollection = GetVariableCollection(collectionName);
	if (pCollection)
	{
		pCollection->SetVariableValue(variableName.c_str(), CHashedString(value.c_str()));
	}
}

void CSchematycEntityDrsComponent::SetIntVariable(const Schematyc::CSharedString& collectionName, const Schematyc::CSharedString& variableName, int value)
{
	DRS::IVariableCollection* pCollection = GetVariableCollection(collectionName);
	if (pCollection)
	{
		pCollection->SetVariableValue(variableName.c_str(), value);
	}
}

DRS::IVariableCollection* CSchematycEntityDrsComponent::GetVariableCollection(const Schematyc::CSharedString& collectionName)
{
	if (collectionName == "Local" || collectionName == "local")
		return m_pDrsEntityComp->GetLocalVariableCollection();
	else
		return gEnv->pDynamicResponseSystem->GetCollection(collectionName.c_str());
}

void CSchematycEntityDrsComponent::OnSignalProcessingStarted(SSignalInfos& signal, DRS::IResponseInstance* pStartedResponse)
{
	OutputSignal(SResponseStartedSignal { (int)signal.id });
}

void CSchematycEntityDrsComponent::OnSignalProcessingFinished(SSignalInfos& signal, DRS::IResponseInstance* pFinishedResponse, eProcessingResult outcome)
{
	OutputSignal(SResponseFinishedSignal { signal.id, outcome });
}

void CSchematycEntityDrsComponent::OnLineEvent(const DRS::IResponseActor* pSpeaker, const CHashedString& lineID, eLineEvent lineEvent, const DRS::IDialogLine* pLine)
{
	//remark: every DRS Component will currently receive events for any Speaker
	const Schematyc::CSharedString text = (pLine) ? pLine->GetText() : lineID.GetText();
	const Schematyc::CSharedString speakerName = (pSpeaker) ? pSpeaker->GetName() : "No Actor";

	if (lineEvent == DRS::ISpeakerManager::IListener::eLineEvent_HasEndedInAnyWay)
	{
		OutputSignal(SLineEndedSignal {
			text,
			speakerName,
			(lineEvent == DRS::ISpeakerManager::IListener::eLineEvent_Canceled)
		});
	}
	else if (lineEvent == DRS::ISpeakerManager::IListener::eLineEvent_Started)
	{
		OutputSignal(SLineStartedSignal {
			text,
			speakerName
		});
	}
}

void CSchematycEntityDrsComponent::SResponseStartedSignal::ReflectType(Schematyc::CTypeDesc<SResponseStartedSignal>& desc)
{
	desc.SetGUID("f01fdb01-b03f-4eab-a0af-8d2359b4547b"_cry_guid);
	desc.SetLabel("ResponseStarted");
	desc.SetDescription("Sent when a response is started.");
	desc.AddMember(&SResponseStartedSignal::m_signalId, 'id', "responseId", "ResponseId", nullptr, 0);  //actually ResponseInstanceId
}

void CSchematycEntityDrsComponent::SResponseFinishedSignal::ReflectType(Schematyc::CTypeDesc<SResponseFinishedSignal>& desc)
{
	desc.SetGUID("cece4601-9f11-4e7c-800c-222c601200fa"_cry_guid);
	desc.SetLabel("ResponseFinished");
	desc.SetDescription("Sent when a response has finished (or was not even started/existing).");
	desc.AddMember(&SResponseFinishedSignal::m_signalId, 'id', "responseId", "ResponseId", nullptr, 0);  //actually ResponseInstanceId
	desc.AddMember(&SResponseFinishedSignal::m_result, 'res', "result", "Result", nullptr, 0);
}

void CSchematycEntityDrsComponent::SLineStartedSignal::ReflectType(Schematyc::CTypeDesc<SLineStartedSignal>& desc)
{
	desc.SetGUID("e397e62c-5c7f-4fab-9195-12032f670c9f"_cry_guid);
	desc.SetLabel("LineStarted");
	desc.SetDescription("Sent when a dialog line is started.");
	desc.AddMember(&SLineStartedSignal::m_text, 'text', "text", "Text", nullptr, "");
	desc.AddMember(&SLineStartedSignal::m_speakerName, 'act', "speaker", "Speaker", nullptr, "");
}

void CSchematycEntityDrsComponent::SLineEndedSignal::ReflectType(Schematyc::CTypeDesc<SLineEndedSignal>& desc)
{
	desc.SetGUID("75e5e2ac-377f-4992-84ad-42c551f96e46"_cry_guid);
	desc.SetLabel("LineEnded");
	desc.SetDescription("Sent when a dialog line has finished/wasCanceled.");
	desc.AddMember(&SLineEndedSignal::m_text, 'text', "text", "Text", nullptr, "");
	desc.AddMember(&SLineEndedSignal::m_speakerName, 'act', "speaker", "Speaker", nullptr, "");
	desc.AddMember(&SLineEndedSignal::m_bWasCanceled, 'id', "wasCanceled", "WasCanceled", nullptr, true);
}
