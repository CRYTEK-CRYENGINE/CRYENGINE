// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// IFactoryDatabaseListener<>
		//
		//===================================================================================

		template <class TFactory>
		struct IFactoryDatabaseListener
		{
			virtual                     ~IFactoryDatabaseListener() {}
			virtual void                OnFactoryRegistered(TFactory* pFreshlyRegisteredFactory) = 0;
		};

		//===================================================================================
		//
		// CFactoryDatabase<>
		//
		//===================================================================================

		template <class TFactory>
		class CFactoryDatabase : public IFactoryDatabase<TFactory>
		{
		public:
			explicit                      CFactoryDatabase();

			// IFactoryDatabase<TFactory>
			virtual void                  RegisterFactory(TFactory* pFactoryToRegister, const char* szName, const CryGUID& guid) override final;
			virtual TFactory*             FindFactoryByName(const char* szName) const override final;
			virtual TFactory*             FindFactoryByGUID(const CryGUID& guid) const override final;
			virtual TFactory*             FindFactoryByCallback(const std::function<bool(const TFactory&)>& callback) const final;
			virtual size_t                GetFactoryCount() const override final;
			virtual TFactory&             GetFactory(size_t index) const override final;
			// ~IFactoryDatabase<TFactory>

			void                          RegisterListener(IFactoryDatabaseListener<TFactory>* pListener);
			void                          UnregisterListener(IFactoryDatabaseListener<TFactory>* pListener);

			// the returned map contains only names of duplicate factories along with their total number of registration attempts (i. e. that counter will always be >= 2)
			std::map<string, int>         GetDuplicateFactoryNames() const;
			std::map<CryGUID, int>        GetDuplicateFactoryGUIDs() const;

			// names of factories that contain empty GUIDs
			std::set<string>              GetFactoryNamesWithEmptyGUIDs() const;

			void                          PrintToConsole(CLogger& logger, const char* szDatabaseNameToPrint) const;

		private:
			                              UQS_NON_COPYABLE(CFactoryDatabase);

		private:
			typedef IFactoryDatabaseListener<TFactory> Listener;

			std::vector<TFactory*>        m_list;                    // for fast random-access
			std::map<string, TFactory*>   m_mapByName;               // for fast lookup by name
			std::map<CryGUID, TFactory*>  m_mapByGUID;               // for fast lookup by GUID
			std::map<string, int>         m_name2registrationCount;  // to detect duplicate factory names that occurred via RegisterFactory() (which is an error from the client side)
			std::map<CryGUID, int>        m_guid2registrationCount;  // to detect duplicate factory GUIDs that occurred via RegisterFactory() (which is an error from the client side)
			std::list<Listener*>          m_listeners;
		};

		template <class TFactory>
		CFactoryDatabase<TFactory>::CFactoryDatabase()
		{
			// nothing
		}

		template <class TFactory>
		void CFactoryDatabase<TFactory>::RegisterFactory(TFactory* pFactoryToRegister, const char* szName, const CryGUID& guid)
		{
			assert(pFactoryToRegister);

			// assert that the consistency checks haven't been done yet (client code shall never register any factory afterwards, because inconsistencies/duplicates/etc. would go unnoticed)
			assert(!Hub_HaveConsistencyChecksBeenDoneAlready());

			// see how many registration attempts for a factory with given name and GUID were done before already
			int& registrationCountByNameSoFar = m_name2registrationCount[szName];
			int& registrationCountByGUIDSoFar = m_guid2registrationCount[guid];
			if (registrationCountByNameSoFar == 0 && registrationCountByGUIDSoFar == 0)
			{
				// fine, this will be the (hopefully) one and only factory under this name and GUID
				m_list.push_back(pFactoryToRegister);
				m_mapByName[szName] = pFactoryToRegister;
				m_mapByGUID[guid] = pFactoryToRegister;

				// notify all listeners
				// (notice that since we're using a std::list, the currently being called listener is even allowed to unregister himself
				//  during the callback without affecting the remaining iterators)
				for (auto it = m_listeners.begin(); it != m_listeners.end(); )
				{
					Listener* pListener = *it++;
					pListener->OnFactoryRegistered(pFactoryToRegister);
				}
			}
			++registrationCountByNameSoFar;
			++registrationCountByGUIDSoFar;
		}

		template <class TFactory>
		TFactory* CFactoryDatabase<TFactory>::FindFactoryByName(const char* szName) const
		{
			auto it = m_mapByName.find(szName);
			return (it == m_mapByName.cend()) ? nullptr : it->second;
		}

		template <class TFactory>
		TFactory* CFactoryDatabase<TFactory>::FindFactoryByGUID(const CryGUID& guid) const
		{
			auto it = m_mapByGUID.find(guid);
			return (it == m_mapByGUID.cend()) ? nullptr : it->second;
		}

		template <class TFactory>
		TFactory* CFactoryDatabase<TFactory>::FindFactoryByCallback(const std::function<bool(const TFactory&)>& callback) const
		{
			for (TFactory* pFactory : m_list)
			{
				if (callback(*pFactory))
					return pFactory;
			}
			return nullptr;
		}

		template <class TFactory>
		size_t CFactoryDatabase<TFactory>::GetFactoryCount() const
		{
			return m_list.size();
		}

		template <class TFactory>
		TFactory& CFactoryDatabase<TFactory>::GetFactory(size_t index) const
		{
			assert(index < m_list.size());
			return *m_list[index];
		}

		template <class TFactory>
		void CFactoryDatabase<TFactory>::RegisterListener(IFactoryDatabaseListener<TFactory>* pListener)
		{
			stl::push_back_unique(m_listeners, pListener);
		}

		template <class TFactory>
		void CFactoryDatabase<TFactory>::UnregisterListener(IFactoryDatabaseListener<TFactory>* pListener)
		{
			m_listeners.remove(pListener);
		}

		template <class TFactory>
		std::map<string, int> CFactoryDatabase<TFactory>::GetDuplicateFactoryNames() const
		{
			std::map<string, int> duplicates;

			for (const auto& pair : m_name2registrationCount)
			{
				if (pair.second > 1)
				{
					duplicates[pair.first] = pair.second;
				}
			}

			return duplicates;
		}

		template <class TFactory>
		std::map<CryGUID, int> CFactoryDatabase<TFactory>::GetDuplicateFactoryGUIDs() const
		{
			std::map<CryGUID, int> duplicates;

			for (const auto& pair : m_guid2registrationCount)
			{
				if (pair.second > 1)
				{
					duplicates[pair.first] = pair.second;
				}
			}

			return duplicates;
		}

		template <class TFactory>
		std::set<string> CFactoryDatabase<TFactory>::GetFactoryNamesWithEmptyGUIDs() const
		{
			std::set<string> factoryNamesWithEmptyGUIDs;

			for (const TFactory* pFactory : m_list)
			{
				if (pFactory->GetGUID() == CryGUID::Null())
				{
					factoryNamesWithEmptyGUIDs.insert(pFactory->GetName());
				}
			}

			return factoryNamesWithEmptyGUIDs;
		}

		template <class TFactory>
		void CFactoryDatabase<TFactory>::PrintToConsole(CLogger& logger, const char* szDatabaseNameToPrint) const
		{
			const size_t numFactories = GetFactoryCount();

			logger.Printf("=== %i %s-factories in the database: ===", (int)numFactories, szDatabaseNameToPrint);

			CLoggerIndentation indent;

			for (size_t i = 0; i < numFactories; ++i)
			{
				const TFactory& factory = GetFactory(i);
				const char* szFactoryName = factory.GetName();   // this compiles only as long as the TFactory class has such a GetName() method
				const CryGUID& guid = factory.GetGUID();         // this compiles only as long as the TFactory class has such a GetGUID() method
				Shared::CUqsString guidAsString;
				Shared::Internal::CGUIDHelper::ToString(guid, guidAsString);
				logger.Printf("%s [%s]", szFactoryName, guidAsString.c_str());
			}
		}

	}
}
