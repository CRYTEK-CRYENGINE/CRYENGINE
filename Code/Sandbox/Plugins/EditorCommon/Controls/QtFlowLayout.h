#pragma once

/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QLayout>
#include <QRect>
#include <QStyle>

#include "EditorCommonAPI.h"

class EDITOR_COMMON_API QFlowLayout : public QLayout
{
public:
	explicit QFlowLayout(QWidget* parent, int margin = -1, int hSpacing = -1, int vSpacing = -1);
	explicit QFlowLayout(int margin = -1, int hSpacing = -1, int vSpacing = -1);
	~QFlowLayout();

	void             addItem(QLayoutItem* item) Q_DECL_OVERRIDE;
	int              horizontalSpacing() const;
	int              verticalSpacing() const;
	Qt::Orientations expandingDirections() const Q_DECL_OVERRIDE;
	bool             hasHeightForWidth() const Q_DECL_OVERRIDE;
	int              heightForWidth(int) const Q_DECL_OVERRIDE;
	int              count() const Q_DECL_OVERRIDE;
	QLayoutItem*     itemAt(int index) const Q_DECL_OVERRIDE;
	QSize            minimumSize() const Q_DECL_OVERRIDE;
	void             setGeometry(const QRect& rect) Q_DECL_OVERRIDE;
	QSize            sizeHint() const Q_DECL_OVERRIDE;
	QLayoutItem*     takeAt(int index) Q_DECL_OVERRIDE;

private:
	int doLayout(const QRect& rect, bool testOnly) const;
	int smartSpacing(QStyle::PixelMetric pm) const;

	QList<QLayoutItem*> itemList;
	int                 m_hSpace;
	int                 m_vSpace;
};

