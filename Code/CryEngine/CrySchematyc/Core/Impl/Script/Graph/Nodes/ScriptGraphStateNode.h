// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Runtime/RuntimeGraph.h>
#include <CrySchematyc/SerializationUtils/SerializationQuickSearch.h>
#include <CrySchematyc/Utils/GUID.h>

#include "Script/Graph/ScriptGraphNodeModel.h"

namespace Schematyc
{
class CScriptGraphStateNode : public CScriptGraphNodeModel
{
public:

	enum class EOutputType
	{
		Unknown,
		EnvSignal,
		ScriptSignal,
		ScriptTimer
	};

private:

	struct EInputIdx
	{
		enum : uint32
		{
			In = 0,
			Value
		};
	};

	struct SOutputParams // #SchematycTODO : Rename STransitionParams?
	{
		SOutputParams();
		SOutputParams(EOutputType _type, const CryGUID& _guid);

		void Serialize(Serialization::IArchive& archive);

		bool operator==(const SOutputParams& rhs) const;

		EOutputType type;
		CryGUID       guid;
	};

	typedef SerializationUtils::SQuickSearchTypeWrapper<SOutputParams> Output;
	typedef std::vector<Output>                                        Outputs;

	struct SRuntimeData
	{
		SRuntimeData(uint32 _stateIdx);
		SRuntimeData(const SRuntimeData& rhs);

		static void ReflectType(CTypeDesc<SRuntimeData>& desc);

		uint32       stateIdx;
	};

public:

	CScriptGraphStateNode();
	CScriptGraphStateNode(const CryGUID& stateGUID);

	// CScriptGraphNodeModel
	virtual CryGUID GetTypeGUID() const override;
	virtual void  CreateLayout(CScriptGraphNodeLayout& layout) override;
	virtual void  Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const override;
	virtual void  LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void  Save(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void  Edit(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void  Validate(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void  RemapDependencies(IGUIDRemapper& guidRemapper) override;
	// ~CScriptGraphNodeModel

	static void Register(CScriptGraphNodeFactory& factory);

private:

	void                  Edit(Serialization::IArchive& archive);
	void                  Validate(Serialization::IArchive& archive);

	static SRuntimeResult Execute(SRuntimeContext& context, const SRuntimeActivationParams& activationParams);

public:

	static const CryGUID ms_typeGUID;

private:

	CryGUID   m_stateGUID;
	Outputs m_outputs;
};
}
