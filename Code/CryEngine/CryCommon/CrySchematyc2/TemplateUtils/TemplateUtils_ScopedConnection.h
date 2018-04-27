// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "TemplateUtils_IntrusiveList.h"

namespace TemplateUtils
{
	class CConnectionScope;

	// Scoped connection.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CScopedConnection
	{
		friend class CConnectionScope;

	public:

		CScopedConnection();
		explicit CScopedConnection(CConnectionScope& scope);
		CScopedConnection(const CScopedConnection& rhs);

		~CScopedConnection();

		void Connect(CConnectionScope& scope);
		void Disconnect();
		bool IsConnected() const;
		CConnectionScope* GetScope() const;

		void operator = (const CScopedConnection& rhs);
		bool operator == (const CScopedConnection& rhs) const;
		bool operator != (const CScopedConnection& rhs) const;

	private:

		TemplateUtils::intrusive_list_node m_listNode;
		CConnectionScope*                  m_pScope;
	};

	// Connection scope.
	// This is a container for automated connection management. All connections will be released when
	// this object goes out of scope.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CConnectionScope
	{
		friend class CScopedConnection;

	private:

		typedef TemplateUtils::intrusive_list<CScopedConnection, &CScopedConnection::m_listNode> ConnectionList;

	public:

		CConnectionScope();

		~CConnectionScope();

		void Release();

	private:

		CConnectionScope(const CConnectionScope&);
		CConnectionScope& operator = (const CConnectionScope&);

		void Connect(CScopedConnection& connection);
		void Disconnect(CScopedConnection& connection);

	private:

		ConnectionList m_connections;
	};

	//////////////////////////////////////////////////////////////////////////
	inline CScopedConnection::CScopedConnection()
		: m_pScope(nullptr)
	{}

	//////////////////////////////////////////////////////////////////////////
	inline CScopedConnection::CScopedConnection(CConnectionScope& scope)
		: m_pScope(nullptr)
	{
		Connect(scope);
	}

	//////////////////////////////////////////////////////////////////////////
	inline CScopedConnection::CScopedConnection(const CScopedConnection& rhs)
		: m_pScope(nullptr)
	{
		if(rhs.m_pScope)
		{
			Connect(*rhs.m_pScope);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	inline CScopedConnection::~CScopedConnection()
	{
		Disconnect();
	}

	//////////////////////////////////////////////////////////////////////////
	inline void CScopedConnection::Connect(CConnectionScope& scope)
	{
		scope.Connect(*this);
	}

	//////////////////////////////////////////////////////////////////////////
	inline void CScopedConnection::Disconnect()
	{
		if(m_pScope)
		{
			m_pScope->Disconnect(*this);
			m_pScope = nullptr;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	inline bool CScopedConnection::IsConnected() const
	{
		return m_pScope != nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	inline CConnectionScope* CScopedConnection::GetScope() const
	{
		return m_pScope;
	}

	//////////////////////////////////////////////////////////////////////////
	inline void CScopedConnection::operator = (const CScopedConnection& rhs)
	{
		Disconnect();
		if(rhs.m_pScope)
		{
			Connect(*rhs.m_pScope);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	inline bool CScopedConnection::operator == (const CScopedConnection& rhs) const
	{
		return m_pScope == rhs.m_pScope;
	}

	//////////////////////////////////////////////////////////////////////////
	inline bool CScopedConnection::operator != (const CScopedConnection& rhs) const
	{
		return m_pScope != rhs.m_pScope;
	}

	//////////////////////////////////////////////////////////////////////////
	inline CConnectionScope::CConnectionScope() {}

	//////////////////////////////////////////////////////////////////////////
	inline CConnectionScope::~CConnectionScope()
	{
		Release();
	}

	//////////////////////////////////////////////////////////////////////////
	inline void CConnectionScope::Release()
	{
		for(CScopedConnection& connection : m_connections)
		{
			connection.m_pScope = nullptr;
		}
		m_connections.clear();
	}

	//////////////////////////////////////////////////////////////////////////
	inline void CConnectionScope::Connect(CScopedConnection& connection)
	{
		if(connection.m_pScope)
		{
			connection.m_pScope->Disconnect(connection);
		}
		m_connections.push_back(connection);
		connection.m_pScope = this;
	}

	//////////////////////////////////////////////////////////////////////////
	inline void CConnectionScope::Disconnect(CScopedConnection& connection)
	{
		if(connection.m_pScope == this)
		{
			m_connections.remove(connection);
			connection.m_pScope = nullptr;
		}
	}

	// Scoped connection array.
	// #SchematycTODO : Replace with CScopedConnectionManager!
	////////////////////////////////////////////////////////////////////////////////////////////////////
	template <typename TYPE> class CScopedConnectionArray
	{
	private:

		typedef std::pair<TYPE, CScopedConnection> Connection;
		typedef DynArray<Connection>               Connections;

	public:

		inline void Add(const TYPE& value, CConnectionScope& scope)
		{
			m_connections.push_back(Connection(value, CScopedConnection(scope)));
		}

		inline void Remove(const TYPE& value, CConnectionScope& scope)
		{
			m_connections.erase(std::remove(m_connections.begin(), m_connections.end(), Connection(value, CScopedConnection(scope))), m_connections.end());
		}

		template <typename PREDICATE> void Process(PREDICATE& predicate)
		{
			size_t connectionCount = m_connections.size();
			for(size_t connectionIdx = 0; connectionIdx < connectionCount; )
			{
				Connection& connection = m_connections[connectionIdx];
				if(connection.second.IsConnected())
				{
					predicate(connection.first);
					++ connectionIdx;
				}
				else
				{
					-- connectionCount;
					if(connectionIdx < connectionCount)
					{
						connection = m_connections[connectionCount];
					}
				}
			}
			m_connections.resize(connectionCount);
		}

		inline void Cleanup()
		{
			size_t connectionCount = m_connections.size();
			for(size_t connectionIdx = 0; connectionIdx < connectionCount; )
			{
				Connection& connection = m_connections[connectionIdx];
				if(!connection.second.IsConnected())
				{
					-- connectionCount;
					if(connectionIdx < connectionCount)
					{
						connection = m_connections[connectionCount];
					}
				}
			}
			m_connections.resize(connectionCount);
		}

	private:

		Connections m_connections;
	};

	// Scoped connection manager.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	template <typename VALUE, typename KEY = void, class KEY_COMPARE = std::less<KEY> > class CScopedConnectionManager
	{
	public:

		typedef KEY         Key;
		typedef KEY_COMPARE KeyCompare;
		typedef VALUE       Value;

	private:

		struct SSlot
		{
			inline SSlot()
				: value()
			{}

			inline SSlot(const Key& _key, const Value& _value, CConnectionScope& _scope)
				: key(_key)
				, value(_value)
				, connection(_scope)
			{}

			Key               key;
			Value             value;
			CScopedConnection connection;
		};

		typedef DynArray<SSlot> Slots;

	public:

		inline CScopedConnectionManager(const KeyCompare& keyCompare = KeyCompare())
			: m_keyCompare(keyCompare)
			, m_bSortSlots(false)
			, m_bLocked(false)
		{}

		inline void Reserve(size_t size)
		{
			m_slots.reserve(size);
		}

		inline void Add(const Key& key, const Value& value, CConnectionScope& scope)
		{
			m_slots.push_back(SSlot(key, value, scope));
			m_bSortSlots = true;
		}

		/*inline void Remove(CConnectionScope& scope)
		{
			for(SSlot& slot : m_slots)
			{
				if(slot.connection.GetScope() == &scope)
				{
					slot.connection.Disconnect();
				}
			}
		}*/

		template <typename DELEGATE> inline void Process(const DELEGATE& delegate)
		{
			CRY_ASSERT(!m_bLocked);
			if(!m_bLocked)
			{
				m_bLocked = true;

				if(m_bSortSlots)
				{
					SortSlots();
				}

				size_t emptySlotCount = 0;
				for(SSlot& slot : m_slots)
				{
					if(slot.connection.IsConnected())
					{
						delegate(slot.value);
					}
					else
					{
						++ emptySlotCount;
					}
				}

				if(emptySlotCount)
				{
					Cleanup();
				}

				m_bLocked = false;
			}
		}

		template <typename DELEGATE> inline void Process(const Key& key, const DELEGATE& delegate)
		{
			CRY_ASSERT(!m_bLocked);
			if(!m_bLocked)
			{
				m_bLocked = true;

				if(m_bSortSlots)
				{
					SortSlots();
				}

				auto compareKey = [this] (const SSlot& lhs, const Key& rhs) -> bool
				{
					return m_keyCompare(lhs.key, rhs);
				};
				typename Slots::iterator itSlot = std::lower_bound(m_slots.begin(), m_slots.end(), key, compareKey);
				
				size_t emptySlotCount = 0;
				for(typename Slots::iterator itEndSlot = m_slots.end(); itSlot != itEndSlot; ++ itSlot)
				{
					if(m_keyCompare(key, itSlot->key))
					{
						break;
					}
					else
					{
						if(itSlot->connection.IsConnected())
						{
							delegate(itSlot->value);
						}
						else
						{
							++ emptySlotCount;
						}
					}
				}

				if(emptySlotCount)
				{
					Cleanup();
				}

				m_bLocked = false;
			}
		}

		inline void Cleanup()
		{
			auto isEmptySlot = [] (const SSlot& slot) -> bool
			{
				return slot.connection.IsConnected();
			};
			m_slots.erase(std::remove_if(m_slots.begin(), m_slots.end(), isEmptySlot), m_slots.end());
		}

	private:

		inline void SortSlots()
		{
			auto compareKeys = [this] (const SSlot& lhs, const SSlot& rhs) -> bool
			{
				return m_keyCompare(lhs.key, rhs.key);
			};
			std::sort(m_slots.begin(), m_slots.end(), compareKeys);
			m_bSortSlots = false;
		}

	private:

		KeyCompare m_keyCompare;
		Slots      m_slots;
		bool       m_bSortSlots;
		bool       m_bLocked;
	};

	// Connection manager.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	template <typename VALUE> class CScopedConnectionManager<VALUE>
	{
	public:

		typedef void            Key;
		typedef std::less<void> KeyCompare;
		typedef VALUE           Value;

	private:

		struct SSlot
		{
			inline SSlot()
				: value()
			{}

			inline SSlot(const Value& _value, CConnectionScope& _scope)
				: value(_value)
				, connection(_scope)
			{}

			Value             value;
			CScopedConnection connection;
		};

		typedef DynArray<SSlot> Slots;

	public:

		inline CScopedConnectionManager()
			: m_bLocked(false)
		{}

		inline void Reserve(size_t size)
		{
			m_slots.reserve(size);
		}

		inline void Add(const Value& value, CConnectionScope& scope)
		{
			m_slots.push_back(SSlot(value, scope));
		}

		/*inline void Remove(CConnectionScope& scope)
		{
			for(SSlot& slot : m_slots)
			{
				if(slot.connection.GetScope() == &scope)
				{
					slot.connection.Disconnect();
				}
			}
		}*/

		template <typename DELEGATE> inline void Process(const DELEGATE& delegate)
		{
			CRY_ASSERT(!m_bLocked);
			if(!m_bLocked)
			{
				m_bLocked = true;

				const size_t slotCount = m_slots.size();
				if(slotCount)
				{
					size_t slotIdx = 0;
					size_t endSlotIdx = slotCount;
					while(slotIdx < endSlotIdx)
					{
						SSlot& slot = m_slots[slotIdx];
						if(slot.connection.IsConnected())
						{
							delegate(slot.value);
							++ slotIdx;
						}
						else
						{
							-- endSlotIdx;
							if(slotIdx < endSlotIdx)
							{
								std::swap(slot, m_slots[endSlotIdx]);
							}
						}
					}

					if(const size_t emptySlotCount = slotCount - endSlotIdx)
					{
						endSlotIdx += m_slots.size() - slotCount;
						while(slotIdx < endSlotIdx)
						{
							std::swap(m_slots[slotIdx], m_slots[slotIdx + emptySlotCount]);
						}
						m_slots.resize(endSlotIdx);
					}
				}

				m_bLocked = false;
			}
		}

		inline void Cleanup()
		{
			Process( [] (const Value& value) {} );
		}

	private:

		Slots m_slots;
		bool  m_bLocked;
	};
}