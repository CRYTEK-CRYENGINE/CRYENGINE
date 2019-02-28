// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorWidget.h"
#include "GraphView.h"
#include "GraphViewModel.h"
#include "IEditor.h"

#include <QVboxLayout>
#include <QDialogButtonBox>
#include <AssetSystem/Asset.h>
#include "EditorSubstanceManager.h"
#include "Renderers/Preview.h"
#include "EditorFramework/Inspector.h"
#include "EditorFramework/Events.h"
#include "EditorFramework/BroadcastManager.h"

namespace EditorSubstance
{
	namespace OutputEditor
	{


		CSubstanceOutputEditorWidget::CSubstanceOutputEditorWidget(const std::vector<SSubstanceOutput>& originalOutputs, const std::vector<SSubstanceOutput>& customOutputs, bool showPreviews, QWidget* pParent)
			: QWidget(pParent)
			, m_pGraphViewModel(new CGraphViewModel(originalOutputs, customOutputs, showPreviews))
		{
			QVBoxLayout* pMainLayout = new QVBoxLayout(this);
			pMainLayout->setContentsMargins(0, 0, 0, 0);
			CGraphView* const pGraphView = new CGraphView(m_pGraphViewModel.get());
			pGraphView->setWindowTitle(tr("Graph"));
;
			m_pSplitter = new QSplitter(this);
			m_pInspector = new CInspector(this);
			m_pInspector->setMinimumWidth(400);
			m_pSplitter->addWidget(pGraphView);			
			m_pSplitter->addWidget(m_pInspector);			
			pMainLayout->addWidget(m_pSplitter);
			setLayout(pMainLayout);		
		}

		void CSubstanceOutputEditorWidget::LoadUpdatedOutputSettings(std::vector<SSubstanceOutput>& outputs)
		{
			outputs.clear();
			for (SSubstanceOutput* output : m_pGraphViewModel->GetOutputs())
			{
				outputs.push_back(*output);
			}
		}

		

	}
}
