// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/AggregateTypeId.h>
#include <CrySchematyc2/IEnvTypeDesc.h>
#include <CrySchematyc2/Script/IScriptFile.h>
#include <CrySchematyc2/Script/IScriptGraph.h>

namespace Schematyc2
{
	typedef std::vector<string>  StringVector;
	typedef std::vector<SGUID>   GUIDVector;
	typedef std::vector<IAnyPtr> IAnyPtrVector;

	class CDocGraphNodeBase : public IScriptGraphNode
	{
	public:

		CDocGraphNodeBase(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const char* szName, EScriptGraphNodeType type, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos);

		// IScriptGraphNode
		virtual void SetGUID(const SGUID& guid) override;
		virtual SGUID GetGUID() const override;
		virtual void SetName(const char* szName) override;
		virtual const char* GetName() const override;
		virtual EScriptGraphNodeType GetType() const override;
		virtual SGUID GetContextGUID() const override;
		virtual SGUID GetRefGUID() const override;
		virtual void SetPos(Vec2 pos) override;
		virtual Vec2 GetPos() const override;
		virtual size_t GetInputCount() const override;
		virtual uint32 FindInput(const char* szName) const override;
		virtual const char* GetInputName(size_t inputIdx) const override;
		virtual EScriptGraphPortFlags GetInputFlags(size_t inputIdx) const override;
		virtual CAggregateTypeId GetInputTypeId(size_t inputIdx) const override;
		virtual size_t GetOutputCount() const override;
		virtual uint32 FindOutput(const char* szName) const override;
		virtual const char* GetOutputName(size_t outputIdx) const override;
		virtual EScriptGraphPortFlags GetOutputFlags(size_t outputIdx) const override;
		virtual CAggregateTypeId GetOutputTypeId(size_t outputIdx) const override;
		virtual void RemoveOutput(size_t outputIdx) override;
		virtual void Refresh(const SScriptRefreshParams& params) override;
		virtual void Serialize(Serialization::IArchive& archive) override;
		virtual void RemapGUIDs(IGUIDRemapper& guidRemapper) override;
		// ~IScriptGraphNode

	protected:

		IScriptFile& GetFile();
		const IScriptFile& GetFile() const;
		IDocGraph& GetGraph();
		const IDocGraph& GetGraph() const;
		const char* GetDebugLabel() const; // #SchematycTODO : Move this function to the IScriptGraphNode interface?
		void SetContextGUID(const SGUID& contextGUID);
		void SetRefGUID(const SGUID& refGUID);
		size_t AddInput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId = CAggregateTypeId());
		void SetInputTypeId(size_t inputIdx, const CAggregateTypeId& typeId);
		void RemoveInput(size_t inputIdx);
		size_t AddOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId = CAggregateTypeId());
		void SetOutputTypeId(size_t outputIdx, const CAggregateTypeId& typeId);

	private:

		struct SPort
		{
			SPort(const char* _szName, EScriptGraphPortFlags _flags, const CAggregateTypeId& _typeId);

			string                name;
			EScriptGraphPortFlags flags;
			CAggregateTypeId      typeId;
		};

		typedef std::vector<SPort> PortVector;

		IScriptFile&               m_file;
		IDocGraph&                 m_graph;
		SGUID                      m_guid;
		string                     m_name;
		string                     m_debugLabel;
		const EScriptGraphNodeType m_type;
		SGUID                      m_contextGUID;
		SGUID                      m_refGUID;
		Vec2                       m_pos;
		PortVector                 m_inputs;
		PortVector                 m_outputs;
	};

	namespace DocGraphNodeUtils
	{
		void CopyInputsToTopOfStack(const CDocGraphNodeBase& node, size_t firstInputIdx, const IAnyPtrVector& inputValues, IDocGraphNodeCompiler& compiler);
		void CopyInputsToStack(const CDocGraphNodeBase& node, size_t firstInputIdx, const IAnyPtrVector& inputValues, IDocGraphNodeCompiler& compiler);
		void CopyInputsToTopOfStack(const CDocGraphNodeBase& node, size_t firstInputIdx, const IAnyPtrVector& inputValues, size_t firstInputValueIdx, IDocGraphNodeCompiler& compiler);
		void CopyInputsToStack(const CDocGraphNodeBase& node, size_t firstInputIdx, const IAnyPtrVector& inputValues, size_t firstInputValueIdx, IDocGraphNodeCompiler& compiler);
	}
}
