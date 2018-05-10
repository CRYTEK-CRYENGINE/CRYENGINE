// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/StaticInstanceList.h>
#include <CrySerialization/Forward.h>

#include "CrySchematyc/Utils/GUID.h"

#define SCHEMATYC_REGISTER_SCRIPT_GRAPH_NODE(function) static Schematyc::CScriptGraphNodeRegistrar SCHEMATYC_PP_JOIN_XY(schematycScriptGraphNodeRegistrar, __COUNTER__)(function);

namespace Schematyc
{
// Forward declare interfaces.
struct IGraphNodeCompiler;
struct IGUIDRemapper;
struct ISerializationContext;
// Froward declare structures.
struct SCompilerContext;

// Forward declare classes.
class CScriptGraphNode;
class CScriptGraphNodeFactory;
class CScriptGraphNodeLayout;

class CScriptGraphNodeModel
{
public:

	virtual ~CScriptGraphNodeModel() {}

	virtual void            Init();

	virtual CryGUID           GetTypeGUID() const = 0;
	virtual void            CreateLayout(CScriptGraphNodeLayout& layout) = 0;
	virtual void            Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const = 0;

	virtual void            LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context);
	virtual void            Load(Serialization::IArchive& archive, const ISerializationContext& context); // #SchematycTODO : Load is not currently implemented. Should it be?
	virtual void            PostLoad(Serialization::IArchive& archive, const ISerializationContext& context);
	virtual void            Save(Serialization::IArchive& archive, const ISerializationContext& context);
	virtual void            Edit(Serialization::IArchive& archive, const ISerializationContext& context);
	virtual void            Validate(Serialization::IArchive& archive, const ISerializationContext& context);
	virtual void            RemapDependencies(IGUIDRemapper& guidRemapper);

	void                    Attach(CScriptGraphNode& node);
	CScriptGraphNode&       GetNode();
	const CScriptGraphNode& GetNode() const;

private:

	CScriptGraphNode* m_pNode = nullptr;
};

typedef void (* ScriptGraphNodeRegistrarFunctionPtr)(CScriptGraphNodeFactory& factory);

class CScriptGraphNodeRegistrar : public CStaticInstanceList<CScriptGraphNodeRegistrar>
{
public:

	CScriptGraphNodeRegistrar(ScriptGraphNodeRegistrarFunctionPtr pFunction);

	static void Process(CScriptGraphNodeFactory& factory);

private:

	ScriptGraphNodeRegistrarFunctionPtr m_pFunction;
};
} // Schematyc
