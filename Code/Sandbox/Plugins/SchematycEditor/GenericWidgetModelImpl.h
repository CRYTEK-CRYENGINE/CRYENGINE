// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <CrySchematyc2/GUID.h>
#include <CrySchematyc2/Script/IScriptFile.h>

#include "Util.h"
#include "GenericWidgetModel.h"

//////////////////////////////////////////////////////////////////////////
namespace Cry {	
namespace SchematycEd {

//////////////////////////////////////////////////////////////////////////
template <class T>
struct CGenericWidgetDictionaryModel::Unit : public virtual CGenericWidgetDictionaryModel
{
	template <typename V> void Visit(const V& v)
	{
		LoadScriptElementType<T>(std::get<0>(v), std::get<1>(v));
	}
};

//////////////////////////////////////////////////////////////////////////
template <class T>
struct CGenericWidgetDictionaryModel::Impl : public CHierarchy<typename T::Typelist, CGenericWidgetDictionaryModel::Unit>
{
	virtual const char* GetName() const override
	{
		return T::Name();
	}

	virtual void BuildFromScriptClass(IScriptFile* file, const SGUID& classGUID) override
	{
		CHierarchy<typename T::Typelist, CGenericWidgetDictionaryModel::Unit>::Visit(std::make_tuple(file, classGUID));
	}
};

//////////////////////////////////////////////////////////////////////////
template <typename T, size_t gridx, size_t gridy> struct CCategoryTraits;

#define DECL_CATEGORY(name, gridx, gridy, typelist) \
	template <> struct CCategoryTraits<typelist, gridx, gridy> \
	{ \
		using Category             = CGenericWidgetDictionaryModel::Impl<CCategoryTraits<typelist, gridx, gridy>>; \
		using Typelist             = typelist; \
		static const size_t GRID_X = gridx; \
		static const size_t GRID_Y = gridy; \
		static const char* Name() { return #name; } \
	}; \
	using C##name##CategoryTraits = CCategoryTraits<typelist, gridx, gridy>; \
	using C##name##CategoryModel  = typename CCategoryTraits<typelist, gridx, gridy>::Category; \


//////////////////////////////////////////////////////////////////////////
DECL_CATEGORY(Types,      0, 0, TYPELIST_2( IScriptStructure, IScriptEnumeration ))
DECL_CATEGORY(Graphs,     1, 0, TYPELIST_1( IDocGraph ))
DECL_CATEGORY(Signals,    2, 0, TYPELIST_1( IScriptSignal ))
DECL_CATEGORY(Variables,  0, 1, TYPELIST_2( IScriptTimer, IScriptVariable ))
DECL_CATEGORY(Components, 1, 1, TYPELIST_3( IScriptState, IScriptStateMachine, IScriptComponentInstance ))

} //namespace SchematycEd
} //namespace Cry
