// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/ILib.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>

#include "Lib.h"
#include "Deprecated/VMOps.h"

#define SCHEMATYC2_COMPILER_PREVIEW_OUTPUT 0	// Disabled because this system is WIP. Once complete it Will provide more verbose compiler preview output than existing implementation.

namespace Schematyc2
{
	class CDocGraphSequencePreCompiler : public IDocGraphSequencePreCompiler
	{
	public:

		CDocGraphSequencePreCompiler(CLibClass& libClass, const LibFunctionId& libFunctionId, CLibFunction& libFunction);

		// IDocGraphSequencePreCompiler
		virtual bool BindFunctionToGUID(const SGUID& guid) override;
		virtual void SetFunctionName(const char* name) override;
		virtual void AddFunctionInput(const char* name, const char* textDesc, const IAny& value) override;
		virtual void AddFunctionOutput(const char* name, const char* textDesc, const IAny& value) override;
		// ~IDocGraphSequencePreCompiler

		void PrecompileSequence(const IScriptGraphNode& node, size_t iNodeOutput);

	private:

		CLibClass&    m_libClass;
		LibFunctionId m_libFunctionId;
		CLibFunction& m_libFunction;
	};

	class CDocGraphSequenceLinker : public IDocGraphSequenceLinker
	{
	public:

		CDocGraphSequenceLinker(CLibClass& libClass, CLibStateMachine* pLibStateMachine, CLibState* pLibState);

		// IDocGraphSequenceLinker
		virtual ILibStateMachine* GetLibStateMachine() override;
		virtual ILibState* GetLibState() override;
		virtual void CreateConstructor(const SGUID& guid, const LibFunctionId& libFunctionId) override;
		virtual void CreateDestructor(const SGUID& guid, const LibFunctionId& libFunctionId) override;
		virtual void CreateSignalReceiver(const SGUID& guid, const SGUID& contextGUID, const LibFunctionId& libFunctionId) override;
		virtual void CreateTransition(const SGUID& guid, const SGUID& stateGUID, const SGUID& contextGUID, const LibFunctionId& libFunctionId) override;
		// ~IDocGraphSequenceLinker

		void LinkSequence(const IScriptGraphNode& node, size_t iNodeOutput, const LibFunctionId& libFunctionId);

	private:

		CLibClass&				m_libClass;
		CLibStateMachine*	m_pLibStateMachine;
		CLibState*				m_pLibState;
	};

	class CDocGraphNodeCompiler : public IDocGraphNodeCompiler
	{
	public:

		CDocGraphNodeCompiler(const IScriptFile& file, const IDocGraph& docGraph, CLib& lib, CLibClass& libClass, CLibFunction& libFunction);

		// IDocGraphSequenceCompiler
		virtual ILibClass& GetLibClass() const override;
		virtual size_t GetStackSize() const override;
		virtual size_t GetStackInputPos() const override;
		virtual size_t GetStackOutputPos() const override;
		virtual void CreateStackFrame(const IScriptGraphNode& node, size_t label) override;
		virtual void CollapseStackFrame(const IScriptGraphNode& node, size_t label) override;
		virtual void AddOutputToStack(const IScriptGraphNode& node, size_t iOutput, const IAny& value) override;
		virtual size_t BindOutputToStack(const IScriptGraphNode& node, size_t pos, size_t iOutput, const IAny& value) override;
		virtual size_t FindInputOnStack(const IScriptGraphNode& node, size_t iInput) override;
		virtual size_t FindOutputOnStack(const IScriptGraphNode& node, size_t iOutput) override;
		virtual void BindVariableToStack(const IScriptGraphNode& node, size_t pos, size_t label) override;
		virtual size_t FindVariableOnStack(const IScriptGraphNode& node, size_t label) const override;
		virtual void CreateMarker(const IScriptGraphNode& node, size_t label) override;
		virtual void Branch(const IScriptGraphNode& node, size_t label) override;
		virtual void BranchIfZero(const IScriptGraphNode& node, size_t label) override;
		virtual void BranchIfNotZero(const IScriptGraphNode& node, size_t label) override;
		virtual void GetObject() override;
		virtual void Push(const IAny& value) override;
		virtual void Set(size_t pos, const IAny& value) override;
		virtual void Copy(size_t srcPos, size_t dstPos, const IAny& value) override;
		virtual void Pop(size_t count) override;
		virtual size_t Load(const SGUID& guid) override;
		virtual size_t Store(const SGUID& guid) override;
		virtual void ContainerAdd(const SGUID& guid, const IAny& value) override;
		virtual void ContainerRemoveByIndex(const SGUID& guid) override;
		virtual void ContainerRemoveByValue(const SGUID& guid, const IAny& value) override;
		virtual void ContainerClear(const SGUID& guid) override;
		virtual void ContainerSize(const SGUID& guid, const IAny& value) override;
		virtual void ContainerGet(const SGUID& guid, const IAny& value) override;
		virtual void ContainerSet(const SGUID& guid, const IAny& value) override;
		virtual void ContainerFindByValue(const SGUID& guid, const IAny& value) override;
		virtual void Compare(size_t lhsPos, size_t rhsPos, size_t count) override;
		virtual void IncrementInt32(size_t pos) override;
		virtual void LessThanInt32(size_t lhsPos, size_t rhsPos) override;
		virtual void GreaterThanInt32(size_t lhsPos, size_t rhsPos) override;
		virtual void StartTimer(const SGUID& guid) override;
		virtual void StopTimer(const SGUID& guid) override;
		virtual void ResetTimer(const SGUID& guid) override;
		virtual void SendSignal(const SGUID& guid) override;
		virtual void BroadcastSignal(const SGUID& guid) override;
		virtual void CallGlobalFunction(const SGUID& guid) override;
		virtual void CallEnvAbstractInterfaceFunction(const SGUID& abstractInterfaceGUID, const SGUID& functionGUID) override;
		virtual void CallLibAbstractInterfaceFunction(const SGUID& abstractInterfaceGUID, const SGUID& functionGUID) override;
		virtual void CallComponentMemberFunction(const SGUID& guid) override;
		virtual void CallActionMemberFunction(const SGUID& guid) override;
		virtual void CallLibFunction(const LibFunctionId& functionId) override;
		virtual void Return() override;
		// ~IDocGraphSequenceCompiler

		void CompileSequence(const IScriptGraphNode& node, size_t iNodeOutput);

	private:

		struct SOutputBinding
		{
			SOutputBinding(const SGUID& _nodeGUID, size_t _output);

			SGUID		nodeGUID;
			size_t	output;
		};

		typedef std::vector<SOutputBinding> TOutputBindingVector;

		struct SLabelBinding
		{
			SLabelBinding(const SGUID& _nodeGUID, size_t _label);

			SGUID		nodeGUID;
			size_t	label;
		};

		typedef std::vector<SLabelBinding> TLabelBindingVector;

		struct SStackVariable
		{
			SStackVariable();

			TOutputBindingVector	outputBindings;
			TLabelBindingVector		labelBindings;
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			string								name;
#endif
		};

		typedef std::vector<SStackVariable> TStackVariableVector;

		struct SStackFrame
		{
			SStackFrame(const SGUID& _nodeGUID, size_t _label, size_t _pos);

			SGUID		nodeGUID;
			size_t	label;
			size_t	pos;
		};

		typedef std::vector<SStackFrame> TStackFrameVector;

		struct SMarker
		{
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			SMarker(const SGUID& _nodeGUID, size_t _label, size_t _opPos, size_t _iPreviewLine);
#else
			SMarker(const SGUID& _nodeGUID, size_t _label, size_t _opPos);
#endif

			SGUID		nodeGUID;
			size_t	label;
			size_t	opPos;
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			size_t	iPreviewLine;
#endif
		};

		typedef std::vector<SMarker> TMarkerVector;

		struct SBranch
		{
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			SBranch(const SGUID& _nodeGUID, size_t _label, size_t _opPos, size_t _iPreviewLine);
#else
			SBranch(const SGUID& _nodeGUID, size_t _label, size_t _opPos);
#endif

			SGUID		nodeGUID;
			size_t	label;
			size_t	opPos;
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			size_t	iPreviewLine;
#endif
		};

		typedef std::vector<SBranch>	TBranchVector;
		typedef std::vector<string>		TStringVector;

		void BeginSequence();
		void EndSequence();
		void VisitSequenceNode(const IScriptGraphNode& scriptGraphNode, EDocGraphSequenceStep sequenceStep, size_t iNodePort);
		size_t FindOutputOnStack(const SGUID& nodeGUID, size_t output) const;
		size_t FindVariableOnStack(const SGUID& nodeGUID, size_t label) const;
		size_t GetStackFramePos(const SGUID& nodeGUID, size_t label) const;
		void DestroyStackFrame(const SGUID& nodeGUID, size_t label);

		const IScriptFile&   m_file;
		const IDocGraph&     m_docGraph;
		CLib&	               m_lib;
		CLibClass&           m_libClass;
		CLibFunction&        m_libFunction;
		TStackVariableVector m_stack;
		TStackFrameVector    m_stackFrames;
		TMarkerVector        m_markers;
		TBranchVector        m_branches;
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		TStringVector        m_previewLines;
#endif
	};

	class CCompiler : public ICompiler
	{
	public:

		// ICompiler
		virtual ILibPtr Compile(const IScriptFile& scriptFile) override;
		virtual void Compile(const IScriptElement& scriptElement) override;
		virtual void CompileAll() override;
		// ~ICompiler

	private:

		struct SDocGraphSequence
		{
			SDocGraphSequence(CLibClass* _pLibClass, size_t _iLibStateMachine, size_t _iLibState, const IDocGraph* _pDocGraph, const IScriptGraphNode* _pScriptGraphNode, size_t	_iDocGraphNodeOutput);

			CLibClass*              pLibClass;
			size_t                  iLibStateMachine;
			size_t                  iLibState;
			const IDocGraph*        pDocGraph;
			const IScriptGraphNode* pScriptGraphNode;
			size_t                  iDocGraphNodeOutput;
			LibFunctionId           libFunctionId;
			CLibFunction*           pLibFunction;
		};

		typedef std::vector<SDocGraphSequence> TDocGraphSequenceVector;

		EVisitStatus VisitAndCompileScriptFile(IScriptFile& scriptFile);
		void CompileSignal(const IScriptFile& scriptFile, const IScriptSignal& scriptSignal, CLib& lib);
		void CompileAbstractInterface(const IScriptFile& scriptFile, const IScriptAbstractInterface& scriptAbstractInterface, CLib& lib);
		void CompileAbstractInterfaceFunction(const IScriptFile& scriptFile, const IScriptAbstractInterfaceFunction& scriptAbstractInterfaceFunction, CLib& lib);
		void CompileClass(const IScriptFile& scriptFile, const IScriptClass& scriptClass, CLib& lib, TDocGraphSequenceVector& docGraphSequences);
		void CompileClass(const IScriptFile& scriptFile, const IScriptClass& scriptClass, CLib& lib, CLibClass& libClass, TDocGraphSequenceVector& docGraphSequences);
		void CompileClassProperties(const IScriptFile& scriptFile, const IScriptClass& scriptClass, CLib& lib, CLibClass& libClass, TDocGraphSequenceVector& docGraphSequences);
		void LinkClass(const IScriptFile& scriptFile, const IScriptClass& scriptClass, CLib& lib, CLibClass& libClass);
	};
}
