// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/IObject.h>
#include <CrySchematyc2/IObjectManager.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Services/IUpdateScheduler.h>
#include <CrySchematyc2/Utils/StringUtils.h>

#include "Lib.h"

namespace Schematyc2
{
	struct IAction;
	struct IActionFactory;
	struct IComponent;
	struct IComponentFactory;

	// Object.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CObject : public IObject
	{
	public:

		CObject(const ObjectId& objectId, const SObjectParams& params);

		~CObject();

		// IObject

		virtual void Run(ESimulationMode simulationMode) override;
		virtual void ConnectSignalObserver(const SGUID& signalGUID, const SignalCallback& signalCallback, TemplateUtils::CConnectionScope& connectionScope) override;
		virtual void DisconnectSignalObserver(const SGUID& signalGUID, const SignalCallback& signalCallback, TemplateUtils::CConnectionScope& connectionScope) override;
		virtual void ProcessSignal(const SGUID& signalGUID, const TVariantConstArray& inputs = TVariantConstArray()) override;
		virtual bool CallAbstractInterfaceFunction(const SGUID& interfaceGUID, const SGUID& functionGUID, const TVariantConstArray& inputs = TVariantConstArray(), const TVariantArray& outputs = TVariantArray()) override;
		virtual bool RunTask(const SGUID& stateMachineGUID, CStack& stack, const TaskCallbackConnection& callbackConnection) override;
		virtual void CancelTask(const SGUID& stateMachineGUID) override;

		virtual IObjectPreviewPropertiesPtr GetPreviewProperties() const override;
		virtual void Preview(const SRendParams& params, const SRenderingPassInfo& passInfo, const IObjectPreviewProperties& previewProperties) const override;

		virtual ObjectId GetObjectId() const override;
		virtual const ILibClass& GetLibClass() const override;
		virtual INetworkObject& GetNetworkObject() const override;
		virtual INetworkObject* GetNetworkObjectPtr() const override;
		virtual void GetNetworkSpawnParams(IObjectNetworkSpawnParamsPtr& pSpawnParams) const override;
		virtual ExplicitEntityId GetEntityId() const override;
		virtual IComponent* GetSingletonComponent(const SGUID& componentGUID) const override;

		virtual size_t GetState(size_t iStateMachine) const override;
		virtual void SetVariable(size_t iVariable, const IAny& value) override;
		virtual CVariant GetVariant(size_t iVariant) const override;
		virtual const CVariantContainer* GetContainer(size_t iContainer) const override;
		virtual IAnyConstPtr GetProperty(uint32 propertyIdx) const override;
		virtual TimerId GetTimerId(size_t iTimer) const override;
		virtual IComponentPtr GetComponentInstance(size_t componentInstanceIdx) override;
		virtual IPropertiesPtr GetComponentInstanceProperties(size_t componentInstanceIdx) override;
		virtual void VisitActiveTimers(const ObjectActiveTimerVisitor& visitor) const override;
		virtual void VisitActiveActionInstances(const ObjectActiveActionInstanceVisitor& visitor) const override;
		virtual const ObjectSignalHistory& GetSignalHistory() const override;

		// ~IObject

	private:

		struct SStateMachine
		{
			SStateMachine();

			size_t                 iCurrentState;
			size_t                 iRequestedState;
			TaskCallbackConnection taskCallbackConnection;
		};

		struct STimer
		{
			STimer(CObject* _pObject, const SGUID& _guid);

			void OnTimer();

			CObject* pObject;
			SGUID    guid;
			TimerId  timerId;
		};

		struct SComponentInstance
		{
			SComponentInstance(const IPropertiesPtr& _pProperties, const IComponentPtr& _pComponent);

			IPropertiesPtr pProperties;
			IComponentPtr  pComponent;
		};

		typedef std::vector<SComponentInstance> ComponentInstances;

		struct SActionInstance
		{
			SActionInstance(const IActionPtr& _pAction, EActionFlags _flags, bool _active);

			IActionPtr   pAction;
			EActionFlags flags;
			bool         active;
		};

		typedef std::vector<SStateMachine>			TStateMachineVector;
		typedef std::vector<CVariantContainer>	TContainerVector;
		typedef std::vector<STimer>							TTimerVector;
		typedef std::vector<SActionInstance>		TActionInstanceVector;

		struct SEvent
		{
			enum EType
			{
				NONE = 0,
				SIGNAL,
				REQUEST_STATE,
				FAILED_STATE,
				RELEASE_STATE,
				ENTERING_STATE,
				EXITING_STATE
			};

			SEvent();

			SEvent(EType _type, const SGUID& _guid);

			EType	type;
			SGUID	guid;
		};

		struct SPreviewProperties : public IObjectPreviewProperties
		{
			struct SComponentPreviewProperties
			{
				SComponentPreviewProperties(const char* _componentName, const IComponentPreviewPropertiesPtr& _pData);

				string                         componentName;
				IComponentPreviewPropertiesPtr pData;
			};

			typedef std::vector<SComponentPreviewProperties> ComponentPreviewProperties;

			SPreviewProperties(const ILibClass& libClass, const ComponentInstances& componentInstances);

			// IObjectPreviewProperties

			virtual void Serialize(Serialization::IArchive& archive) override;

			// ~IObjectPreviewProperties
			
			ComponentPreviewProperties componentPreviewProperties;
		};

		typedef std::pair<SGUID, SignalCallback>												TSignalObserver;
		typedef TemplateUtils::CScopedConnectionArray<TSignalObserver>	TSignalObserverConnectionArray; // #SchematycTODO : Replace with CScopedConnectionManager!

		struct SSignalObserverConnectionProcessor
		{
			inline SSignalObserverConnectionProcessor(const SGUID& _signalGUID, const TVariantConstArray& _inputs)
				: signalGUID(_signalGUID)
				, inputs(_inputs)
			{}

			inline void operator () (const TSignalObserver& observer) const
			{
				if(observer.first == signalGUID)
				{
					observer.second(signalGUID, inputs);
				}
			}

			SGUID								signalGUID;
			TVariantConstArray	inputs;
		};

		class CStateNetIdxMapper
		{
		public:
			typedef int32 StateNetIdx;
			enum : StateNetIdx { INVALID_NET_IDX = -1 };
		private:
			typedef std::vector<StateNetIdx> TStateIndexVector;
			// TODO pavloi 2017.11.03: pedantically, second value should be size_t instead of int32, but this way whole element is 16 bytes
			typedef VectorMap<std::pair<size_t, StateNetIdx>, int32> TReverseStateIndexMap;

		public:
			void Init(const ILibClass* pLibClass, const TActionInstanceVector& actionInstances);

			StateNetIdx GetStateCount(size_t iStateMachine) const;
			StateNetIdx Encode(size_t iState) const;
			size_t Decode(size_t iStateMachine, StateNetIdx stateNetIdx) const;

		private:
			TStateIndexVector m_stateIndexMapping;
			TReverseStateIndexMap m_reverseStateIndexMapping;
			TStateIndexVector m_stateMachineCounts;
		};

		void InitProperties(const ILibClassProperties& properties);
		void CreateAndInitComponents(const IObjectNetworkSpawnParamsPtr& pNetworkSpawnParams);
		void CreateAndInitActions();

		int32 GetNetworkAspect() const;
		void MarkAspectDirtyForStateMachine(size_t iStateMachine);
		bool HaveNetworkAuthority() const;
		bool EvaluateTransitions(size_t iStateMachine, size_t iState, const SEvent& event, const TVariantConstArray& inputs);
		void ChangeState(size_t iStateMachine, size_t iNewState, const SEvent& event, const TVariantConstArray& inputs);
		void EnterState(size_t iStateMachine, size_t iState, const TVariantConstArray& inputs);
		void ExitState(size_t iStateMachine, size_t iState, const SEvent& event, const TVariantConstArray& inputs);
		void StartTimer(const SGUID& guid);
		void StopTimer(const SGUID& guid);
		void ResetTimer(const SGUID& guid);
		void InitStateMachineVariables(size_t iStateMachine, CStack& stack);
		void ProcessFunction(const LibFunctionId& functionId, const TVariantConstArray& inputs, const TVariantArray& outputs);
		void NetworkSerialize(TSerialize serialize, int32 aspects, uint8 profile, int flags);

		ObjectId												m_objectId;
		ILibClassConstPtr								m_pLibClass;
		INetworkObject*									m_pNetworkObject;
		ExplicitEntityId								m_entityId;
		int32														m_serverAspect;
		int32														m_clientAspect;
		EObjectFlags										m_flags;
		ESimulationMode									m_simulationMode;
		TStateMachineVector							m_stateMachines;
		TVariantVector									m_variants;
		TContainerVector								m_containers;
		TTimerVector										m_timers;
		ComponentInstances              m_componentInstances;
		TActionInstanceVector						m_actionInstances;
		ObjectSignalHistory							m_signalHistory;
		TSignalObserverConnectionArray	m_signalObservers;
		TemplateUtils::CConnectionScope	m_connectionScope;
		CStateNetIdxMapper									m_stateNetIdxMapper;
	};
}
