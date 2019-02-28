// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_RingBuffer.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_ScopedConnection.h>

#include "CrySchematyc2/GUID.h"
#include "CrySchematyc2/IAny.h"
#include "CrySchematyc2/ILib.h"
#include "CrySchematyc2/INetworkObject.h"
#include "CrySchematyc2/Deprecated/Variant.h"
#include "CrySchematyc2/Services/ITimerSystem.h"

struct SRendParams;
struct SRenderingPassInfo;

#define SCHEMATYC2_PREVIEW_ENTITY_NAME "Preview"


namespace Schematyc2
{
	class  CStack;
	struct IComponent;
	struct ILibClass;
	struct ILibClassProperties;

	DECLARE_SHARED_POINTERS(IComponent)
	DECLARE_SHARED_POINTERS(ILibClassProperties)

	typedef TemplateUtils::CDelegate<void (const SGUID& signalGUID, const TVariantConstArray&)> SignalCallback;
	// #SchematycTODO : Wrap signal callback and connection in a std::pair? It would be nice if we could be consistent.
	//typedef std::pair<SignalCallback, TemplateUtils::CScopedConnection>                         SignalCallbackConnection;
	typedef TemplateUtils::CDelegate<void (bool)>                                                 TaskCallback;
	typedef std::pair<TaskCallback, TemplateUtils::CScopedConnection>                             TaskCallbackConnection;

	enum class EObjectFlags
	{
		None                    = 0,
		NetworkReplicateActions = BIT(1),
		UpdateInEditor          = BIT(2)
	};

	DECLARE_ENUM_CLASS_FLAGS(EObjectFlags)

	struct SObjectParams
	{
		inline SObjectParams()
			: pNetworkObject(nullptr)
			, entityId(0)
			, serverAspect(0)
			, clientAspect(0)
			, flags(EObjectFlags::None)
		{}

		ILibClassConstPtr            pLibClass;
		ILibClassPropertiesConstPtr  pProperties;
		INetworkObject*              pNetworkObject;
		IObjectNetworkSpawnParamsPtr pNetworkSpawnParams;
		ExplicitEntityId             entityId;
		int32                        serverAspect;
		int32                        clientAspect;
		EObjectFlags                 flags;
	};

	struct IObjectPreviewProperties
	{
		virtual ~IObjectPreviewProperties() {}

		virtual void Serialize(Serialization::IArchive& archive) = 0;
	};

	DECLARE_SHARED_POINTERS(IObjectPreviewProperties)

	typedef TemplateUtils::CDelegate<EVisitStatus (size_t)> ObjectActiveTimerVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (size_t)> ObjectActiveActionInstanceVisitor;
	typedef TemplateUtils::CRingBuffer<SGUID, 8>            ObjectSignalHistory;

	struct IObject
	{
		virtual ~IObject() {}

		virtual void Run(ESimulationMode simulationMode) = 0;
		virtual void ConnectSignalObserver(const SGUID& signalGUID, const SignalCallback& signalCallback, TemplateUtils::CConnectionScope& connectionScope) = 0;
		virtual void DisconnectSignalObserver(const SGUID& signalGUID, const SignalCallback& signalCallback, TemplateUtils::CConnectionScope& connectionScope) = 0;
		virtual void ProcessSignal(const SGUID& signalGUID, const TVariantConstArray& inputs = TVariantConstArray()) = 0;
		virtual bool CallAbstractInterfaceFunction(const SGUID& interfaceGUID, const SGUID& functionGUID, const TVariantConstArray& inputs = TVariantConstArray(), const TVariantArray& outputs = TVariantArray()) = 0;
		virtual bool RunTask(const SGUID& stateMachineGUID, CStack& stack, const TaskCallbackConnection& callbackConnection) = 0;
		virtual void CancelTask(const SGUID& stateMachineGUID) = 0;
		
		virtual IObjectPreviewPropertiesPtr GetPreviewProperties() const = 0;
		virtual void Preview(const SRendParams& params, const SRenderingPassInfo& passInfo, const IObjectPreviewProperties& previewProperties) const = 0;

		virtual ObjectId GetObjectId() const = 0;
		virtual const ILibClass& GetLibClass() const = 0;
		virtual INetworkObject& GetNetworkObject() const = 0;
		virtual INetworkObject* GetNetworkObjectPtr() const = 0;
		virtual void GetNetworkSpawnParams(IObjectNetworkSpawnParamsPtr& pSpawnParams) const = 0;
		virtual ExplicitEntityId GetEntityId() const = 0;
		virtual IComponent* GetSingletonComponent(const SGUID& componentGUID) const = 0;

		virtual size_t GetState(size_t stateMachineIdx) const = 0;
		virtual void SetVariable(size_t variableIdx, const IAny& value) = 0; // #SchematycTODO : Pass properties with SObjectParams!
		virtual CVariant GetVariant(size_t variantIdx) const = 0;
		virtual const CVariantContainer* GetContainer(size_t containerIdx) const = 0;
		virtual IAnyConstPtr GetProperty(uint32 propertyIdx) const = 0;
		virtual TimerId GetTimerId(size_t timerIdx) const = 0;
		virtual IComponentPtr GetComponentInstance(size_t componentInstanceIdx) = 0;
		virtual IPropertiesPtr GetComponentInstanceProperties(size_t componentInstanceIdx) = 0;
		virtual void VisitActiveTimers(const ObjectActiveTimerVisitor& visitor) const = 0;
		virtual void VisitActiveActionInstances(const ObjectActiveActionInstanceVisitor& visitor) const = 0;
		virtual const ObjectSignalHistory& GetSignalHistory() const = 0;
	};
}
