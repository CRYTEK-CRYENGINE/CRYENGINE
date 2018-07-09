// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SubstanceOutputNodeBase.h"
#include "OutputEditor/Pins/SubstanceBasePinItem.h"
#include "NodeGraph/NodeWidget.h"
#include "../GraphViewModel.h"

namespace EditorSubstance
{
	namespace OutputEditor
	{

		int gPixmapSize = 100;
		QPixmap* gDefaultPixmap = nullptr;

		class DefaultImage
		{
		public:
			static const QPixmap& Get()
			{
				if (!gDefaultPixmap)
				{
					gDefaultPixmap = new QPixmap(gPixmapSize, gPixmapSize);
					QPainter painter(gDefaultPixmap);

					painter.fillRect(0, 0, gPixmapSize, gPixmapSize, QBrush(QColor(255, 0, 0)));
					painter.setFont(QFont("Verdana", 18));
					painter.setPen(QPen(QColor(255, 255, 255)));
					painter.drawText(QRect(0, 0, gPixmapSize, gPixmapSize), Qt::AlignCenter, "Replace\nMe");
				}
				return *gDefaultPixmap;
			}

		};

		CSubstanceOutputNodeBase::CSubstanceOutputNodeBase(const SSubstanceOutput& output, CryGraphEditor::CNodeGraphViewModel& viewModel)
			: CAbstractNodeItem(viewModel)
			, m_pOutput(output)
			, m_pNodeContentWidget(nullptr)
		{
			m_name = output.name;
		}

		CSubstanceOutputNodeBase::~CSubstanceOutputNodeBase()
		{
			for (CryGraphEditor::CAbstractPinItem* pItem : m_pins)
			{
				CSubstanceBasePinItem* pBasePin = static_cast<CSubstanceBasePinItem*>(pItem);
				delete pBasePin;
			}
		}

		QVariant CSubstanceOutputNodeBase::GetId() const
		{
			return QVariant::fromValue(QString(m_name + GetIdSuffix(GetNodeType()).c_str()));
		}

		bool CSubstanceOutputNodeBase::HasId(QVariant id) const
		{
			QString nodeName = m_name + GetIdSuffix(GetNodeType()).c_str();
			return (nodeName == id.value<QString>());
		}

		QVariant CSubstanceOutputNodeBase::GetTypeId() const
		{
			return QVariant();
		}

		const CryGraphEditor::PinItemArray& CSubstanceOutputNodeBase::GetPinItems() const
		{
			return m_pins;
		}

		string CSubstanceOutputNodeBase::NameAsString() const
		{
			return m_name.toStdString().c_str();
		}

		CryGraphEditor::CNodeWidget* CSubstanceOutputNodeBase::CreateWidget(CryGraphEditor::CNodeGraphView& view)
		{
			CryGraphEditor::CNodeWidget* pNode = new CryGraphEditor::CNodeWidget(*this, view);
			pNode->SetDeactivated(IsDeactivated());
			pNode->SetHeaderNameWidth(120);
			CGraphViewModel* viewModel = static_cast<CGraphViewModel*>(&GetViewModel());
			m_pNodeContentWidget = new CSubstanceNodeContentWidget(*pNode, view, m_outputType, &m_pOutput, viewModel->GetShowPreviews());
			pNode->SetContentWidget(m_pNodeContentWidget);
			ResetPreviewImage();
			return pNode;
		}

		void CSubstanceOutputNodeBase::SetPreviewImage(std::shared_ptr<QImage> image)
		{
			m_originalImage = image;
			QPixmap pixmap = QPixmap::fromImage(image->convertToFormat(QImage::Format_RGBX8888));

			m_pNodeContentWidget->SetPreviewImage(pixmap.scaled(gPixmapSize, gPixmapSize));
		}

		const QImage& CSubstanceOutputNodeBase::GetPreviewImage() const
		{
			return *m_originalImage.get();
		}

		void CSubstanceOutputNodeBase::ResetPreviewImage()
		{
			SetPreviewImage(std::make_shared<QImage>(DefaultImage::Get().toImage()));
		}

		const string CSubstanceOutputNodeBase::GetIdSuffix(const ESubstanceGraphNodeType& nodeType)
		{
			return nodeType == ESubstanceGraphNodeType::eInput ? "_input" : "_output";
		}

		void CSubstanceOutputNodeBase::SetName(const QString& name)
		{

			if (m_name != name)
			{
				// naive but working solution to create unique node names if there is a conflict
				CGraphViewModel& model = static_cast<CGraphViewModel&>(GetViewModel());
				string baseName(name.toStdString().c_str());
				string newName = baseName;
				int startIndex(1);
				while (!model.IsNameUnique(newName, GetNodeType()))
				{
					newName = string().Format("%s_%02d", baseName, startIndex++);

					if (startIndex == 100)
					{
						startIndex = 1;
						baseName += "_";
					}
				}
				
				model.UpdateNodeCrc(m_name.toStdString().c_str(), newName, GetNodeType());
				m_name = newName;
				m_pOutput.name = newName;
				SignalNameChanged();
			}
			
		}
	}
}
