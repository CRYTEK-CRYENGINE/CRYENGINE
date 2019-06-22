// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/IAISystem.h>
#include <CryNetwork/ISerialize.h>
#include <CryMemory/PoolAllocator.h>
#include <CryAISystem/ISignal.h>

namespace AISignals
{
	class CSignalManager;

	struct AISignalExtraData final: public IAISignalExtraData
	{
	public:
		static void CleanupPool();

	public:
		AISignalExtraData();
		AISignalExtraData(const AISignalExtraData& other) : IAISignalExtraData(other), sObjectName() { SetObjectName(other.sObjectName); }
		~AISignalExtraData();

		AISignalExtraData& operator=(const AISignalExtraData& other);
		virtual void       Serialize(TSerialize ser) override;

		inline void*       operator new(size_t size)
		{
			return m_signalExtraDataAlloc.Allocate();
		}

		inline void operator delete(void* p)
		{
			return m_signalExtraDataAlloc.Deallocate(p);
		}

		virtual const char* GetObjectName() const override { return sObjectName ? sObjectName : ""; }
		virtual void        SetObjectName(const char* szObjectName) override;

		// To/from script table
		virtual void        ToScriptTable(SmartScriptTable& table) const override;
		virtual void        FromScriptTable(const SmartScriptTable& table) override;

	private:
		char*               sObjectName;

		typedef stl::PoolAllocator<sizeof(IAISignalExtraData) + sizeof(void*),
			stl::PoolAllocatorSynchronizationSinglethreaded> SignalExtraDataAlloc;
		static SignalExtraDataAlloc m_signalExtraDataAlloc;
	};

	class CSignalDescription final : public ISignalDescription
	{
		friend class CSignalManager;
	public:
		virtual const char* GetName() const override;
		virtual uint32      GetCrc() const override;
		virtual ESignalTag  GetTag() const override;

		virtual bool        IsNone() const override;
	private:
		CSignalDescription(const ESignalTag tag_, const string& name_);

		ESignalTag          m_tag;
		string              m_name;
		uint32              m_crc;
	};
	

	class CSignal final : public ISignal
	{
		friend class CSignalManager;
	public:
		virtual const int                      GetNSignal() const override;
		virtual const ISignalDescription&      GetSignalDescription() const override;
		virtual const EntityId                 GetSenderID() const override;
		virtual IAISignalExtraData*            GetExtraData() override;

		virtual void                           SetExtraData(IAISignalExtraData* val) override;
		virtual bool                           HasSameDescription(const ISignal* pOther) const override;
		virtual void                           Serialize(TSerialize ser) override;
	private:
		CSignal(const int nSignal, const ISignalDescription& signalDescription, const EntityId senderID = INVALID_ENTITYID, IAISignalExtraData* pEData = nullptr);

		// AISIGNAL_INCLUDE_DISABLED     0
		// AISIGNAL_DEFAULT              1
		// AISIGNAL_PROCESS_NEXT_UPDATE  3
		// AISIGNAL_NOTIFY_ONLY          9
		// AISIGNAL_ALLOW_DUPLICATES     10
		// AISIGNAL_RECEIVED_PREV_UPDATE 30 
		// AISIGNAL_INITIALIZATION       -100
		int                                    m_nSignal;
		const ISignalDescription*              m_pSignalDescription;
		EntityId                               m_senderID;
		IAISignalExtraData*                    m_pExtraData;
	};

	// Only class that can instantiate CSignal
	// Only class that can instantiate CSiganlDescription
	class CSignalManager final : public ISignalManager
	{
	private:
		typedef std::unique_ptr<const CSignalDescription> SignalDescriptionUniquePtr;
		typedef std::unordered_map<uint32, SignalDescriptionUniquePtr> CrcToSignalDescriptionUniquePtrMap;
		typedef std::vector<const CSignalDescription*> SignalsDescriptionsPtrVec;

#ifdef SIGNAL_MANAGER_DEBUG
		struct SSignalDebug
		{
			const CSignal    signal;
			const CTimeValue timestamp;

			SSignalDebug(const CSignal& signal_, const CTimeValue timestamp_)
				: signal(signal_)
				, timestamp(timestamp_)
			{}
		};
#endif //SIGNAL_MANAGER_DEBUG

	public:
		CSignalManager();
		~CSignalManager();

		virtual const SBuiltInSignalsDescriptions& GetBuiltInSignalDescriptions() const override;

		virtual size_t                             GetSignalDescriptionsCount() const override;
		virtual const ISignalDescription&          GetSignalDescription(const size_t index) const override;
		virtual const ISignalDescription&          GetSignalDescription(const char * szSignalDescName) const override;

		virtual const ISignalDescription&          RegisterGameSignalDescription(const char * signalName) override;
		virtual void                               DeregisterGameSignalDescription(const ISignalDescription& signalDescription) override;

#ifdef SIGNAL_MANAGER_DEBUG
		virtual SignalSharedPtr                    CreateSignal(const int nSignal, const ISignalDescription& signalDescription, const EntityId senderID = INVALID_ENTITYID, IAISignalExtraData* pEData = nullptr) override;
#else
		virtual SignalSharedPtr                    CreateSignal(const int nSignal, const ISignalDescription& signalDescription, const EntityId senderID = INVALID_ENTITYID, IAISignalExtraData* pEData = nullptr) const override;
#endif //SIGNAL_MANAGER_DEBUG

		virtual SignalSharedPtr                    CreateSignal_DEPRECATED(const int nSignal, const char* szCustomSignalTypeName, const EntityId senderID = INVALID_ENTITYID, IAISignalExtraData* pEData = nullptr) override;

#ifdef SIGNAL_MANAGER_DEBUG
		virtual void                               DebugDrawSignalsHistory() const override;
#endif //SIGNAL_MANAGER_DEBUG

	private:
		const ISignalDescription*                  RegisterSignalDescription(const ESignalTag signalTag, const char* signalDescriptionName);

		void                                       RegisterBuiltInSignalDescriptions();
		void                                       DeregisterBuiltInSignalDescriptions();

#ifdef SIGNAL_MANAGER_DEBUG
		void                                       AddSignalToDebugHistory(const CSignal& signal);
#endif //SIGNAL_MANAGER_DEBUG

		SBuiltInSignalsDescriptions                m_builtInSignals;
		CrcToSignalDescriptionUniquePtrMap         m_signalDescriptionMap;
		SignalsDescriptionsPtrVec                  m_signalDescriptionsVec;

#ifdef SIGNAL_MANAGER_DEBUG
		std::deque<SSignalDebug>                   m_debugSignalsHistory;
#endif //SIGNAL_MANAGER_DEBUG
	};
}