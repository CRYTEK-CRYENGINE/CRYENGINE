// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "Controls/EditorDialog.h"
#include <QStringList>
#include <QEvent>
#include <QResizeEvent>
#include "SubstanceCommon.h"

class CAssetFoldersView;
class CFileNameLineEdit;
class QDialogButtonBox;
class CAsset;

namespace EditorSubstance
{
	class OutputsWidget;
	class CSubstanceOutputEditorDialog;
	
	class CPressetCreator : public CEditorDialog
	{
	public:
		CPressetCreator(CAsset* asset, const string& graphName, std::vector<SSubstanceOutput>& outputs, const Vec2i& resolution, QWidget* parent = nullptr);
		virtual ~CPressetCreator();
		const string& GetTargetFileName() const;
		const string& GetGraphName() const { return m_graphName; };
		const string& GetSubstanceArchive() const { return m_substanceArchive; }
		const Vec2i& GetResolution() const { return m_resolution; }
		const std::vector<SSubstanceOutput>& GetOutputs() const { return m_outputs; };
	protected:
		void OnAccept();
		void OnEditOutputs();

		virtual bool eventFilter(QObject *, QEvent *) override;

		virtual void resizeEvent(QResizeEvent *e) override;

		virtual bool event(QEvent *e) override;
		void OnOutputEditorAccepted();
		void OnOutputEditorRejected();

		virtual void keyPressEvent(QKeyEvent *) override;

	private:
		std::vector<SSubstanceOutput> m_outputs;
		OutputsWidget* m_pOutputsWidget;
		QDialogButtonBox* m_pButtons;
		string m_substanceArchive;
		string m_graphName;
		string m_finalFileName;
		QWidget* m_pModalGuard;
		CAssetFoldersView* m_foldersView;
		CFileNameLineEdit* m_pPathEdit;
		CSubstanceOutputEditorDialog* m_pOutputsGraphEditor;
		Vec2i m_resolution;
	};

}
