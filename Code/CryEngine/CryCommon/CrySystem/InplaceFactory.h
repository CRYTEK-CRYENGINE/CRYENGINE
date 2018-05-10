// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// Helper to enable inplace construction and destruction of objects
//
// History:
// Thu Apr 22 11:10:12 2010: Created by Chris Raine
//
/////////////////////////////////////////////////////////////////////////////
#ifndef InplaceFactory_H
#define InplaceFactory_H

// Inspired by the boost inplace/typed_inplace factory, written by
// Fernando Luis Cacciola Carballal and Tobias Schwinger
//
// See
// http://www.boost.org/doc/libs/1_42_0/libs/utility/in_place_factories.html
// for a detailed description
//

class CInplaceFactory0
{

public:

	explicit CInplaceFactory0()
	{}

	template<class T>
	void* apply(void* address) const
	{
		return new(address) T();
	}

	template<class T>
	void* apply(void* address, std::size_t n) const
	{
		for (char* next = address = this->template apply<T>(address); !!--n; )
			this->template apply<T>(next = next + sizeof(T));
		return address;
	}
};

template<typename Arg0>
class CInplaceFactory1
{
	Arg0& m_Arg0;

public:

	explicit CInplaceFactory1(Arg0& arg)
		: m_Arg0(arg)
	{}

	template<class T>
	void* apply(void* address) const
	{
		return new(address) T(m_Arg0);
	}

	template<class T>
	void* apply(void* address, std::size_t n) const
	{
		for (char* next = address = this->template apply<T>(address); !!--n; )
			this->template apply<T>(next = next + sizeof(T));
		return address;
	}
};

template
<
  typename Arg0,
  typename Arg1
>
class CInplaceFactory2
{
	Arg0& m_Arg0;
	Arg1& m_Arg1;

public:

	explicit CInplaceFactory2(Arg0& arg0, Arg1& arg1)
		: m_Arg0(arg0),
		m_Arg1(arg1)
	{}

	template<class T>
	void* apply(void* address) const
	{
		return new(address) T(
		  m_Arg0,
		  m_Arg1);
	}

	template<class T>
	void* apply(void* address, std::size_t n) const
	{
		for (char* next = address = this->template apply<T>(address); !!--n; )
			this->template apply<T>(next = next + sizeof(T));
		return address;
	}
};

template
<
  typename Arg0,
  typename Arg1,
  typename Arg2
>
class CInplaceFactory3
{
	Arg0& m_Arg0;
	Arg1& m_Arg1;
	Arg2& m_Arg2;

public:

	explicit CInplaceFactory3(Arg0& arg0, Arg1& arg1, Arg2& arg2)
		: m_Arg0(arg0),
		m_Arg1(arg1),
		m_Arg2(arg2)
	{}

	template<class T>
	void* apply(void* address) const
	{
		return new(address) T(
		  m_Arg0,
		  m_Arg1,
		  m_Arg2);
	}

	template<class T>
	void* apply(void* address, std::size_t n) const
	{
		for (char* next = address = this->template apply<T>(address); !!--n; )
			this->template apply<T>(next = next + sizeof(T));
		return address;
	}
};

template
<
  typename Arg0,
  typename Arg1,
  typename Arg2,
  typename Arg3
>
class CInplaceFactory4
{
	Arg0& m_Arg0;
	Arg1& m_Arg1;
	Arg2& m_Arg2;
	Arg3& m_Arg3;

public:

	explicit CInplaceFactory4(Arg0& arg0, Arg1& arg1, Arg2& arg2, Arg3& arg3)
		: m_Arg0(arg0),
		m_Arg1(arg1),
		m_Arg2(arg2),
		m_Arg3(arg3)
	{}

	template<class T>
	void* apply(void* address) const
	{
		return new(address) T(
		  m_Arg0,
		  m_Arg1,
		  m_Arg2,
		  m_Arg3);
	}

	template<class T>
	void* apply(void* address, std::size_t n) const
	{
		for (char* next = address = this->template apply<T>(address); !!--n; )
			this->template apply<T>(next = next + sizeof(T));
		return address;
	}
};

template
<
  typename Arg0,
  typename Arg1,
  typename Arg2,
  typename Arg3,
  typename Arg4
>
class CInplaceFactory5
{
	Arg0& m_Arg0;
	Arg1& m_Arg1;
	Arg2& m_Arg2;
	Arg3& m_Arg3;
	Arg4& m_Arg4;

public:

	explicit CInplaceFactory5(Arg0& arg0, Arg1& arg1, Arg2& arg2, Arg3& arg3, Arg4& arg4)
		: m_Arg0(arg0),
		m_Arg1(arg1),
		m_Arg2(arg2),
		m_Arg3(arg3),
		m_Arg4(arg4)
	{}

	template<class T>
	void* apply(void* address) const
	{
		return new(address) T(
		  m_Arg0,
		  m_Arg1,
		  m_Arg2,
		  m_Arg3,
		  m_Arg4);
	}

	template<class T>
	void* apply(void* address, std::size_t n) const
	{
		for (char* next = address = this->template apply<T>(address); !!--n; )
			this->template apply<T>(next = next + sizeof(T));
		return address;
	}
};

inline CInplaceFactory0 InplaceFactory()
{
	return CInplaceFactory0();
}

template<typename Arg0>
inline CInplaceFactory1<Arg0> InplaceFactory(Arg0& arg0)
{
	return CInplaceFactory1<Arg0>(arg0);
}

template
<
  typename Arg0,
  typename Arg1

>
inline CInplaceFactory2<Arg0, Arg1> InplaceFactory(Arg0& arg0, Arg1& arg1)
{
	return CInplaceFactory2<Arg0, Arg1>(arg0, arg1);
}

template
<
  typename Arg0,
  typename Arg1,
  typename Arg2
>
inline CInplaceFactory3<Arg0, Arg1, Arg2> InplaceFactory(Arg0& arg0, Arg1& arg1, Arg2& arg2)
{
	return CInplaceFactory3<Arg0, Arg1, Arg2>(arg0, arg1, arg2);
}

template
<
  typename Arg0,
  typename Arg1,
  typename Arg2,
  typename Arg3
>
inline CInplaceFactory4<Arg0, Arg1, Arg2, Arg3> InplaceFactory(
  Arg0& arg0, Arg1& arg1, Arg2& arg2, Arg3& arg3)
{
	return CInplaceFactory4<Arg0, Arg1, Arg2, Arg3>(arg0, arg1, arg2, arg3);
}

template
<
  typename Arg0,
  typename Arg1,
  typename Arg2,
  typename Arg3,
  typename Arg4
>
inline CInplaceFactory5<Arg0, Arg1, Arg2, Arg3, Arg4> InplaceFactory(
  Arg0& arg0, Arg1& arg1, Arg2& arg2, Arg3& arg3, Arg4& arg4)
{
	return CInplaceFactory5<Arg0, Arg1, Arg2, Arg3, Arg4>(arg0, arg1, arg2, arg3, arg4);
}

#endif // #infndef InplaceFactory_H
