// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/IAny.h" // #SchematycTODO : Can we forward declare?

namespace Schematyc2
{
	struct ICustomFunctionDesc;
	struct ILib;
	struct ILibClass;
	struct ILibStateMachine;
	struct ILibState;
	struct ILibAction;
	struct IScriptElement;
	struct IScriptFile;
	struct IScriptGraphNode;

	DECLARE_SHARED_POINTERS(ILib)

	struct IDocGraphSequencePreCompiler
	{
		virtual ~IDocGraphSequencePreCompiler() {}

		virtual bool BindFunctionToGUID(const SGUID& guid) = 0;
		virtual void SetFunctionName(const char* szName) = 0;
		virtual void AddFunctionInput(const char* szName, const char* szDescription, const IAny& value) = 0;
		virtual void AddFunctionOutput(const char* szName, const char* szDescription, const IAny& value) = 0;
	};

	struct IDocGraphSequenceLinker
	{
		virtual ~IDocGraphSequenceLinker() {}

		virtual ILibStateMachine* GetLibStateMachine() = 0;
		virtual ILibState* GetLibState() = 0;
		virtual void CreateConstructor(const SGUID& guid, const LibFunctionId& functionId) = 0;
		virtual void CreateDestructor(const SGUID& guid, const LibFunctionId& functionId) = 0;
		virtual void CreateSignalReceiver(const SGUID& guid, const SGUID& contextGUID, const LibFunctionId& functionId) = 0;
		virtual void CreateTransition(const SGUID& guid, const SGUID& stateGUID, const SGUID& contextGUID, const LibFunctionId& functionId) = 0;
	};

	struct IDocGraphNodeCompiler
	{
		virtual ~IDocGraphNodeCompiler() {}

		virtual ILibClass& GetLibClass() const = 0;
		virtual size_t GetStackSize() const = 0;
		virtual size_t GetStackInputPos() const = 0;
		virtual size_t GetStackOutputPos() const = 0;
		virtual void CreateStackFrame(const IScriptGraphNode& node, size_t label) = 0;
		virtual void CollapseStackFrame(const IScriptGraphNode& node, size_t label) = 0;
		virtual void AddOutputToStack(const IScriptGraphNode& node, size_t outputIdx, const IAny& value) = 0;
		virtual size_t BindOutputToStack(const IScriptGraphNode& node, size_t pos, size_t outputIdx, const IAny& value) = 0;
		virtual size_t FindInputOnStack(const IScriptGraphNode& node, size_t inputIdx) = 0;
		virtual size_t FindOutputOnStack(const IScriptGraphNode& node, size_t outputIdx) = 0;
		virtual void BindVariableToStack(const IScriptGraphNode& node, size_t pos, size_t label) = 0;
		virtual size_t FindVariableOnStack(const IScriptGraphNode& node, size_t label) const = 0;
		virtual void CreateMarker(const IScriptGraphNode& node, size_t label) = 0;
		virtual void Branch(const IScriptGraphNode& node, size_t label) = 0;
		virtual void BranchIfZero(const IScriptGraphNode& node, size_t label) = 0;
		virtual void BranchIfNotZero(const IScriptGraphNode& node, size_t label) = 0;
		virtual void GetObject() = 0;
		virtual void Push(const IAny& value) = 0;
		virtual void Set(size_t pos, const IAny& value) = 0;
		virtual void Copy(size_t srcPos, size_t dstPos, const IAny& value) = 0;
		virtual void Pop(size_t count) = 0;
		virtual size_t Load(const SGUID& guid) = 0;
		virtual size_t Store(const SGUID& guid) = 0;
		virtual void ContainerAdd(const SGUID& guid, const IAny& value) = 0;
		virtual void ContainerRemoveByIndex(const SGUID& guid) = 0;
		virtual void ContainerRemoveByValue(const SGUID& guid, const IAny& value) = 0;
		virtual void ContainerClear(const SGUID& guid) = 0;
		virtual void ContainerSize(const SGUID& guid, const IAny& value) = 0;
		virtual void ContainerGet(const SGUID& guid, const IAny& value) = 0;
		virtual void ContainerSet(const SGUID& guid, const IAny& value) = 0;
		virtual void ContainerFindByValue(const SGUID& guid, const IAny& value) = 0;
		virtual void Compare(size_t lhsPos, size_t rhsPos, size_t count) = 0;
		virtual void IncrementInt32(size_t pos) = 0;
		virtual void LessThanInt32(size_t lhsPos, size_t rhsPos) = 0;
		virtual void GreaterThanInt32(size_t lhsPos, size_t rhsPos) = 0;
		virtual void StartTimer(const SGUID& guid) = 0;
		virtual void StopTimer(const SGUID& guid) = 0;
		virtual void ResetTimer(const SGUID& guid) = 0;
		virtual void SendSignal(const SGUID& guid) = 0;
		virtual void BroadcastSignal(const SGUID& guid) = 0;
		virtual void CallGlobalFunction(const SGUID& guid) = 0;
		virtual void CallEnvAbstractInterfaceFunction(const SGUID& abstractInterfaceGUID, const SGUID& functionGUID) = 0;
		virtual void CallLibAbstractInterfaceFunction(const SGUID& abstractInterfaceGUID, const SGUID& functionGUID) = 0;
		virtual void CallComponentMemberFunction(const SGUID& guid) = 0;
		virtual void CallActionMemberFunction(const SGUID& guid) = 0;
		virtual void CallLibFunction(const LibFunctionId& functionId) = 0;
		virtual void Return() = 0;
	};

	struct ICompiler
	{
		virtual ~ICompiler() {}

		virtual ILibPtr Compile(const IScriptFile& file) = 0;
		virtual void Compile(const IScriptElement& scriptElement) = 0;
		virtual void CompileAll() = 0;
	};
}
