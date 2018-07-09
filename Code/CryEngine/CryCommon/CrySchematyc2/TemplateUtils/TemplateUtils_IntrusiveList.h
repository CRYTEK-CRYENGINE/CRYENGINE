// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "TemplateUtils_TypeUtils.h"

#ifdef _DEBUG
#define TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG 1
#else
#define TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG 0
#endif

namespace TemplateUtils
{
	namespace Private
	{
		class intrusive_list_base;
	}

	class intrusive_list_node
	{
		friend class Private::intrusive_list_base;

	public:

		inline intrusive_list_node()
			: m_prev(nullptr)
			, m_next(nullptr)
		{}

		inline intrusive_list_node(intrusive_list_node* prev, intrusive_list_node* next)
			: m_prev(prev)
			, m_next(next)
		{}

		inline ~intrusive_list_node()
		{
			if(m_prev)
			{
				m_prev->m_next = m_next;
			}
			if(m_next)
			{
				m_next->m_prev = m_prev;
			}
		}

		inline intrusive_list_node *prev()
		{
			return m_prev;
		}

		inline const intrusive_list_node *prev() const
		{
			return m_prev;
		}

		inline intrusive_list_node *next()
		{
			return m_next;
		}

		inline const intrusive_list_node *next() const
		{
			return m_next;
		}

	private:

		intrusive_list_node* m_prev;
		intrusive_list_node* m_next;
	};

	namespace Private
	{
		class intrusive_list_base
		{
		protected:

			inline bool insert(intrusive_list_node &node, intrusive_list_node &next)
			{
				if(node.m_prev || node.m_next)
				{
					return false;
				}
				node.m_prev         = next.m_prev;
				node.m_next         = &next;
				next.m_prev->m_next = &node;
				next.m_prev         = &node;
				return true;
			}

			inline void erase(intrusive_list_node &node)
			{
				if(node.m_prev)
				{
					node.m_prev->m_next = node.m_next;
				}
				if(node.m_next)
				{
					node.m_next->m_prev = node.m_prev;
				}
				node.m_prev = nullptr;
				node.m_next = nullptr;
			}
		};
	}

	template <class VALUE, intrusive_list_node VALUE::*MEMBER> class intrusive_list : private Private::intrusive_list_base
	{
	public:

		typedef VALUE        value_type;
		typedef VALUE&       reference;
		typedef const VALUE& const_reference;
		typedef VALUE*       pointer;
		typedef const VALUE* const_pointer;

	private:

		class member_node : public intrusive_list_node
		{
			friend class intrusive_list;

		public:

			inline member_node() {}

			inline member_node(member_node* _prev, member_node* _next)
				: intrusive_list_node(_prev, _next)
			{}

			inline pointer value()
			{
				return reinterpret_cast<pointer>((uint8*)(this) - TemplateUtils::GetMemberOffset<value_type, intrusive_list_node>(MEMBER));
			}

			inline const_pointer value() const
			{
				return reinterpret_cast<const_pointer>((const uint8*)(this) - TemplateUtils::GetMemberOffset<value_type, intrusive_list_node>(MEMBER));
			}

			inline member_node* prev()
			{
				return static_cast<member_node*>(intrusive_list_node::prev());
			}

			inline const member_node* prev() const
			{
				return static_cast<const member_node*>(intrusive_list_node::prev());
			}

			inline member_node* next()
			{
				return static_cast<member_node*>(intrusive_list_node::next());
			}

			inline const member_node* next() const
			{
				return static_cast<const member_node*>(intrusive_list_node::next());
			}

			static inline member_node& from_value(value_type &value)
			{
				return *reinterpret_cast<member_node*>(&(value.*MEMBER));
			}

			static inline const member_node& from_value(const value_type &value)
			{
				return *reinterpret_cast<const member_node*>(&(value.*MEMBER));
			}
		};

	public:

		class const_iterator;

		class iterator
		{
			friend class intrusive_list;
			friend class const_iterator;

		public:

			inline iterator()
				: m_node(nullptr)
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				, m_root(nullptr)
#endif
			{}

			inline iterator(const iterator &rhs)
				: m_node(rhs.m_node)
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				, m_root(rhs.m_root)
#endif
			{}

			inline reference operator * () const
			{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				CRY_ASSERT(m_node);
				CRY_ASSERT(m_node != m_root);
#endif
				return *m_node->value();
			}

			inline pointer operator -> () const
			{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				CRY_ASSERT(m_node);
				CRY_ASSERT(m_node != m_root);
#endif
				return m_node->value();
			}

			inline void operator = (const iterator &rhs)
			{
				m_node = rhs.m_node;
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				m_root = rhs.m_root;
#endif
			}

			inline iterator& operator ++ ()
			{
				m_node = m_node->next();
				return *this;
			}

			inline iterator operator ++ (int)
			{
				member_node* prev_node = m_node;
				m_node = m_node->next();
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				return iterator(prev_node, m_root);
#else
				return iterator(prev_node);
#endif
			}

			inline iterator& operator -- ()
			{
				m_node = m_node->prev();
				return *this;
			}

			inline iterator operator -- (int)
			{
				member_node* prev_node = m_node;
				m_node = m_node->prev();
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				return iterator(prev_node, m_root);
#else
				return iterator(prev_node);
#endif
			}

			inline bool operator == (const iterator& rhs) const
			{
				return m_node == rhs.m_node;
			}

			inline bool operator != (const iterator& rhs) const
			{
				return m_node != rhs.m_node;
			}

		private:

#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			inline iterator(member_node* node, const member_node* root)
				: m_node(node)
				, m_root(root)
#else
			inline iterator(member_node* node)
				: m_node(node)
#endif
			{}

			member_node*       m_node;
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			const member_node* m_root;
#endif
		};

		class const_iterator
		{
			friend class intrusive_list;

		public:

			inline const_iterator()
				: m_node(nullptr)
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				, m_root(nullptr)
#endif
			{}

			inline const_iterator(const iterator &rhs)
				: m_node(rhs.m_node)
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				, m_root(rhs.m_root)
#endif
			{}

			inline const_iterator(const const_iterator &rhs)
				: m_node(rhs.m_node)
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				, m_root(rhs.m_root)
#endif
			{}

			inline const_reference operator * () const
			{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				CRY_ASSERT(m_node);
				CRY_ASSERT(m_node != m_root);
#endif
				return *m_node->value();
			}

			inline const_pointer operator -> () const
			{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				CRY_ASSERT(m_node);
				CRY_ASSERT(m_node != m_root);
#endif
				return m_node->value();
			}

			inline void operator = (const iterator &rhs)
			{
				m_node = rhs.m_node;
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				m_root = rhs.m_root;
#endif
			}

			inline void operator = (const const_iterator &rhs)
			{
				m_node = rhs.m_node;
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				m_root = rhs.m_root;
#endif
			}

			inline const_iterator& operator ++ ()
			{
				m_node = m_node->next();
				return *this;
			}

			inline const_iterator operator ++ (int)
			{
				const member_node* prev_node = m_node;
				m_node = m_node->next();
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				return const_iterator(prev_node, m_root);
#else
				return const_iterator(prev_node);
#endif
			}

			inline const_iterator& operator -- ()
			{
				m_node = m_node->prev();
				return *this;
			}

			inline const_iterator operator -- (int)
			{
				const member_node* prev_node = m_node;
				m_node = m_node->prev();
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				return const_iterator(prev_node, m_root);
#else
				return const_iterator(prev_node);
#endif
			}

			inline bool operator == (const iterator &rhs) const
			{
				return m_node == rhs.m_node;
			}

			inline bool operator == (const const_iterator &rhs) const
			{
				return m_node == rhs.m_node;
			}

			inline bool operator != (const iterator &rhs) const
			{
				return m_node != rhs.m_node;
			}

			inline bool operator != (const const_iterator &rhs) const
			{
				return m_node != rhs.m_node;
			}

		private:

#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			inline const_iterator(const member_node* node, const member_node* root) 
				: m_node(node)
				, m_root(root)
#else
			inline const_iterator(const member_node* node) 
				: m_node(node)
#endif
			{}

			const member_node* m_node;
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			const member_node* m_root;
#endif
		};

		class const_reverse_iterator;

		class reverse_iterator
		{
			friend class intrusive_list;
			friend class const_reverse_iterator;

		public:

			inline reverse_iterator()
				: m_node(nullptr)
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				, m_root(nullptr)
#endif
			{}

			inline reverse_iterator(const reverse_iterator &rhs)
				: m_node(rhs.m_node)
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				, m_root(rhs.m_root)
#endif
			{}

			inline reference operator * () const
			{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				CRY_ASSERT(m_node);
				CRY_ASSERT(m_node != m_root);
#endif
				return *m_node->value();
			}

			inline pointer operator -> () const
			{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				CRY_ASSERT(m_node);
				CRY_ASSERT(m_node != m_root);
#endif
				return m_node->value();
			}

			inline void operator = (const reverse_iterator &rhs)
			{
				m_node = rhs.m_node;
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				m_root = rhs.m_root;
#endif
			}

			inline reverse_iterator& operator ++ ()
			{
				m_node = m_node->prev();
				return *this;
			}

			inline reverse_iterator operator ++ (int)
			{
				member_node* prev_node = m_node;
				m_node = m_node->prev();
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				return reverse_iterator(prev_node, m_root);
#else
				return reverse_iterator(prev_node);
#endif
			}

			inline reverse_iterator& operator -- ()
			{
				m_node = m_node->next();
				return *this;
			}

			inline reverse_iterator operator -- (int)
			{
				member_node* prev_node = m_node;
				m_node = m_node->next();
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				return reverse_iterator(prev_node, m_root);
#else
				return reverse_iterator(prev_node);
#endif
			}

			inline bool operator == (const reverse_iterator &rhs) const
			{
				return m_node == rhs.m_node;
			}

			inline bool operator != (const reverse_iterator &rhs) const
			{
				return m_node != rhs.m_node;
			}

		private:

#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			inline reverse_iterator(member_node* node, const member_node* root)
				: m_node(node)
				, m_root(root)
#else
			inline reverse_iterator(member_node* node)
				: m_node(node)
#endif
			{}

			member_node*       m_node;
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			const member_node* m_root;
#endif
		};

		class const_reverse_iterator
		{
			friend class intrusive_list;

		public:

			inline const_reverse_iterator()
				: m_node(nullptr)
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				, m_root(nullptr)
#endif
			{}

			inline const_reverse_iterator(const reverse_iterator &rhs)
				: m_node(rhs.m_node)
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				, m_root(rhs.m_root)
#endif
			{}

			inline const_reverse_iterator(const const_reverse_iterator &rhs)
				: m_node(rhs.m_node)
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				, m_root(rhs.m_root)
#endif
			{}

			inline const_reference operator * () const
			{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				CRY_ASSERT(m_node);
				CRY_ASSERT(m_node != m_root);
#endif
				return *m_node->value();
			}

			inline const_pointer operator -> () const
			{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				CRY_ASSERT(m_node);
				CRY_ASSERT(m_node != m_root);
#endif
				return m_node->value();
			}

			inline void operator = (const reverse_iterator &rhs)
			{
				m_node = rhs.m_node;
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				m_root = rhs.m_root;
#endif
			}

			inline void operator = (const const_reverse_iterator &rhs)
			{
				m_node = rhs.m_node;
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				m_root = rhs.m_root;
#endif
			}

			inline const_reverse_iterator& operator ++ ()
			{
				m_node = m_node->prev();
				return *this;
			}

			inline const_reverse_iterator operator ++ (int)
			{
				const member_node* prev_node = m_node;
				m_node = m_node->prev();
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				return const_reverse_iterator(prev_node, m_root);
#else
				return const_reverse_iterator(prev_node);
#endif
			}

			inline const_reverse_iterator& operator -- ()
			{
				m_node = m_node->next();
				return *this;
			}

			inline const_reverse_iterator operator -- (int)
			{
				const member_node* prev_node = m_node;
				m_node = m_node->next();
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
				return ConstReverseIterator(prev_node, m_root);
#else
				return ConstReverseIterator(prev_node);
#endif
			}

			inline bool operator == (const reverse_iterator &rhs) const
			{
				return m_node == rhs.m_node;
			}

			inline bool operator == (const const_reverse_iterator &rhs) const
			{
				return m_node == rhs.m_node;
			}

			inline bool operator != (const reverse_iterator &rhs) const
			{
				return m_node != rhs.m_node;
			}

			inline bool operator != (const const_reverse_iterator &rhs) const
			{
				return m_node != rhs.m_node;
			}

		private:

#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			inline const_reverse_iterator(const member_node* node, const member_node* root)
				: m_node(node)
				, m_root(root)
#else
			inline const_reverse_iterator(const member_node* node)
				: m_node(node)
#endif
			{}

			const member_node* m_node;
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			const member_node* m_root;
#endif	
		};

		inline intrusive_list()
			: m_root(&m_root, &m_root)
		{}

		inline ~intrusive_list()
		{
			clear();
		}

		inline bool push_front(value_type &value)
		{
			return intrusive_list_base::insert(member_node::from_value(value), *m_root.next());
		}

		inline bool push_back(value_type &value)
		{
			return intrusive_list_base::insert(member_node::from_value(value), m_root);
		}

		inline bool insert(const iterator& pos, value_type &value)
		{
			return intrusive_list_base::insert(member_node::from_value(value), *pos.m_node);
		}

		inline void pop_front()
		{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			CRY_ASSERT(m_root.next() != &m_root);
#endif
			intrusive_list_base::erase(*m_root.next());
		}

		inline void pop_back()
		{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			CRY_ASSERT(m_root.prev() != &m_root);
#endif
			intrusive_list_base::erase(*m_root.prev());
		}

		inline void erase(const iterator &pos)
		{
			if(pos.m_node != &m_root)
			{
				intrusive_list_base::erase(*pos.m_node);
			}
		}

		inline void erase(const iterator &begin, const iterator &end)
		{
			for(iterator pos = begin; pos != end;)
			{
				iterator	erase_pos = pos ++;
				intrusive_list_base::erase(*erase_pos.m_node);
			}
		}

		inline void remove(value_type &value)
		{
			intrusive_list_base::erase(member_node::from_value(value));
		}

		inline void clear()
		{
			erase(begin(), end());
		}

		inline bool empty() const
		{
			return m_root.prev() == m_root.next();
		}

		inline const iterator begin()
		{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			return iterator(m_root.next(), &m_root);
#else
			return iterator(m_root.next());
#endif
		}

		inline const const_iterator begin() const
		{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			return const_iterator(m_root.next(), &m_root);
#else
			return const_iterator(m_root.next());
#endif
		}

		inline const iterator end()
		{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			return iterator(&m_root, &m_root);
#else
			return iterator(&m_root);
#endif
		}

		inline const const_iterator end() const
		{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			return const_iterator(&m_root, &m_root);
#else
			return const_iterator(&m_root);
#endif
		}

		inline const reverse_iterator rbegin()
		{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			return reverse_iterator(m_root.prev(), &m_root);
#else
			return reverse_iterator(m_root.prev());
#endif
		}

		inline const const_reverse_iterator rbegin() const
		{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			return const_reverse_iterator(m_root.prev(), &m_root);
#else
			return const_reverse_iterator(m_root.prev());
#endif
		}

		inline const reverse_iterator rend()
		{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			return reverse_iterator(&m_root, &m_root);
#else
			return reverse_iterator(&m_root);
#endif
		}

		inline const const_reverse_iterator rend() const
		{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			return const_reverse_iterator(&m_root, &m_root);
#else
			return const_reverse_iterator(&m_root);
#endif
		}

		inline value_type& front()
		{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			CRY_ASSERT(m_root.next() != &m_root);
#endif
			return *m_root.next()->value();
		}

		inline const value_type& front() const
		{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			CRY_ASSERT(m_root.next() != &m_root);
#endif
			return *m_root.next()->value();
		}

		inline value_type& back()
		{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			CRY_ASSERT(m_root.prev() != &m_root);
#endif
			return *m_root.prev()->value();
		}

		inline const value_type& back() const
		{
#if TEMPLATE_UTILS_INTRUSIVE_LIST_DEBUG
			CRY_ASSERT(m_root.prev() != &m_root);
#endif
			return *m_root.prev()->value();
		}

	private:

		member_node m_root;
	};
}