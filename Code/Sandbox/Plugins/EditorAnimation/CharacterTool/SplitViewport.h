// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QWidget>

class QViewport;
class QBoxLayout;
template<class T>
struct QuatT_tpl;
typedef QuatT_tpl<float> QuatT;

#include <CrySerialization/Forward.h>

namespace CharacterTool
{

class QSplitViewport : public QWidget
{
	Q_OBJECT
public:
	QSplitViewport(QWidget* parent);

	QViewport* OriginalViewport()   { return m_originalViewport; }
	QViewport* CompressedViewport() { return m_viewport; }

	bool       IsSplit() const      { return m_isSplit; }
	void       SetSplit(bool setSplit);
	void       Serialize(Serialization::IArchive& ar);
	void       FixLayout();

	// Drag&Drop.
	virtual void dragEnterEvent(QDragEnterEvent* pEvent) override;
	virtual void dropEvent(QDropEvent* pEvent) override;
signals:
	void         dropFile(const QString& filePath);
protected slots:
	void         OnCameraMoved(const QuatT& m);
private:
	QViewport*  m_originalViewport;
	QViewport*  m_viewport;
	QBoxLayout* m_layout;
	bool        m_isSplit;
};

}

