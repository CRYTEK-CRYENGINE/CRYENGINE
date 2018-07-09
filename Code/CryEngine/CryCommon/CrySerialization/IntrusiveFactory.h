// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/StlUtils.h>

template<class TBase>
class CIntrusiveFactory
{
private:
	struct ICreator
	{
		virtual TBase* Create() const = 0;
	};

public:
	template<class TDerived>
	struct SCreator : ICreator
	{
		SCreator() { CIntrusiveFactory<TBase>::Instance().template RegisterType<TDerived>(this); }

		TBase* Create() const override { return new TDerived(); }
	};

	static CIntrusiveFactory& Instance() { static CIntrusiveFactory instance; return instance; }

	TBase*                    Create(const char* keyType) const
	{
		typename TCreatorByType::const_iterator it = m_creators.find(keyType);
		if (it == m_creators.end() || it->second == 0)
			return 0;
		else
			return it->second->Create();
	}

	struct SSerializer
	{
		_smart_ptr<TBase>& pointer;

		SSerializer(_smart_ptr<TBase>& pointer) : pointer(pointer) {}

		void Serialize(Serialization::IArchive& ar);
	};

private:
	template<class TDerived>
	void RegisterType(ICreator* creator)
	{
		const char* type = TDerived::GetType();
		m_creators[type] = creator;
	}

	typedef std::map<const char*, ICreator*, stl::less_strcmp<const char*>> TCreatorByType;
	TCreatorByType m_creators;
};

template<class TBase>
void CIntrusiveFactory<TBase>::SSerializer::Serialize(Serialization::IArchive & ar)
{
	string type = pointer.get() ? pointer->GetInstanceType() : "";
	string oldType = type;
	ar(type, "type", "Type");
	if (ar.isInput())
	{
		if (oldType != type)
			pointer.reset(CIntrusiveFactory<TBase>::Instance().Create(type.c_str()));
	}
	if (pointer)
		pointer->Serialize(ar);
}

#define REGISTER_IN_INTRUSIVE_FACTORY(BaseType, DerivedType) namespace { CIntrusiveFactory<BaseType>::SCreator<DerivedType> baseType ## DerivedType ## _Creator; }
