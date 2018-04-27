// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AssetSystem/AssetEditor.h>
#include <QWidget>
class QPropertyTree;
class ISubstancePreset;
struct ISubstancePresetSerializer;
class QSplitter;
class QScrollableBox;
class QMenuComboBox;

namespace EditorSubstance
{
	class CSubstanceOutputEditorDialog;
	class OutputsWidget;

	class CSubstancePresetEditor : public CAssetEditor
	{
		Q_OBJECT;
	public:
		CSubstancePresetEditor(QWidget* pParent = nullptr);

		virtual const char* GetEditorName() const override { return "Substance Instance Editor"; };

		virtual bool OnOpenAsset(CAsset* pAsset) override;
		virtual bool OnSaveAsset(CEditableAsset& editAsset) override;
		virtual void OnCloseAsset() override;

	protected:
		void PushPresetToRender();
		void SetPreviewResolution();
		protected slots:
		void OnEditOutputs();
		void OnOutputEditorAccepted();
		void OnOutputEditorRejected();
		void UniformResolutionClicked();
		void ResolutionChanged(QMenuComboBox* sender, int index);
	private:
		//QSplitter* m_pSplitter;
		QWidget* m_pResolutionWidget;
		CAbstractMenu* m_pSubstanceMenu;
		QPropertyTree* m_propertyTree;
		ISubstancePreset* m_pPreset;
		ISubstancePresetSerializer* m_pSerializer;
		QScrollableBox* m_pScrollBox;
		OutputsWidget* m_pOutputsWidget;
		CSubstanceOutputEditorDialog* m_pOutputsGraphEditor;
		QMenuComboBox* m_pComboXRes;
		QMenuComboBox* m_pComboYRes;
		QToolButton* m_pResUnified;
	};

}
