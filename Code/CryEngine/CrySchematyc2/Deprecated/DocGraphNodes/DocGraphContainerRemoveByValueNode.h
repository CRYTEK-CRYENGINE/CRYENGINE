// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Deprecated/DocGraphNodes/DocGraphNodeBase.h"

namespace Schematyc2
{
	class CDocGraphContainerRemoveByValueNode : public CDocGraphNodeBase
	{
	public:

		CDocGraphContainerRemoveByValueNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid = SGUID(), const SGUID& contextGUID = SGUID(), const SGUID& refGUID = SGUID(), Vec2 pos = Vec2(ZERO));

		// IScriptGraphNode
		virtual IAnyConstPtr GetCustomOutputDefault() const override;
		virtual size_t AddCustomOutput(const IAny& value) override;
		virtual void EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) override;
		virtual size_t AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId) override;
		virtual void RemoveOutput(size_t outputIdx) override;
		virtual void Refresh(const SScriptRefreshParams& params) override;
		virtual void Serialize(Serialization::IArchive& archive) override;
		virtual void PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const override;
		virtual void LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const override;
		virtual void Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const override;
		// ~IScriptGraphNode

	private:

		struct EInput
		{
			enum
			{
				In = 0,
				Value
			};
		};

		struct EOutput
		{
			enum
			{
				True = 0,
				False
			};
		};

		struct EStackFrame
		{
			enum
			{
				Body,
				True,
				False
			};
		};

		struct EMarker
		{
			enum
			{
				False,
				End
			};
		};

		void CompileInputs(IDocGraphNodeCompiler& compiler) const;
		void CompileTrue(IDocGraphNodeCompiler& compiler) const;
		void CompileFalse(IDocGraphNodeCompiler& compiler) const;
		void CompileEnd(IDocGraphNodeCompiler& compiler) const;

		IAnyPtr m_pValue;
	};
}
