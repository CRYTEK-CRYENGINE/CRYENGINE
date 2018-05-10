// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __HashGrid_h__
#define __HashGrid_h__

#pragma once

template<typename Key, typename DiscreetKey>
struct hash_grid_2d
{
	typedef Key                                key_type;
	typedef typename key_type::value_type      key_value;

	typedef DiscreetKey                        discreet_type;
	typedef typename discreet_type::value_type discreet_value;

	typedef hash_grid_2d<Key, DiscreetKey>     type;

	hash_grid_2d(const key_value& cellSizeX, const key_value& cellSizeY, const key_value& cellSizeZ)
		: scaleFactorX(1 / cellSizeX)
		, scaleFactorY(1 / cellSizeY)
	{
	}

	inline discreet_type discreet(const key_type& key) const
	{
		return discreet_type(static_cast<discreet_value>(key[0] * scaleFactorX),
		                     static_cast<discreet_value>(key[1] * scaleFactorY),
		                     static_cast<discreet_value>(0));
	}

	inline size_t hash(const key_type& key) const
	{
		return hash(discreet(key));
	}

	inline size_t hash(const discreet_type& discreet) const
	{
		return static_cast<size_t>(
		  (discreet[0] ^ 920129341) +
		  (discreet[1] ^ 1926129311));
	}

	inline void swap(type& other)
	{
		std::swap(scaleFactorX, other.scaleFactorX);
		std::swap(scaleFactorY, other.scaleFactorY);
	}

private:
	key_value scaleFactorX;
	key_value scaleFactorY;
};

template<typename Key, typename DiscreetKey>
struct hash_grid_3d
{
	typedef Key                                key_type;
	typedef typename key_type::value_type      key_value;

	typedef DiscreetKey                        discreet_type;
	typedef typename discreet_type::value_type discreet_value;

	typedef hash_grid_3d<Key, DiscreetKey>     type;

	hash_grid_3d(const key_value& cellSizeX, const key_value& cellSizeY, const key_value& cellSizeZ)
		: scaleFactorX(1 / cellSizeX)
		, scaleFactorY(1 / cellSizeY)
		, scaleFactorZ(1 / cellSizeZ)
	{
	}

	inline discreet_type discreet(const key_type& key) const
	{
		return discreet_type(static_cast<discreet_value>(key[0] * scaleFactorX),
		                     static_cast<discreet_value>(key[1] * scaleFactorY),
		                     static_cast<discreet_value>(key[2] * scaleFactorZ));
	}

	inline size_t hash(const key_type& key) const
	{
		return hash(discreet(key));
	}

	inline size_t hash(const discreet_type& discreet) const
	{
		return static_cast<size_t>(
		  (discreet[0] ^ 920129341ul) +
		  (discreet[1] ^ 1926129311ul) +
		  (discreet[2] ^ 3926129401ul));
	}

	inline void swap(type& other)
	{
		std::swap(scaleFactorX, other.scaleFactorX);
		std::swap(scaleFactorY, other.scaleFactorY);
		std::swap(scaleFactorZ, other.scaleFactorZ);
	}

private:
	key_value scaleFactorX;
	key_value scaleFactorY;
	key_value scaleFactorZ;
};

template<typename KeyType, typename ValueType>
struct hash_grid_no_position
{
	KeyType operator()(const ValueType&) const
	{
		switch (0)
		{
		case 0:
			"hash_grid query performed without a valid position-retriever implementation";
		}
		;

		return KeyType();
	}
};

template<int NumberOfCells, typename ValueType, typename KeyHash,
         typename PositionRetriever = hash_grid_no_position<typename KeyHash::key_type, ValueType>>
class hash_grid :
	protected KeyHash
{
public:
	enum
	{
		CellCount = NumberOfCells,
	};

	typedef ValueType                                                       value_type;
	typedef KeyHash                                                         key_hash;

	typedef typename key_hash::key_type                                     key_type;
	typedef typename key_type::value_type                                   key_value;
	typedef typename key_hash::discreet_type                                discreet_type;
	typedef typename discreet_type::value_type                              discreet_value;

	typedef PositionRetriever                                               position_retriever_type;

	typedef hash_grid<NumberOfCells, ValueType, KeyHash, PositionRetriever> type;

	typedef std::vector<value_type>                                         items_type;

	struct cell_type
	{
		cell_type()
			: query(0)
		{
		}

		mutable uint32 query;
		items_type     items;
	};
	typedef std::vector<cell_type> cells_type;

	inline hash_grid(float cellSizeX = 20.0f, float cellSizeY = 20.0f, float cellSizeZ = 20.0f,
	                 const position_retriever_type& _position = position_retriever_type())
		: key_hash(cellSizeX, cellSizeY, cellSizeZ)
		, position(_position)
		, m_cells(CellCount)
		, m_count(0)
		, m_query(0)
	{
	}

	inline void clear()
	{
		m_cells.clear();
		m_cells.resize(CellCount);
		m_count = 0;
		m_query = 0;
	}

	inline void swap(type& other)
	{
		m_cells.swap(other);

		std::swap(m_count, other.m_count);
		key_hash::swap(other);
	}

	inline size_t size() const
	{
		return m_count;
	}

	inline bool empty() const
	{
		return m_count == 0;
	}

	struct iterator
	{
		iterator()
			: cell(~0u)
			, item(~0u)
			, grid(0)
		{
		}

		value_type& operator*()
		{
			return grid->m_cells[cell][item];
		}

		const value_type& operator*() const
		{
			return grid->m_cells[cell][item];
		}

		value_type* operator->() const
		{
			return (&**this);
		}

		iterator& operator++()
		{
			assert(cell < grid_type::CellCount);
			cell_type& items = grid->m_cells[cell];

			if (!items.empty() && (item < items.size() - 1))
				++item;
			else
			{
				item = 0;
				++cell;

				while ((cell < type::CellCount) && grid->m_cells[cell].empty())
					++cell;
			}

			return *this;
		}

		iterator operator++(int)
		{
			iterator tmp = *this;
			++*this;
			return tmp;
		}

		iterator& operator--()
		{
			if (item > 0)
				--item;
			else
			{
				--cell;
				while ((cell > 0) && grid->m_cells[cell].empty())
					--cell;

				assert(cell < type::CellCount);
				cell_type& items = grid->m_cells[cell];
				item = items.size() - 1;
			}

			return *this;
		}

		iterator operator--(int)
		{
			iterator tmp = *this;
			++*this;
			return tmp;
		}

		bool operator==(const iterator& other) const
		{
			return (cell == other.cell) && (item == other.item) && (grid == other.grid);
		}

		bool operator!=(const iterator& other) const
		{
			return !(*this == other);
		}

	private:
		friend class hash_grid<NumberOfCells, ValueType, KeyHash, PositionRetriever>;
		typedef hash_grid<NumberOfCells, ValueType, KeyHash, PositionRetriever> grid_type;

		iterator(size_t _cell, size_t _item, grid_type* _grid)
			: grid(_grid)
			, item(_item)
			, cell(_cell)
		{
		}

		grid_type* grid;
		size_t     cell;
		size_t     item;
	};

	inline iterator begin()
	{
		uint32 item = 0;
		uint32 cell = 0;

		while ((cell < type::CellCount) && m_cells[cell].empty())
			++cell;

		return iterator(cell, item, this);
	}

	inline iterator end()
	{
		return iterator(CellCount, 0, this);
	}

	inline iterator insert(const key_type& key, const value_type& value)
	{
		size_t hash_value = KeyHash::hash(key);
		size_t index = hash_value % CellCount;

		cell_type& cell = m_cells[index];
		items_type& items = cell.items;

		items.push_back(value);
		++m_count;

		return iterator(index, items.size() - 1, this);
	}

	inline void erase(const key_type& key, const value_type& value)
	{
		size_t hash_value = KeyHash::hash(key);
		size_t index = hash_value % CellCount;

		cell_type& cell = m_cells[index];
		items_type& items = cell.items;

		typename items_type::iterator it = items.begin();
		typename items_type::iterator end = items.end();

		for (; it != end; ++it)
		{
			if (*it == value)
			{
				std::swap(*it, items.back());
				items.pop_back();
				--m_count;
				return;
			}
		}
	}

	inline iterator erase(const iterator& it)
	{
		--m_count;
		cell_type& cell = m_cells[it.cell];
		items_type& items = cell.items;
		std::swap(items[it.item], items.back());
		items.pop_back();
		if (!items.empty())
			return it;

		uint32 index = it.cell;
		while ((index < CellCount) && m_cells[index].items.empty())
			++index;

		return iterator(index, 0, this);
	}

	inline iterator find(const key_type& key, const value_type& value)
	{
		size_t index = KeyHash::hash(key) % CellCount;

		cell_type& cell = m_cells[index];
		items_type& items = cell.items;
		typename items_type::iterator it = items.begin();
		typename items_type::iterator iend = items.end();

		for (; it != iend; ++it)
		{
			if (*it == value)
				return iterator(index, it - items.begin(), this);
		}

		return end();
	}

	inline iterator move(const iterator& it, const key_type& to)
	{
		size_t index = KeyHash::hash(to) % CellCount;

		if (index == it.cell)
			return it;

		cell_type& cell = m_cells[it.cell];
		items_type& items = cell.items;
		typename items_type::iterator iit = items.begin() + it.item;

		cell_type& to_cell = m_cells[index];
		items_type& to_items = to_cell.items;
		to_items.push_back(*iit);

		std::swap(items[it.item], items.back());
		items.pop_back();

		return iterator(index, to_items.size() - 1, this);
	}

	template<typename Container>
	uint32 query_sphere(const key_type& center, const key_value& radius, Container& container) const
	{
		uint32 count = 0;

		if (!empty())
		{
			++m_query;

			key_type minc(center - key_type(radius));
			key_type maxc(center + key_type(radius));

			discreet_type mind = KeyHash::discreet(minc);
			discreet_type maxd = KeyHash::discreet(maxc);
			discreet_type current = mind;

			float radius_sq = radius * radius;

			for (; current[0] <= maxd[0]; ++current[0])
			{
				for (; current[1] <= maxd[1]; ++current[1])
				{
					for (; current[2] <= maxd[2]; ++current[2])
					{
						size_t hash_value = KeyHash::hash(current);
						size_t index = hash_value % CellCount;

						const cell_type& cell = m_cells[index];

						if (cell.query != m_query)
						{
							cell.query = m_query;
							const items_type& items = cell.items;

							typename items_type::const_iterator it = items.begin();
							typename items_type::const_iterator end = items.end();

							for (; it != end; ++it)
							{
								if ((position(*it) - center).len2() <= radius_sq)
								{
									container.push_back(*it);
									++count;
								}
							}
						}
					}
					current[2] = mind[2];
				}
				current[1] = mind[1];
			}
		}

		return count;
	}

	template<typename Container>
	uint32 query_sphere_distance(const key_type& center, const key_value& radius, Container& container) const
	{
		uint32 count = 0;

		if (!empty())
		{
			++m_query;

			key_type minc(center - key_type(radius));
			key_type maxc(center + key_type(radius));

			discreet_type mind = KeyHash::discreet(minc);
			discreet_type maxd = KeyHash::discreet(maxc);
			discreet_type current = mind;

			float radius_sq = radius * radius;

			for (; current[0] <= maxd[0]; ++current[0])
			{
				for (; current[1] <= maxd[1]; ++current[1])
				{
					for (; current[2] <= maxd[2]; ++current[2])
					{
						size_t hash_value = KeyHash::hash(current);
						size_t index = hash_value % CellCount;

						const cell_type& cell = m_cells[index];

						if (cell.query != m_query)
						{
							cell.query = m_query;
							const items_type& items = cell.items;

							typename items_type::const_iterator it = items.begin();
							typename items_type::const_iterator end = items.end();

							for (; it != end; ++it)
							{
								float distance_sq = (position(*it) - center).len2();
								if (distance_sq <= radius_sq)
								{
									container.push_back(std::make_pair(distance_sq, *it));
									++count;
								}
							}
						}
					}
					current[2] = mind[2];
				}
				current[1] = mind[1];
			}
		}

		return count;
	}

	template<typename Container>
	uint32 query_box(const key_type& minc, const key_type& maxc, Container& container) const
	{
		uint32 count = 0;

		if (!empty())
		{
			++m_query;

			discreet_type mind = KeyHash::discreet(minc);
			discreet_type maxd = KeyHash::discreet(maxc);
			discreet_type current = mind;

			for (; current[0] <= maxd[0]; ++current[0])
			{
				for (; current[1] <= maxd[1]; ++current[1])
				{
					for (; current[2] <= maxd[2]; ++current[2])
					{
						size_t hash_value = KeyHash::hash(current);
						size_t index = hash_value % CellCount;

						const cell_type& cell = m_cells[index];

						if (cell.query != m_query)
						{
							cell.query = m_query;
							const items_type& items = cell.items;

							typename items_type::const_iterator it = items.begin();
							typename items_type::const_iterator end = items.end();

							for (; it != end; ++it)
							{
								key_type pos = position(*it);

								if (pos[0] >= minc[0] &&
								    pos[1] >= minc[1] &&
								    pos[2] >= minc[2] &&
								    pos[0] <= maxc[0] &&
								    pos[1] <= maxc[1] &&
								    pos[2] <= maxc[2])
								{
									container.push_back(*it);
									++count;
								}
							}
						}
					}
					current[2] = mind[2];
				}
				current[1] = mind[1];
			}
		}

		return count;
	}

protected:
	position_retriever_type position;
	cells_type              m_cells;
	uint32                  m_count;
	mutable uint32          m_query;
};

#endif //__HashGrid_h__
