// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		template <class TFunction, class TReturn, IFunctionFactory::ELeafFunctionKind tLeafFunctionKind>
		class CFunctionBase;  // below

		namespace Internal
		{

			//===================================================================================
			//
			// SFunctionExecuteHelper<>
			//
			// - helper structure that is used by CFunctionBase<>::Execute() to call the underlying function with or without input parameters
			// - leaf-functions never get parameters passed in, while non-leaf functions do
			// - this goes hand in hand with how CFunctionFactory<> decided to register (or not register) those parameters
			// - actually, this template should better reside in the private section of CFunctionBase<>, but template specializations are not allowed inside a class
			//
			//===================================================================================

			template <class TFunction, class TReturn, IFunctionFactory::ELeafFunctionKind tLeafFunctionKind, bool bIsLeafFunction>
			struct SFunctionExecuteHelper
			{
			};

			// specialization for leaf-functions (call *without* input parameters)
			template <class TFunction, class TReturn, IFunctionFactory::ELeafFunctionKind tLeafFunctionKind>
			struct SFunctionExecuteHelper<TFunction, TReturn, tLeafFunctionKind, true>
			{
				static void Execute(const CFunctionBase<TFunction, TReturn, tLeafFunctionKind>* pThis, const IFunction::SExecuteContext& executeContext, void* pReturnValue)
				{
					pThis->ExecuteWithoutInputParameters(executeContext, pReturnValue);
				}
			};

			// specialization for non-leaf functions (call *with* input parameters)
			template <class TFunction, class TReturn, IFunctionFactory::ELeafFunctionKind tLeafFunctionKind>
			struct SFunctionExecuteHelper<TFunction, TReturn, tLeafFunctionKind, false>
			{
				static void Execute(const CFunctionBase<TFunction, TReturn, tLeafFunctionKind>* pThis, const IFunction::SExecuteContext& executeContext, void* pReturnValue)
				{
					pThis->ExecuteWithInputParameters(executeContext, pReturnValue);
				}
			};

		} // namespace Internal

		//===================================================================================
		//
		// CFunctionBase
		//
		//===================================================================================

		template <class TFunction, class TReturn, IFunctionFactory::ELeafFunctionKind tLeafFunctionKind>
		class CFunctionBase : public IFunction
		{
		public:
			typedef TReturn                                    ReturnType;                             // used by CFunctionFactory<>::GetReturnType() and GetContainedType()

			static const IFunctionFactory::ELeafFunctionKind   kLeafFunctionKind = tLeafFunctionKind;  // used by CFunctionFactory's ctor and GetLeafFunctionKind()

		public:
			// IFunction
			virtual void                                       AddChildAndTransferItsOwnership(FunctionUniquePtr& childPtr) override final;
			virtual const Shared::CTypeInfo&                   DebugAssertGetReturnType() const override final;
			virtual void                                       DebugAssertChildReturnTypes() const override final;
			virtual void                                       Execute(const SExecuteContext& executeContext, void* pReturnValue) const override final;
			// ~IFunction

			// these are called by SFunctionExecuteHelper<>::Execute()
			void                                               ExecuteWithoutInputParameters(const SExecuteContext& executeContext, void* pReturnValue) const;  // called by leaf-functions (these don't get input parameters passed in)
			void                                               ExecuteWithInputParameters(const SExecuteContext& executeContext, void* pReturnValue) const;     // called by non-leaf functions (these *do* get input parameters passed in)

		protected:
			explicit                                           CFunctionBase(const SCtorContext& ctorContext);

		private:
			IItemFactory*                                      m_pItemFactoryOfReturnValue;
			std::vector<FunctionUniquePtr>                     m_children;
			const IInputParameterRegistry&                     m_inputParameterRegistry;
			std::vector<size_t>                                m_inputParameterOffsets;       // just a cached version of the offsets in m_inputParameterRegistry to circumvent virtual function calls
			bool                                               m_bAddReturnValueToDebugRenderWorldUponExecution;
		};

		template <class TFunction, class TReturn, IFunctionFactory::ELeafFunctionKind tLeafFunctionKind>
		CFunctionBase<TFunction, TReturn, tLeafFunctionKind>::CFunctionBase(const SCtorContext& ctorContext)
			: m_pItemFactoryOfReturnValue(nullptr)
			, m_inputParameterRegistry(ctorContext.inputParameterRegistry)
			, m_bAddReturnValueToDebugRenderWorldUponExecution(ctorContext.bAddReturnValueToDebugRenderWorldUponExecution)
		{
			const size_t inputParameterCount = m_inputParameterRegistry.GetParameterCount();

			// cache the memory offset of all input parameters (to save virtual method calls on m_inputParameterInputs in ExecuteWithInputParameters())
			m_inputParameterOffsets.reserve(inputParameterCount);
			for (size_t i = 0; i < inputParameterCount; ++i)
			{
				m_inputParameterOffsets.push_back(m_inputParameterRegistry.GetParameter(i).offset);
			}

			// find the ItemFactory by the return type of this function
			m_pItemFactoryOfReturnValue = UQS::Core::IHubPlugin::GetHub().GetUtils().FindItemFactoryByType(Shared::SDataTypeHelper<TReturn>::GetTypeInfo());

			// if this assert() fails, then a function returns a type that is not registered as an item-factory
			// FIXME: this should not only trigger in debug builds
			assert(m_pItemFactoryOfReturnValue);
		}

		template <class TFunction, class TReturn, IFunctionFactory::ELeafFunctionKind tLeafFunctionKind>
		void CFunctionBase<TFunction, TReturn, tLeafFunctionKind>::AddChildAndTransferItsOwnership(Client::FunctionUniquePtr& childPtr)
		{
			assert(childPtr.get() != nullptr);
			m_children.push_back(std::move(childPtr));
		}

		template <class TFunction, class TReturn, IFunctionFactory::ELeafFunctionKind tLeafFunctionKind>
		const Shared::CTypeInfo& CFunctionBase<TFunction, TReturn, tLeafFunctionKind>::DebugAssertGetReturnType() const
		{
			return Shared::SDataTypeHelper<TReturn>::GetTypeInfo();
		}

		template <class TFunction, class TReturn, IFunctionFactory::ELeafFunctionKind tLeafFunctionKind>
		void CFunctionBase<TFunction, TReturn, tLeafFunctionKind>::DebugAssertChildReturnTypes() const
		{
			assert(m_inputParameterRegistry.GetParameterCount() == m_children.size());

			const size_t numChildren = m_children.size();

			for (size_t i = 0; i < numChildren; ++i)
			{
				const Shared::CTypeInfo& childReturnType = m_children[i]->DebugAssertGetReturnType();
				const Shared::CTypeInfo& expectedReturnType = m_inputParameterRegistry.GetParameter(i).type;
				assert(childReturnType == expectedReturnType);
			}
		}

		template <class TFunction, class TReturn, IFunctionFactory::ELeafFunctionKind tLeafFunctionKind>
		void CFunctionBase<TFunction, TReturn, tLeafFunctionKind>::Execute(const SExecuteContext& executeContext, void* pReturnValue) const
		{
			assert(m_inputParameterRegistry.GetParameterCount() == m_children.size());

			const bool bIsLeafFunction = (tLeafFunctionKind != IFunctionFactory::ELeafFunctionKind::None);
			Internal::SFunctionExecuteHelper<TFunction, TReturn, tLeafFunctionKind, bIsLeafFunction>::Execute(this, executeContext, pReturnValue);

			IF_UNLIKELY(m_bAddReturnValueToDebugRenderWorldUponExecution && executeContext.blackboard.pDebugRenderWorldPersistent)
			{
				m_pItemFactoryOfReturnValue->AddItemToDebugRenderWorld(pReturnValue, *executeContext.blackboard.pDebugRenderWorldPersistent);
			}
		}

		template <class TFunction, class TReturn, IFunctionFactory::ELeafFunctionKind tLeafFunctionKind>
		void CFunctionBase<TFunction, TReturn, tLeafFunctionKind>::ExecuteWithoutInputParameters(const SExecuteContext& executeContext, void* pReturnValue) const
		{
			const TFunction* pActualFunction = static_cast<const TFunction*>(this);
			TReturn* pActualReturnValue = static_cast<TReturn*>(pReturnValue);
			*pActualReturnValue = pActualFunction->DoExecute(executeContext);
		}

		template <class TFunction, class TReturn, IFunctionFactory::ELeafFunctionKind tLeafFunctionKind>
		void CFunctionBase<TFunction, TReturn, tLeafFunctionKind>::ExecuteWithInputParameters(const SExecuteContext& executeContext, void* pReturnValue) const
		{
			assert(m_inputParameterOffsets.size() == m_inputParameterRegistry.GetParameterCount());
			assert(m_inputParameterOffsets.size() == m_children.size());

			typename TFunction::SParams params;

			// call the children and write their return values back into our parameters
			for (size_t i = 0, n = m_children.size(); i < n; ++i)
			{
				void* pDest = reinterpret_cast<char*>(&params) + m_inputParameterOffsets[i];
				const IFunction* pChild = m_children[i].get();
				pChild->Execute(executeContext, pDest);

				// if an exception just occurred, then unwind the whole function tree without doing any more calls
				// (actually this exception might have already been propagated up from a much deeper function call)
				if (executeContext.bExceptionOccurred)
				{
					return;
				}
			}

			const TFunction* pActualFunction = static_cast<const TFunction*>(this);
			TReturn* pActualReturnValue = static_cast<TReturn*>(pReturnValue);
			*pActualReturnValue = pActualFunction->DoExecute(executeContext, params);
		}

	}
}
