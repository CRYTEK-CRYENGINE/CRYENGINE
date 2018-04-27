// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/AggregateTypeId.h>

#include "Deprecated/DocGraphNodes/DocGraphNodeBase.h"

namespace Schematyc2
{
	class CDocGraphSwitchNode : public CDocGraphNodeBase
	{
	private:

		struct EInputIdx
		{
			enum
			{
				In = 0,
				Value
			};
		};

		struct EOutputIdx
		{
			enum
			{
				Default = 0,
				FirstCase
			};
		};

		struct EStackFrame
		{
			enum
			{
				Value,
				Case
			};
		};

		struct EBranchMarker
		{
			enum
			{
				End,
				FirstCase
			};
		};

		typedef std::vector<CAggregateTypeId> TypeIds;

		struct STypeVisitor
		{
			STypeVisitor(TypeIds& _typeIds, Serialization::StringList& _typeNames);

			EVisitStatus VisitEnvTypeDesc(const IEnvTypeDesc& typeDesc);
			void VisitScriptEnumeration(const IScriptFile& enumerationFile, const IScriptEnumeration& enumeration);

			TypeIds&                   typeIds;
			Serialization::StringList& typeNames;
		};

		struct SCase
		{
			SCase();
			SCase(const IAnyPtr& _pValue);

			void Serialize(Serialization::IArchive& archive);

			IAnyPtr	pValue;
		};

		typedef std::vector<SCase> Cases;

	public:

		CDocGraphSwitchNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid = SGUID(), const SGUID& contextGUID = SGUID(), const SGUID& refGUID = SGUID(), Vec2 pos = Vec2(ZERO));

		// IScriptGraphNode
		virtual IAnyConstPtr GetCustomOutputDefault() const override;
		virtual size_t AddCustomOutput(const IAny& value) override;
		virtual void EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) override;
		virtual size_t AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId) override;
		virtual void RemoveOutput(size_t iOutput) override;
		virtual void Refresh(const SScriptRefreshParams& params) override;
		virtual void Serialize(Serialization::IArchive& archive) override;
		virtual void PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const override;
		virtual void LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const override;
		virtual void Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const override;
		// ~IScriptGraphNode

	private:

		void Validate(Serialization::IArchive& archive);
		void SetType(const CAggregateTypeId& typeId);
		void CompileInputs(IDocGraphNodeCompiler& compiler) const;
		void CompileCaseBegin(IDocGraphNodeCompiler& compiler, size_t portIdx) const;
		void CompileCaseEnd(IDocGraphNodeCompiler& compiler) const;
		void CompileEnd(IDocGraphNodeCompiler& compiler) const;

		CAggregateTypeId m_typeId; // #SchematycTODO : Do we really need to store type id when we can query if from m_pValue?
		IAnyPtr          m_pValue;
		Cases            m_cases;
	};
}
