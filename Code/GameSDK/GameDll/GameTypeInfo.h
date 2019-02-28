// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GAME_TYPE_INFO_H__
#define __GAME_TYPE_INFO_H__

#if _MSC_VER > 1000
# pragma once
#endif


/*
* NOTE: It is not yet safe to access CGameTypeInfo internal data while
* the application is still initializing CRT.
*/



class CGameTypeInfo
{
public:
	CGameTypeInfo(const char* name, const CGameTypeInfo* pParent)
		:	m_pParent(pParent)
		,	m_name(name) {}

	const CGameTypeInfo* GetParent() const {return m_pParent;}
	bool IsCastable(const CGameTypeInfo* type) const;
	const char* GetName() const {return m_name;}

private:
	const CGameTypeInfo* m_pParent;
	const char* m_name;
};


#define CRY_INTERFACE_GTI(Type)													\
	static const CGameTypeInfo* GetStaticType();					\
	virtual const CGameTypeInfo* GetRunTimeType() const = 0;

#define CRY_DECLARE_GTI_BASE(Type)														\
	static const CGameTypeInfo* GetStaticType();					\
	virtual const CGameTypeInfo* GetRunTimeType() const;

#define CRY_DECLARE_GTI(Type)														\
	static const CGameTypeInfo* GetStaticType();					\
	virtual const CGameTypeInfo* GetRunTimeType() const override;

#define CRY_IMPLEMENT_GTI_BASE(Type)															\
	namespace { static CGameTypeInfo typeInfo ## Type(#Type, 0); }	\
	const CGameTypeInfo* Type::GetStaticType()											\
	{																																\
		return &typeInfo ## Type;																			\
	}																																\
	const CGameTypeInfo* Type::GetRunTimeType() const {return Type::GetStaticType();}

#define CRY_IMPLEMENT_GTI(Type, BaseType)																									\
	namespace { static CGameTypeInfo typeInfo ## Type(#Type, BaseType::GetStaticType()); }	\
	const CGameTypeInfo* Type::GetStaticType()																							\
	{																																												\
		return &typeInfo ## Type;																															\
	}																																												\
	const CGameTypeInfo* Type::GetRunTimeType() const {return Type::GetStaticType();}



template<typename LeftType, typename RightType>
struct crygti_cast_impl
{
	LeftType operator() (RightType rhv)
	{
		return static_cast<LeftType>(rhv);
	}
};



template<typename LeftType, typename RightType>
struct crygti_cast_impl<LeftType*, RightType*>
{
	LeftType* operator() (RightType* rhv)
	{
		if (rhv && rhv->GetRunTimeType()->IsCastable(LeftType::GetStaticType()))
			return static_cast<LeftType*>(rhv);
		return 0;
	}
};


template<typename LeftType, typename RightType>
LeftType crygti_cast(RightType rhv)
{
	crygti_cast_impl<LeftType, RightType> impl;
	return impl(rhv);
}





template<typename TestType, typename ThisType>
struct crygti_isof_impl
{
	bool operator() (ThisType rhv)
	{
		return rhv.GetRunTimeType()->IsCastable(TestType::GetStaticType());
	}
};



template<typename TestType, typename ThisType>
struct crygti_isof_impl<TestType, ThisType*>
{
	bool operator() (ThisType* rhv)
	{
		if (rhv)
			return rhv->GetRunTimeType()->IsCastable(TestType::GetStaticType());
		return false;
	}
};



template<typename TestType, typename ThisType>
bool crygti_isof(ThisType obj)
{
	crygti_isof_impl<TestType, ThisType> impl;
	return impl(obj);
}



#endif
