// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <vector>
#include <functional>

#include <CryCore/smartptr.h>

/*
 *	Non-thread-safe resource pool
 */
template <typename T>
class CResourcePool 
{
	static_assert(std::is_base_of<_i_reference_target<uint32_t>, T>::value, "T must derive from _i_reference_target<uint32_t>");

public:
	using value_type = _smart_ptr<T>;

private:
	using pool_t = std::vector<value_type>;

	pool_t pool;

public:
	CResourcePool() = default;
	~CResourcePool()
	{
		clear();
	}

	CResourcePool(CResourcePool&&) = default;
	CResourcePool &operator=(CResourcePool&&) = default;
	CResourcePool(const CResourcePool&) = delete;
	CResourcePool &operator=(const CResourcePool&) = delete;

	template <typename... Ts>
	void allocate(Ts&&... ts) 
	{
		auto ptr = value_type(new T(std::forward<Ts>(ts)...));
		pool.emplace_back(std::move(ptr));
	}
	void clear() 
	{
		for (auto &p : *this)
		{
			// Resource in use?
			CRY_ASSERT(p->UseCount() == 1);

			delete p.ReleaseOwnership();
		}
		pool.clear();
	}

	typename pool_t::iterator erase(typename pool_t::iterator it)
	{
		CRY_ASSERT((*it)->UseCount() == 1);
		delete it->ReleaseOwnership();
		return pool.erase(it);
	}

	typename pool_t::iterator               begin() noexcept { return pool.begin(); }
	typename pool_t::const_iterator         begin() const noexcept { return pool.begin(); }
	typename pool_t::const_iterator         cbegin() const noexcept { return pool.cbegin(); }
	typename pool_t::reverse_iterator       rbegin() noexcept { return pool.rbegin(); }
	typename pool_t::const_reverse_iterator rbegin() const noexcept { return pool.rbegin(); }
	typename pool_t::const_reverse_iterator crbegin() const noexcept { return pool.crbegin(); }
	typename pool_t::iterator               end() noexcept { return pool.end(); }
	typename pool_t::const_iterator         end() const noexcept { return pool.end(); }
	typename pool_t::const_iterator         cend() const noexcept { return pool.cend(); }
	typename pool_t::reverse_iterator       rend() noexcept { return pool.rend(); }
	typename pool_t::const_reverse_iterator rend() const noexcept { return pool.rend(); }
	typename pool_t::const_reverse_iterator crend() const noexcept { return pool.crend(); }

	value_type& back() { return pool.back(); }
	value_type& back() const { return pool.back(); }
	value_type& front() { return pool.front(); }
	value_type& front() const { return pool.front(); }

	size_t size() const noexcept { return pool.size(); }

	value_type& operator[](int idx) { return pool[idx]; }
	value_type& operator[](int idx) const { return pool[idx]; }
};

