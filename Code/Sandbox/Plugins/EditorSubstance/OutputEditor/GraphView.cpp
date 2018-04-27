// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GraphView.h"
#include "NodeGraph/AbstractNodeGraphViewModel.h"
#include "Nodes/SubstanceOutputNodeBase.h"
#include "GraphViewModel.h"
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>

namespace EditorSubstance
{
	namespace OutputEditor
	{


		CGraphView::CGraphView(CryGraphEditor::CNodeGraphViewModel* p)
		{
			SetModel(p);
		}

		QWidget* CGraphView::CreatePropertiesWidget(CryGraphEditor::GraphItemSet& selectedItems)
		{
			CGraphViewModel* model = static_cast<CGraphViewModel*>(GetModel());
			return new CSubstanceGraphViewPropertiesWidget(selectedItems, model->GetShowPreviews());
		}

		CSubstanceGraphViewPropertiesWidget::CSubstanceGraphViewPropertiesWidget(CryGraphEditor::GraphItemSet& items, bool showPreview)
			: CryGraphEditor::CNodeGraphItemPropertiesWidget(items)
		{
			if (showPreview && items.size() == 1)
			{
				LoadPreviewImage(**items.begin());
			}
		}

		CSubstanceGraphViewPropertiesWidget::CSubstanceGraphViewPropertiesWidget(CryGraphEditor::CAbstractNodeGraphViewModelItem& item, bool showPreview)
			: CryGraphEditor::CNodeGraphItemPropertiesWidget(item)
		{
			if (showPreview)
			{
				LoadPreviewImage(item);
			}
		}

		void CSubstanceGraphViewPropertiesWidget::LoadPreviewImage(const CryGraphEditor::CAbstractNodeGraphViewModelItem& item)
		{
			const int32 type = item.GetType();
			if (type == CryGraphEditor::eItemType_Node)
			{
				QGroupBox* rgb = new QGroupBox("RGB");
				rgb->setLayout(new QVBoxLayout);
				QGroupBox* alpha = new QGroupBox("Alpha");
				alpha->setLayout(new QVBoxLayout);

				const CSubstanceOutputNodeBase* node = static_cast<const CSubstanceOutputNodeBase*>(&item);
				m_pictureRGB = new QLabel();
				m_pictureRGB->setPixmap(QPixmap::fromImage(node->GetPreviewImage().convertToFormat(QImage::Format_RGBX8888)));
				m_pictureAlpha = new QLabel();				
				m_pictureAlpha->setPixmap(QPixmap::fromImage(node->GetPreviewImage().alphaChannel()));
				rgb->layout()->addWidget(m_pictureRGB);
				alpha->layout()->addWidget(m_pictureAlpha);
				addWidget(rgb);
				addWidget(alpha);
			}
			
		}

	}
}

