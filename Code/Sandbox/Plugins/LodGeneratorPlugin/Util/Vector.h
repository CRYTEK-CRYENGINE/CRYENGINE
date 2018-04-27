#pragma once

#include <vector>
#include "Allocator.h"

namespace LODGenerator 
{
	template<class T>
	class CVector 
	{
	public:		
		CVector() 
		{
		}

		CVector(uint size) 
		{ 
			m_List.resize(size); 
		}

		void resize(uint size) 
		{ 
			m_List.resize(size); 
		}

		void resize(uint size,T v) 
		{ 
			m_List.resize(size,v); 
		}

		CVector(const CVector& rhs)
		{
			m_List.clear();
			for (int i=0;i<v.size();i++)
			{
				m_List.push_back(v[i]);
			}
		}

		void allocate(unsigned int n)
		{
			clear();
			m_List.reserve(n);
		}

		void zero()
		{
			for (std::vector<T>::iterator iter = m_List.begin();iter != m_List.end();iter++)
			{
				*iter = 0;
			}
		}

		void operator += (const CVector& v)
		{
			ASSERT(m_List.size() != v.size());
			for (int i=0;i<m_List.size();i++)
			{
				m_List[i] += v[i];
			}
		}

		void operator -= (const CVector& v)
		{
			ASSERT(m_List.size() != v.size());
			for (int i=0;i<m_List.size();i++)
			{
				m_List[i] -= v[i];
			}
		}

		void operator=(const CVector& v)
		{
			ASSERT(m_List.size() != v.size());
			for (int i=0;i<m_List.size();i++)
			{
				m_List[i] -= v[i];
			}
		}

		T& operator [] (int seat)
		{
			ASSERT(seat < m_List.size());
			return m_List[seat];
		}

		T operator [] (int seat) const
		{
			ASSERT(seat < m_List.size());
			return m_List[seat];
		}

		T& operator () (int seat)
		{
			ASSERT(seat < m_List.size());
			return m_List[seat];
		}

		T operator () (int seat) const
		{
			ASSERT(seat < m_List.size());
			return m_List[seat];
		}

		int size() const
		{
			return m_List.size();
		}

		bool has_nan()
		{
			for(unsigned int i=0; i<m_List.size(); i++) {
				if(_isnan(m_List[i])) {
					return true ;
				}
			}
			return false ;
		}

		T* data()
		{
			return m_List.data();
		}

		const T* data() const
		{
			return m_List.data();
		}

		void clear()
		{
			m_List.clear();
		}

	public: 
		std::vector<T> m_List;
	} ;

	typedef CVector<double> CVectorD;
	typedef CVector<int> CVectorI;
} 

