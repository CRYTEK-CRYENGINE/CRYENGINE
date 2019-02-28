// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/IScriptGraph.h>
#include <CrySchematyc/Utils/Any.h>
#include <CrySchematyc/Utils/Assert.h>

namespace Schematyc
{

// Forward declare classes.
class CScriptGraphNodeModel;

struct SScriptGraphNodePort
{
	SScriptGraphNodePort();
	SScriptGraphNodePort(const CUniqueId& _id, const char* _szName, const CryGUID& _typeGUID, const ScriptGraphPortFlags& _flags, const CAnyValuePtr& _pData);

	CUniqueId            id;
	string               name;
	CryGUID                typeGUID;
	ScriptGraphPortFlags flags;
	CAnyValuePtr         pData;
};

typedef std::vector<SScriptGraphNodePort> ScriptGraphNodePorts;

class CScriptGraphNodeLayout   // #SchematycTODO : Move to separate cpp/h files?
{
public:

	void                        SetName(const char* szBehavior, const char* szSubject = nullptr);
	const char*                 GetName() const;
	void                        SetStyleId(const char* szStyleId);
	const char*                 GetStyleId() const;
	ScriptGraphNodePorts&       GetInputs(); // #SchematycTODO : Rather than allowing non-const access should we just provide a Serialize() function?
	const ScriptGraphNodePorts& GetInputs() const;
	ScriptGraphNodePorts&       GetOutputs(); // #SchematycTODO : Rather than allowing non-const access should we just provide a Serialize() function?
	const ScriptGraphNodePorts& GetOutputs() const;

	void                        Exchange(CScriptGraphNodeLayout& rhs);

	inline void                 AddInput(const char* szName, const CryGUID& typeGUID, const ScriptGraphPortFlags& flags)
	{
		AddInput(CUniqueId::FromIdx(m_inputs.size()), szName, typeGUID, flags, CAnyValuePtr());
	}

	inline void AddInput(const CUniqueId& id, const char* szName, const CryGUID& typeGUID, const ScriptGraphPortFlags& flags)
	{
		AddInput(id, szName, typeGUID, flags, CAnyValuePtr());
	}

	inline void AddInputWithData(const char* szName, const CryGUID& typeGUID, const ScriptGraphPortFlags& flags, const CAnyConstRef& value)
	{
		AddInput(CUniqueId::FromIdx(m_inputs.size()), szName, typeGUID, flags, CAnyValue::CloneShared(value));
	}

	inline void AddInputWithData(const CUniqueId& id, const char* szName, const CryGUID& typeGUID, const ScriptGraphPortFlags& flags, const CAnyConstRef& value)
	{
		AddInput(id, szName, typeGUID, flags, CAnyValue::CloneShared(value));
	}

	inline void AddOutput(const char* szName, const CryGUID& typeGUID, const ScriptGraphPortFlags& flags)
	{
		AddOutput(CUniqueId::FromIdx(m_outputs.size()), szName, typeGUID, flags, CAnyValuePtr());
	}

	inline void AddOutput(const CUniqueId& id, const char* szName, const CryGUID& typeGUID, const ScriptGraphPortFlags& flags)
	{
		AddOutput(id, szName, typeGUID, flags, CAnyValuePtr());
	}

	inline void AddOutputWithData(const char* szName, const CryGUID& typeGUID, const ScriptGraphPortFlags& flags, const CAnyConstRef& value)
	{
		AddOutput(CUniqueId::FromIdx(m_outputs.size()), szName, typeGUID, flags, CAnyValue::CloneShared(value));
	}

	inline void AddOutputWithData(const CUniqueId& id, const char* szName, const CryGUID& typeGUID, const ScriptGraphPortFlags& flags, const CAnyConstRef& value)
	{
		AddOutput(id, szName, typeGUID, flags, CAnyValue::CloneShared(value));
	}

private:

	void AddInput(const CUniqueId& id, const char* szName, const CryGUID& typeGUID, const ScriptGraphPortFlags& flags, const CAnyValuePtr& pData);
	void AddOutput(const CUniqueId& id, const char* szName, const CryGUID& typeGUID, const ScriptGraphPortFlags& flags, const CAnyValuePtr& pData);

private:

	string               m_name;
	string               m_styleId;
	ScriptGraphNodePorts m_inputs;
	ScriptGraphNodePorts m_outputs;
};

class CScriptGraphNode : public IScriptGraphNode
{
public:

	CScriptGraphNode(const CryGUID& guid, std::unique_ptr<CScriptGraphNodeModel> pModel);                  // #SchematycTODO : Make pModel the first parameter?
	CScriptGraphNode(const CryGUID& guid, std::unique_ptr<CScriptGraphNodeModel> pModel, const Vec2& pos); // #SchematycTODO : Make pModel the first parameter?

	// IScriptGraphNode
	virtual void                 Attach(IScriptGraph& graph) override;
	virtual IScriptGraph&        GetGraph() override;
	virtual const IScriptGraph&  GetGraph() const override;
	virtual CryGUID                GetTypeGUID() const override;
	virtual CryGUID                GetGUID() const override;
	virtual const char*          GetName() const override;
	virtual const char*          GetStyleId() const override;
	virtual ScriptGraphNodeFlags GetFlags() const override;
	virtual void                 SetPos(Vec2 pos) override;
	virtual Vec2                 GetPos() const override;
	virtual uint32               GetInputCount() const override;
	virtual uint32               FindInputById(const CUniqueId& id) const override;
	virtual CUniqueId            GetInputId(uint32 inputIdx) const override;
	virtual const char*          GetInputName(uint32 inputIdx) const override;
	virtual CryGUID                GetInputTypeGUID(uint32 inputIdx) const override;
	virtual ScriptGraphPortFlags GetInputFlags(uint32 inputIdx) const override;
	virtual CAnyConstPtr         GetInputData(uint32 inputIdx) const override;
	virtual ColorB               GetInputColor(uint32 inputIdx) const override;
	virtual uint32               GetOutputCount() const override;
	virtual uint32               FindOutputById(const CUniqueId& id) const override;
	virtual CUniqueId            GetOutputId(uint32 outputIdx) const override;
	virtual const char*          GetOutputName(uint32 outputIdx) const override;
	virtual CryGUID                GetOutputTypeGUID(uint32 outputIdx) const override;
	virtual ScriptGraphPortFlags GetOutputFlags(uint32 outputIdx) const override;
	virtual CAnyConstPtr         GetOutputData(uint32 outputIdx) const override;
	virtual ColorB               GetOutputColor(uint32 outputIdx) const override;
	virtual void                 EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const override;
	virtual void                 RemapDependencies(IGUIDRemapper& guidRemapper) override;
	virtual void                 ProcessEvent(const SScriptEvent& event) override;
	virtual void                 Serialize(Serialization::IArchive& archive) override;
	virtual void                 Copy(Serialization::IArchive& archive) override;
	virtual void                 Paste(Serialization::IArchive& archive) override;
	virtual void                 Validate(const Validator& validator) const override;
	virtual void                 Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const override;
	// ~IScriptGraphNode

	void   SetFlags(const ScriptGraphNodeFlags& flags);
	uint32 FindInputByName(const char* szName) const;
	uint32 FindOutputByName(const char* szName) const;

private:

	void RefreshLayout();

	void SerializeBasicInfo(Serialization::IArchive& archive);
	void SerializeInputs(Serialization::IArchive& archive);

private:

	CryGUID                                  m_guid;
	std::unique_ptr<CScriptGraphNodeModel> m_pModel;
	Vec2                                   m_pos = Vec2(ZERO);
	string                                 m_styleId;
	ScriptGraphNodeFlags                   m_flags;
	CScriptGraphNodeLayout                 m_layout;
	IScriptGraph*                          m_pGraph = nullptr;
};

} // Schematyc
