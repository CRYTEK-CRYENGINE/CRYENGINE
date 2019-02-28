// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Utils/Delegate.h"
#include "CrySchematyc/Utils/PreprocessorUtils.h"
#include "CrySchematyc/Utils/ScopedConnection.h"

namespace Schematyc
{
template<typename SIGNATURE, typename KEY = void, class KEY_COMPARE = typename SDefaultCompare<KEY>::Type> class CSignal;

template<typename KEY, class KEY_COMPARE, typename RETURN_TYPE, typename ... PARAM_TYPES> class CSignal<RETURN_TYPE(PARAM_TYPES ...), KEY, KEY_COMPARE>;

template<typename SIGNATURE, typename KEY = void, class KEY_COMPARE = typename SDefaultCompare<KEY>::Type> class CSignalSlots;

template<typename KEY, class KEY_COMPARE, typename RETURN_TYPE, typename ... PARAM_TYPES> class CSignalSlots<RETURN_TYPE(PARAM_TYPES ...), KEY, KEY_COMPARE>
{
	friend class CSignal<RETURN_TYPE(PARAM_TYPES ...), KEY, KEY_COMPARE>;

public:

	typedef KEY                                     Key;
	typedef KEY_COMPARE                             KeyCompare;
	typedef std::function<RETURN_TYPE(PARAM_TYPES ...)> Delegate;

	class CSelectionScope
	{
	public:

		inline CSelectionScope(CSignalSlots& signalSlots, const Key& key)
			: m_signalSlots(signalSlots)
			, m_key(key)
		{}

		inline void Connect(const Delegate& delegate, CConnectionScope& scope) const
		{
			m_signalSlots.Connect(m_key, delegate, scope);
		}

	private:

		CSignalSlots& m_signalSlots;
		const Key&    m_key;
	};

private:

	typedef CScopedConnectionManager<Delegate, Key, KeyCompare> Connections;

public:

	inline CSignalSlots(const KeyCompare& keyCompare = KeyCompare())
		: m_connections(keyCompare)
	{}

	inline const CSelectionScope Select(const Key& key)
	{
		return CSelectionScope(*this, key);
	}

	inline void Reserve(uint32 size)
	{
		return m_connections.Reserve(size);
	}

	inline void Connect(const Delegate& delegate, CConnectionScope& scope)
	{
		m_connections.Add(delegate, scope);
	}

	inline void Disconnect(CConnectionScope& scope)
	{
		m_connections.Remove(scope);
	}

	inline void Cleanup()
	{
		m_connections.Cleanup();
	}

private:

	inline void Connect(const Key& key, const Delegate& delegate, CConnectionScope& scope)
	{
		m_connections.Add(key, delegate, scope);
	}

private:

	Connections m_connections;
};

template<typename RETURN_TYPE, typename ... PARAM_TYPES> class CSignalSlots<RETURN_TYPE(PARAM_TYPES ...)>
{
	friend class CSignal<RETURN_TYPE(PARAM_TYPES ...)>;

public:

	typedef void                                        Key;
	typedef void                                        KeyCompare;
	typedef std::function<RETURN_TYPE(PARAM_TYPES ...)> Delegate;

private:

	typedef CScopedConnectionManager<Delegate> Connections;

public:

	inline void Reserve(uint32 size)
	{
		return m_connections.Reserve(size);
	}

	inline void Connect(const Delegate& delegate, CConnectionScope& scope)
	{
		m_connections.Add(delegate, scope);
	}

	inline void Disconnect(CConnectionScope& scope)
	{
		m_connections.Remove(scope);
	}

	inline void Cleanup()
	{
		m_connections.Cleanup();
	}

private:

	Connections m_connections;
};

template<typename KEY, class KEY_COMPARE, typename RETURN_TYPE, typename ... PARAM_TYPES> class CSignal<RETURN_TYPE(PARAM_TYPES ...), KEY, KEY_COMPARE>
{
public:

	typedef KEY                                                          Key;
	typedef KEY_COMPARE                                                  KeyCompare;
	typedef std::function<RETURN_TYPE(PARAM_TYPES ...)>                  Delegate;
	typedef CSignalSlots<RETURN_TYPE(PARAM_TYPES ...), KEY, KEY_COMPARE> Slots;

	class CSelectionScope
	{
	public:

		inline CSelectionScope(CSignal& signal, const Key& key)
			: m_signal(signal)
			, m_key(key)
		{}

		inline void Send(PARAM_TYPES ... params) const
		{
			m_signal.Send(m_key, params ...);
		}

	private:

		CSignal&   m_signal;
		const Key& m_key;
	};

public:

	inline CSignal(const KeyCompare& keyCompare = KeyCompare())
		: m_slots(keyCompare)
	{}

	inline Slots& GetSlots()
	{
		return m_slots;
	}

	inline const CSelectionScope Select(const Key& key)
	{
		return CSelectionScope(*this, key);
	}

	inline void Send(PARAM_TYPES ... params)
	{
		auto delegate = [&params ...](const Delegate& delegate)
		{
			delegate(params ...);
		};
		m_slots.m_connections.Process(delegate);
	}

private:

	inline void Send(const Key& key, PARAM_TYPES ... params)
	{
		auto delegate = [&params ...](const Delegate& delegate)
		{
			delegate(params ...);
		};
		m_slots.m_connections.Process(key, delegate);
	}

private:

	Slots m_slots;
};

template<typename RETURN_TYPE, typename ... PARAM_TYPES> class CSignal<RETURN_TYPE(PARAM_TYPES ...)>
{
public:

	typedef void                                        Key;
	typedef void                                        KeyCompare;
	typedef std::function<RETURN_TYPE(PARAM_TYPES ...)> Delegate;
	typedef CSignalSlots<RETURN_TYPE(PARAM_TYPES ...)>  Slots;

public:

	inline Slots& GetSlots()
	{
		return m_slots;
	}

	inline void Send(PARAM_TYPES ... params)
	{
		auto delegate = [&params ...](const Delegate& delegate)
		{
			delegate(params ...);
		};
		m_slots.m_connections.Process(delegate);
	}

private:

	Slots m_slots;
};
} // Schematyc
