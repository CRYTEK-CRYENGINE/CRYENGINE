// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Script/IScriptElement.h"
#include "CrySchematyc/Script/IScriptExtension.h"
#include "CrySchematyc/Script/ScriptDependencyEnumerator.h"
#include "CrySchematyc/Utils/EnumFlags.h"
#include "CrySchematyc/Utils/GUID.h"
#include "CrySchematyc/Utils/Signal.h"
#include "CrySchematyc/Utils/UniqueId.h"
#include "CrySchematyc/Utils/Validator.h"

namespace Schematyc
{

// Forward declare interfaces.
struct IGraphNodeCompiler;
struct IScriptGraph;
struct IScriptGraphNodeCreationMenu;
// Forward declare structures.
struct SCompilerContext;
struct SElementId;
// Forward declare classes.
class CAnyConstPtr;

enum class EScriptGraphNodeFlags
{
	NotCopyable  = BIT(0),
	NotRemovable = BIT(1)
};

typedef CEnumFlags<EScriptGraphNodeFlags> ScriptGraphNodeFlags;

enum class EScriptGraphPortFlags
{
	None = 0,

	// Category.
	Signal = BIT(0),        // Port is signal.
	Flow   = BIT(1),        // Port controls internal flow.
	Data   = BIT(2),        // Port has data.

	// Connection modifiers.
	MultiLink = BIT(3),     // Multiple links can be connected to port.

	// Flow modifiers.
	Begin = BIT(4),         // Port initiates flow.
	End   = BIT(5),         // Port terminates flow.

	// Data modifiers.
	Ptr        = BIT(6),  // Data is pointer.
	Array      = BIT(7),  // Data is array.
	Persistent = BIT(8),  // Data will be saved to file.
	Editable   = BIT(9),  // Data can be modified in editor.
	Pull       = BIT(10), // Data can be pulled from node without need to trigger an input.

	// Layout modifiers.
	SpacerAbove = BIT(11),   // Draw spacer above port.
	SpacerBelow = BIT(12)    // Draw spacer below port.
};

typedef CEnumFlags<EScriptGraphPortFlags> ScriptGraphPortFlags;

struct IScriptGraphNode // #SchematycTODO : Move to separate header?
{
	virtual ~IScriptGraphNode() {}

	virtual void                Attach(IScriptGraph& graph) = 0;
	virtual IScriptGraph&       GetGraph() = 0;
	virtual const IScriptGraph& GetGraph() const = 0;
	virtual CryGUID             GetTypeGUID() const = 0;
	virtual CryGUID             GetGUID() const = 0;
	virtual const char*         GetName() const = 0;
	//virtual const char*          GetBehavior() const = 0;
	//virtual const char*          GetSubject() const = 0;
	//virtual const char*          GetDescription() const = 0;
	virtual const char*          GetStyleId() const = 0;
	virtual ScriptGraphNodeFlags GetFlags() const = 0;
	virtual void                 SetPos(Vec2 pos) = 0;
	virtual Vec2                 GetPos() const = 0;
	virtual uint32               GetInputCount() const = 0;
	virtual uint32               FindInputById(const CUniqueId& id) const = 0;
	virtual CUniqueId            GetInputId(uint32 inputIdx) const = 0;
	virtual const char*          GetInputName(uint32 inputIdx) const = 0;
	virtual CryGUID              GetInputTypeGUID(uint32 inputIdx) const = 0;
	virtual ScriptGraphPortFlags GetInputFlags(uint32 inputIdx) const = 0;
	virtual CAnyConstPtr         GetInputData(uint32 inputIdx) const = 0;
	virtual ColorB               GetInputColor(uint32 inputIdx) const = 0;
	virtual uint32               GetOutputCount() const = 0;
	virtual uint32               FindOutputById(const CUniqueId& id) const = 0;
	virtual CUniqueId            GetOutputId(uint32 outputIdx) const = 0;
	virtual const char*          GetOutputName(uint32 outputIdx) const = 0;
	virtual CryGUID              GetOutputTypeGUID(uint32 outputIdx) const = 0;
	virtual ScriptGraphPortFlags GetOutputFlags(uint32 outputIdx) const = 0;
	virtual CAnyConstPtr         GetOutputData(uint32 outputIdx) const = 0;
	virtual ColorB               GetOutputColor(uint32 outputIdx) const = 0;
	virtual void                 EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const = 0;
	virtual void                 ProcessEvent(const SScriptEvent& event) = 0;
	virtual void                 Serialize(Serialization::IArchive& archive) = 0;
	virtual void                 Copy(Serialization::IArchive& archive) = 0;  // #SchematycTODO : Rather than having an explicit copy function consider using standard save pass with a copy flag set in the context.
	virtual void                 Paste(Serialization::IArchive& archive) = 0; // #SchematycTODO : Rather than having an explicit paste function consider using standard load passes with a paste flag set in the context.
	virtual void                 Validate(const Validator& validator) const = 0;
	virtual void                 RemapDependencies(IGUIDRemapper& guidRemapper) = 0;
	virtual void                 Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const {}
};

DECLARE_SHARED_POINTERS(IScriptGraphNode)

typedef std::function<EVisitStatus(IScriptGraphNode&)>       ScriptGraphNodeVisitor;
typedef std::function<EVisitStatus(const IScriptGraphNode&)> ScriptGraphNodeConstVisitor;

struct IScriptGraphLink // #SchematycTODO : Once all ports are referenced by id we can replace this with a simple SSCriptGraphLink structure.
{
	virtual ~IScriptGraphLink() {}

	virtual void      SetSrcNodeGUID(const CryGUID& guid) = 0;
	virtual CryGUID   GetSrcNodeGUID() const = 0;
	virtual CUniqueId GetSrcOutputId() const = 0;
	virtual void      SetDstNodeGUID(const CryGUID& guid) = 0;
	virtual CryGUID   GetDstNodeGUID() const = 0;
	virtual CUniqueId GetDstInputId() const = 0;
	virtual void      Serialize(Serialization::IArchive& archive) = 0;
};

DECLARE_SHARED_POINTERS(IScriptGraphLink)

typedef std::function<EVisitStatus(IScriptGraphLink&)>       ScriptGraphLinkVisitor;
typedef std::function<EVisitStatus(const IScriptGraphLink&)> ScriptGraphLinkConstVisitor;

enum class EScriptGraphType // #SchematycTODO : Do we really need this enumeration or can we get the information we need from the element that owns the graph?
{
	Construction,
	Signal,
	Function,
	Transition
};

struct SScriptGraphParams
{
	inline SScriptGraphParams(const CryGUID& _scopeGUID, const char* _szName, EScriptGraphType _type, const CryGUID& _contextGUID)
		: scopeGUID(_scopeGUID)
		, szName(_szName)
		, type(_type)
		, contextGUID(_contextGUID)
	{}

	const CryGUID&   scopeGUID;
	const char*      szName;
	EScriptGraphType type;
	const CryGUID&   contextGUID;
};

typedef CSignal<void (const IScriptGraphLink&)> ScriptGraphLinkRemovedSignal;

struct IScriptGraphNodeCreationCommand
{
	virtual ~IScriptGraphNodeCreationCommand() {}

	virtual const char*         GetBehavior() const = 0;      // What is the node's responsibility?
	virtual const char*         GetSubject() const = 0;       // What does the node operate on?
	virtual const char*         GetDescription() const = 0;   // Get detailed node description.
	virtual const char*         GetStyleId() const = 0;       // Get unique id of node's style.
	virtual IScriptGraphNodePtr Execute(const Vec2& pos) = 0; // Create node.
};

DECLARE_SHARED_POINTERS(IScriptGraphNodeCreationCommand) // #SchematycTODO : Shouldn't these be unique pointers?

struct IScriptGraphNodeCreationMenu
{
	virtual ~IScriptGraphNodeCreationMenu() {}

	virtual bool AddCommand(const IScriptGraphNodeCreationCommandPtr& pCommand) = 0;
};

struct IScriptGraph : public IScriptExtensionBase<EScriptExtensionType::Graph>
{
	virtual ~IScriptGraph() {}

	virtual EScriptGraphType                     GetType() const = 0;

	virtual void                                 SetPos(Vec2 pos) = 0;
	virtual Vec2                                 GetPos() const = 0;

	virtual void                                 PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu) = 0;
	virtual bool                                 AddNode(const IScriptGraphNodePtr& pNode) = 0;
	virtual IScriptGraphNodePtr                  AddNode(const CryGUID& typeGUID) = 0;
	virtual void                                 RemoveNode(const CryGUID& guid) = 0;
	virtual uint32                               GetNodeCount() const = 0;
	virtual IScriptGraphNode*                    GetNode(const CryGUID& guid) = 0;
	virtual const IScriptGraphNode*              GetNode(const CryGUID& guid) const = 0;
	virtual void                                 VisitNodes(const ScriptGraphNodeVisitor& visitor) = 0;
	virtual void                                 VisitNodes(const ScriptGraphNodeConstVisitor& visitor) const = 0;

	virtual bool                                 CanAddLink(const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId) const = 0;
	virtual IScriptGraphLink*                    AddLink(const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId) = 0;
	virtual void                                 RemoveLink(uint32 iLink) = 0;
	virtual void                                 RemoveLinks(const CryGUID& nodeGUID) = 0;
	virtual uint32                               GetLinkCount() const = 0;
	virtual IScriptGraphLink*                    GetLink(uint32 linkIdx) = 0;
	virtual const IScriptGraphLink*              GetLink(uint32 linkIdx) const = 0;
	virtual uint32                               FindLink(const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId) const = 0;
	virtual EVisitResult                         VisitLinks(const ScriptGraphLinkVisitor& visitor) = 0;
	virtual EVisitResult                         VisitLinks(const ScriptGraphLinkConstVisitor& visitor) const = 0;
	virtual EVisitResult                         VisitInputLinks(const ScriptGraphLinkVisitor& visitor, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId) = 0;
	virtual EVisitResult                         VisitInputLinks(const ScriptGraphLinkConstVisitor& visitor, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId) const = 0;
	virtual EVisitResult                         VisitOutputLinks(const ScriptGraphLinkVisitor& visitor, const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId) = 0;
	virtual EVisitResult                         VisitOutputLinks(const ScriptGraphLinkConstVisitor& visitor, const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId) const = 0;
	virtual bool                                 GetLinkSrc(const IScriptGraphLink& link, IScriptGraphNode*& pNode, uint32& outputIdx) = 0;
	virtual bool                                 GetLinkSrc(const IScriptGraphLink& link, const IScriptGraphNode*& pNode, uint32& outputIdx) const = 0;
	virtual bool                                 GetLinkDst(const IScriptGraphLink& link, IScriptGraphNode*& pNode, uint32& inputIdx) = 0;
	virtual bool                                 GetLinkDst(const IScriptGraphLink& link, const IScriptGraphNode*& pNode, uint32& inputIdx) const = 0;

	virtual void                                 RemoveBrokenLinks() = 0;
	virtual ScriptGraphLinkRemovedSignal::Slots& GetLinkRemovedSignalSlots() = 0;

	virtual void                                 FixMapping(IScriptGraphNode& node) = 0;
};

} // Schematyc
