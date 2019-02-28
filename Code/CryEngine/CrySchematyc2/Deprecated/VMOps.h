// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/GUID.h> // #SchematycTODO : Remove this include?

namespace Schematyc2
{
	struct SVMOp
	{
		enum EOpCode
		{
			PUSH = 0,															// Push value onto top of stack.
			SET,																	// Set value at given position on stack.
			COPY,																	// Copy value on stack.
			POP,																	// Pop value from top of stack.
			COLLAPSE,															// Collapse stack to a specified size.
			LOAD,																	// Load variable to top of stack.
			STORE,																// Store value at top of stack in variable.
			CONTAINER_ADD,												// Add value at top of stack in container.
			CONTAINER_REMOVE_BY_INDEX,						// Remove index at top of stack from container.
			CONTAINER_REMOVE_BY_VALUE,						// Remove value at top of stack from container.
			CONTAINER_CLEAR,											// Clear container.
			CONTAINER_SIZE,												// Copy size of container to top of stack.
			CONTAINER_GET,												// Copy element at given index from container to top of stack.
			CONTAINER_SET,												// Set value to container. Container index and value are at top of stack.
			CONTAINER_FIND_BY_VALUE,						// Find value's index in container by value at top of stack.
			COMPARE,															// Compare two values on stack.
			INCREMENT_INT32,											// Increment int32.
			LESS_THAN_INT32,											// Compare two int32s.
			GREATER_THAN_INT32,										// Compare two int32s.
			BRANCH,																// Branch to another location.
			BRANCH_IF_ZERO,												// Branch to another location if the value at the top of the stack is zero.
			BRANCH_IF_NOT_ZERO,										// Branch to another location if the value at the top of the stack is not zero.
			GET_OBJECT,														// Copy object id to stack.
			START_TIMER,													// Start a timer.
			STOP_TIMER,														// Stop a timer.
			RESET_TIMER,													// Reset a timer.
			SEND_SIGNAL,													// Send a signal.
			BROADCAST_SIGNAL,											// Broadcast a signal.
			CALL_GLOBAL_FUNCTION,									// Call a global function.
			CALL_ENV_ABSTRACT_INTERFACE_FUNCTION,	// Call an environment abstract interface function.
			CALL_LIB_ABSTRACT_INTERFACE_FUNCTION,	// Call a library abstract interface function.
			CALL_COMPONENT_MEMBER_FUNCTION,				// Call a component function.
			CALL_ACTION_MEMBER_FUNCTION,					// Call a action function.
			CALL_LIB_FUNCTION,										// Call a library function.
			RETURN																// Exit the current function.
		};

		inline SVMOp(SVMOp::EOpCode _opCode, size_t _size)
			: opCode(_opCode)
			, size(_size)
		{}

		SVMOp::EOpCode	opCode;
		size_t					size;
	};

	struct SVMPushOp : public SVMOp
	{
		inline SVMPushOp(size_t _iConstValue)
			: SVMOp(SVMOp::PUSH, sizeof(*this))
			, iConstValue(_iConstValue)
		{}

		size_t	iConstValue;
	};

	struct SVMSetOp : public SVMOp
	{
		inline SVMSetOp(size_t _pos, size_t _iConstValue)
			: SVMOp(SVMOp::SET, sizeof(*this))
			, pos(_pos)
			, iConstValue(_iConstValue)
		{}

		size_t	pos;
		size_t	iConstValue;
	};

	struct SVMCopyOp : public SVMOp
	{
		inline SVMCopyOp(size_t _srcPos, size_t _dstPos)
			: SVMOp(SVMOp::COPY, sizeof(*this))
			, srcPos(_srcPos)
			, dstPos(_dstPos)
		{}

		size_t	srcPos;
		size_t	dstPos;
	};

	struct SVMPopOp : public SVMOp
	{
		inline SVMPopOp(size_t _count)
			: SVMOp(SVMOp::POP, sizeof(*this))
			, count(_count)
		{}

		size_t	count;
	};

	struct SVMCollapseOp : public SVMOp
	{
		inline SVMCollapseOp(size_t _pos)
			: SVMOp(SVMOp::COLLAPSE, sizeof(*this))
			, pos(_pos)
		{}

		size_t	pos;
	};

	struct SVMLoadOp : public SVMOp
	{
		inline SVMLoadOp(size_t _pos, size_t _count)
			: SVMOp(SVMOp::LOAD, sizeof(*this))
			, pos(_pos)
			, count(_count)
		{}

		size_t	pos;
		size_t	count;
	};

	struct SVMStoreOp : public SVMOp
	{
		inline SVMStoreOp(size_t _pos, size_t _count)
			: SVMOp(SVMOp::STORE, sizeof(*this))
			, pos(_pos)
			, count(_count)
		{}

		size_t	pos;
		size_t	count;
	};

	struct SVMContainerAddOp : public SVMOp
	{
		inline SVMContainerAddOp(size_t _iContainer, size_t _count)
			: SVMOp(SVMOp::CONTAINER_ADD, sizeof(*this))
			, iContainer(_iContainer)
			, count(_count)
		{}

		size_t	iContainer;
		size_t	count;
	};

	struct SVMContainerRemoveByIndexOp : public SVMOp
	{
		inline SVMContainerRemoveByIndexOp(size_t _iContainer)
			: SVMOp(SVMOp::CONTAINER_REMOVE_BY_INDEX, sizeof(*this))
			, iContainer(_iContainer)
		{}

		size_t	iContainer;
	};

	struct SVMContainerRemoveByValueOp : public SVMOp
	{
		inline SVMContainerRemoveByValueOp(size_t _iContainer, size_t _count)
			: SVMOp(SVMOp::CONTAINER_REMOVE_BY_VALUE, sizeof(*this))
			, iContainer(_iContainer)
			, count(_count)
		{}

		size_t	iContainer;
		size_t	count;
	};

	struct SVMContainerClearOp : public SVMOp
	{
		inline SVMContainerClearOp(size_t _iContainer)
			: SVMOp(SVMOp::CONTAINER_CLEAR, sizeof(*this))
			, iContainer(_iContainer)
		{}

		size_t	iContainer;
	};

	struct SVMContainerSizeOp : public SVMOp
	{
		inline SVMContainerSizeOp(size_t _iContainer, size_t _divisor)
			: SVMOp(SVMOp::CONTAINER_SIZE, sizeof(*this))
			, iContainer(_iContainer)
			, divisor(_divisor)
		{}

		size_t	iContainer;
		size_t	divisor;
	};

	struct SVMContainerGetOp : public SVMOp
	{
		inline SVMContainerGetOp(size_t _iContainer, size_t _count)
			: SVMOp(SVMOp::CONTAINER_GET, sizeof(*this))
			, iContainer(_iContainer)
			, count(_count)
		{}

		size_t	iContainer;
		size_t	count;
	};

	struct SVMContainerSetOp : public SVMOp
	{
		inline SVMContainerSetOp(size_t _iContainer, size_t _count)
			: SVMOp(SVMOp::CONTAINER_SET, sizeof(*this))
			, iContainer(_iContainer)
			, count(_count)
		{}

		size_t	iContainer;
		size_t	count;
	};

	struct SVMContainerFindByValueOp : public SVMOp
	{
		inline SVMContainerFindByValueOp(size_t _iContainer, size_t _count)
			: SVMOp(SVMOp::CONTAINER_FIND_BY_VALUE, sizeof(*this))
			, iContainer(_iContainer)
			, count(_count)
		{}

		size_t	iContainer;
		size_t	count;
	};

	struct SVMCompareOp : public SVMOp
	{
		inline SVMCompareOp(size_t _lhsPos, size_t _rhsPos, size_t _count)
			: SVMOp(SVMOp::COMPARE, sizeof(*this))
			, lhsPos(_lhsPos)
			, rhsPos(_rhsPos)
			, count(_count)
		{}

		size_t	lhsPos;
		size_t	rhsPos;
		size_t	count;
	};

	struct SVMIncrementInt32Op : public SVMOp
	{
		inline SVMIncrementInt32Op(size_t _pos)
			: SVMOp(SVMOp::INCREMENT_INT32, sizeof(*this))
			, pos(_pos)
		{}

		size_t	pos;
	};

	struct SVMLessThanInt32Op : public SVMOp
	{
		inline SVMLessThanInt32Op(size_t _lhsPos, size_t _rhsPos)
			: SVMOp(SVMOp::LESS_THAN_INT32, sizeof(*this))
			, lhsPos(_lhsPos)
			, rhsPos(_rhsPos)
		{}

		size_t	lhsPos;
		size_t	rhsPos;
	};

	struct SVMGreaterThanInt32Op : public SVMOp
	{
		inline SVMGreaterThanInt32Op(size_t _lhsPos, size_t _rhsPos)
			: SVMOp(SVMOp::GREATER_THAN_INT32, sizeof(*this))
			, lhsPos(_lhsPos)
			, rhsPos(_rhsPos)
		{}

		size_t	lhsPos;
		size_t	rhsPos;
	};

	struct SVMBranchOp : public SVMOp
	{
		inline SVMBranchOp(size_t _pos)
			: SVMOp(SVMOp::BRANCH, sizeof(*this))
			, pos(_pos)
		{}

		size_t pos;
	};

	struct SVMBranchIfZeroOp : public SVMOp
	{
		inline SVMBranchIfZeroOp(size_t _pos)
			: SVMOp(SVMOp::BRANCH_IF_ZERO, sizeof(*this))
			, pos(_pos)
		{}

		size_t pos;
	};

	struct SVMBranchIfNotZeroOp : public SVMOp
	{
		inline SVMBranchIfNotZeroOp(size_t _pos)
			: SVMOp(SVMOp::BRANCH_IF_NOT_ZERO, sizeof(*this))
			, pos(_pos)
		{}

		size_t pos;
	};

	struct SVMGetObjectOp : public SVMOp
	{
		inline SVMGetObjectOp()
			: SVMOp(SVMOp::GET_OBJECT, sizeof(*this))
		{}
	};

	struct SVMSendSignalOp : public SVMOp
	{
		inline SVMSendSignalOp(const SGUID& _guid)
			: SVMOp(SVMOp::SEND_SIGNAL, sizeof(*this))
			, guid(_guid)
		{}

		SGUID guid;
	};

	struct SVMBroadcastSignalOp : public SVMOp
	{
		inline SVMBroadcastSignalOp(const SGUID& _guid)
			: SVMOp(SVMOp::BROADCAST_SIGNAL, sizeof(*this))
			, guid(_guid)
		{}

		SGUID guid;
	};

	struct SVMCallGlobalFunctionOp : public SVMOp
	{
		inline SVMCallGlobalFunctionOp(const uint32 _iGlobalFunction)
			: SVMOp(SVMOp::CALL_GLOBAL_FUNCTION, sizeof(*this))
			, iGlobalFunction(_iGlobalFunction)
		{}

		uint32	iGlobalFunction;
	};

	struct SVMCallEnvAbstractInterfaceFunctionOp : public SVMOp
	{
		inline SVMCallEnvAbstractInterfaceFunctionOp(const SGUID& _abstractInterfaceGUID, const SGUID& _functionGUID)
			: SVMOp(SVMOp::CALL_ENV_ABSTRACT_INTERFACE_FUNCTION, sizeof(*this))
			, abstractInterfaceGUID(_abstractInterfaceGUID)
			, functionGUID(_functionGUID)
		{}

		const SGUID	abstractInterfaceGUID;
		const SGUID	functionGUID;
	};

	struct SVMCallLibAbstractInterfaceFunctionOp : public SVMOp
	{
		inline SVMCallLibAbstractInterfaceFunctionOp(const SGUID& _abstractInterfaceGUID, const SGUID& _functionGUID)
			: SVMOp(SVMOp::CALL_LIB_ABSTRACT_INTERFACE_FUNCTION, sizeof(*this))
			, abstractInterfaceGUID(_abstractInterfaceGUID)
			, functionGUID(_functionGUID)
		{}

		const SGUID	abstractInterfaceGUID;
		const SGUID	functionGUID;
	};

	struct SVMCallComponentMemberFunctionOp : public SVMOp
	{
		inline SVMCallComponentMemberFunctionOp(const uint32 _iComponentMemberFunction)
			: SVMOp(SVMOp::CALL_COMPONENT_MEMBER_FUNCTION, sizeof(*this))
			, iComponentMemberFunction(_iComponentMemberFunction)
		{}

		uint32 iComponentMemberFunction;
	};

	struct SVMCallActionMemberFunctionOp : public SVMOp
	{
		inline SVMCallActionMemberFunctionOp(const uint32 _iActionMemberFunction)
			: SVMOp(SVMOp::CALL_ACTION_MEMBER_FUNCTION, sizeof(*this))
			, iActionMemberFunction(_iActionMemberFunction)
		{}

		uint32 iActionMemberFunction;
	};

	struct SVMCallLibFunctionOp : public SVMOp
	{
		inline SVMCallLibFunctionOp(const LibFunctionId& _functionId)
			: SVMOp(SVMOp::CALL_LIB_FUNCTION, sizeof(*this))
			, functionId(_functionId)
		{}

		LibFunctionId functionId;
	};

	struct SVMStartTimerOp : public SVMOp
	{
		inline SVMStartTimerOp(const SGUID& _guid)
			: SVMOp(SVMOp::START_TIMER, sizeof(*this))
			, guid(_guid)
		{}

		SGUID guid;
	};

	struct SVMStopTimerOp : public SVMOp
	{
		inline SVMStopTimerOp(const SGUID& _guid)
			: SVMOp(SVMOp::STOP_TIMER, sizeof(*this))
			, guid(_guid)
		{}

		SGUID guid;
	};

	struct SVMResetTimerOp : public SVMOp
	{
		inline SVMResetTimerOp(const SGUID& _guid)
			: SVMOp(SVMOp::RESET_TIMER, sizeof(*this))
			, guid(_guid)
		{}

		SGUID guid;
	};

	struct SVMReturnOp : public SVMOp
	{
		inline SVMReturnOp()
			: SVMOp(SVMOp::RETURN, sizeof(*this))
		{}
	};
}
