// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <NodeGraph/NodeGraphView.h>
#include <NodeGraph/NodeGraphItemPropertiesWidget.h>

namespace CryGraphEditor
{

class CNodeGraphViewStyle;
class CNodeGraphViewBackground;

}
class QLabel;
namespace EditorSubstance
{
	namespace OutputEditor
	{

		class CSubstanceGraphViewPropertiesWidget : public CryGraphEditor::CNodeGraphItemPropertiesWidget
		{
			Q_OBJECT
		public:
			CSubstanceGraphViewPropertiesWidget(CryGraphEditor::GraphItemSet& items, bool showPreview);
			CSubstanceGraphViewPropertiesWidget(CryGraphEditor::CAbstractNodeGraphViewModelItem& item, bool showPreview);
		protected:
			void LoadPreviewImage(const CryGraphEditor::CAbstractNodeGraphViewModelItem& item);
		private:
			QLabel* m_pictureRGB;
			QLabel* m_pictureAlpha;
		};

		class CGraphView : public CryGraphEditor::CNodeGraphView
		{
		public:
			CGraphView(CryGraphEditor::CNodeGraphViewModel* pViewModel);

			virtual QWidget* CreatePropertiesWidget(CryGraphEditor::GraphItemSet& selectedItems) override;


		private:
		};
	}
}
