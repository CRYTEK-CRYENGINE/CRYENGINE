// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Script/GraphNodes/ScriptGraphNodeBase.h"

#include <CrySchematyc2/Runtime/IRuntime.h>

namespace Schematyc2
{
	class CScriptGraphNodeFactory;

	class CScriptGraphFunctionNode : public CScriptGraphNodeBase
	{
	public:

		enum class EFunctionType
		{
			Unknown,
			ComponentPropertiesMember
		};

		struct EInputIdx
		{
			enum : uint32
			{
				In = 0,
				FirstParam
			};
		};

		struct EOutputIdx
		{
			enum : uint32
			{
				Out = 0,
				FirstParam
			};
		};

		struct EAttributeId
		{
			enum : uint32
			{
				Params
			};
		};

	private:

		struct SFunctionInput
		{
			SFunctionInput();
			SFunctionInput(uint32 _id, const IAnyPtr& _pValue);

			uint32  id;
			IAnyPtr pValue;
		};

		typedef std::vector<SFunctionInput> FunctionInputs;

	public:

		CScriptGraphFunctionNode(); // #SchematycTODO : Do we really need a default constructor?
		CScriptGraphFunctionNode(const SGUID& guid);
		CScriptGraphFunctionNode(const SGUID& guid, const Vec2& pos, EFunctionType functionType, const SGUID& contextGUID, const SGUID& refGUID);

		// IScriptGraphNode
		virtual SGUID GetTypeGUID() const override;
		virtual EScriptGraphColor GetColor() const override;
		virtual void Refresh(const SScriptRefreshParams& params) override;
		virtual void Serialize(Serialization::IArchive& archive) override;
		virtual void RemapGUIDs(IGUIDRemapper& guidRemapper) override;
		virtual void Compile_New(IScriptGraphNodeCompiler& compiler) const override;
		// ~IScriptGraphNode

		static void RegisterCreator(CScriptGraphNodeFactory& factory);

	private:

		void CreateInputValues();
		void SerializeBasicInfo(Serialization::IArchive& archive);
		void SerializeFunctionInputs(Serialization::IArchive& archive);
		void Edit(Serialization::IArchive& archive);
		void Validate(Serialization::IArchive& archive);

		static SRuntimeResult Execute(IObject* pObject, const SRuntimeActivationParams& activationParams, CRuntimeNodeData& data);

	public:

		static const SGUID s_typeGUID;

	private:

		EFunctionType  m_functionType;
		SGUID          m_contextGUID;
		SGUID          m_refGUID;
		FunctionInputs m_functionInputs;
	};
}
