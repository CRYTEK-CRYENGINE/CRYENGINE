// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// TODO : Create specialized CStringView class for strings?

#pragma once

namespace TemplateUtils
{
	// Array view.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	template <typename TYPE> class CArrayView
	{
	public:

		inline CArrayView()
			: m_pData(nullptr)
			, m_size(0)
		{}

		inline CArrayView(TYPE &ref)
			: m_pData(&ref)
			, m_size(1)
		{}

		template <size_t SIZE> inline CArrayView(TYPE (&pData)[SIZE])
			: m_pData(pData)
			, m_size(SIZE)
		{}

		inline CArrayView(std::vector<TYPE>& vector)
			: m_pData(vector.empty() ? nullptr : &vector.front())
			, m_size(vector.empty() ? 0 : vector.size())
		{}

		inline CArrayView(TYPE* pData, size_t size)
			: m_pData(pData)
			, m_size(pData ? size : 0)
		{}

		inline CArrayView(const CArrayView<TYPE>& rhs)
			: m_pData(rhs.m_pData)
			, m_size(rhs.m_size)
		{}

		inline size_t size() const
		{
			return m_size;
		}

		inline bool empty() const
		{
			return !m_size;
		}

		inline TYPE* begin() const
		{
			return m_pData;
		}

		inline TYPE* end() const
		{
			return m_pData + m_size;
		}

		inline TYPE& operator [] (size_t idx) const
		{
			CRY_ASSERT(idx < m_size);
			return m_pData[idx];
		}

	private:

		TYPE*  m_pData;
		size_t m_size;
	};

	// Partial template specialization of array view for const types.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	template <typename TYPE> class CArrayView<const TYPE>
	{
	public:

		inline CArrayView()
			: m_pData(nullptr)
			, m_size(0)
		{}

		inline CArrayView(const TYPE &ref)
			: m_pData(&ref)
			, m_size(1)
		{}

		template <size_t SIZE> inline CArrayView(const TYPE (&pData)[SIZE])
			: m_pData(pData)
			, m_size(SIZE)
		{}

		inline CArrayView(const std::vector<TYPE>& vector)
			: m_pData(vector.empty() ? nullptr : &vector.front())
			, m_size(vector.empty() ? 0 : vector.size())
		{}

		inline CArrayView(const TYPE* pData, size_t size)
			: m_pData(pData)
			, m_size(pData ? size : 0)
		{}

		inline CArrayView(const CArrayView<TYPE>& rhs)
			: m_pData(rhs.begin())
			, m_size(rhs.size())
		{}

		inline CArrayView(const CArrayView<const TYPE>& rhs)
			: m_pData(rhs.m_pData)
			, m_size(rhs.m_size)
		{}

		inline size_t size() const
		{
			return m_size;
		}

		inline bool empty() const
		{
			return !m_size;
		}

		inline const TYPE* begin() const
		{
			return m_pData;
		}

		inline const TYPE* end() const
		{
			return m_pData + m_size;
		}

		inline const TYPE& operator [] (size_t idx) const
		{
			CRY_ASSERT(idx < m_size);
			return m_pData[idx];
		}

	private:

		const TYPE* m_pData;
		size_t      m_size;
	};
}
