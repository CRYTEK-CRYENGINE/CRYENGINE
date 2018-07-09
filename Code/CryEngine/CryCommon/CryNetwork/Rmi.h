// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryNetwork/INetwork.h>
#include <CryEntitySystem/IEntityComponent.h>
#include <CryMemory/PoolAllocator.h>
#include <CryGame/IGameFramework.h>

template<size_t N>
stl::PoolAllocator<N>& GetRMIAllocator()
{
	static stl::PoolAllocator<N> allocator;
	return allocator;
}

// \cond INTERNAL
namespace CryRmi {

template<class User, class Param>
using RmiCallback = bool (User::*)(Param&&, INetChannel*);

// This cruft is a copy of CRMIAtSyncItem. It stores the deserialized params and
// postpones the RMI invocation on the remote end until TO_GAME() queue is processed.
template<class User, class Param>
class CeRMIAtSyncItem final : public INetAtSyncItem
{
public:
	bool Sync() override
	{
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_id);
		User *pUser = pEntity ? pEntity->GetComponent<User>() : nullptr;
		bool ok = pUser ? (pUser->*m_fn)(std::move(m_params), m_pChannel) : false;

		CRY_ASSERT_MESSAGE(pEntity && pUser && ok, "RMI remote at sync: failed with"
			" entity id %d. pEntity:%x, pUser:%x (guid:%x), ok:%d", m_id, pEntity,
			pUser, cryiidof<User>().hipart, ok);
		return ok;
	}

	bool SyncWithError(EDisconnectionCause& disconnectCause, string& disconnectMessage) override
	{
		return Sync();
	}
	void DeleteThis() override
	{
		this->~CeRMIAtSyncItem();
		GetRMIAllocator<sizeof(CeRMIAtSyncItem)>().Deallocate(this);
	}

public:
	CeRMIAtSyncItem(Param&& p, EntityId id, INetChannel* pChannel, RmiCallback<User, Param> fn)
		: m_params(std::move(p))
		, m_pChannel(pChannel)
		, m_fn(fn)
		, m_id(id)
	{};

	Param			m_params;
	INetChannel*	m_pChannel;
	RmiCallback<User, Param> m_fn;
	EntityId		m_id;
};

// -----------------------------------------------------------------------------
// This is a copy of IGameObject::CRMIBodyImpl, serializing the additional 
// parameter, rmi_index.
template<class T>
class CeRMIBody final : public IRMIMessageBody
{
public:
	void SerializeWith(TSerialize ser) override
	{
		// #rmi Potential b/w optimization: send exactly as many bits as needed here.
		ser.Value("rmi_index", m_rmi_index.value); // writing only.
		m_params.SerializeWith(ser);
	}

	size_t GetSize() override
	{
		return sizeof(*this);
	}

#if ENABLE_RMI_BENCHMARK
	virtual const SRMIBenchmarkParams* GetRMIBenchmarkParams() override
	{
		return NetGetRMIBenchmarkParams<T>(m_params);
	}
#endif

	static CeRMIBody* Create(const INetEntity::SRmiHandler& ermi,
		const INetEntity::SRmiIndex rmi_idx, EntityId id, const T& params,
		IRMIListener* pListener, int userId, EntityId dependentId)
	{
		return new(GetRMIAllocator<sizeof(CeRMIBody)>().Allocate())
			CeRMIBody(ermi, rmi_idx, id, params, pListener, userId, dependentId);
	}

	void DeleteThis() override
	{
		this->~CeRMIBody();
		GetRMIAllocator<sizeof(CeRMIBody)>().Deallocate(this);
	}

private:
	T m_params;
	INetEntity::SRmiIndex m_rmi_index;

	CeRMIBody(const INetEntity::SRmiHandler& ermi, const INetEntity::SRmiIndex rmi_idx,
		EntityId id, const T& params, IRMIListener* pListener, int userId, EntityId dependentId)
		: IRMIMessageBody(ermi.reliability, ermi.attach_type, id, nullptr,
			pListener, userId, dependentId)
		, m_params(params)
		, m_rmi_index(rmi_idx)
	{
	}
};

} // namespace
//! \endcond

// -----------------------------------------------------------------------------
template<class F, F>
struct SRmi;

//! This is an intermediate class for RMI support in game components.
//! It allows the game code to register remote invocations while preserving
//! the type safety of the callback.
//! \par Example
//! \include CryEntitySystem/Examples/ComponentRemoteMethodInvocation.cpp
template <class User, class Param, CryRmi::RmiCallback<User, Param> fn>
struct SRmi<CryRmi::RmiCallback<User, Param>, fn>
{
	static INetAtSyncItem* Decoder(TSerialize* ser, EntityId eid, INetChannel* pChannel)
	{
		// Deserialize and call the user here.
		Param p;
		p.SerializeWith(*ser);
		using namespace CryRmi;
		return new(GetRMIAllocator<sizeof(CeRMIAtSyncItem<User, Param>)>().Allocate())
			CeRMIAtSyncItem<User, Param>(std::move(p), eid, pChannel, fn);
	}

	static void Register(User *this_user, const ERMIAttachmentType attach,
		const bool is_server, const ENetReliabilityType reliability)
	{
		this_user->GetEntity()->GetNetEntity()->RmiRegister(
			INetEntity::SRmiHandler{ Decoder, reliability, attach });
	}

protected:
	static void Invoke(User *this_user, Param &&p, int where, int channel = -1, const EntityId dependentId = 0)
	{
		INetEntity::SRmiHandler *handler = nullptr;
		INetEntity::SRmiIndex idx = this_user->GetEntity()->GetNetEntity()->RmiByDecoder(
			Decoder, &handler);
		gEnv->pGameFramework->DoInvokeRMI(CryRmi::CeRMIBody<Param>::Create(
			*handler, idx, this_user->GetEntityId(), p, 0, 0, dependentId),
		where, channel, false);
	}

public:
	static inline void InvokeOnRemoteClients(User *this_user, Param&& p, const EntityId dependentId = 0)
	{
		CRY_ASSERT(gEnv->bServer);

		Invoke(this_user, std::forward<Param>(p), eRMI_ToRemoteClients, -1, dependentId);
	}

	static inline void InvokeOnClient(User *this_user, Param&& p, int targetClientChannelId, const EntityId dependentId = 0)
	{
		CRY_ASSERT(gEnv->bServer);

		Invoke(this_user, std::forward<Param>(p), eRMI_ToClientChannel, targetClientChannelId, dependentId);
	}

	static inline void InvokeOnOtherClients(User *this_user, Param&& p, const EntityId dependentId = 0)
	{
		Invoke(this_user, std::forward<Param>(p), eRMI_ToOtherClients, -1, dependentId);
	}

	static inline void InvokeOnAllClients(User *this_user, Param&& p, const EntityId dependentId = 0)
	{
		CRY_ASSERT(gEnv->bServer);

		Invoke(this_user, std::forward<Param>(p), eRMI_ToAllClients, -1, dependentId);
	}

	static inline void InvokeOnServer(User *this_user, Param&& p, const EntityId dependentId = 0)
	{
		Invoke(this_user, std::forward<Param>(p), eRMI_ToServer, -1, dependentId);
	}
};

#define RMI_WRAP(x) decltype(x),x