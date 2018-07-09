// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		struct IFunction;             // below

		namespace Internal
		{
			class CFunctionDeleter;   // IFunctionFactory.h
		}

		//===================================================================================
		//
		// FunctionUniquePtr
		//
		//===================================================================================

		// note: should be better placed in IFunctionFactory.h, but IFunction::AddChildAndTransferItsOwnership() needs to see it
		typedef std::unique_ptr<IFunction, Internal::CFunctionDeleter> FunctionUniquePtr;

		//===================================================================================
		//
		// IFunction
		//
		//===================================================================================

		struct IFunction
		{
			struct SCtorContext
			{
				explicit                              SCtorContext(const Core::ILeafFunctionReturnValue* _pOptionalReturnValueInCaseOfLeafFunction, const Core::SQueryBlackboard& _blackboard, const IInputParameterRegistry& _inputParameterRegistry, bool _bAddReturnValueToDebugRenderWorldUponExecution);
				const Core::ILeafFunctionReturnValue* pOptionalReturnValueInCaseOfLeafFunction;    // is a nullptr if this function it not a leaf-function
				const Core::SQueryBlackboard&         blackboard;
				const IInputParameterRegistry&        inputParameterRegistry;
				bool                                  bAddReturnValueToDebugRenderWorldUponExecution;
			};

			struct SValidationContext
			{
				explicit                              SValidationContext(const char* _szNameOfFunctionBeingValidated, Shared::IUqsString& _error);
				const char*                           szNameOfFunctionBeingValidated;   // for adding more info to a potential error message
				Shared::IUqsString&                   error;
			};

			struct SExecuteContext
			{
				explicit                              SExecuteContext(size_t _currentItemIndex, const Core::SQueryBlackboard& _blackboard, Shared::IUqsString& _error, bool& _bExceptionOccurred);
				size_t                                currentItemIndex;     // this shall only to be used when we're currently in the evaluation phase (i. e. blackboard.pItemIterationContext will be set)
				const Core::SQueryBlackboard&         blackboard;           // stuff that is shared by other sub systems in a query that doesn't change anymore (query initially writes to it, functions can read from it)
				Shared::IUqsString&                   error;                // if an exception occurs in a function, this is the place to write the message to (see .bExceptionOccurred)
				bool&                                 bExceptionOccurred;   // can be set to true from inside a function call to notify of an unrecoverable error (see .error)
			};

			virtual                                   ~IFunction() {}

			// instantiation time
			virtual void                              AddChildAndTransferItsOwnership(FunctionUniquePtr& childPtr) = 0;

			// type validation (at instantiation time)
			virtual const Shared::CTypeInfo&          DebugAssertGetReturnType() const = 0;
			virtual void                              DebugAssertChildReturnTypes() const = 0;
			virtual bool                              ValidateDynamic(const SValidationContext& validationContext) const { return true; }   // optional and used only by leaf functions: ensure that expected global params etc. are present and match the type

			// runtime
			virtual void                              Execute(const SExecuteContext& executeContext, void* pReturnValue) const = 0;   // called recursively on child functions to use their return value as one of our input parameters
		};

		inline IFunction::SCtorContext::SCtorContext(const Core::ILeafFunctionReturnValue* _pOptionalReturnValueInCaseOfLeafFunction, const Core::SQueryBlackboard& _blackboard, const IInputParameterRegistry& _inputParameterRegistry, bool _bAddReturnValueToDebugRenderWorldUponExecution)
			: pOptionalReturnValueInCaseOfLeafFunction(_pOptionalReturnValueInCaseOfLeafFunction)
			, blackboard(_blackboard)
			, inputParameterRegistry(_inputParameterRegistry)
			, bAddReturnValueToDebugRenderWorldUponExecution(_bAddReturnValueToDebugRenderWorldUponExecution)
		{}

		inline IFunction::SValidationContext::SValidationContext(const char* _szNameOfFunctionBeingValidated, Shared::IUqsString& _error)
			: szNameOfFunctionBeingValidated(_szNameOfFunctionBeingValidated)
			, error(_error)
		{}

		inline IFunction::SExecuteContext::SExecuteContext(size_t _currentItemIndex, const Core::SQueryBlackboard& _blackboard, Shared::IUqsString& _error, bool& _bExceptionOccurred)
			: currentItemIndex(_currentItemIndex)
			, blackboard(_blackboard)
			, error(_error)
			, bExceptionOccurred(_bExceptionOccurred)
		{
			bExceptionOccurred = false;
		}

	}
}
