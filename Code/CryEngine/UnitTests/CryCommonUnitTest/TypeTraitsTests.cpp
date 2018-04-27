// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! Meta-programming unit tests are implemented as compile-time checks
//! Therefore none of these tests will appear in the unit test system
//! But the tests still silently guard the correctness of the tested libraries

#include <CrySTL/type_traits.h>

struct IArchive
{
	virtual ~IArchive() {}

	//forbids instantiation of the class for demonstrating the usage of std::declval
	virtual void PureVirtual() = 0; 
};

class CSerializableClass
{
public:
	void Serialize(IArchive& ar);
};

class CNonSerializableClass
{
};

template<class T> using SerializationFunc = decltype( std::declval<T>().Serialize( std::declval<IArchive&>() ) );

static_assert(stl::is_detected<SerializationFunc, CSerializableClass>::value, "stl::is_detected should correctly detect the existence of the function");
static_assert(!stl::is_detected<SerializationFunc, CNonSerializableClass>::value, "stl::is_detected should correctly detect the non-existence of the function");