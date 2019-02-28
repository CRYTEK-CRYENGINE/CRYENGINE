// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// TODO : Consider renaming CDelegateContextAllocator to CAbstractAllocator and moving to separate file, it might be useful.
// TODO : Can we avoid heap allocation when using lambda functions?

#pragma once

#include "TemplateUtils_PreprocessorUtils.h"
#include "TemplateUtils_TypeUtils.h"

#define DELEGATE_FASTCALL __fastcall

namespace TemplateUtils
{
	// Delegate class.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	template <typename SIGNATURE> class CDelegate;

	// Helper for constructing delegates.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	template <typename FUNCTION_PTR_TYPE, FUNCTION_PTR_TYPE FUNCTION_PTR> struct SMakeDelegate;

	// Delegate context allocator. This class abstracts the allocation, cloning and destruction of
	// objects of any type.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CDelegateContextAllocator
	{
	public:

		inline CDelegateContextAllocator()
			: m_pStub(nullptr)
		{}

		inline CDelegateContextAllocator(const CDelegateContextAllocator& rhs)
			: m_pStub(rhs.m_pStub)
		{}

		template <typename TYPE> inline void SetType()
		{
			m_pStub = Stub<TYPE>;
		}

		inline void* Clone(const void* pContext) const
		{
			if(m_pStub)
			{
				return m_pStub(ECommand::Clone, const_cast<void*>(pContext));
			}
			return nullptr;
		}

		inline void Delete(void* pContext) const
		{
			if(m_pStub)
			{
				m_pStub(ECommand::Delete, pContext);
			}
		}

		inline operator bool () const
		{
			return m_pStub != nullptr;
		}

	private:

		enum class ECommand
		{
			Clone,
			Delete
		};

		typedef void* (*StubPtr)(ECommand, void*);

		template <typename TYPE> static void* Stub(ECommand command, void* pContext)
		{
			switch(command)
			{
			case ECommand::Clone:
				{
					return new (CryModuleMalloc(sizeof(TYPE))) TYPE(*static_cast<const TYPE*>(pContext));
				}
			case ECommand::Delete:
				{
					static_cast<TYPE*>(pContext)->~TYPE();
					CryModuleFree(pContext);
					break;
				}
			}
			return nullptr;
		}

		StubPtr m_pStub;
	};
}

// Delegate template specializations.
////////////////////////////////////////////////////////////////////////////////////////////////////
#define INCLUDING_FROM_TEMPLATE_UTILS_DELEGATE_HEADER

#define PARAM_COUNT 0
#include "TemplateUtils_DelegateImpl.h"
#undef PARAM_COUNT

#define PARAM_COUNT 1
#include "TemplateUtils_DelegateImpl.h"
#undef PARAM_COUNT

#define PARAM_COUNT 2
#include "TemplateUtils_DelegateImpl.h"
#undef PARAM_COUNT

#define PARAM_COUNT 3
#include "TemplateUtils_DelegateImpl.h"
#undef PARAM_COUNT

#define PARAM_COUNT 4
#include "TemplateUtils_DelegateImpl.h"
#undef PARAM_COUNT

#define PARAM_COUNT 5
#include "TemplateUtils_DelegateImpl.h"
#undef PARAM_COUNT

#define PARAM_COUNT 6
#include "TemplateUtils_DelegateImpl.h"
#undef PARAM_COUNT

#define PARAM_COUNT 7
#include "TemplateUtils_DelegateImpl.h"
#undef PARAM_COUNT

#define PARAM_COUNT 8
#include "TemplateUtils_DelegateImpl.h"
#undef PARAM_COUNT

#undef INCLUDING_FROM_TEMPLATE_UTILS_DELEGATE_HEADER