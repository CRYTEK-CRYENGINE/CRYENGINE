// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Utils/Delegate.h"
#include <functional>

namespace Schematyc
{
// Forward declare classes.
class CConnectionScope;

// Callback to be executed when scope is disconnected.
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::function<void (CConnectionScope&)> ScopedConnectionCallback;

// Container for management of connections. All connections will be released automatically when
// this object's destructor is called.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CConnectionScope
{
	friend class CScopedConnection;

private:

	struct SNode
	{
		inline SNode()
			: pScope(nullptr)
			, pPrev(nullptr)
			, pNext(nullptr)
		{}

		SNode(const SNode&) = delete;
		SNode(SNode&&) = delete;
		SNode& operator=(const SNode&) = delete;
		SNode& operator=(SNode&&) = delete;

		ScopedConnectionCallback callback;
		CConnectionScope*        pScope;
		SNode*                   pPrev;
		SNode*                   pNext;
	};

public:

	inline CConnectionScope()
		: m_pFirstNode(nullptr)
		, m_pLastNode(nullptr)
	{}

	inline ~CConnectionScope()
	{
		Release();
	}

	inline bool HasConnections() const
	{
		return m_pFirstNode != nullptr;
	}

	inline void Release()
	{
		for (SNode* pNode = m_pFirstNode; pNode; pNode = pNode->pNext)
		{
			if (pNode->callback)
			{
				pNode->callback(*this);
			}
		}
		while (m_pFirstNode)
		{
			Disconnect(*m_pFirstNode);
		}
	}

	CConnectionScope(const CConnectionScope&) = delete;
	CConnectionScope& operator=(const CConnectionScope&) = delete;

private:

	inline void Connect(SNode& node)
	{
		Disconnect(node);

		node.pScope = this;
		node.pPrev = m_pLastNode;
		node.pNext = nullptr;

		if (m_pFirstNode)
		{
			m_pLastNode->pNext = &node;
		}
		else
		{
			m_pFirstNode = &node;
		}

		m_pLastNode = &node;
	}

	inline void Disconnect(SNode& node)
	{
		if (node.pScope)
		{
			node.pScope = nullptr;

			if (node.pPrev)
			{
				node.pPrev->pNext = node.pNext;
			}
			else
			{
				m_pFirstNode = node.pNext;
			}
			if (node.pNext)
			{
				node.pNext->pPrev = node.pPrev;
			}
			else
			{
				m_pLastNode = node.pPrev;
			}

			node.pPrev = nullptr;
			node.pNext = nullptr;
		}
	}

private:

	SNode* m_pFirstNode;
	SNode* m_pLastNode;
};

// Connection that will be released automatically when scope's destructor is called.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScopedConnection
{
public:

	inline CScopedConnection() {}

	inline explicit CScopedConnection(CConnectionScope& scope, const ScopedConnectionCallback& callback = ScopedConnectionCallback())
	{
		m_node.callback = callback;
		scope.Connect(m_node);
	}

	inline CScopedConnection(CScopedConnection&& rhs)
	{
		m_node.callback = rhs.m_node.callback;
		CConnectionScope* pScope = rhs.Disconnect();
		if (pScope)
		{
			pScope->Connect(m_node);
		}
	}

	inline ~CScopedConnection()
	{
		Disconnect();
	}

	inline void Connect(CConnectionScope& scope, const ScopedConnectionCallback& callback = ScopedConnectionCallback())
	{
		m_node.callback = callback;
		scope.Connect(m_node);
	}

	inline CConnectionScope* Disconnect()
	{
		CConnectionScope* pScope = m_node.pScope;
		if (pScope)
		{
			m_node.callback = ScopedConnectionCallback();
			pScope->Disconnect(m_node);
		}
		return pScope;
	}

	inline bool IsConnected() const
	{
		return m_node.pScope != nullptr;
	}

	inline CConnectionScope* GetScope() const
	{
		return m_node.pScope;
	}

	inline CScopedConnection& operator=(CScopedConnection&& rhs)
	{
		Disconnect();
		m_node.callback = rhs.m_node.callback;
		CConnectionScope* pScope = rhs.Disconnect();
		if (pScope)
		{
			pScope->Connect(m_node);
		}
		return *this;
	}

	CScopedConnection(const CConnectionScope&) = delete;
	CScopedConnection& operator=(const CScopedConnection&) = delete;

private:

	CConnectionScope::SNode m_node;
};

// Default key comparison selector.
////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename TYPE> struct SDefaultCompare
{
	typedef std::less<TYPE> Type;
};

template<> struct SDefaultCompare<void>
{
	typedef void Type;
};

// Scoped connection manager.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename TYPE, typename KEY = void, class KEY_COMPARE = typename SDefaultCompare<KEY>::Type> class CScopedConnectionManager
{
public:

	typedef KEY         Key;
	typedef KEY_COMPARE KeyCompare;
	typedef TYPE        Value;

private:

	struct SSlot
	{
		inline SSlot()
			: value()
		{}

		inline SSlot(const Key& _key, const Value& _value)
			: key(_key)
			, value(_value)
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

	inline void Reserve(uint32 size)
	{
		m_slots.reserve(size);
	}

	inline void Add(const Key& key, const Value& value, CConnectionScope& scope)
	{
		m_slots.push_back(SSlot(key, value));
		m_slots.back().connection.Connect(scope, [this] { this->Remove(); });
		m_bSortSlots = true;
	}

	inline void Remove(CConnectionScope& scope)
	{
		for (SSlot& slot : m_slots)
		{
			if (slot.connection.GetScope() == &scope)
			{
				slot.connection.Disconnect();
			}
		}
	}

	template<typename DELEGATE> inline void Process(const DELEGATE& delegate)
	{
		CRY_ASSERT(!m_bLocked);
		if (!m_bLocked)
		{
			m_bLocked = true;

			if (m_bSortSlots)
			{
				SortSlots();
			}

			uint32 emptySlotCount = 0;
			for (SSlot& slot : m_slots)
			{
				if (slot.connection.IsConnected())
				{
					delegate(slot.value);
				}
				else
				{
					++emptySlotCount;
				}
			}

			if (emptySlotCount)
			{
				Cleanup();
			}

			m_bLocked = false;
		}
	}

	template<typename DELEGATE> inline void Process(const Key& key, const DELEGATE& delegate)
	{
		CRY_ASSERT(!m_bLocked);
		if (!m_bLocked)
		{
			m_bLocked = true;

			if (m_bSortSlots)
			{
				SortSlots();
			}

			auto compareKey = [this](const SSlot& lhs, const Key& rhs) -> bool
			{
				return m_keyCompare(lhs.key, rhs);
			};
			typename Slots::iterator itSlot = std::lower_bound(m_slots.begin(), m_slots.end(), key, compareKey);

			uint32 emptySlotCount = 0;
			for (typename Slots::iterator itEndSlot = m_slots.end(); itSlot != itEndSlot; ++itSlot)
			{
				if (m_keyCompare(key, itSlot->key))
				{
					break;
				}
				else
				{
					if (itSlot->connection.IsConnected())
					{
						delegate(itSlot->value);
					}
					else
					{
						++emptySlotCount;
					}
				}
			}

			if (emptySlotCount)
			{
				Cleanup();
			}

			m_bLocked = false;
		}
	}

	inline void Cleanup()
	{
		auto isEmptySlot = [](const SSlot& slot) -> bool
		{
			return slot.connection.IsConnected();
		};
		m_slots.erase(std::remove_if(m_slots.begin(), m_slots.end(), isEmptySlot), m_slots.end());
	}

private:

	inline void SortSlots()
	{
		auto compareKeys = [this](const SSlot& lhs, const SSlot& rhs) -> bool
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

// Scoped connection manager.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename TYPE> class CScopedConnectionManager<TYPE>
{
public:

	typedef void Key;
	typedef void KeyCompare;
	typedef TYPE Value;

private:

	struct SSlot
	{
		inline SSlot()
			: value()
		{}

		inline SSlot(const Value& _value)
			: value(_value)
		{}

		Value             value;
		CScopedConnection connection;
	};

	typedef DynArray<SSlot> Slots;

public:

	inline CScopedConnectionManager()
		: m_bLocked(false)
	{}

	inline void Reserve(uint32 size)
	{
		m_slots.reserve(size);
	}

	inline void Add(const Value& value, CConnectionScope& scope)
	{
		m_slots.push_back(SSlot(value));
		m_slots.back().connection.Connect(scope, [this](CConnectionScope& scope){this->Remove(scope);} );
	}

	inline void Remove(CConnectionScope& scope)
	{
		for (SSlot& slot : m_slots)
		{
			if (slot.connection.GetScope() == &scope)
			{
				slot.connection.Disconnect();
			}
		}
	}

	template<typename DELEGATE> inline void Process(const DELEGATE& delegate)
	{
		CRY_ASSERT(!m_bLocked);
		if (!m_bLocked)
		{
			m_bLocked = true;

			const uint32 slotCount = m_slots.size();
			if (slotCount)
			{
				uint32 slotIdx = 0;
				uint32 endSlotIdx = slotCount;
				while (slotIdx < endSlotIdx)
				{
					SSlot& slot = m_slots[slotIdx];
					if (slot.connection.IsConnected())
					{
						delegate(slot.value);
						++slotIdx;
					}
					else
					{
						--endSlotIdx;
						if (slotIdx < endSlotIdx)
						{
							std::swap(slot, m_slots[endSlotIdx]);
						}
					}
				}

				const uint32 emptySlotCount = slotCount - endSlotIdx;
				if (emptySlotCount)
				{
					endSlotIdx += m_slots.size() - slotCount;
					while (slotIdx < endSlotIdx)
					{
						std::swap(m_slots[slotIdx], m_slots[slotIdx + emptySlotCount]);
						++slotIdx;
					}
					m_slots.resize(endSlotIdx);
				}
			}

			m_bLocked = false;
		}
	}

	inline void Cleanup()
	{
		Process([](const Value& value) {});
	}

private:

	Slots m_slots;
	bool  m_bLocked;
};
} // Schematyc
