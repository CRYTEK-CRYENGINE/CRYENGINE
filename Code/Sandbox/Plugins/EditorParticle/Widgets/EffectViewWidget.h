// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Widgets/ViewWidget.h"

#include <QWidget>

namespace CryParticleEditor {

class CEffectAsset;
class CEffectAssetModel;

class CEffectAssetWidget : public QWidget
{
	Q_OBJECT

public:
	CEffectAssetWidget(std::shared_ptr<CEffectAssetModel>& pEffectAssetModel, QWidget* pParent = nullptr);
	~CEffectAssetWidget();

	const pfx2::IParticleEffectPfx2* GetEffect() const;
	pfx2::IParticleEffectPfx2* GetEffect();
	const char*                GetName() const;

	void                       SetModified();
	void                       OnDeleteSelected();
	void                       CopyComponents();
	void                       OnPasteComponent();
	void                       OnNewComponent();
	void                       OnOptionsChanged();

	bool                       MakeNewComponent(const char* szTemplateName);

protected:
	// QWidget
	virtual void customEvent(QEvent* event) override;
	// ~QWidget

private:
	void OnBeginEffectAssetChange();
	void OnEndEffectAssetChange();

private:
	std::shared_ptr<CEffectAssetModel> m_pEffectAssetModel;
	CEffectAsset* m_pEffectAsset;
	CryParticleEditor::CGraphView*                          m_pGraphView;
};

}

