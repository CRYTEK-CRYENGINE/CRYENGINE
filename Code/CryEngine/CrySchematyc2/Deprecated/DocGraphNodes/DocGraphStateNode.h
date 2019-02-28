// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Env/IEnvRegistry.h"

#include "Deprecated/DocGraphNodes/DocGraphNodeBase.h"

namespace Schematyc2
{
	class CDocGraphStateNode : public CDocGraphNodeBase
	{
	public:

		CDocGraphStateNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid = SGUID(), const SGUID& contextGUID = SGUID(), const SGUID& refGUID = SGUID(), Vec2 pos = Vec2(ZERO));

		// IScriptGraphNode
		virtual IAnyConstPtr GetCustomOutputDefault() const override;
		virtual size_t AddCustomOutput(const IAny& value) override;
		virtual void EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) override;
		virtual size_t AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId) override;
		virtual void RemoveOutput(size_t outputIdx) override;
		virtual void Refresh(const SScriptRefreshParams& params) override;
		virtual void PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const override;
		virtual void LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const override;
		virtual void Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const override;
		virtual void Serialize(Serialization::IArchive& archive) override;
		virtual void RemapGUIDs(IGUIDRemapper& guidRemapper) override;
		// ~IScriptGraphNode

	private:

		struct EInput
		{
			enum
			{
				In = 0
			};
		};

		typedef std::vector<SGUID> OutputTypeGUIDs;

		struct SEnumerateOptionalOutputs
		{
			SEnumerateOptionalOutputs(const IScriptFile& _file, const SGUID& _refGUID, const ScriptGraphNodeOptionalOutputEnumerator& _enumerator);

			EVisitStatus VisitSignal(const ISignalConstPtr& pSignal);
			void VisitScriptSignal(const IScriptFile& signalFile, const IScriptSignal& signal);
			EVisitStatus VisitScriptTimer(const IScriptTimer& scriptTimer);
			EVisitStatus VisitScriptState(const IScriptState& scriptState);

			const IScriptFile&                             file;
			const SGUID                                    refGUID;
			const ScriptGraphNodeOptionalOutputEnumerator& enumerator;
		};

		void SelectState(IDocGraphNodeCompiler& compiler) const;

		OutputTypeGUIDs m_outputTypeGUIDs;
	};
}
