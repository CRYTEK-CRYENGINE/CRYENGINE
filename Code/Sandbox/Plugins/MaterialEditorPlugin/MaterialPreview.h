// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QWidget>

class CMaterialEditor;
class CMaterial;
class QPreviewWidget;

class CMaterialPreviewWidget : public QWidget
{
public:
	CMaterialPreviewWidget(CMaterialEditor* pMatEd);
	~CMaterialPreviewWidget();

private:

	void OnContextMenu();

	void SetPreviewModel(const char* model);
	void OnPickPreviewModel();

	void OnMaterialChanged(CMaterial* pEditorMaterial);

	void dragEnterEvent(QDragEnterEvent* pEvent) override;
	void dropEvent(QDropEvent* pEvent) override;

	CMaterialEditor* m_pMatEd;
	QPreviewWidget* m_pPreviewWidget;
};
