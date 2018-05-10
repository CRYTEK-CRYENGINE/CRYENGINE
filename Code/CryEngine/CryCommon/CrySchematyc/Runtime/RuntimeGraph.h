// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Move functions to .inl file?

#pragma once

#include "CrySchematyc/Runtime/RuntimeParamMap.h"
#include "CrySchematyc/Utils/Assert.h"
#include "CrySchematyc/Services/ILog.h"
#include "CrySchematyc/Utils/Any.h"
#include "CrySchematyc/Utils/Scratchpad.h"
#include "CrySchematyc/Utils/UniqueId.h"

namespace Schematyc
{

struct SRuntimeContext;

typedef uint16 RuntimeGraphNodeIdx;

enum : uint16
{
	InvalidRuntimeGraphNodeIdx = 0xffff
};

typedef uint8 RuntimeGraphPortIdx;

enum : uint8
{
	InvalidRuntimeGraphPortIdx = 0xff
};

enum class ERuntimeStatus : uint8 // #SchematycTODO : Simplify this enumeration by using flags?
{
	Continue = 0,
	ContinueAndRepeat,
	Break,
	Return,
	Error
};

struct SRuntimeResult
{
	inline SRuntimeResult(ERuntimeStatus _status, RuntimeGraphPortIdx _outputIdx = InvalidRuntimeGraphPortIdx)
		: status(_status)
		, outputIdx(_outputIdx)
	{}

	ERuntimeStatus      status;
	RuntimeGraphPortIdx outputIdx;
};

enum class EActivationMode : uint8
{
	NotSet,
	Input,
	Output
};

struct SRuntimeActivationParams
{
	inline SRuntimeActivationParams()
		: nodeIdx(InvalidRuntimeGraphNodeIdx)
		, portIdx(InvalidRuntimeGraphPortIdx)
		, mode(EActivationMode::NotSet)
	{}

	inline SRuntimeActivationParams(RuntimeGraphNodeIdx _nodeIdx, RuntimeGraphPortIdx _portIdx, EActivationMode _mode)
		: nodeIdx(_nodeIdx)
		, portIdx(_portIdx)
		, mode(_mode)
	{}

	inline bool IsInput(RuntimeGraphPortIdx inputIdx) const
	{
		return (mode == EActivationMode::Input) && (portIdx == inputIdx);
	}

	inline bool IsOutput(RuntimeGraphPortIdx outputIdx) const
	{
		return (mode == EActivationMode::Output) && (portIdx == outputIdx);
	}

	RuntimeGraphNodeIdx nodeIdx;
	RuntimeGraphPortIdx portIdx;
	EActivationMode     mode;
};

typedef SRuntimeResult (* RuntimeGraphNodeCallbackPtr)(SRuntimeContext& context, const SRuntimeActivationParams& activationParams);

struct ERuntimeGraphPortFlags // #SchematycTODO : Consider using CEnumFlags if it doesn't affect size.
{
	enum : uint8
	{
		Unused = BIT(0),
		Signal = BIT(1),
		Flow = BIT(2),
		Data = BIT(3),
		Pull = BIT(4),
		Linked = BIT(5),
	};
};

struct SRuntimeGraphPort
{
	inline SRuntimeGraphPort()
		: dataPos(InvalidIdx)
		, linkNodeIdx(InvalidRuntimeGraphNodeIdx)
		, linkPortIdx(InvalidRuntimeGraphPortIdx)
		, flags(ERuntimeGraphPortFlags::Unused)
	{}

	CUniqueId           id;
	uint32              dataPos;
	RuntimeGraphNodeIdx linkNodeIdx;
	RuntimeGraphPortIdx linkPortIdx;
	uint8               flags;
};

class CRuntimeGraphNode
{
private:

	typedef DynArray<SRuntimeGraphPort> Ports; // #SchematycTODO : This should of course not be dynamically allocated.

public:

	inline CRuntimeGraphNode(const CryGUID& guid, const char* szName, RuntimeGraphNodeCallbackPtr pCallback, RuntimeGraphPortIdx inputCount, RuntimeGraphPortIdx outputCount)
		: m_guid(guid)
		, m_name(szName)
		, m_pCallback(pCallback)
		, m_dataPos(InvalidIdx)
		, m_inputs(inputCount)
		, m_outputs(outputCount)
	{}

	inline CryGUID GetGUID() const
	{
		return m_guid;
	}

	inline const char* GetName() const
	{
		return m_name.c_str();
	}

	RuntimeGraphNodeCallbackPtr GetCallback() const
	{
		return m_pCallback;
	}

	inline void SetDataPos(uint32 dataPos)
	{
		m_dataPos = dataPos;
	}

	inline uint32 GetDataPos() const
	{
		return m_dataPos;
	}

	inline void SetInputId(RuntimeGraphPortIdx inputIdx, const CUniqueId& inputId)
	{
		// Validate parameters.

		if (inputIdx >= m_inputs.size())
		{
			// Invalid port index!
			return;
		}

		m_inputs[inputIdx].id = inputId;
	}

	inline bool ConfigureInput(RuntimeGraphPortIdx inputIdx, uint32 dataPos, RuntimeGraphNodeIdx linkNodeIdx, RuntimeGraphPortIdx linkPortIdx, uint8 flags)
	{
		// Validate parameters.

		if (inputIdx >= m_inputs.size())
		{
			// Invalid input index!
			return false;
		}

		SRuntimeGraphPort& input = m_inputs[inputIdx];

		input.dataPos = dataPos;
		input.linkNodeIdx = linkNodeIdx;
		input.linkPortIdx = linkPortIdx;
		input.flags = flags;

		return true;
	}

	inline RuntimeGraphPortIdx GetInputCount() const
	{
		return m_inputs.size();
	}

	inline const SRuntimeGraphPort* GetInputs() const
	{
		return !m_inputs.empty() ? &m_inputs[0] : nullptr;
	}

	inline void SetOutputId(RuntimeGraphPortIdx outputIdx, const CUniqueId& outputId)
	{
		// Validate parameters.

		if (outputIdx >= m_outputs.size())
		{
			// Invalid port index!
			return;
		}

		m_outputs[outputIdx].id = outputId;
	}

	inline bool ConfigureOutput(RuntimeGraphPortIdx outputIdx, uint32 dataPos, RuntimeGraphNodeIdx linkNodeIdx, RuntimeGraphPortIdx linkPortIdx, uint8 flags)
	{
		// Validate parameters.

		if (outputIdx >= m_outputs.size())
		{
			// Invalid output index!
			return false;
		}

		SRuntimeGraphPort& output = m_outputs[outputIdx];

		if (dataPos != InvalidIdx)
		{
			output.dataPos = dataPos;
		}

		if (linkNodeIdx != InvalidRuntimeGraphNodeIdx)
		{
			output.linkNodeIdx = linkNodeIdx;
		}

		if (linkPortIdx != InvalidRuntimeGraphPortIdx)
		{
			output.linkPortIdx = linkPortIdx;
		}

		output.flags = (output.flags & ~ERuntimeGraphPortFlags::Unused) | flags;

		return true;
	}

	inline RuntimeGraphPortIdx GetOutputCount() const
	{
		return m_outputs.size();
	}

	inline const SRuntimeGraphPort* GetOutputs() const
	{
		return !m_outputs.empty() ? &m_outputs[0] : nullptr;
	}

private:

	CryGUID                     m_guid;
	string                      m_name;
	RuntimeGraphNodeCallbackPtr m_pCallback;
	uint32                      m_dataPos;
	Ports                       m_inputs;
	Ports                       m_outputs;
};

class CRuntimeGraphNodeInstance
{
public:

	inline CRuntimeGraphNodeInstance(const CRuntimeGraphNode& node, CScratchpad& scratchpad)
		: m_node(node)
		, m_scratchpad(scratchpad)
	{}

	inline const CryGUID GetGUID() const
	{
		return m_node.GetGUID();
	}

	inline CAnyPtr GetData()
	{
		return m_scratchpad.Get(m_node.GetDataPos());
	}

	inline RuntimeGraphPortIdx GetInputCount() const
	{
		return m_node.GetInputCount();
	}

	inline CUniqueId GetInputId(RuntimeGraphPortIdx inputIdx) const
	{
		const SRuntimeGraphPort& input = GetInput(inputIdx);
		return input.id;
	}

	inline bool IsDataInput(RuntimeGraphPortIdx inputIdx) const
	{
		if (inputIdx >= m_node.GetInputCount())
		{
			return false;
		}

		const SRuntimeGraphPort& input = GetInput(inputIdx);
		return (input.flags & ERuntimeGraphPortFlags::Data) != 0;
	}

	inline bool IsInputLinked(RuntimeGraphPortIdx inputIdx) const
	{
		if (inputIdx >= m_node.GetInputCount())
		{
			return false;
		}

		const SRuntimeGraphPort& input = GetInput(inputIdx);
		return (input.flags & ERuntimeGraphPortFlags::Linked) != 0;
	}

	inline CAnyConstPtr GetInputData(RuntimeGraphPortIdx inputIdx) const
	{
		const SRuntimeGraphPort& input = GetInput(inputIdx);
		return static_cast<const CScratchpad&>(m_scratchpad).Get(input.dataPos);
	}

	inline RuntimeGraphPortIdx GetOutputCount() const
	{
		return m_node.GetOutputCount();
	}

	inline CUniqueId GetOutputId(RuntimeGraphPortIdx outputIdx) const
	{
		const SRuntimeGraphPort& output = GetOutput(outputIdx);
		return output.id;
	}

	inline bool IsDataOutput(RuntimeGraphPortIdx outputIdx) const
	{
		if (outputIdx >= m_node.GetOutputCount())
		{
			return false;
		}

		const SRuntimeGraphPort& output = GetOutput(outputIdx);
		return (output.flags & ERuntimeGraphPortFlags::Data) != 0;
	}

	inline CAnyPtr GetOutputData(RuntimeGraphPortIdx outputIdx)
	{
		const SRuntimeGraphPort& output = GetOutput(outputIdx);
		return m_scratchpad.Get(output.dataPos);
	}

	inline void BindParams(CRuntimeParamMap& params) const
	{
		// #SchematycTODO : Rather than populating the runtime parameter map every time we reference a node it might make more sense to pre-allocate node instances every time we instantiate a graph.
		for (const SRuntimeGraphPort* pInput = m_node.GetInputs(), * pEndInput = pInput + m_node.GetInputCount(); pInput != pEndInput; ++pInput)
		{
			if (pInput->flags & ERuntimeGraphPortFlags::Data)
			{
				params.BindInput(pInput->id, static_cast<const CScratchpad&>(m_scratchpad).Get(pInput->dataPos));
			}
		}
		for (const SRuntimeGraphPort* pOutput = m_node.GetOutputs(), * pEndOutput = pOutput + m_node.GetOutputCount(); pOutput != pEndOutput; ++pOutput)
		{
			if (pOutput->flags & ERuntimeGraphPortFlags::Data)
			{
				params.BindOutput(pOutput->id, m_scratchpad.Get(pOutput->dataPos));
			}
		}
	}

private:

	inline const SRuntimeGraphPort& GetInput(RuntimeGraphPortIdx inputIdx) const
	{
		SCHEMATYC_CORE_ASSERT(inputIdx < m_node.GetInputCount());
		return m_node.GetInputs()[inputIdx];
	}

	inline const SRuntimeGraphPort& GetOutput(RuntimeGraphPortIdx outputIdx) const
	{
		SCHEMATYC_CORE_ASSERT(outputIdx < m_node.GetOutputCount());
		return m_node.GetOutputs()[outputIdx];
	}

private:

	const CRuntimeGraphNode& m_node;
	CScratchpad&             m_scratchpad;
};

class CRuntimeGraph
{
private:

	typedef DynArray<CRuntimeGraphNode>   Nodes;
	typedef DynArray<RuntimeGraphNodeIdx> NodeLookup;

public:

	inline CRuntimeGraph(const CryGUID& guid, const char* szName)
		: m_guid(guid)
		, m_name(szName)
	{}

	inline const CryGUID GetGUID() const
	{
		return m_guid;
	}

	inline bool AddNode(const CryGUID& guid, const char* szName, RuntimeGraphNodeCallbackPtr pCallback, RuntimeGraphPortIdx inputCount, RuntimeGraphPortIdx outputCount)
	{
		if ((inputCount >= InvalidRuntimeGraphPortIdx) || (outputCount >= InvalidRuntimeGraphPortIdx))
		{
			return false;
		}

		m_nodes.push_back(CRuntimeGraphNode(guid, szName, pCallback, inputCount, outputCount));
		m_nodeLookup.push_back(m_nodeLookup.size());

		return true;
	}

	inline void SetNodeInputId(RuntimeGraphNodeIdx nodeIdx, RuntimeGraphPortIdx inputIdx, const CUniqueId& inputId)
	{
		if (nodeIdx >= m_nodeLookup.size())
		{
			// Invalid node index!
		}

		CRuntimeGraphNode& node = m_nodes[m_nodeLookup[nodeIdx]];
		node.SetInputId(inputIdx, inputId);
	}

	inline void SetNodeOutputId(RuntimeGraphNodeIdx nodeIdx, RuntimeGraphPortIdx outputIdx, const CUniqueId& outputId)
	{
		if (nodeIdx >= m_nodeLookup.size())
		{
			// Invalid node index!
		}

		CRuntimeGraphNode& node = m_nodes[m_nodeLookup[nodeIdx]];
		node.SetOutputId(outputIdx, outputId);
	}

	inline void SetNodeData(RuntimeGraphNodeIdx nodeIdx, const CAnyConstRef& value)
	{
		if (nodeIdx >= m_nodeLookup.size())
		{
			// Invalid node index!
			return;
		}

		CRuntimeGraphNode& node = m_nodes[m_nodeLookup[nodeIdx]];
		node.SetDataPos(m_scratchpad.Add(value));
	}

	inline bool SetNodeInputData(RuntimeGraphNodeIdx nodeIdx, RuntimeGraphPortIdx inputIdx, const CAnyConstRef& value)
	{
		if (nodeIdx >= m_nodeLookup.size())
		{
			// Invalid node index!
			return false;
		}

		CRuntimeGraphNode& node = m_nodes[m_nodeLookup[nodeIdx]];
		return node.ConfigureInput(inputIdx, m_scratchpad.Add(value), InvalidRuntimeGraphNodeIdx, InvalidRuntimeGraphPortIdx, ERuntimeGraphPortFlags::Data);
	}

	inline bool SetNodeOutputData(RuntimeGraphNodeIdx nodeIdx, RuntimeGraphPortIdx outputIdx, const CAnyConstRef& value)
	{
		if (nodeIdx >= m_nodeLookup.size())
		{
			// Invalid node index!
			return false;
		}

		CRuntimeGraphNode& node = m_nodes[m_nodeLookup[nodeIdx]];
		return node.ConfigureOutput(outputIdx, m_scratchpad.Add(value), InvalidRuntimeGraphNodeIdx, InvalidRuntimeGraphPortIdx, ERuntimeGraphPortFlags::Data);
	}

	inline bool AddSignalLink(RuntimeGraphNodeIdx srcNodeIdx, RuntimeGraphPortIdx outputIdx, RuntimeGraphNodeIdx dstNodeIdx, RuntimeGraphPortIdx inputIdx)
	{
		if ((srcNodeIdx >= m_nodeLookup.size()) || (dstNodeIdx >= m_nodeLookup.size()))
		{
			// Invalid node index!
			return false;
		}

		CRuntimeGraphNode& srcNode = m_nodes[m_nodeLookup[srcNodeIdx]];
		CRuntimeGraphNode& dstNode = m_nodes[m_nodeLookup[dstNodeIdx]];

		if ((outputIdx >= srcNode.GetOutputCount()) || (inputIdx >= dstNode.GetInputCount()))
		{
			// Invalid port index!
			return false;
		}

		return srcNode.ConfigureOutput(outputIdx, InvalidIdx, m_nodeLookup[dstNodeIdx], inputIdx, ERuntimeGraphPortFlags::Signal | ERuntimeGraphPortFlags::Linked);
	}

	inline bool AddFlowLink(RuntimeGraphNodeIdx srcNodeIdx, RuntimeGraphPortIdx outputIdx, RuntimeGraphNodeIdx dstNodeIdx, RuntimeGraphPortIdx inputIdx)
	{
		if ((srcNodeIdx >= m_nodeLookup.size()) || (dstNodeIdx >= m_nodeLookup.size()))
		{
			// Invalid node index!
			return false;
		}

		CRuntimeGraphNode& srcNode = m_nodes[m_nodeLookup[srcNodeIdx]];
		CRuntimeGraphNode& dstNode = m_nodes[m_nodeLookup[dstNodeIdx]];

		if ((outputIdx >= srcNode.GetOutputCount()) || (inputIdx >= dstNode.GetInputCount()))
		{
			// Invalid port index!
			return false;
		}

		return srcNode.ConfigureOutput(outputIdx, InvalidIdx, m_nodeLookup[dstNodeIdx], inputIdx, ERuntimeGraphPortFlags::Flow | ERuntimeGraphPortFlags::Linked);
	}

	inline bool AddDataLink(RuntimeGraphNodeIdx srcNodeIdx, RuntimeGraphPortIdx outputIdx, RuntimeGraphNodeIdx dstNodeIdx, RuntimeGraphPortIdx inputIdx)
	{
		if ((srcNodeIdx >= m_nodeLookup.size()) || (dstNodeIdx >= m_nodeLookup.size()))
		{
			// Invalid node index!
			return false;
		}

		const CRuntimeGraphNode& srcNode = m_nodes[m_nodeLookup[srcNodeIdx]];
		CRuntimeGraphNode& dstNode = m_nodes[m_nodeLookup[dstNodeIdx]];

		if ((outputIdx >= srcNode.GetOutputCount()) || (inputIdx >= dstNode.GetInputCount()))
		{
			// Invalid port index!
			return false;
		}

		return dstNode.ConfigureInput(inputIdx, srcNode.GetOutputs()[outputIdx].dataPos, InvalidRuntimeGraphNodeIdx, InvalidRuntimeGraphPortIdx, ERuntimeGraphPortFlags::Data | ERuntimeGraphPortFlags::Linked);
	}

	inline bool AddPullLink(RuntimeGraphNodeIdx srcNodeIdx, RuntimeGraphPortIdx outputIdx, RuntimeGraphNodeIdx dstNodeIdx, RuntimeGraphPortIdx inputIdx)
	{
		if ((srcNodeIdx >= m_nodeLookup.size()) || (dstNodeIdx >= m_nodeLookup.size()))
		{
			// Invalid node index!
			return false;
		}

		const CRuntimeGraphNode& srcNode = m_nodes[m_nodeLookup[srcNodeIdx]];
		CRuntimeGraphNode& dstNode = m_nodes[m_nodeLookup[dstNodeIdx]];

		if ((outputIdx >= srcNode.GetOutputCount()) || (inputIdx >= dstNode.GetInputCount()))
		{
			// Invalid port index!
			return false;
		}

		return dstNode.ConfigureInput(inputIdx, srcNode.GetOutputs()[outputIdx].dataPos, m_nodeLookup[srcNodeIdx], outputIdx, ERuntimeGraphPortFlags::Data | ERuntimeGraphPortFlags::Pull | ERuntimeGraphPortFlags::Linked);
	}

	inline const CRuntimeGraphNode* GetNode(RuntimeGraphNodeIdx nodeIdx) const
	{
		if (nodeIdx >= m_nodeLookup.size())
		{
			// Invalid node index!
			return nullptr;
		}

		return &m_nodes[m_nodeLookup[nodeIdx]];
	}

	inline const CScratchpad& GetScratchpad() const
	{
		return m_scratchpad;
	}

private:

	CryGUID     m_guid;
	string      m_name;
	Nodes       m_nodes;
	NodeLookup  m_nodeLookup;
	CScratchpad m_scratchpad;
};

class CRuntimeGraphInstance
{
private:

	enum class ELinkResult : uint8
	{
		Unused = 0,
		Valid,
		Invalid
	};

	typedef DynArray<SRuntimeActivationParams> Bookmarks;

public:

	inline CRuntimeGraphInstance(const CRuntimeGraph& graph)
		: m_graph(graph)
		, m_scratchpad(graph.GetScratchpad())
	{}

	inline const CryGUID GetGUID() const
	{
		return m_graph.GetGUID();
	}

	bool Execute(void* pObject, CRuntimeParamMap& params, SRuntimeActivationParams activationParams);

	void Reset()
	{
		m_scratchpad = m_graph.GetScratchpad();
	}

private:

	ERuntimeStatus PullNodesRecursive(void* pObject, CRuntimeParamMap& params, const CRuntimeGraphNode& node);
	ELinkResult    Link(const CRuntimeGraphNode& node, RuntimeGraphPortIdx outputIdx, SRuntimeActivationParams& activationParams) const;

private:

	const CRuntimeGraph& m_graph;
	CScratchpad          m_scratchpad;
};

struct SRuntimeContext
{
	inline SRuntimeContext(void* _pObject, CRuntimeParamMap& _params, const CRuntimeGraphInstance& _graph, CRuntimeGraphNodeInstance& _node)
		: pObject(_pObject)
		, params(_params)
		, graph(_graph)
		, node(_node)
	{}

	void*                        pObject;
	CRuntimeParamMap&            params;
	const CRuntimeGraphInstance& graph;
	CRuntimeGraphNodeInstance&   node;
};

inline bool CRuntimeGraphInstance::Execute(void* pObject, CRuntimeParamMap& params, SRuntimeActivationParams activationParams)
{
	const CRuntimeGraphNode* pNode = m_graph.GetNode(activationParams.nodeIdx);
	if (!pNode)
	{
		return false;
	}

	// #SchematycTODO : Make sure these activation parameters are correct?

	if (activationParams.portIdx >= pNode->GetOutputCount())  // #SchematycTODO : Perform this check inside the loop?
	{
		// Invalid port index!
		return false;
	}

	Bookmarks bookmarks;
	bookmarks.reserve(32);

	do
	{
		if (activationParams.mode == EActivationMode::Input)
		{
			const ERuntimeStatus status = PullNodesRecursive(pObject, params, *pNode);
			if (status != ERuntimeStatus::Continue)
			{
				break;
			}
		}

		SRuntimeResult result(ERuntimeStatus::Error);

		RuntimeGraphNodeCallbackPtr pCallback = pNode->GetCallback();
		if (pCallback)
		{
			CLogMetaData logMetaData;
			logMetaData.Set(ELogMetaField::LinkCommand, CryLinkUtils::ECommand::Show);
			logMetaData.Set(ELogMetaField::ElementGUID, m_graph.GetGUID());
			logMetaData.Set(ELogMetaField::DetailGUID, pNode->GetGUID());
			SCHEMATYC_LOG_SCOPE(logMetaData);

			CRuntimeGraphNodeInstance nodeInstance(*pNode, m_scratchpad);
			SRuntimeContext context(pObject, params, *this, nodeInstance);

			result = (*pCallback)(context, activationParams);
		}

		if ((result.status == ERuntimeStatus::Continue) || (result.status == ERuntimeStatus::ContinueAndRepeat))
		{
			if (result.status == ERuntimeStatus::ContinueAndRepeat)
			{
				bookmarks.push_back(SRuntimeActivationParams(activationParams.nodeIdx, result.outputIdx, EActivationMode::Output));
			}

			switch (Link(*pNode, result.outputIdx, activationParams))
			{
			case ELinkResult::Unused:
				{
					result.status = ERuntimeStatus::Break;
					break;
				}
			case ELinkResult::Invalid:
				{
					result.status = ERuntimeStatus::Error;
					break;
				}
			}
		}

		if (result.status == ERuntimeStatus::Break)
		{
			if (bookmarks.empty())
			{
				break;
			}
			else
			{
				activationParams = bookmarks.back();
				bookmarks.pop_back();
			}
		}
		else if ((result.status == ERuntimeStatus::Return) || (result.status == ERuntimeStatus::Error))
		{
			break;
		}

		pNode = m_graph.GetNode(activationParams.nodeIdx);
		SCHEMATYC_CORE_ASSERT(pNode);
	}
	while (pNode);

	return true;
}

inline ERuntimeStatus CRuntimeGraphInstance::PullNodesRecursive(void* pObject, CRuntimeParamMap& params, const CRuntimeGraphNode& node)
{
	const SRuntimeGraphPort* pInputs = node.GetInputs();
	for (uint32 inputIdx = 0, inputCount = node.GetInputCount(); inputIdx < inputCount; ++inputIdx)
	{
		const SRuntimeGraphPort& input = pInputs[inputIdx];
		if ((input.flags & ERuntimeGraphPortFlags::Pull) != 0)
		{
			const CRuntimeGraphNode* pPullNode = m_graph.GetNode(input.linkNodeIdx);
			SCHEMATYC_CORE_ASSERT(pPullNode);
			if (pPullNode)
			{
				const ERuntimeStatus status = PullNodesRecursive(pObject, params, *pPullNode);

				if (status != ERuntimeStatus::Continue)
				{
					return status;
				}

				CRuntimeGraphNodeInstance nodeInstance(*pPullNode, m_scratchpad);
				SRuntimeContext context(pObject, params, *this, nodeInstance);
				RuntimeGraphNodeCallbackPtr pCallback = pPullNode->GetCallback();

				SRuntimeResult result = pCallback ? (*pCallback)(context, SRuntimeActivationParams(input.linkNodeIdx, input.linkPortIdx, EActivationMode::Output)) : SRuntimeResult(ERuntimeStatus::Error);

				if (result.status != ERuntimeStatus::Continue)
				{
					return result.status;
				}
			}
		}
	}

	return ERuntimeStatus::Continue;
}

inline CRuntimeGraphInstance::ELinkResult CRuntimeGraphInstance::Link(const CRuntimeGraphNode& node, RuntimeGraphPortIdx outputIdx, SRuntimeActivationParams& activationParams) const
{
	if (outputIdx < node.GetOutputCount())
	{
		const SRuntimeGraphPort& output = node.GetOutputs()[outputIdx];
		if ((output.flags & ERuntimeGraphPortFlags::Unused) != 0)
		{
			return ELinkResult::Unused;
		}
		if ((output.flags & (ERuntimeGraphPortFlags::Signal | ERuntimeGraphPortFlags::Flow)) != 0)
		{
			activationParams.nodeIdx = output.linkNodeIdx;
			activationParams.portIdx = output.linkPortIdx;
			activationParams.mode = EActivationMode::Input;
			return ELinkResult::Valid;
		}
	}
	return ELinkResult::Invalid;
}

} // Schematyc
