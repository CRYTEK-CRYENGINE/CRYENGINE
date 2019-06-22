// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GamePlatform.h"

#include "User.h"

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

namespace Cry
{
	namespace GamePlatform
	{
		namespace Predicate
		{
			struct SWithServiceId
			{
				explicit SWithServiceId(const ServiceIdentifier& svcId) : m_svcId(svcId) {}
				bool operator()(const IService* pService) const { return pService && pService->GetServiceIdentifier() == m_svcId; }
				bool operator()(const IAccount* pAccount) const { return pAccount->GetServiceIdentifier() == m_svcId; }

			private:
				ServiceIdentifier m_svcId;
			};

			struct SWithAccount
			{
				explicit SWithAccount(const IAccount& acc) : m_acc(acc) {}
				bool operator()(const std::unique_ptr<CUser>& pUser) const { return pUser && pUser->HasAccount(m_acc); }

			private:
				const IAccount& m_acc;
			};
		}

		CPlugin::CPlugin()
		{
			// First element is reserved for main service
			m_services.emplace_back(nullptr);
			// First element is reserved for local user
			m_users.emplace_back(nullptr);
		}

		CPlugin::~CPlugin()
		{
			gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
		}

		bool CPlugin::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
		{
			gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "GamePlatform");

			return true;
		}

		void CPlugin::MainUpdate(float frameTime)
		{
			CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
		}

		IUser* CPlugin::GetLocalClient() const
		{
			if (m_users[0] == nullptr)
			{
				DynArray<IAccount*> userAccounts;
				for (IService* pSvc : m_services)
				{
					IAccount* pSvcLocalAccount = pSvc ? pSvc->GetLocalAccount() : nullptr;

					if (pSvcLocalAccount)
					{
						userAccounts.push_back(pSvcLocalAccount);
					}
				}

				CollectConnectedAccounts(userAccounts);

				m_users[0] = stl::make_unique<CUser>(userAccounts);
			}

			return m_users[0].get();
		}

		IUser* CPlugin::GetUserById(const UserIdentifier& id) const
		{
			return TryGetUser(id);
		}

		IUser* CPlugin::GetUserById(const AccountIdentifier& accountId) const
		{
			return TryGetUser(accountId);
		}

		const DynArray<IUser*>& CPlugin::GetFriends() const
		{
			m_friends.clear();

			if (const IService* pMainService = GetMainService())
			{
				const DynArray<IAccount*>& accounts = pMainService->GetFriendAccounts();
				for (IAccount* pAccount : accounts)
				{
					stl::push_back_unique(m_friends, TryGetUser(*pAccount));
				}
			}

			return m_friends;
		}

#if CRY_GAMEPLATFORM_EXPERIMENTAL
		const DynArray<IUser*>& CPlugin::GetBlockedUsers() const
		{
			m_blockedUsers.clear();

			if (const IService* pMainService = GetMainService())
			{
				const DynArray<IAccount*>& accounts = pMainService->GetBlockedAccounts();
				for (IAccount* pAccount : accounts)
				{
					stl::push_back_unique(m_blockedUsers, TryGetUser(*pAccount));
				}
			}

			return m_blockedUsers;
		}

		const DynArray<IUser*>& CPlugin::GetMutedUsers() const
		{
			m_mutedUsers.clear();

			if (const IService* pMainService = GetMainService())
			{
				const DynArray<IAccount*>& accounts = pMainService->GetMutedAccounts();
				for (IAccount* pAccount : accounts)
				{
					stl::push_back_unique(m_mutedUsers, TryGetUser(*pAccount));
				}
			}

			return m_mutedUsers;
		}
#endif // CRY_GAMEPLATFORM_EXPERIMENTAL

		IServer* CPlugin::CreateServer(bool bLocal)
		{
			if (IService* pMainService = GetMainService())
			{
				return pMainService->CreateServer(bLocal);
			}

			return nullptr;
		}

		IServer* CPlugin::GetLocalServer() const
		{
			if (const IService* pMainService = GetMainService())
			{
				return pMainService->GetLocalServer();
			}

			return nullptr;
		}

		ILeaderboards* CPlugin::GetLeaderboards() const
		{
			if (const IService* pMainService = GetMainService())
			{
				return pMainService->GetLeaderboards();
			}

			return nullptr;
		}

		IStatistics* CPlugin::GetStatistics() const
		{
			if (const IService* pMainService = GetMainService())
			{
				return pMainService->GetStatistics();
			}

			return nullptr;
		}

		IRemoteStorage* CPlugin::GetRemoteStorage() const
		{
			if (const IService* pMainService = GetMainService())
			{
				return pMainService->GetRemoteStorage();
			}

			return nullptr;
		}

		IMatchmaking* CPlugin::GetMatchmaking() const
		{
			if (const IService* pMainService = GetMainService())
			{
				return pMainService->GetMatchmaking();
			}

			return nullptr;
		}

		INetworking* CPlugin::GetNetworking() const
		{
			if (const IService* pMainService = GetMainService())
			{
				return pMainService->GetNetworking();
			}

			return nullptr;
		}

		IUser* CPlugin::TryGetUser(const UserIdentifier& id) const
		{
			if (m_users[0] == nullptr)
			{
				return GetLocalClient();
			}

			for (const std::unique_ptr<CUser>& pUser : m_users)
			{
				if (pUser->GetIdentifier() == id)
				{
					return pUser.get();
				}
			}

			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[GamePlatform::TryGetUser] no user '%s' was found", id.ToDebugString());

			return nullptr;
		}

		IUser* CPlugin::TryGetUser(const AccountIdentifier& id) const
		{
			const ServiceIdentifier& svcId = id.Service();
			
			for (const std::unique_ptr<CUser>& pUser : m_users)
			{
				if (const IAccount* pAccount = pUser->GetAccount(svcId))
				{
					if (pAccount->GetIdentifier() == id)
					{
						return pUser.get();
					}
				}
			}

			if (IAccount* pAccount = GetAccount(id))
			{
				return AddUser(*pAccount);
			}

			return nullptr;
		}

		IUser* CPlugin::TryGetUser(IAccount& account) const
		{
			const ServiceIdentifier& svcId = account.GetServiceIdentifier();
			for (const std::unique_ptr<CUser>& pUser : m_users)
			{
				if (pUser->GetAccount(svcId) == &account)
				{
					return pUser.get();
				}
			}

			return AddUser(account);
		}

		IUser* CPlugin::AddUser(IAccount& account) const
		{
			DynArray<IAccount*> userAccounts;
			userAccounts.push_back(&account);

			CollectConnectedAccounts(userAccounts);
			EnsureMainAccountFirst(userAccounts);
			
			m_users.emplace_back(stl::make_unique<CUser>(userAccounts));

			return m_users.back().get();
		}

		void CPlugin::CollectConnectedAccounts(DynArray<IAccount*>& userAccounts) const
		{
			for (size_t idx = 0; idx < userAccounts.size(); ++idx)
			{
				// This call can increase the size of userAccounts, thus it is very important to use
				// index-based looping and not caching the DynArray's size
				AddAccountConnections(*userAccounts[idx], userAccounts);
			}
		}

		void CPlugin::AddAccountConnections(const IAccount& account, DynArray<IAccount*>& userAccounts) const
		{
			const DynArray<AccountIdentifier>& connAccIds = account.GetConnectedAccounts();
			for (const AccountIdentifier& accId : connAccIds)
			{
				if (IAccount* pAccount = GetAccount(accId))
				{
					stl::push_back_unique(userAccounts, pAccount);
				}
			}
		}

		DynArray<IAccount*>::iterator CPlugin::FindMainAccount(DynArray<IAccount*>& userAccounts) const
		{
			return std::find_if(userAccounts.begin(), userAccounts.end(), Predicate::SWithServiceId(GetMainServiceIdentifier()));
		}

		void CPlugin::EnsureMainAccountFirst(DynArray<IAccount*>& userAccounts) const
		{
			if (userAccounts.empty())
			{
				return;
			}

			if (userAccounts[0]->GetServiceIdentifier() != GetMainServiceIdentifier())
			{
				DynArray<IAccount*>::iterator mainAccPos = FindMainAccount(userAccounts);
				if (mainAccPos != userAccounts.end() && mainAccPos != userAccounts.begin())
				{
					// Sebastien recommended me to add the using to support user-customized swap()
					using std::swap;
					swap(userAccounts[0], *mainAccPos);
				}
			}
		}

		void CPlugin::AddOrUpdateUser(DynArray<IAccount*> userAccounts)
		{
			EnsureMainAccountFirst(userAccounts);
			
			IAccount* pMainAcc = userAccounts.empty() ? nullptr : userAccounts[0];
			
			if(pMainAcc)
			{
				auto userPos = std::find_if(m_users.begin(), m_users.end(), Predicate::SWithAccount(*pMainAcc));
				if (userPos != m_users.end())
				{
					(*userPos)->SetAccounts(userAccounts);
				}
				else
				{
					AddUser(*pMainAcc);
				}
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[GamePlatform::AddOrUpdateUser] no suitable account found.");
			}
		}

		IAccount* CPlugin::GetAccount(const AccountIdentifier& id) const
		{
			if (IService* pSvc = GetService(id.Service()))
			{
				return pSvc->GetAccountById(id);
			}

			return nullptr;
		}
		
		IAccount* CPlugin::GetMainLocalAccount() const
		{
			if (IService* pMainSvc = GetMainService())
			{
				return pMainSvc->GetLocalAccount();
			}

			return nullptr;
		}

		bool CPlugin::GetAuthToken(string &tokenOut, int &issuerId)
		{
			if (IService* pMainService = GetMainService())
			{
				return pMainService->GetAuthToken(tokenOut, issuerId);
			}

			return false;
		}

		EConnectionStatus CPlugin::GetConnectionStatus() const
		{
			if (const IService* pMainService = GetMainService())
			{
				return pMainService->GetConnectionStatus();
			}

			return EConnectionStatus::Disconnected;
		}

		void CPlugin::OnShutdown(const ServiceIdentifier& serviceId)
		{
			UnregisterService(serviceId);
		}

		void CPlugin::OnAccountAdded(IAccount& account)
		{
			DynArray<IAccount*> userAccounts;
			userAccounts.push_back(&account);
			CollectConnectedAccounts(userAccounts);

			AddOrUpdateUser(std::move(userAccounts));
		}

		void CPlugin::OnAccountRemoved(IAccount& account)
		{
			auto usrPos = std::find_if(m_users.begin(), m_users.end(), Predicate::SWithAccount(account));
			if (usrPos != m_users.end())
			{
				CUser* const pUser = usrPos->get();

				pUser->RemoveAccount(account);
				if (pUser->GetAccounts().empty())
				{
					stl::find_and_erase(m_friends, pUser);

					// First place is reserved for local user
					if (usrPos == m_users.begin())
					{
						m_users[0].reset();
					}
					else
					{
						m_users.erase(usrPos);
					}
				}
			}
		}

		void CPlugin::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
		{
			if (event == ESYSTEM_EVENT_FAST_SHUTDOWN || event == ESYSTEM_EVENT_FULL_SHUTDOWN)
			{
				for (IService* pSvc : m_services)
				{
					if (pSvc)
					{
						pSvc->RemoveListener(*this);
						pSvc->Shutdown();
					}
				}

				m_friends.clear();

				m_users.erase(m_users.begin() + 1, m_users.end());
				m_users[0] = nullptr;

				m_services.erase(m_services.begin() + 1, m_services.end());
				m_services[0] = nullptr;
			}
		}

		void CPlugin::RegisterMainService(IService& service)
		{
			CRY_ASSERT_MESSAGE(GetMainService() == nullptr, "Main service already registered");
			if (GetMainService() == nullptr)
			{
				m_services[0] = &service;
				m_services[0]->AddListener(*this);
			}
		}

		void CPlugin::RegisterService(IService& service)
		{
			if (stl::push_back_unique(m_services, &service))
			{
				service.AddListener(*this);
			}
		}

		Cry::GamePlatform::ServiceIdentifier CPlugin::GetMainServiceIdentifier() const
		{
			const IService* pMainService = GetMainService();
			return pMainService ? pMainService->GetServiceIdentifier() : NullServiceID;
		}

		void CPlugin::UnregisterService(const ServiceIdentifier& service)
		{
			if (GetMainServiceIdentifier() == service)
			{
				m_services[0] = nullptr;
			}
			else
			{
				stl::find_and_erase_if(m_services, Predicate::SWithServiceId(service));
			}
		}

		IService* CPlugin::GetMainService() const
		{
			// Constructor guarantees us we can access the first element
			return m_services[0];
		}

		IService* CPlugin::GetService(const ServiceIdentifier& svcId) const
		{
			auto svcPos = std::find_if(m_services.begin(), m_services.end(), Predicate::SWithServiceId(svcId));
			return svcPos != m_services.end() ? *svcPos : nullptr;
		}

		CRYREGISTER_SINGLETON_CLASS(CPlugin)
	}
}

