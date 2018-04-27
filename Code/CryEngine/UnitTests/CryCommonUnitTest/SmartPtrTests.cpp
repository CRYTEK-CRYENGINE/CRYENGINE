// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <UnitTest.h>
#include <CryCore/smartptr.h>

bool gHasCalledDtor = false;

template<typename TargetType>
class I : public TargetType
{
public:
	virtual ~I() {
		gHasCalledDtor = true;
	}
	virtual int GetValue() const = 0;
};

template<typename TargetType>
class C : public I<TargetType>
{
	int value = 0;
public:

	C() = default;
	C(const C&) = default;
	explicit C(int _val) : value(_val) {}

	virtual int GetValue() const override
	{
		return value;
	}
};

TEST(CrySmartPtrTest, Basics)
{
	typedef _smart_ptr<I<_i_reference_target_t>> Ptr;
	gHasCalledDtor = false;
	{
		Ptr ptr = new C<_i_reference_target_t>();
		REQUIRE(ptr->UseCount() == 1);
		{
			Ptr ptr2 = ptr;
			REQUIRE(ptr->UseCount() == 2);
			REQUIRE(ptr2->UseCount() == 2);

			Ptr ptr3 = nullptr;
			ptr3 = ptr2;
			REQUIRE(ptr->UseCount() == 3);
			REQUIRE(ptr2->UseCount() == 3);
			REQUIRE(ptr3->UseCount() == 3);
		}
		REQUIRE(ptr->UseCount() == 1);
	}
	REQUIRE(gHasCalledDtor);
}

TEST(CrySmartPtrTest, Copy)
{
	typedef C<_i_reference_target_t> MyType;
	typedef _smart_ptr<MyType> Ptr;
	Ptr ptr1 = new MyType(42);
	Ptr ptr2 = new MyType(0x1337);
	Ptr ptr3 = ptr2;
	REQUIRE(ptr1->UseCount() == 1);
	REQUIRE(ptr2->UseCount() == 2);

	//when object pointed by _smart_ptr gets assigned, it should copy the value but not the refcount
	*ptr2 = *ptr1;
	REQUIRE(ptr1->UseCount() == 1);
	REQUIRE(ptr2->UseCount() == 2);
	REQUIRE(ptr1->GetValue() == 42);
	REQUIRE(ptr2->GetValue() == 42);

	//when a new object is copy constructed, it should copy the value but not the refcount
	_smart_ptr<I<_i_reference_target_t>> ptr4 = new MyType(*ptr2);
	REQUIRE(ptr4->GetValue() == 42);
	REQUIRE(ptr4->UseCount() == 1);
}