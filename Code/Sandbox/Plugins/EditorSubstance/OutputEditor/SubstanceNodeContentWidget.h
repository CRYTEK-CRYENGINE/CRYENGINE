// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <NodeGraph\AbstractNodeContentWidget.h>
#include <NodeGraph\PinWidget.h>

#include <QGraphicsWidget>
#include <QVector>
#include <QLabel>

class QGraphicsGridLayout;
class QGraphicsWidget;
struct SSubstanceOutput;

namespace CryGraphEditor
{
	class CNodeWidget;
	class CPinWidget;

}
namespace EditorSubstance
{
	namespace OutputEditor
	{
		class OutputPreviewImage;

		class CSubstanceNodeContentWidget : public CryGraphEditor::CAbstractNodeContentWidget
		{
			Q_OBJECT

		public:
			enum EOutputType
			{
				Standard,
				Virtual
			};

			CSubstanceNodeContentWidget(CryGraphEditor::CNodeWidget& node, CryGraphEditor::CNodeGraphView& view, const EOutputType outputType, SSubstanceOutput* output, bool showPreview);

			virtual void OnInputEvent(CryGraphEditor::CNodeWidget* pSender, CryGraphEditor::SMouseInputEventArgs& args) override;
			void Update();
			void SetPreviewImage(const QPixmap& pixmap);
		protected:
			virtual ~CSubstanceNodeContentWidget();

			// CAbstractNodeContentWidget
			virtual void DeleteLater() override;
			virtual void OnItemInvalidated() override;
			virtual void OnLayoutChanged() override;
			// ~CAbstractNodeContentWidget

			void AddPin(CryGraphEditor::CPinWidget& pinWidget);
			void RemovePin(CryGraphEditor::CPinWidget& pinWidget);

			void OnPinAdded(CryGraphEditor::CAbstractPinItem& item);
			void OnPinRemoved(CryGraphEditor::CAbstractPinItem& item);

		private:
			QGraphicsGridLayout* m_pLayout;
			uint8                m_numLeftPins;
			uint8                m_numRightPins;
			QLabel*				 m_pPreset;
			QLabel*				 m_pResolution;
			SSubstanceOutput*	 m_pOutput;
			EOutputType			 m_outputType;
			CryGraphEditor::CPinWidget*          m_pLastEnteredPin;
			OutputPreviewImage*  m_pPreviewImage;
			bool				 m_showPreview;
		};

	}
}
