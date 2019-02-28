// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

//////////////////////////////////////////////////////////////////////////
//! A helper class for an automatic registration
template<typename T>
class CAutoRegister
{
public:
	CAutoRegister(const std::function<void()>& registerFunction, const std::function<void()>& unregisterFunction)
		: m_RegisterFunction(registerFunction)
		, m_UnregisterFunction(unregisterFunction)
		, m_pNext(nullptr)
		, m_pPrevious(nullptr)
	{
		AddToList();
	}

	CAutoRegister(const std::function<void()>& registerFunction)
		: m_RegisterFunction(registerFunction)
		, m_UnregisterFunction()
		, m_pNext(nullptr)
		, m_pPrevious(nullptr)
	{
		AddToList();
	}

	~CAutoRegister()
	{
		Unregister();
		RemoveFromList();
	}

	static void RegisterAll()
	{
		CAutoRegister* pHelper = CAutoRegister::s_pFirst;

		while (pHelper)
		{
			pHelper->Register();
			pHelper = pHelper->m_pNext;
		}
	}

	static void UnregisterAll()
	{
		CAutoRegister* pHelper = CAutoRegister::s_pFirst;

		while (pHelper)
		{
			pHelper->Unregister();
			pHelper = pHelper->m_pNext;
		}
	}

private:

	void AddToList()
	{
		if (!s_pLast)
		{
			s_pFirst = this;
			m_pPrevious = nullptr;
		}
		else
		{
			s_pLast->m_pNext = this;
			m_pPrevious = s_pLast;
		}

		s_pLast = this;
	}

	void RemoveFromList()
	{
		if (m_pPrevious)
		{
			m_pPrevious->m_pNext = m_pNext;

			if (m_pNext)
			{
				m_pNext->m_pPrevious = m_pPrevious;
			}

		}
		else
		{
			s_pFirst = m_pNext;

			if (m_pNext)
			{
				m_pNext->m_pPrevious = nullptr;
			}
		}

		if (s_pLast == this)
		{
			s_pLast = m_pPrevious;
		}

		m_pPrevious = nullptr;
		m_pNext = nullptr;
	}

	void Register()
	{
		if (m_RegisterFunction)
		{
			m_RegisterFunction();
			m_RegisterFunction = std::function<void()>();
		}
	}

	void Unregister()
	{
		// Only unregister if Register has been called and invalidated m_RegisterFunction.
		if (m_UnregisterFunction && !m_RegisterFunction)
		{
			m_UnregisterFunction();
			m_UnregisterFunction = std::function<void()>();
		}
	}

	std::function<void()>    m_RegisterFunction;
	std::function<void()>    m_UnregisterFunction;

	CAutoRegister<T>*        m_pNext;
	CAutoRegister<T>*        m_pPrevious;

	static CAutoRegister<T>* s_pFirst;
	static CAutoRegister<T>* s_pLast;
};

template<typename T> CAutoRegister<T>* CAutoRegister<T>::s_pLast = nullptr;
template<typename T> CAutoRegister<T>* CAutoRegister<T>::s_pFirst = nullptr;

