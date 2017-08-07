#pragma once

#include <QWidget>
#include <vector>
#include <QBitmap>


class ITexture;
class CCMeshBakerTextureControl : public QWidget
{
	Q_OBJECT

public:
	CCMeshBakerTextureControl();
	CCMeshBakerTextureControl(QWidget* qWidget);
	virtual ~CCMeshBakerTextureControl();

public:
	void SetTexture(ITexture *pTex, bool bShowAlpha=false);
	ITexture* GetTexture() { return m_pCurrentTex; }

private:
	void UpdateBitmap();
	void enterEvent(QEvent * ev);
	void leaveEvent(QEvent * ev);
	void paintEvent(QPaintEvent *ev);

private:
	_smart_ptr<ITexture> m_pCurrentTex;
	bool m_bShowAlpha;
	bool m_bHovering;
	bool m_bTooltip;
	QImage* m_bitmap;

	CCMeshBakerTextureControl *m_pToolTip;
};
