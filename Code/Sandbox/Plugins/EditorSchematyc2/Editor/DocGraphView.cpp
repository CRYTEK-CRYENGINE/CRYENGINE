// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DocGraphView.h"

#include <QBoxLayout>
#include <Util/QParentWndWidget.h>
#include <QPushButton>
#include <QSplitter>
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySystem/ITimer.h>
#include <CrySchematyc2/TypeInfo.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Utils/CryLinkUtils.h>
#include <CrySchematyc2/Services/ILog.h>

#include "BrowserIcons.h"
#include "QuickSearchDlg.h"
#include "QMFCApp/QWinHost.h"

#include <Controls/EditorDialog.h>
#include <QAdvancedPropertyTree.h>
#include <QVBoxLayout>

namespace Schematyc2
{
	class CPropertyTreeDlg : public CEditorDialog
	{
	public:
		CPropertyTreeDlg(const QString& moduleName, const Serialization::SStruct& properties)
			: CEditorDialog(moduleName)
		{
			QAdvancedPropertyTree* pPropertyTree = new QAdvancedPropertyTree("Properties Dialog", this);
			pPropertyTree->attach(properties);

			//---

			QPushButton* okButton = new QPushButton(tr("Ok"), this);
			okButton->setDefault(true);
			connect(okButton, &QPushButton::clicked, [this]()
			{
				accept();
			});

			QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);
			connect(cancelButton, &QPushButton::clicked, [this]()
			{
				reject();
			});

			auto buttonsLayout = new QHBoxLayout();
			buttonsLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred));
			buttonsLayout->addWidget(okButton);
			buttonsLayout->addWidget(cancelButton);

			//---

			auto mainLayout = new QVBoxLayout();
			mainLayout->addWidget(pPropertyTree);
			mainLayout->addLayout(buttonsLayout);
			setLayout(mainLayout);
		}
	};

	namespace DocGraphViewUtils
	{
		static const struct { EScriptGraphNodeType scriptGraphNodeType; Gdiplus::Color color; } HEADER_COLORS[] =
		{
			{ EScriptGraphNodeType::Begin,                         Gdiplus::Color(38, 184, 33)   },
			{ EScriptGraphNodeType::BeginConstructor ,             Gdiplus::Color(38, 184, 33)   },
			{ EScriptGraphNodeType::BeginSignalReceiver,           Gdiplus::Color(38, 184, 33)   },
			{ EScriptGraphNodeType::Return,                        Gdiplus::Color(38, 184, 33)   },
			{ EScriptGraphNodeType::Branch,                        Gdiplus::Color(38, 184, 33)   },
			{ EScriptGraphNodeType::Set,                           Gdiplus::Color(0, 108, 217)   },
			{ EScriptGraphNodeType::Get,                           Gdiplus::Color(0, 108, 217)   },
			{ EScriptGraphNodeType::SendSignal,                    Gdiplus::Color(38, 184, 33)   },
			{ EScriptGraphNodeType::Function,                      Gdiplus::Color(215, 55, 55)   },
			{ EScriptGraphNodeType::Condition,                     Gdiplus::Color(250, 232, 12)  },
			{ EScriptGraphNodeType::AbstractInterfaceFunction,     Gdiplus::Color(160, 160, 160) },
			{ EScriptGraphNodeType::StartTimer,                    Gdiplus::Color(38, 184, 33)   },
			{ EScriptGraphNodeType::StopTimer,                     Gdiplus::Color(38, 184, 33)   },
			{ EScriptGraphNodeType::ResetTimer,                    Gdiplus::Color(38, 184, 33)   },
			{ EScriptGraphNodeType::State,                         Gdiplus::Color(0, 108, 217)   },
			{ EScriptGraphNodeType::Comment,                       Gdiplus::Color(255, 255, 255) }
		};

		static const Gdiplus::Color DEFAULT_HEADER_COLOR = Gdiplus::Color(38, 184, 33);

		inline Gdiplus::Color GetHeaderColor(EScriptGraphNodeType scriptGraphNodeType)
		{
			for(size_t headerColorIdx = 0; headerColorIdx < CRY_ARRAY_COUNT(HEADER_COLORS); ++ headerColorIdx)
			{
				if(HEADER_COLORS[headerColorIdx].scriptGraphNodeType == scriptGraphNodeType)
				{
					return HEADER_COLORS[headerColorIdx].color;
				}
			}
			return DEFAULT_HEADER_COLOR;
		}

		static const struct { EScriptGraphColor color; Gdiplus::Color value; } s_colorValues[] =
		{
			{ EScriptGraphColor::Red,    Gdiplus::Color(215, 55, 55)   },
			{ EScriptGraphColor::Green , Gdiplus::Color(38, 184, 33)   },
			{ EScriptGraphColor::Blue,   Gdiplus::Color(0, 108, 217)   },
			{ EScriptGraphColor::Yellow, Gdiplus::Color(250, 232, 12)  },
			{ EScriptGraphColor::Orange, Gdiplus::Color(255, 100, 15)  },
			{ EScriptGraphColor::White,  Gdiplus::Color(255, 255, 255) }
		};

		inline Gdiplus::Color GetColorValue(EScriptGraphColor color)
		{
			for(uint32 colorIdx = 0; colorIdx < CRY_ARRAY_COUNT(s_colorValues); ++ colorIdx)
			{
				if(s_colorValues[colorIdx].color == color)
				{
					return s_colorValues[colorIdx].value;
				}
			}
			return Gdiplus::Color();
		}

		static const struct { EnvTypeId typeId; Gdiplus::Color color; } PORT_COLORS[] =
		{
			{ GetEnvTypeId<bool>(),             Gdiplus::Color(0, 108, 217)   },
			{ GetEnvTypeId<int32>(),            Gdiplus::Color(215, 55, 55)   },
			{ GetEnvTypeId<uint32>(),           Gdiplus::Color(215, 55, 55)   },
			{ GetEnvTypeId<float>(),            Gdiplus::Color(185, 185, 185) },
			{ GetEnvTypeId<Vec3>(),             Gdiplus::Color(250, 232, 12)  },
			{ GetEnvTypeId<SGUID>(),            Gdiplus::Color(38, 184, 33)   },
			{ GetEnvTypeId<CPoolString>(),      Gdiplus::Color(128, 100, 162) },
			{ GetEnvTypeId<ExplicitEntityId>(), Gdiplus::Color(215, 55, 55)   }
		};

		static const Gdiplus::Color DEFAULT_PORT_COLOR = Gdiplus::Color(28, 212, 22);

		inline Gdiplus::Color GetPortColor(const EnvTypeId& typeId)
		{
			for(size_t iPortColor = 0; iPortColor < CRY_ARRAY_COUNT(PORT_COLORS); ++ iPortColor)
			{
				if(PORT_COLORS[iPortColor].typeId == typeId)
				{
					return PORT_COLORS[iPortColor].color;
				}
			}
			return DEFAULT_PORT_COLOR;
		}

		struct SAnyWrapper
		{
			inline SAnyWrapper(const IAnyPtr& _pValue)
				: pValue(_pValue)
			{}

			void Serialize(Serialization::IArchive& archive)
			{
				archive(*pValue, "value", "Value");
			}

			IAnyPtr	pValue;
		};

		struct SOptionalNodeOutput
		{
			inline SOptionalNodeOutput()
				: flags(EScriptGraphPortFlags::None)
			{}

			inline SOptionalNodeOutput(const char* _szName, const char* _szFullName, EScriptGraphPortFlags _flags, const CAggregateTypeId& _typeId)
				: name(_szName)
				, fullName(_szFullName)
				, flags(_flags)
				, typeId(_typeId)
			{}

			string                name;
			string                fullName;
			EScriptGraphPortFlags flags;
			CAggregateTypeId      typeId;
		};

		typedef std::vector<SOptionalNodeOutput> OptionalNodeOutputs;

		class COptionalNodeOutputQuickSearchOptions : public Schematyc2::IQuickSearchOptions
		{
		public:

			inline COptionalNodeOutputQuickSearchOptions(IScriptGraphNode& scriptGraphNode)
			{
				m_optionalNodeOutputs.reserve(256);
				scriptGraphNode.EnumerateOptionalOutputs(ScriptGraphNodeOptionalOutputEnumerator::FromMemberFunction<COptionalNodeOutputQuickSearchOptions, &COptionalNodeOutputQuickSearchOptions::EnumerateOptionalNodeOutput>(*this));
			}

			// IQuickSearchOptions

			virtual const char* GetToken() const override
			{
				return "::";
			}

			virtual size_t GetCount() const override
			{
				return m_optionalNodeOutputs.size();
			}

			virtual const char* GetLabel(size_t iOption) const override
			{
				return iOption < m_optionalNodeOutputs.size() ? m_optionalNodeOutputs[iOption].fullName.c_str() : "";
			}

			virtual const char* GetDescription(size_t iOption) const override
			{
				return nullptr;
			}

			virtual const char* GetWikiLink(size_t iOption) const override
			{
				return nullptr;
			}

			// ~IQuickSearchOptions

			inline const SOptionalNodeOutput* GetOptionalNodeOutput(size_t iOptionalNodeOutput) const
			{
				return iOptionalNodeOutput < m_optionalNodeOutputs.size() ? &m_optionalNodeOutputs[iOptionalNodeOutput] : nullptr;
			}

		private:

			void EnumerateOptionalNodeOutput(const char* szName, const char* szFullName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
			{
				m_optionalNodeOutputs.push_back(SOptionalNodeOutput(szName, szFullName, flags, typeId));
			}

			OptionalNodeOutputs m_optionalNodeOutputs;
		};

		inline bool AddOptionalNodeOutput(CWnd* pWnd, CPoint point, IScriptGraphNode& scriptGraphNode)
		{
			SET_LOCAL_RESOURCE_SCOPE

			COptionalNodeOutputQuickSearchOptions quickSearchOptions(scriptGraphNode);
			CQuickSearchDlg                       quickSearchDlg(pWnd, point, "Output", nullptr, nullptr, quickSearchOptions);
			if(quickSearchDlg.DoModal() == IDOK)
			{
				const SOptionalNodeOutput*	pOptionalNodeOutput = quickSearchOptions.GetOptionalNodeOutput(quickSearchDlg.GetResult());
				SCHEMATYC2_SYSTEM_ASSERT(pOptionalNodeOutput);
				if(pOptionalNodeOutput)
				{
					scriptGraphNode.AddOptionalOutput(pOptionalNodeOutput->name.c_str(), pOptionalNodeOutput->flags, pOptionalNodeOutput->typeId);
					return true;
				}
			}
			return false;
		}

		static const float s_dragAndDropOffset = 10.0f;
	}

	CDocGraphViewNode::CDocGraphViewNode(const SGraphViewGrid& grid, Gdiplus::PointF pos, TScriptFile& scriptFile, IScriptGraph& scriptGraph, IScriptGraphNode& scriptGraphNode)
		: CGraphViewNode(grid, pos)
		, m_scriptFile(scriptFile)
		, m_scriptGraph(scriptGraph)
		, m_scriptGraphNode(scriptGraphNode)
	{}

	const char* CDocGraphViewNode::GetName() const
	{
		return m_scriptGraphNode.GetName();
	}

	const char* CDocGraphViewNode::GetContents() const
	{
		return m_scriptGraphNode.GetComment();
	}

	Gdiplus::Color CDocGraphViewNode::GetHeaderColor() const
	{
		// Workaround for old nodes where color derived from type.
		if(m_scriptGraphNode.GetColor() == EScriptGraphColor::NotSet)
		{
			return DocGraphViewUtils::GetHeaderColor(m_scriptGraphNode.GetType());
		}
		return DocGraphViewUtils::GetColorValue(m_scriptGraphNode.GetColor());
	}

	size_t CDocGraphViewNode::GetInputCount() const
	{
		return m_scriptGraphNode.GetInputCount();
	}

	size_t CDocGraphViewNode::FindInput(const char* szName) const
	{
		return DocUtils::FindGraphNodeInput(m_scriptGraphNode, szName);
	}

	const char* CDocGraphViewNode::GetInputName(size_t inputIdx) const
	{
		return m_scriptGraphNode.GetInputName(inputIdx);
	}

	EScriptGraphPortFlags CDocGraphViewNode::GetInputFlags(size_t inputIdx) const
	{
		return m_scriptGraphNode.GetInputFlags(inputIdx);
	}

	Gdiplus::Color CDocGraphViewNode::GetInputColor(size_t inputIdx) const
	{
		return DocGraphViewUtils::GetPortColor(GetInputTypeId(inputIdx).AsEnvTypeId());
	}

	size_t CDocGraphViewNode::GetOutputCount() const
	{
		return m_scriptGraphNode.GetOutputCount();
	}

	size_t CDocGraphViewNode::FindOutput(const char* szName) const
	{
		return DocUtils::FindGraphNodeOutput(m_scriptGraphNode, szName);
	}

	const char* CDocGraphViewNode::GetOutputName(size_t outputIdx) const
	{
		return m_scriptGraphNode.GetOutputName(outputIdx);
	}

	EScriptGraphPortFlags CDocGraphViewNode::GetOutputFlags(size_t outputIdx) const
	{
		return m_scriptGraphNode.GetOutputFlags(outputIdx);
	}

	Gdiplus::Color CDocGraphViewNode::GetOutputColor(size_t outputIdx) const
	{
		return DocGraphViewUtils::GetPortColor(GetOutputTypeId(outputIdx).AsEnvTypeId());
	}
	
	IScriptGraphNode& CDocGraphViewNode::GetScriptGraphNode()
	{
		return m_scriptGraphNode;
	}

	EScriptGraphNodeType CDocGraphViewNode::GetType() const
	{
		return m_scriptGraphNode.GetType();
	}

	SGUID CDocGraphViewNode::GetGUID() const
	{
		return m_scriptGraphNode.GetGUID();
	}

	SGUID CDocGraphViewNode::GetRefGUID() const
	{
		return m_scriptGraphNode.GetRefGUID();
	}

	SGUID CDocGraphViewNode::GetGraphGUID() const
	{
		return m_scriptGraph.GetType() == EScriptGraphType::Transition ? m_scriptGraph.GetElement_New().GetScopeGUID() : m_scriptGraph.GetElement_New().GetGUID();
	}

	CAggregateTypeId CDocGraphViewNode::GetInputTypeId(size_t inputIdx) const
	{
		return m_scriptGraphNode.GetInputTypeId(inputIdx);
	}

	CAggregateTypeId CDocGraphViewNode::GetOutputTypeId(size_t outputIdx) const
	{
		return m_scriptGraphNode.GetOutputTypeId(outputIdx);
	}

	void CDocGraphViewNode::Serialize(Serialization::IArchive& archive)
	{
		m_scriptGraphNode.Serialize(archive);
	}

	void CDocGraphViewNode::OnMove(Gdiplus::RectF paintRect)
	{
		m_scriptGraphNode.SetPos(Vec2(paintRect.X, paintRect.Y));
	}

	SDocGraphViewSelection::SDocGraphViewSelection(const CDocGraphViewNodePtr& _pNode, SGraphViewLink* _pLink)
		: pNode(_pNode)
		, pLink(_pLink)
	{}

	BEGIN_MESSAGE_MAP(CDocGraphView, CGraphView)
	END_MESSAGE_MAP()

	CDocGraphView::CDocGraphView()
		: CGraphView(SGraphViewGrid(Gdiplus::PointF(10.0f, 10.0f), Gdiplus::RectF(-5000.0f, -5000.0f, 10000.0f, 10000.0f)))
		, m_pScriptFile(nullptr)
		, m_pScriptGraph(nullptr)
		, m_bRemovingLinks(false)
	{
		CGraphView::Enable(false);
	}

	void CDocGraphView::Refresh()
	{
		if(m_pScriptGraph)
		{
			if(m_pScriptGraph->GetType() == EScriptGraphType::Property)
			{
				static_cast<IScriptGraphExtension*>(m_pScriptGraph)->Refresh_New(SScriptRefreshParams(EScriptRefreshReason::EditorSelect)); // #SchematycTODO : Once all graphs are implemented as script extensions we can remove this cast!
			}
			else
			{
				m_pScriptGraph->GetElement_New().Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorSelect));
			}
		}
		CGraphView::Refresh();
	}

	void CDocGraphView::Load(TScriptFile* pScriptFile, IScriptGraph* pScriptGraph)
	{
		if(m_pScriptGraph)
		{
			m_connectionScope.Release();
			CGraphView::Clear();
		}
		m_pScriptFile  = pScriptFile;
		m_pScriptGraph = pScriptGraph;
		if(m_pScriptFile && m_pScriptGraph)
		{
			CGraphView::Enable(true);
			const Vec2	pos = m_pScriptGraph->GetPos();
			CGraphView::SetScrollOffset(Gdiplus::PointF(pos.x, pos.y));
			if(m_pScriptGraph->GetType() == EScriptGraphType::Property)
			{
				static_cast<IScriptGraphExtension*>(m_pScriptGraph)->Refresh_New(SScriptRefreshParams(EScriptRefreshReason::EditorSelect)); // #SchematycTODO : Once all graphs are implemented as script extensions we can remove this cast!
			}
			else
			{
				m_pScriptGraph->GetElement_New().Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorSelect));
			}
			m_pScriptGraph->VisitNodes(ScriptGraphNodeVisitor::FromMemberFunction<CDocGraphView, &CDocGraphView::VisitGraphNode>(*this));
			for(size_t iLink = 0, linkCount = m_pScriptGraph->GetLinkCount(); iLink < linkCount; )
			{
				if(AddLink(*m_pScriptGraph->GetLink(iLink)))
				{
					++ iLink;
				}
				else
				{
					// #SchematycTODO : May make more sense to strip out broken links when loading the script file.
					SCHEMATYC2_SYSTEM_WARNING("Removing broken link from graph : %s::%s", pScriptFile->GetFileName(), pScriptGraph->GetElement_New().GetName());
					m_bRemovingLinks = true;
					m_pScriptGraph->RemoveLink(iLink);
					m_bRemovingLinks = false;
					-- linkCount;
				}
			}
			m_pScriptGraph->Signals().linkRemoved.Connect(ScriptGraphLinkRemovedSignal::Delegate::FromMemberFunction<CDocGraphView, &CDocGraphView::OnDocLinkRemoved>(*this), m_connectionScope);
		}
		else
		{
			CGraphView::Enable(false);
		}
		UnrollSequence();
		__super::Invalidate(TRUE);
	}

	TScriptFile* CDocGraphView::GetScriptFile() const
	{
		return m_pScriptFile;
	}

	IScriptGraph* CDocGraphView::GetScriptGraph() const
	{
		return m_pScriptGraph;
	}

	void CDocGraphView::SelectAndFocusNode(const SGUID& nodeGuid)
	{
		if(nodeGuid)
		{
			CGraphViewNodePtr pNode = GetNode(nodeGuid);
			if(pNode)
			{
				SelectNode(pNode);
				Gdiplus::RectF  rect = pNode->GetPaintRect();
				Gdiplus::PointF pos(rect.X + (rect.Width / 2), rect.Y + (rect.Height / 2));
				CenterPosition(pos);
			}
		}
	}

	SDocGraphViewSignals& CDocGraphView::Signals()
	{
		return m_signals;
	}

	void CDocGraphView::OnMove(Gdiplus::PointF scrollOffset)
	{
		if(m_pScriptGraph)
		{
			m_pScriptGraph->SetPos(Vec2(scrollOffset.X, scrollOffset.Y));
		}
	}

	void CDocGraphView::OnSelection(const CGraphViewNodePtr& pSelectedNode, SGraphViewLink* pSelectedLink)
	{
		m_signals.selection.Send(SDocGraphViewSelection(std::static_pointer_cast<CDocGraphViewNode>(pSelectedNode), pSelectedLink));
	}

	void CDocGraphView::OnUpArrowPressed()
	{
		m_signals.gotoSelection.Send();
	}

	void CDocGraphView::OnDownArrowPressed()
	{
		m_signals.goBackFromSelection.Send();
	}

	void CDocGraphView::OnLinkModify(const SGraphViewLink& link) {}

	void CDocGraphView::OnNodeRemoved(const CGraphViewNode& node)
	{
		CRY_ASSERT(m_pScriptFile && m_pScriptGraph);
		if(m_pScriptFile && m_pScriptGraph)
		{
			m_pScriptGraph->RemoveNode(static_cast<const CDocGraphViewNode&>(node).GetGUID());
			InvalidateDoc();
		}
	}

	bool CDocGraphView::CanCreateLink(const CGraphViewNode& srcNode, const char* szSrcOutputName, const CGraphViewNode& dstNode, const char* szDstInputName) const
	{
		CRY_ASSERT(szSrcOutputName && szDstInputName && m_pScriptGraph);
		if(szSrcOutputName && szDstInputName && m_pScriptGraph)
		{
			const CDocGraphViewNode& srcNodeImpl = static_cast<const CDocGraphViewNode&>(srcNode);
			const CDocGraphViewNode& dstNodeImpl = static_cast<const CDocGraphViewNode&>(dstNode);
			return m_pScriptGraph->CanAddLink(srcNodeImpl.GetGUID(), szSrcOutputName, dstNodeImpl.GetGUID(), szDstInputName);
		}
		return false;
	}

	void CDocGraphView::OnLinkCreated(const SGraphViewLink& link)
	{
		CRY_ASSERT(m_pScriptFile && m_pScriptGraph);
		if(m_pScriptFile && m_pScriptGraph)
		{
			CDocGraphViewNodePtr pSrcNode = std::static_pointer_cast<CDocGraphViewNode>(link.pSrcNode.lock());
			CDocGraphViewNodePtr pDstNode = std::static_pointer_cast<CDocGraphViewNode>(link.pDstNode.lock());
			CRY_ASSERT(pSrcNode && pDstNode);
			if(pSrcNode && pDstNode)
			{
				m_pScriptGraph->AddLink(pSrcNode->GetGUID(), link.srcOutputName.c_str(), pDstNode->GetGUID(), link.dstInputName.c_str());
				InvalidateDoc();
			}
		}
	}

	void CDocGraphView::OnLinkRemoved(const SGraphViewLink& link)
	{
		if(!m_bRemovingLinks)
		{
			CRY_ASSERT(m_pScriptFile && m_pScriptGraph);
			if(m_pScriptFile && m_pScriptGraph)
			{
				CDocGraphViewNodePtr pSrcNode = std::static_pointer_cast<CDocGraphViewNode>(link.pSrcNode.lock());
				CDocGraphViewNodePtr pDstNode = std::static_pointer_cast<CDocGraphViewNode>(link.pDstNode.lock());
				CRY_ASSERT(pSrcNode && pDstNode);
				if(pSrcNode && pDstNode)
				{
					const size_t linkIdx = m_pScriptGraph->FindLink(pSrcNode->GetGUID(), link.srcOutputName.c_str(), pDstNode->GetGUID(), link.dstInputName.c_str());
					if(linkIdx != INVALID_INDEX)
					{
						m_bRemovingLinks = true;
						m_pScriptGraph->RemoveLink(linkIdx);
						m_bRemovingLinks = false;
						InvalidateDoc();
					}
				}
			}
		}
	}

	DROPEFFECT CDocGraphView::OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
	{
		if(m_pScriptFile && m_pScriptGraph)
		{
			PluginUtils::SDragAndDropData	dragAndDropData;
			if(PluginUtils::GetDragAndDropData(pDataObject, dragAndDropData))
			{
				switch(dragAndDropData.icon)
				{
				case BrowserIcon::BRANCH:
					{
						if(CanAddNode(EScriptGraphNodeType::Branch))
						{
							return DROPEFFECT_MOVE;
						}
						break;
					}
				case BrowserIcon::FOR_LOOP:
					{
						if(CanAddNode(EScriptGraphNodeType::ForLoop))
						{
							return DROPEFFECT_MOVE;
						}
						break;
					}
				case BrowserIcon::RETURN:
					{
						if(CanAddNode(EScriptGraphNodeType::Return))
						{
							return DROPEFFECT_MOVE;
						}
						break;
					}
				default:
					{
						if(CanAddNode(dragAndDropData.guid))
						{
							return DROPEFFECT_MOVE;
						}
						break;
					}
				}
			}
		}
		return DROPEFFECT_NONE;
	}

	BOOL CDocGraphView::OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
	{
		if(m_pScriptFile && m_pScriptGraph)
		{
			PluginUtils::SDragAndDropData dragAndDropData;
			if(PluginUtils::GetDragAndDropData(pDataObject, dragAndDropData))
			{
				const Gdiplus::PointF graphPos = CGraphView::ClientToGraph(Gdiplus::PointF(static_cast<float>(point.x), static_cast<float>(point.y)));
				switch(dragAndDropData.icon)
				{
				case BrowserIcon::BRANCH:
					{
						AddNode(EScriptGraphNodeType::Branch, SGUID(), SGUID(), Gdiplus::PointF(graphPos.X - DocGraphViewUtils::s_dragAndDropOffset, graphPos.Y - DocGraphViewUtils::s_dragAndDropOffset));
						InvalidateDoc();
						return true;
					}
				case BrowserIcon::FOR_LOOP:
					{
						AddNode(EScriptGraphNodeType::ForLoop, SGUID(), SGUID(), Gdiplus::PointF(graphPos.X - DocGraphViewUtils::s_dragAndDropOffset, graphPos.Y - DocGraphViewUtils::s_dragAndDropOffset));
						InvalidateDoc();
						return true;
					}
				case BrowserIcon::RETURN:
					{
						AddNode(EScriptGraphNodeType::Return, SGUID(), SGUID(), Gdiplus::PointF(graphPos.X - DocGraphViewUtils::s_dragAndDropOffset, graphPos.Y - DocGraphViewUtils::s_dragAndDropOffset));
						InvalidateDoc();
						return true;
					}
				default:
					{
						if(m_pScriptGraph->GetType() == EScriptGraphType::Property)
						{
						}
						else
						{
							TSizeTVector availableNodes;
							m_pScriptGraph->RefreshAvailableNodes(CAggregateTypeId());
							for(size_t iAvalailableNode = 0, availableNodeCount = m_pScriptGraph->GetAvailableNodeCount(); iAvalailableNode < availableNodeCount; ++ iAvalailableNode)
							{
								if(m_pScriptGraph->GetAvailableNodeRefGUID(iAvalailableNode) == dragAndDropData.guid)
								{
									availableNodes.push_back(iAvalailableNode);
								}
							}
							if(!availableNodes.empty())
							{
								int	nodeSelection = availableNodes.front() + 1;
								if(availableNodes.size() > 1)
								{
									CMenu popupMenu;
									popupMenu.CreatePopupMenu();
									for(TSizeTVector::const_iterator iAvailableNode = availableNodes.begin(), iEndAvailableNode = availableNodes.end(); iAvailableNode != iEndAvailableNode; ++ iAvailableNode)
									{
										popupMenu.AppendMenu(MF_STRING, *iAvailableNode + 1, m_pScriptGraph->GetAvailableNodeName(*iAvailableNode));
									}
									CPoint cursorPos;
									GetCursorPos(&cursorPos);
									nodeSelection = popupMenu.TrackPopupMenuEx(TPM_RETURNCMD, cursorPos.x, cursorPos.y, this, NULL);
								}
								if(nodeSelection > 0)
								{
									AddNode(m_pScriptGraph->GetAvailableNodeType(nodeSelection - 1), SGUID(), dragAndDropData.guid, Gdiplus::PointF(graphPos.X - DocGraphViewUtils::s_dragAndDropOffset, graphPos.Y - DocGraphViewUtils::s_dragAndDropOffset));
									InvalidateDoc();
									return true;
								}
							}
						}
						break;
					}
				}
			}
		}
		return false;
	}

	void CDocGraphView::GetPopupMenuItems(CMenu& popupMenu, const CGraphViewNodePtr& pNode, size_t nodeInputIdx, size_t nodeOutputIdx, CPoint point)
	{
		if(pNode)
		{
			CDocGraphViewNodePtr pNodeImpl = std::static_pointer_cast<CDocGraphViewNode>(pNode);
			IScriptGraphNode&    scriptGraphNode = pNodeImpl->GetScriptGraphNode();

			EScriptGraphNodeType nodeType = scriptGraphNode.GetType();
			if (nodeType == EScriptGraphNodeType::State && nodeOutputIdx != INVALID_INDEX)
			{
				stack_string gotoLabel;
				gotoLabel.Format("Goto %s", pNodeImpl->GetOutputName(nodeOutputIdx));
				popupMenu.AppendMenu(MF_STRING, EPopupMenuItemEx::DEFINITION, gotoLabel.c_str());
				popupMenu.AppendMenu(MF_STRING, EPopupMenuItemEx::REFERENCES, "Find references");
				popupMenu.AppendMenu(MF_STRING, EPopupMenuItemEx::REFERENCES_ALL, "Find All references");
			}

			if (Schematyc2::NodeUtils::CanGoToNodeType(nodeType))
			{
				popupMenu.AppendMenu(MF_STRING, EPopupMenuItemEx::REFERENCES, "Find references");
				popupMenu.AppendMenu(MF_STRING, EPopupMenuItemEx::REFERENCES_ALL, "Find All references");
				popupMenu.AppendMenu(MF_MENUBARBREAK);
			}

			popupMenu.AppendMenu(MF_STRING, EPopupMenuItemEx::ADD_OUTPUT, "Add Output");	// #SchematycTODO : Check for custom/optional outputs first?
			if(nodeOutputIdx != INVALID_INDEX)
			{
				if((scriptGraphNode.GetOutputFlags(nodeOutputIdx) & EScriptGraphPortFlags::Removable) != 0)
				{
					popupMenu.AppendMenu(MF_STRING, EPopupMenuItemEx::REMOVE_OUTPUT, "Remove Output");
				}
			}
		}
		CGraphView::GetPopupMenuItems(popupMenu, pNode, nodeInputIdx, nodeOutputIdx, point);
	}

	void CDocGraphView::OnPopupMenuResult(BOOL popupMenuItem, const CGraphViewNodePtr& pNode, size_t nodeInputIdx, size_t nodeOutputIdx, CPoint point)
	{
		if(m_pScriptFile)
		{
			switch(popupMenuItem)
			{
			case EPopupMenuItemEx::ADD_OUTPUT:
				{
					CRY_ASSERT(pNode);
					if(pNode)
					{
						SET_LOCAL_RESOURCE_SCOPE

						IScriptGraphNode& scriptGraphNode = std::static_pointer_cast<CDocGraphViewNode>(pNode)->GetScriptGraphNode();
						if(IAnyConstPtr pCustomOutputDefault = scriptGraphNode.GetCustomOutputDefault())
						{
							DocGraphViewUtils::SAnyWrapper wrapper(pCustomOutputDefault->Clone());
							CPropertyTreeDlg propertyTreeDlg("Add Graph Node Output", Serialization::SStruct(wrapper));
							if(propertyTreeDlg.exec())
							{
								scriptGraphNode.AddCustomOutput(*wrapper.pValue);
							}
						}
						else
						{
							DocGraphViewUtils::AddOptionalNodeOutput(this, point, scriptGraphNode);
						}
						m_signals.modification.Send(*m_pScriptFile);
						__super::Invalidate(TRUE);
					}
					break;
				}
			case EPopupMenuItemEx::REMOVE_OUTPUT:
				{
					CRY_ASSERT(pNode);
					if(pNode)
					{
						CDocGraphViewNodePtr	pNodeImpl = std::static_pointer_cast<CDocGraphViewNode>(pNode);
						string	message = "Remove node output named '";
						message.append(pNodeImpl->GetOutputName(nodeOutputIdx));
						message.append("'?");
						if (::MessageBoxA(CView::GetSafeHwnd(), message.c_str(), "Remove Node Output", 1) == IDOK)
						{
							CGraphView::RemoveLinksConnectedToNodeOutput(pNode, pNode->GetOutputName(nodeOutputIdx));
							pNodeImpl->GetScriptGraphNode().RemoveOutput(nodeOutputIdx);
							m_signals.modification.Send(*m_pScriptFile);
							__super::Invalidate(TRUE);
						}
					}
					break;
				}
			case EPopupMenuItemEx::REFERENCES:
			{
				if (pNode)
				{
					CDocGraphViewNodePtr	pNodeImpl = std::static_pointer_cast<CDocGraphViewNode>(pNode);
					const TScriptFile& file = pNodeImpl->GetScriptFile();

					SGUID refGuid = pNodeImpl->GetRefGUID();
					SGUID refGoBack = pNodeImpl->GetGraphGUID();

					stack_string name = pNodeImpl->GetName();

					if (pNodeImpl->GetType() == EScriptGraphNodeType::State && nodeOutputIdx != INVALID_INDEX)
					{
						refGuid = pNodeImpl->GetOutputTypeId(nodeOutputIdx).AsScriptTypeGUID();
						name = pNodeImpl->GetOutputName(nodeOutputIdx);
					}

					stack_string::size_type pos = name.rfind(' ');
					if (pos != stack_string::npos)
					{
						name = name.substr(pos);
					}
					
					Schematyc2::LibUtils::FindAndLogReferences(refGuid, refGoBack, name.c_str(), &file);
				}
				break;
			}
			case EPopupMenuItemEx::REFERENCES_ALL:
			{
				if (pNode)
				{
					CDocGraphViewNodePtr	pNodeImpl = std::static_pointer_cast<CDocGraphViewNode>(pNode);
					const TScriptFile& file = pNodeImpl->GetScriptFile();

					SGUID refGuid = pNodeImpl->GetRefGUID();
					SGUID refGoBack = pNodeImpl->GetGraphGUID();

					stack_string name = pNodeImpl->GetName();

					if (pNodeImpl->GetType() == EScriptGraphNodeType::State && nodeOutputIdx != INVALID_INDEX)
					{
						refGuid = pNodeImpl->GetOutputTypeId(nodeOutputIdx).AsScriptTypeGUID();
						name = pNodeImpl->GetOutputName(nodeOutputIdx);
					}

					stack_string::size_type pos = name.rfind(' ');
					if (pos != stack_string::npos)
					{
						name = name.substr(pos);
					}

					Schematyc2::LibUtils::FindAndLogReferences(refGuid, refGoBack, name.c_str(), &file, true);
				}
				break;
			}
			case EPopupMenuItemEx::DEFINITION:
			{
				if (pNode)
				{
					CDocGraphViewNodePtr	pNodeImpl = std::static_pointer_cast<CDocGraphViewNode>(pNode);
					if (pNodeImpl->GetType() == EScriptGraphNodeType::State && nodeOutputIdx != INVALID_INDEX)
					{
						SGUID refGuid = pNodeImpl->GetOutputTypeId(nodeOutputIdx).AsScriptTypeGUID();
						SGUID refGoBack = pNodeImpl->GetGraphGUID();
						m_signals.gotoSelectionChanged.Send(SGotoSelection(refGuid, refGoBack));
						m_signals.gotoSelection.Send();
					}
				}
			}
			default:
				{
					CGraphView::OnPopupMenuResult(popupMenuItem, pNode, nodeInputIdx, nodeOutputIdx, point);
					break;
				}
			}
		}
	}

	const IQuickSearchOptions* CDocGraphView::GetQuickSearchOptions(CPoint point, const CGraphViewNodePtr& pNode, size_t nodeOutputIdx)
	{
		if(m_pScriptFile && m_pScriptGraph)
		{
			CDocGraphViewNodePtr pNodeImpl = std::static_pointer_cast<CDocGraphViewNode>(pNode);
			m_quickSearchOptions.Refresh(*m_pScriptGraph, pNode ? pNodeImpl->GetOutputTypeId(nodeOutputIdx) : CAggregateTypeId());
			return &m_quickSearchOptions;
		}
		return NULL;
	}

	void CDocGraphView::OnQuickSearchResult(CPoint point, const CGraphViewNodePtr& pNode, size_t nodeOutputIdx, size_t optionIdx)
	{
		CRY_ASSERT(m_pScriptFile && (optionIdx < m_quickSearchOptions.GetCount()));
		if(m_pScriptFile && m_pScriptGraph && (optionIdx < m_quickSearchOptions.GetCount()))
		{
			CGraphView::ScreenToClient(&point);
			const Gdiplus::PointF nodePos = CGraphView::ClientToGraph(point.x, point.y);
			CDocGraphViewNodePtr  pNewNode;
			if(m_pScriptGraph->GetType() == EScriptGraphType::Property)
			{
				IScriptGraphNodePtr pScriptGraphNode = m_quickSearchOptions.ExecuteCommand(optionIdx, Vec2(nodePos.X, nodePos.Y)); // #SchematycTODO : Once all graphs are implemented as script extensions we can remove this cast!
				if(pScriptGraphNode)
				{
					static_cast<IScriptGraphExtension*>(m_pScriptGraph)->AddNode_New(pScriptGraphNode);
					pScriptGraphNode->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
					pNewNode = AddNode(*pScriptGraphNode);
				}
			}
			else
			{
				pNewNode = AddNode(m_quickSearchOptions.GetNodeType(optionIdx), m_quickSearchOptions.GetNodeContextGUID(optionIdx), m_quickSearchOptions.GetNodeRefGUID(optionIdx), nodePos);
			}
			if(pNewNode)
			{
				if(pNode && (nodeOutputIdx != INVALID_INDEX))
				{
					const EScriptGraphPortFlags outputFlags = pNode->GetOutputFlags(nodeOutputIdx);
					const CAggregateTypeId      outputTypeId = std::static_pointer_cast<CDocGraphViewNode>(pNode)->GetOutputTypeId(nodeOutputIdx);
					for(size_t nodeInputIdx = 0, nodeInputCount = pNewNode->GetInputCount(); nodeInputIdx < nodeInputCount; ++ nodeInputIdx)
					{
						const EScriptGraphPortFlags inputFlags = pNewNode->GetInputFlags(nodeInputIdx);
						const CAggregateTypeId      inputTypeId = pNewNode->GetInputTypeId(nodeInputIdx);
						if((((inputFlags & EScriptGraphPortFlags::Execute) != 0) && ((outputFlags & EScriptGraphPortFlags::Execute) != 0)) || (inputTypeId == outputTypeId))
						{
							CGraphView::CreateLink(pNode, pNode->GetOutputName(nodeOutputIdx), pNewNode, pNewNode->GetInputName(nodeInputIdx));
							break;
						}
					}
				}
				InvalidateDoc();
			}
		}
	}

	const char* CDocGraphView::CQuickSearchOptions::GetToken() const
	{
		return "::";
	}

	size_t CDocGraphView::CQuickSearchOptions::GetCount() const
	{
		return m_options.size();
	}

	const char* CDocGraphView::CQuickSearchOptions::GetLabel(size_t optionIdx) const
	{
		return optionIdx < m_options.size() ? m_options[optionIdx].label.c_str() : "";
	}

	const char* CDocGraphView::CQuickSearchOptions::GetDescription(size_t optionIdx) const
	{
		return optionIdx < m_options.size() ? m_options[optionIdx].description.c_str() : "";
	}

	const char* CDocGraphView::CQuickSearchOptions::GetWikiLink(size_t optionIdx) const
	{
		return optionIdx < m_options.size() ? m_options[optionIdx].wikiLink.c_str() : "";
	}

	bool CDocGraphView::CQuickSearchOptions::AddOption(const char* szLabel, const char* szDescription, const char* szWikiLink, const IScriptGraphNodeCreationMenuCommandPtr& pCommand)
	{
		// #SchematycTODO : Check for duplicate options?
		m_options.push_back(SOption(szLabel, szDescription, szWikiLink, pCommand));
		return true;
	}

	void CDocGraphView::CQuickSearchOptions::Refresh(IScriptGraph& scriptGraph, const CAggregateTypeId& inputTypeId)
	{
		m_options.clear();
		if(scriptGraph.GetType() == EScriptGraphType::Property)
		{
			static_cast<IScriptGraphExtension&>(scriptGraph).PopulateNodeCreationMenu(*this); // #SchematycTODO : Once all graphs are implemented as script extensions we can remove this cast!
		}
		else
		{
			scriptGraph.RefreshAvailableNodes(inputTypeId);
			for(size_t avalailableNodeIdx = 0, availableNodeCount = scriptGraph.GetAvailableNodeCount(); avalailableNodeIdx < availableNodeCount; ++ avalailableNodeIdx)
			{
				m_options.push_back(SOption(scriptGraph.GetAvailableNodeName(avalailableNodeIdx), scriptGraph.GetAvailableNodeType(avalailableNodeIdx), scriptGraph.GetAvailableNodeContextGUID(avalailableNodeIdx), scriptGraph.GetAvailableNodeRefGUID(avalailableNodeIdx)));
			}
		}
	}

	EScriptGraphNodeType CDocGraphView::CQuickSearchOptions::GetNodeType(size_t iOption) const
	{
		return iOption < m_options.size() ? m_options[iOption].nodeType : EScriptGraphNodeType::Unknown;
	}

	SGUID CDocGraphView::CQuickSearchOptions::GetNodeContextGUID(size_t iOption) const
	{
		return iOption < m_options.size() ? m_options[iOption].nodeContextGUID : SGUID();
	}

	SGUID CDocGraphView::CQuickSearchOptions::GetNodeRefGUID(size_t iOption) const
	{
		return iOption < m_options.size() ? m_options[iOption].nodeRefGUID : SGUID();
	}

	IScriptGraphNodePtr CDocGraphView::CQuickSearchOptions::ExecuteCommand(size_t optionIdx, const Vec2& pos)
	{
		if(optionIdx < m_options.size())
		{
			IScriptGraphNodeCreationMenuCommandPtr& pCommand = m_options[optionIdx].pCommand;
			if(pCommand)
			{
				return pCommand->Execute(pos);
			}
		}
		return IScriptGraphNodePtr();
	}

	CDocGraphView::CQuickSearchOptions::SOption::SOption(const char* _szLabel, EScriptGraphNodeType _nodeType, const SGUID& _nodeContextGUID, const SGUID& _nodeRefGUID)
		: label(_szLabel)
		, nodeType(_nodeType)
		, nodeContextGUID(_nodeContextGUID)
		, nodeRefGUID(_nodeRefGUID)
	{}

	CDocGraphView::CQuickSearchOptions::SOption::SOption(const char* _szLabel, const char* _szDescription, const char* _szWikiLink, const IScriptGraphNodeCreationMenuCommandPtr& _pCommand)
		: label(_szLabel)
		, description(_szDescription)
		, wikiLink(_szWikiLink)
		, pCommand(_pCommand)
	{}

	EVisitStatus CDocGraphView::VisitGraphNode(IScriptGraphNode& graphNode)
	{
		AddNode(graphNode);
		return EVisitStatus::Continue;
	}

	bool CDocGraphView::CanAddNode(EScriptGraphNodeType type) const
	{
		CRY_ASSERT(m_pScriptGraph);
		if(m_pScriptGraph)
		{
			m_pScriptGraph->RefreshAvailableNodes(CAggregateTypeId());
			for(size_t avalailableNodeIdx = 0, availableNodeCount = m_pScriptGraph->GetAvailableNodeCount(); avalailableNodeIdx < availableNodeCount; ++ avalailableNodeIdx)
			{
				if(m_pScriptGraph->GetAvailableNodeType(avalailableNodeIdx) == type)
				{
					return true;
				}
			}
		}
		return false;
	}

	bool CDocGraphView::CanAddNode(const SGUID& refGUID) const
	{
		CRY_ASSERT(m_pScriptGraph);
		if(m_pScriptGraph)
		{
			m_pScriptGraph->RefreshAvailableNodes(CAggregateTypeId());
			for(size_t avalailableNodeIdx = 0, availableNodeCount = m_pScriptGraph->GetAvailableNodeCount(); avalailableNodeIdx < availableNodeCount; ++ avalailableNodeIdx)
			{
				if(m_pScriptGraph->GetAvailableNodeRefGUID(avalailableNodeIdx) == refGUID)
				{
					return true;
				}
			}
		}
		return false;
	}

	bool CDocGraphView::CanAddNode(EScriptGraphNodeType type, const SGUID& refGUID) const
	{
		CRY_ASSERT(m_pScriptGraph);
		if(m_pScriptGraph)
		{
			m_pScriptGraph->RefreshAvailableNodes(CAggregateTypeId());
			for(size_t iAvalailableNode = 0, availableNodeCount = m_pScriptGraph->GetAvailableNodeCount(); iAvalailableNode < availableNodeCount; ++ iAvalailableNode)
			{
				if((m_pScriptGraph->GetAvailableNodeType(iAvalailableNode) == type) && (m_pScriptGraph->GetAvailableNodeRefGUID(iAvalailableNode) == refGUID))
				{
					return true;
				}
			}
		}
		return false;
	}

	CDocGraphViewNodePtr CDocGraphView::AddNode(EScriptGraphNodeType type, const SGUID& contextGUID, const SGUID& refGUID, Gdiplus::PointF pos)
	{
		CRY_ASSERT(m_pScriptGraph);
		if(m_pScriptGraph)
		{
			if(IScriptGraphNode* pScriptGraphNode = m_pScriptGraph->AddNode(type, contextGUID, refGUID, Vec2(pos.X, pos.Y)))
			{
				CDocGraphViewNodePtr	pNode(new CDocGraphViewNode(CGraphView::GetGrid(), pos, *m_pScriptFile, *m_pScriptGraph, *pScriptGraphNode));
				CGraphView::AddNode(pNode, true, true);
				pScriptGraphNode->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				return pNode;
			}
		}
		return CDocGraphViewNodePtr();
	}

	CDocGraphViewNodePtr CDocGraphView::AddNode(IScriptGraphNode& scriptGraphNode)
	{
		CRY_ASSERT(m_pScriptGraph);
		if(m_pScriptGraph)
		{
			const Vec2						pos = scriptGraphNode.GetPos();
			CDocGraphViewNodePtr	pNode(new CDocGraphViewNode(CGraphView::GetGrid(), Gdiplus::PointF(pos.x, pos.y), *m_pScriptFile, *m_pScriptGraph, scriptGraphNode));
			CGraphView::AddNode(pNode, false, false);
			return pNode;
		}
		return CDocGraphViewNodePtr();
	}

	CDocGraphViewNodePtr CDocGraphView::GetNode(const SGUID& guid) const
	{
		const TGraphViewNodePtrVector&	nodes = GetNodes();
		for(TGraphViewNodePtrVector::const_iterator iNode = nodes.begin(), iEndNode = nodes.end(); iNode != iEndNode; ++ iNode)
		{
			const CDocGraphViewNodePtr&	pNode = std::static_pointer_cast<CDocGraphViewNode>(*iNode);
			if(pNode->GetGUID() == guid)
			{
				return pNode;
			}
		}
		return CDocGraphViewNodePtr();
	}

	bool CDocGraphView::AddLink(const IScriptGraphLink& scriptGraphLink)
	{
		CDocGraphViewNodePtr pSrcNode = std::static_pointer_cast<CDocGraphViewNode>(GetNode(scriptGraphLink.GetSrcNodeGUID()));
		CDocGraphViewNodePtr pDstNode = std::static_pointer_cast<CDocGraphViewNode>(GetNode(scriptGraphLink.GetDstNodeGUID()));
		if((pSrcNode != NULL) && (pDstNode != NULL))
		{
			CGraphView::AddLink(pSrcNode, scriptGraphLink.GetSrcOutputName(), pDstNode, scriptGraphLink.GetDstInputName());
			return true;
		}
		return false;
	}

	void CDocGraphView::InvalidateDoc()
	{
		if(m_pScriptFile && m_pScriptGraph)
		{
			m_signals.modification.Send(*m_pScriptFile);
			UnrollSequence();
		}
	}

	void CDocGraphView::UnrollSequence()
	{
		m_sequenceNodes.clear();
		if(m_pScriptFile && m_pScriptGraph)
		{
			TScriptGraphNodeConstVector nodes;
			DocUtils::CollectGraphNodes(*m_pScriptGraph, nodes);
			for(const IScriptGraphNode* pNode : nodes)
			{
				for(size_t outputIdx = 0, outputCount = pNode->GetOutputCount(); outputIdx < outputCount; ++ outputIdx)
				{
					if((pNode->GetOutputFlags(outputIdx) & EScriptGraphPortFlags::BeginSequence) != 0)
					{
						DocUtils::UnrollGraphSequenceRecursive_Deprecated(*m_pScriptGraph, pNode->GetGUID(), m_sequenceNodes);
					}
				}
			}
			for(const CGraphViewNodePtr& pNode : CGraphView::GetNodes())
			{
				pNode->Enable(std::find(m_sequenceNodes.begin(), m_sequenceNodes.end(), std::static_pointer_cast<CDocGraphViewNode>(pNode)->GetGUID()) != m_sequenceNodes.end());
			}
		}
	}

	void CDocGraphView::OnDocLinkRemoved(const IScriptGraphLink& link)
	{
		if(!m_bRemovingLinks)
		{
			const size_t linkIdx = CGraphView::FindLink(GetNode(link.GetSrcNodeGUID()), link.GetSrcOutputName(), GetNode(link.GetDstNodeGUID()), link.GetDstInputName());
			if(linkIdx != INVALID_INDEX)
			{
				m_bRemovingLinks = true;
				CGraphView::RemoveLink(linkIdx);
				m_bRemovingLinks = false;
			}
		}
	}

	CGraphSettingsWidget::CGraphSettingsWidget(QWidget* pParent)
		: QWidget(pParent)
		, m_pGraphView(nullptr)
	{
		m_pLayout       = new QBoxLayout(QBoxLayout::TopToBottom);
		m_pPropertyTree = new QPropertyTree(this);
	}

	void CGraphSettingsWidget::SetGraphView(CGraphView* pGraphView)
	{
		m_pGraphView = pGraphView;
	}

	void CGraphSettingsWidget::InitLayout()
	{
		QWidget::setLayout(m_pLayout);
		m_pLayout->setSpacing(1);
		m_pLayout->setMargin(0);
		m_pLayout->addWidget(m_pPropertyTree, 1);

		m_pPropertyTree->setSizeHint(QSize(250, 400));
		m_pPropertyTree->setExpandLevels(5);
		m_pPropertyTree->setSliderUpdateDelay(5);
		m_pPropertyTree->setValueColumnWidth(0.6f);
		m_pPropertyTree->attach(Serialization::SStruct(*this));
	}

	void CGraphSettingsWidget::Serialize(Serialization::IArchive& archive)
	{
		if(m_pGraphView)
		{
			archive(m_pGraphView->GetSettings(), "general", "General");
			archive(m_pGraphView->GetPainter()->GetSettings(), "painter", "Painter");
		}
	}

	CDocGraphWidget::CDocGraphWidget(QWidget* pParent, CWnd* pParentWnd)
		: QWidget(pParent)
	{
		m_pMainLayout             = new QBoxLayout(QBoxLayout::TopToBottom);
		m_pSplitter               = new QSplitter(Qt::Horizontal, this);
		m_pSettings               = new CGraphSettingsWidget(this);
		m_pGraphViewHost          = new QWinHost(this);
		m_pGraphView              = new CDocGraphView();
		m_pControlLayout          = new QBoxLayout(QBoxLayout::LeftToRight);
		m_pShowHideSettingsButton = new QPushButton(">> Show Settings", this);
		m_pShowHideSettingsButton->setFocusPolicy(Qt::FocusPolicy::NoFocus);

		m_pGraphView->Create(WS_CHILD, CRect(0, 0, 0, 0), pParentWnd, 0);
		m_pGraphViewHost->setWindow(m_pGraphView->GetSafeHwnd());

		m_pSettings->SetGraphView(m_pGraphView);
		m_pSettings->setVisible(false);

		connect(m_pShowHideSettingsButton, SIGNAL(clicked()), this, SLOT(OnShowHideSettingsButtonClicked()));
	}

	void CDocGraphWidget::InitLayout()
	{
		QWidget::setLayout(m_pMainLayout);
		m_pMainLayout->setSpacing(1);
		m_pMainLayout->setMargin(0);
		m_pMainLayout->addWidget(m_pSplitter, 1);
		m_pMainLayout->addLayout(m_pControlLayout, 0);

		m_pSplitter->setStretchFactor(0, 1);
		m_pSplitter->setStretchFactor(1, 0);
		m_pSplitter->addWidget(m_pSettings);
		m_pSplitter->addWidget(m_pGraphViewHost);

		QList<int> splitterSizes;
		splitterSizes.push_back(60);
		splitterSizes.push_back(200);
		m_pSplitter->setSizes(splitterSizes);

		m_pSettings->InitLayout();

		m_pControlLayout->setSpacing(2);
		m_pControlLayout->setMargin(4);
		m_pControlLayout->addWidget(m_pShowHideSettingsButton, 1);
	}

	void CDocGraphWidget::LoadSettings(const XmlNodeRef& xml)
	{
		Serialization::LoadXmlNode(*m_pSettings, xml);
	}

	XmlNodeRef CDocGraphWidget::SaveSettings(const char* szName)
	{
		return Serialization::SaveXmlNode(*m_pSettings, szName);
	}

	void CDocGraphWidget::Refresh()
	{
		m_pGraphView->Refresh();
	}

	void CDocGraphWidget::LoadGraph(TScriptFile* pScriptFile, IScriptGraph* pScriptGraph)
	{
		m_pGraphView->Load(pScriptFile, pScriptGraph);
	}

	TScriptFile* CDocGraphWidget::GetScriptFile() const
	{
		return m_pGraphView->GetScriptFile();
	}

	IScriptGraph* CDocGraphWidget::GetScriptGraph() const
	{
		return m_pGraphView->GetScriptGraph();
	}

	void CDocGraphWidget::SelectAndFocusNode(const SGUID& nodeGuid)
	{
		m_pGraphView->SelectAndFocusNode(nodeGuid);
	}

	SDocGraphViewSignals& CDocGraphWidget::Signals()
	{
		return m_pGraphView->Signals();
	}

	void CDocGraphWidget::OnShowHideSettingsButtonClicked()
	{
		const bool bShowSettings = !m_pSettings->isVisible();
		m_pShowHideSettingsButton->setText(bShowSettings ? "<< Hide Settings" : ">> Show Settings");
		m_pSettings->setVisible(bShowSettings);
	}

	BEGIN_MESSAGE_MAP(CDocGraphWnd, CWnd)
		ON_WM_SHOWWINDOW()
		ON_WM_SIZE()
	END_MESSAGE_MAP()

	CDocGraphWnd::CDocGraphWnd()
		: m_pParentWndWidget(nullptr)
		, m_pDocGraphWidget(nullptr)
		, m_pLayout(nullptr)
	{}

	CDocGraphWnd::~CDocGraphWnd()
	{
		SAFE_DELETE(m_pLayout);
		SAFE_DELETE(m_pDocGraphWidget);
		SAFE_DELETE(m_pParentWndWidget);
	}

	void CDocGraphWnd::Init()
	{
		LOADING_TIME_PROFILE_SECTION;
		m_pParentWndWidget = new QParentWndWidget(CWnd::GetSafeHwnd());
		m_pDocGraphWidget  = new CDocGraphWidget(m_pParentWndWidget, this);
		m_pLayout          = new QBoxLayout(QBoxLayout::TopToBottom);
	}

	void CDocGraphWnd::InitLayout()
	{
		LOADING_TIME_PROFILE_SECTION
		m_pLayout->setContentsMargins(0, 0, 0, 0);
		m_pLayout->addWidget(m_pDocGraphWidget, 1);
		m_pParentWndWidget->setLayout(m_pLayout);
		m_pParentWndWidget->show();
		m_pDocGraphWidget->InitLayout();
	}

	void CDocGraphWnd::LoadSettings(const XmlNodeRef& xml)
	{
		m_pDocGraphWidget->LoadSettings(xml);
	}

	XmlNodeRef CDocGraphWnd::SaveSettings(const char* szName)
	{
		return m_pDocGraphWidget->SaveSettings(szName);
	}

	void CDocGraphWnd::Refresh()
	{
		m_pDocGraphWidget->Refresh();
	}

	void CDocGraphWnd::LoadGraph(TScriptFile* pScriptFile, IScriptGraph* pScriptGraph)
	{
		m_pDocGraphWidget->LoadGraph(pScriptFile, pScriptGraph);
	}

	TScriptFile* CDocGraphWnd::GetScriptFile() const
	{
		return m_pDocGraphWidget->GetScriptFile();
	}

	IScriptGraph* CDocGraphWnd::GetScriptGraph() const
	{
		return m_pDocGraphWidget->GetScriptGraph();
	}

	void CDocGraphWnd::SelectAndFocusNode(const SGUID& nodeGuid)
	{
		m_pDocGraphWidget->SelectAndFocusNode(nodeGuid);
	}

	SDocGraphViewSignals& CDocGraphWnd::Signals()
	{
		return m_pDocGraphWidget->Signals();
	}

	void CDocGraphWnd::OnShowWindow(BOOL bShow, UINT nStatus)
	{
		if(m_pDocGraphWidget)
		{
			m_pDocGraphWidget->setVisible(bShow);
		}
	}

	void CDocGraphWnd::OnSize(UINT nType, int cx, int cy)
	{
		if(m_pParentWndWidget)
		{
			CRect rect; 
			CWnd::GetClientRect(&rect);
			m_pParentWndWidget->setGeometry(0, 0, rect.Width(), rect.Height());
		}
	}
}

