// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LineEditDelegate.h"

#include "AssetModel.h"
#include "QtUtil.h"

#include <QLineEdit>
#include <QKeyEvent>

namespace Private_LineEditDelegate
{

//! Same as QLineEdit, but writes empty string when editing is aborted.
class CLineEdit final : public QLineEdit
{
public:
	CLineEdit(QWidget* pParent = nullptr)
		: QLineEdit(pParent)
		, m_bFinished(false)
	{
	}

	bool IsFinished() const
	{
		return m_bFinished;
	}

	void SetFinished()
	{
		m_bFinished = true;
	}

public:
	CCrySignal<void()> signalEditingAborted;

protected:
	virtual void focusOutEvent(QFocusEvent* pEvent) override
	{
		signalEditingAborted();
	}

	virtual void keyPressEvent(QKeyEvent* pEvent) override
	{
		if (pEvent->key() == Qt::Key_Escape)
		{
			signalEditingAborted();
		}
		else
		{
			return QLineEdit::keyPressEvent(pEvent);
		}
	}

private:
	// A line edit is marked as finished by the first observer that handles either an "abort" or
	// "finished" event.
	bool m_bFinished;
};


template<typename FN>
void FinishEditing(const CLineEditDelegate* pLineEditDelegate, CLineEdit* pEditor, FN&& fn)
{
	// Handling events when editing is already finished might have undesired side-effects, since
	// the model index for this editor might have been removed already.
	if (pEditor->IsFinished())
	{
		return;
	}
	pEditor->SetFinished();

	fn();

	const_cast<CLineEditDelegate*>(pLineEditDelegate)->closeEditor(pEditor);
}

} // namespace Private_LineEditDelegate

CLineEditDelegate::CLineEditDelegate(QAbstractItemView* pParentView)
	: m_pParentView(pParentView)
	, m_editRole(Qt::EditRole)
	, m_pDelegate(pParentView->itemDelegate())
{
	m_validateNameFunc = [](const string& s) { return s; }; // Identity.
}

QWidget* CLineEditDelegate::createEditor(QWidget* pParent, const QStyleOptionViewItem& option, const QModelIndex& modelIndex) const
{
	using namespace Private_LineEditDelegate;

	if (modelIndex.column() != eAssetColumns_Name && modelIndex.column() != eAssetColumns_Thumbnail)
	{
		return m_pDelegate->createEditor(pParent, option, modelIndex);
	}

	CLineEdit* const pEditor = new CLineEdit(pParent);

	pEditor->signalEditingAborted.Connect(std::function<void()>([this, pEditor, modelIndex]()
	{
		FinishEditing(this, pEditor, [this, modelIndex]()
		{
			signalEditingAborted(modelIndex);
		});
	}));

	connect(pEditor, &CLineEdit::editingFinished, [this, pEditor, modelIndex]()
	{
		FinishEditing(this, pEditor, [this, pEditor, modelIndex]()
		{
			const QString text = QtUtil::ToQString(m_validateNameFunc(QtUtil::ToString(pEditor->text())));
			m_pParentView->model()->setData(modelIndex, text, m_editRole);
			signalEditingFinished(modelIndex);
		});
	});

	return pEditor;
}

