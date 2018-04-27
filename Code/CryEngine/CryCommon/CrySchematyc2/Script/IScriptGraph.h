// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CrySchematyc2/TemplateUtils/TemplateUtils_Signalv2.h>

#include "CrySchematyc2/AggregateTypeId.h"
#include "CrySchematyc2/GUID.h"
#include "CrySchematyc2/IAny.h"
#include "CrySchematyc2/Script/IScriptExtension.h"
#include "CrySchematyc2/Script/IScriptFile.h" // #SchematycTODO : Remove once all graphs are script extensions!!!

namespace Schematyc2
{
	struct IDocGraphNodeCompiler;
	struct IDocGraphSequenceLinker;
	struct IDocGraphSequencePreCompiler;
	struct IDomainContext;
	struct IScriptGraphExtension;
	struct IScriptGraphNodeCompiler;
	struct IScriptGraphNodeCreationMenu;

	enum class EScriptGraphType // #SchematycTODO : Once all graphs are script extensions we on need three graph types: Logic, Transition and Property.
	{
		Unknown,
		AbstractInterfaceFunction,
		Function,
		Condition,
		Constructor,
		Destructor,
		SignalReceiver,
		Transition,
		Property
	};

	struct SScriptGraphParams
	{
		inline SScriptGraphParams(const SGUID& _scopeGUID, const char* _szName, EScriptGraphType _type, const SGUID& _contextGUID)
			: scopeGUID(_scopeGUID)
			, szName(_szName)
			, type(_type)
			, contextGUID(_contextGUID)
		{}

		const SGUID&     scopeGUID;
		const char*      szName;
		EScriptGraphType type;
		const SGUID&     contextGUID;
	};

	enum class EScriptGraphNodeType // #SchematycTODO : Remove once all graphs are script extensions!!!
	{
		Unknown,
		Begin,
		BeginConstructor,
		BeginDestructor,
		BeginSignalReceiver,
		Return,
		Sequence,
		Branch,
		Switch,
		MakeEnumeration,
		ForLoop,
		ProcessSignal,
		SendSignal,
		BroadcastSignal,
		Function,
		Condition,
		AbstractInterfaceFunction,
		GetObject,
		Set,
		Get,
		ContainerAdd,
		ContainerRemoveByIndex,
		ContainerRemoveByValue,
		ContainerClear,
		ContainerSize,
		ContainerGet,
		ContainerSet,
		ContainerFindByValue,
		StartTimer,
		StopTimer,
		ResetTimer,
		State,
		BeginState,
		EndState,
		Comment
	};

	enum class EScriptGraphColor
	{
		NotSet = 0,
		Red,
		Green,
		Blue,
		Yellow,
		Orange,
		White
	};

	enum class EScriptGraphPortFlags // #SchematycTODO : Needs documentation!!!
	{
		None          = 0,
		Removable     = BIT(0),
		MultiLink     = BIT(1),
		BeginSequence = BIT(2),
		EndSequence   = BIT(3),
		Execute       = BIT(4),
		Data          = BIT(5),
		Pull          = BIT(6),
		SpacerAbove   = BIT(7), // Draw spacer above port.
		SpacerBelow   = BIT(8)  // Draw spacer below port.
	};

	DECLARE_ENUM_CLASS_FLAGS(EScriptGraphPortFlags)

	enum class EDocGraphSequenceStep // #SchematycTODO : Remove once all graphs are script extensions!!!
	{
		BeginSequence,
		EndSequence,
		BeginInput,
		EndInput,
		BeginOutput,
		EndOutput,
		PullOutput
	};

	typedef TemplateUtils::CDelegate<void (const char*, const char*, EScriptGraphPortFlags, const CAggregateTypeId&)> ScriptGraphNodeOptionalOutputEnumerator;

	struct IScriptGraphNode // #SchematycTODO : Clean up this interface!!!
	{
		virtual ~IScriptGraphNode() {}

		virtual void Attach(IScriptGraphExtension& graph) {} // #SchematycTODO : Make this pure virtual!!!
		virtual SGUID GetTypeGUID() const { return SGUID(); } // #SchematycTODO : Make this pure virtual!!!
		virtual void SetGUID(const SGUID& guid) = 0; // #SchematycTODO : Remove!!!
		virtual SGUID GetGUID() const = 0;
		// returns a documentation comment for this node
		// if nullptr then the particular node does not contain any documentation
		virtual const char* GetComment() const { return nullptr; }
		virtual void SetName(const char*) = 0; // #SchematycTODO : Remove!!!
		virtual const char* GetName() const = 0;
		virtual EScriptGraphColor GetColor() const { return EScriptGraphColor::NotSet; }  // #SchematycTODO : Make this pure virtual!!!
		virtual EScriptGraphNodeType GetType() const = 0; // #SchematycTODO : Remove!!!
		virtual SGUID GetContextGUID() const = 0; // #SchematycTODO : Remove!!!
		virtual SGUID GetRefGUID() const = 0; // #SchematycTODO : Remove!!!
		virtual void SetPos(Vec2 pos) = 0;
		virtual Vec2 GetPos() const = 0;
		virtual size_t GetInputCount() const = 0;
		virtual uint32 FindInput(const char* szName) const = 0;
		virtual const char* GetInputName(size_t inputIdx) const = 0;
		virtual EScriptGraphPortFlags GetInputFlags(size_t inputIdx) const = 0;
		virtual CAggregateTypeId GetInputTypeId(size_t inputIdx) const = 0;
		virtual size_t GetOutputCount() const = 0;
		virtual uint32 FindOutput(const char* szName) const = 0;
		virtual const char* GetOutputName(size_t outputIdx) const = 0;
		virtual EScriptGraphPortFlags GetOutputFlags(size_t outputIdx) const = 0;
		virtual CAggregateTypeId GetOutputTypeId(size_t outputIdx) const = 0;
		virtual IAnyConstPtr GetCustomOutputDefault() const = 0;
		virtual size_t AddCustomOutput(const IAny& value) = 0;
		virtual void EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) = 0;
		virtual size_t AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId) = 0;
		virtual void RemoveOutput(size_t outputIdx) = 0;
		virtual void Refresh(const SScriptRefreshParams& params) = 0;
		virtual void Serialize(Serialization::IArchive& archive) = 0;
		virtual void RemapGUIDs(IGUIDRemapper& guidRemapper) = 0;
		virtual void PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const = 0; // #SchematycTODO : Remove!!!
		virtual void LinkSequence(IDocGraphSequenceLinker& linker, size_t iOutput, const LibFunctionId& functionId) const = 0; // #SchematycTODO : Remove!!!
		virtual void Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const = 0; // #SchematycTODO : Remove!!!
		virtual void Compile_New(IScriptGraphNodeCompiler& compiler) const {} // #SchematycTODO : Rename Compile()!!!
	};

	DECLARE_SHARED_POINTERS(IScriptGraphNode) // #SchematycTODO : Move to ScriptGraphBase.h!!!

	struct IScriptGraphLink
	{
		virtual ~IScriptGraphLink() {}

		virtual void SetSrcNodeGUID(const SGUID& guid) = 0;
		virtual SGUID GetSrcNodeGUID() const = 0;
		virtual const char* GetSrcOutputName() const = 0;
		virtual void SetDstNodeGUID(const SGUID& guid) = 0;
		virtual SGUID GetDstNodeGUID() const = 0;
		virtual const char* GetDstInputName() const = 0;
		virtual void Serialize(Serialization::IArchive& archive) = 0;
	};

	DECLARE_SHARED_POINTERS(IScriptGraphLink)

	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptGraphNode&)>       ScriptGraphNodeVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptGraphNode&)> ScriptGraphNodeConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptGraphLink&)>       ScriptGraphLinkVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptGraphLink&)> ScriptGraphLinkConstVisitor;

	typedef TemplateUtils::CSignalv2<void (const IScriptGraphLink&)> ScriptGraphLinkRemovedSignal;

	struct SScriptGraphSignals
	{
		ScriptGraphLinkRemovedSignal linkRemoved;
	};

	struct IScriptGraph // #SchematycTODO : Clean up this interface!!!
	{
		virtual ~IScriptGraph() {}

		virtual EScriptGraphType GetType() const = 0;
		virtual SGUID GetContextGUID() const = 0;
		virtual IScriptElement& GetElement_New() = 0; // #SchematycTODO : Rename GetElement() and move to IScriptExtension!!! 
		virtual const IScriptElement& GetElement_New() const = 0; // #SchematycTODO : Rename GetElement() and move to IScriptExtension!!! 
		virtual void SetPos(Vec2 pos) = 0;
		virtual Vec2 GetPos() const = 0;
		virtual size_t GetInputCount() const = 0;
		virtual const char* GetInputName(size_t inputIdx) const = 0;
		virtual IAnyConstPtr GetInputValue(size_t inputIdx) const = 0;
		virtual size_t GetOutputCount() const = 0;
		virtual const char* GetOutputName(size_t outputIdx) const = 0;
		virtual IAnyConstPtr GetOutputValue(size_t outputIdx) const = 0;
		virtual void RefreshAvailableNodes(const CAggregateTypeId& inputTypeId) = 0;
		virtual size_t GetAvailableNodeCount() = 0; // #SchematycTODO : Replace with EnumerateAvaliableNodes() function.
		virtual const char* GetAvailableNodeName(size_t availableNodeIdx) const = 0;
		virtual EScriptGraphNodeType GetAvailableNodeType(size_t availableNodeIdx) const = 0;
		virtual SGUID GetAvailableNodeContextGUID(size_t availableNodeIdx) const = 0;
		virtual SGUID GetAvailableNodeRefGUID(size_t availableNodeIdx) const = 0;
		virtual IScriptGraphNode* AddNode(EScriptGraphNodeType type, const SGUID& contextGUID = SGUID(), const SGUID& refGUID = SGUID(), Vec2 pos = Vec2()) = 0;
		virtual void RemoveNode(const SGUID& guid) = 0;
		virtual IScriptGraphNode* GetNode(const SGUID& guid) = 0;
		virtual const IScriptGraphNode* GetNode(const SGUID& guid) const = 0;
		virtual void VisitNodes(const ScriptGraphNodeVisitor& visitor) = 0;
		virtual void VisitNodes(const ScriptGraphNodeConstVisitor& visitor) const = 0;
		virtual bool CanAddLink(const SGUID& srcNodeGUID, const char* szSrcOutputName, const SGUID& dstNodeGUID, const char* szDstInputName) const = 0;
		virtual IScriptGraphLink* AddLink(const SGUID& srcNodeGUID, const char* szSrcOutputName, const SGUID& dstNodeGUID, const char* szDstInputName) = 0;
		virtual void RemoveLink(size_t iLink) = 0;
		virtual void RemoveLinks(const SGUID& nodeGUID) = 0;
		virtual size_t GetLinkCount() const = 0;
		virtual size_t FindLink(const SGUID& srcNodeGUID, const char* srcOutputName, const SGUID& dstNodeGUID, const char* szDstInputName) const = 0;	// #SchematycTODO : Remove!?!
		virtual IScriptGraphLink* GetLink(size_t linkIdx) = 0;
		virtual const IScriptGraphLink* GetLink(size_t linkIdx) const = 0;
		virtual void VisitLinks(const ScriptGraphLinkVisitor& visitor) = 0;
		virtual void VisitLinks(const ScriptGraphLinkConstVisitor& visitor) const = 0;
		virtual void RemoveBrokenLinks() = 0;
		virtual SScriptGraphSignals &Signals() = 0;
	};

	DECLARE_SHARED_POINTERS(IScriptGraph)

	struct IDocGraph : public IScriptElement, public IScriptGraph
	{
		virtual ~IDocGraph() {}

		virtual IScriptElement& GetElement_New() override
		{
			return *static_cast<IScriptElement*>(this);
		}

		virtual const IScriptElement& GetElement_New() const override
		{
			return *static_cast<const IScriptElement*>(this);
		}
	};

	DECLARE_SHARED_POINTERS(IDocGraph)

	struct IScriptGraphExtension : public IScriptExtension, public IScriptGraph
	{
		virtual ~IScriptGraphExtension() {}

		virtual uint32 GetNodeCount() const = 0;
		virtual bool AddNode_New(const IScriptGraphNodePtr& pNode) = 0; // #SchematycTODO : Rename AddNode()!!!
		virtual void PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu) = 0;
		virtual void VisitInputLinks(const ScriptGraphLinkVisitor& visitor, const SGUID& dstNodeGUID, const char* szDstInputName) = 0;
		virtual void VisitInputLinks(const ScriptGraphLinkConstVisitor& visitor, const SGUID& dstNodeGUID, const char* szDstInputName) const = 0;
		virtual void VisitOutputLinks(const ScriptGraphLinkVisitor& visitor, const SGUID& srcNodeGUID, const char* szSrcOutputName) = 0;
		virtual void VisitOutputLinks(const ScriptGraphLinkConstVisitor& visitor, const SGUID& srcNodeGUID, const char* szSrcOutputName) const = 0;
		virtual bool GetLinkSrc(const IScriptGraphLink& link, IScriptGraphNode*& pNode, uint32& outputIdx) = 0;
		virtual bool GetLinkSrc(const IScriptGraphLink& link, const IScriptGraphNode*& pNode, uint32& outputIdx) const = 0;
		virtual bool GetLinkDst(const IScriptGraphLink& link, IScriptGraphNode*& pNode, uint32& inputIdx) = 0;
		virtual bool GetLinkDst(const IScriptGraphLink& link, const IScriptGraphNode*& pNode, uint32& inputIdx) const = 0;
	};
}
