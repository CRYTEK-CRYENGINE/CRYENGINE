#include "StdAfx.h"
#include "RampControl.h"
#include <algorithm>
#include <QPainter>
#include <QMouseEvent>
#include <QAction>
#include <QMenu>
#include "../panel/LODInterface.h"

using namespace LODGenerator;

/*****************************************************************************************************/
CCRampControl::CCRampControl()
{
	m_max = 5;
	m_ptLeftDown = QPoint(0,0);
	m_bLeftDown = false;
	m_menuPoint.setX(0);
	m_menuPoint.setY(0);
	m_movedHandles = false;
	m_canMoveSelected = false;
	m_mouseMoving = false;
}

CCRampControl::CCRampControl(QWidget* qWidget):QWidget(qWidget)
{
	m_max = 5;
	m_ptLeftDown = QPoint(0,0);
	m_bLeftDown = false;
	m_menuPoint.setX(0);
	m_menuPoint.setY(0);
	m_movedHandles = false;
	m_canMoveSelected = false;
	m_mouseMoving = false;

	setMouseTracking(true);
}

CCRampControl::~CCRampControl()
{

}

void CCRampControl::SetStats(RampControlMeshInfo stats)
{
	m_currentStats = stats;
}

void CCRampControl::DrawBackground(QPainter& painter)
{
	CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();
	const int nNumMoves = pInstance->GetNumMoves();

	QPen pen;
	pen.setWidth(0.1);
	//pen.setColor(QColor(128,128,128));
	pen.setColor(QColor(0,255,0));
	painter.setPen(pen);
	
	QRect rc= this->rect();
	painter.setBackgroundMode(Qt::OpaqueMode);

	{
		m_selectedError=-1.0f;
		m_totalError = 0.0f;

		for (int i=0; i<nNumMoves; i++)
		{
			m_totalError+=pInstance->GetErrorAtMove(i);
		}

		if (m_totalError>0.0f)
		{
			m_totalError=logf(m_totalError);
		}
		else
		{
			m_totalError=1.0f;
		}

		//pen.setColor(QColor(64,64,64));
		pen.setColor(QColor(255,0,255));
		painter.setPen(pen);

		float bar=floorf(m_totalError);

		/*
		while (bar>0.0f)
		{
			QRect lrc=rc;

			lrc.setBottom(lrc.bottom() - (int)(rc.height()*bar/m_totalError));
			lrc.setTop(lrc.bottom()-1);

			pen.setColor(QColor(255,0,255));
			painter.setPen(pen);
			//pen.setColor(QColor(64,64,64));
			painter.drawRect(lrc);
			bar-=2;
		}
		*/

		pen.setColor(QColor(255,255,255));
		painter.setPen(pen);
		float currentError=0.0f;
		int lastX=-1;

		float yScale=(nNumMoves>1)?(1.0f/(float)(nNumMoves-1)):1.0f;
		for (int i=0; i<nNumMoves; i++)
		{
			QRect lrc;
			lrc.setLeft(lrc.left() + +(int)floorf(rc.width()*(1.0f-i*yScale)));
			if (lrc.left()!=lastX)
			{
				lrc.setRight(lrc.left() + 1);
				lrc.setBottom(rc.bottom() -(int)(rc.height()*((currentError==0.0f)?0.0f:logf(currentError))/m_totalError));
				lrc.setTop(lrc.bottom()-1);

				pen.setColor(QColor(255,255,255));
				painter.setPen(pen);
				painter.drawRect(lrc);
				lastX=lrc.left();
			}

			HandleData handleData;
			if ( GetSelectedData(handleData) )
			{
				float selectedPercentage=handleData.percentage;
				int selected=(int)floorf(nNumMoves*(1.0f-selectedPercentage/100.0f)+0.5f);
				if (i==selected)
				{
					m_selectedError=currentError;
				}
			}

			currentError+=pInstance->GetErrorAtMove(i);
		}
	}
}

void CCRampControl::DrawForeground(QPainter& painter)
{
	QPen pen;
	pen.setWidth(1);
	pen.setColor(QColor(255,255,255));
	painter.setPen(pen);
	QRect rc= this->rect();
	painter.setBackgroundMode(Qt::TransparentMode);

	HandleData handleData;
	if ( GetSelectedData(handleData) )
	{
		IStatObj::SStatistics loadedStats = CLodGeneratorInteractionManager::Instance()->GetLoadedStatistics();
		QString str;
		int tris=m_currentStats.nIndices/3;
		int verts=m_currentStats.nVertices;
		if ( tris != 0 && verts != 0 )
		{
			int tempValue;
			QRect ret;
			tempValue = (int)(((float)tris / (float)(loadedStats.nIndices/3)) * 100.0f);
			str = QString("Tris:%1 ( %2 %)\n").arg(tris).arg(tempValue);
			painter.drawText(rc,Qt::AlignRight | Qt::TextSingleLine,str,&ret);
			//dc.DrawText(str, rc, DT_RIGHT|DT_SINGLELINE|DT_END_ELLIPSIS); 
			rc.setTop(rc.top() + ret.height());
			tempValue = (int)(((float)verts / (float)loadedStats.nVertices) * 100.0f);
			str = QString("Verts:%1 ( %2 %)\n").arg(verts).arg(tempValue);
			painter.drawText(rc,Qt::AlignRight | Qt::TextSingleLine,str,&ret);
			rc.setTop(rc.top() + ret.height());
			str = QString("Error:%1 \n").arg(m_selectedError);//arg(QString::number(m_selectedError, 'g', 1));
			painter.drawText(rc,Qt::AlignRight | Qt::TextSingleLine,str,&ret);
			//str.Format("Error:%.1f\n", m_selectedError);
		}
	}

	painter.setBackgroundMode(Qt::OpaqueMode);
}

void CCRampControl::DrawFrame(QPainter& painter)
{
	QRect rect= this->rect();
	QPen pen;
	pen.setWidth(2);
	pen.setColor(QColor(0,0,0));
	painter.setPen(pen);
	QLine line;
	float x0 = rect.left();
	float x1 = rect.right();
	float y0 = rect.top();
	float y1 = rect.bottom();
	line.setLine(x0,y0,x0,y1);
	painter.drawLine(line);
	line.setLine(x1,y0,x1,y1);
	painter.drawLine(line);
	line.setLine(x0,y0,x1,y0);
	painter.drawLine(line);
	line.setLine(x0,y1,x1,y1);
	painter.drawLine(line);

}

void CCRampControl::DrawRamp(QPainter& painter)
{
	DrawFrame(painter);
	DrawBackground(painter);

	QRect rect= this->rect();
	QPen pen;
	pen.setWidth(1);
	pen.setColor(QColor(0,0,0));
	painter.setPen(pen);

	QLine line;
	float x0,y0,x1,y1;
// 	x0 = rect.left();
// 	x1 = rect.right();
// 	y0 = rect.top() + ((rect.bottom() - rect.top())/2);
// 	y1 = y0;
// 	line.setLine(x0,y0,x1 - x0,y1 - y0);
// 	painter.drawLine(line);

	for (int idx = 0; idx < 100; ++idx)
	{
		x0 = (int)((((float)(rect.right()-rect.left()))/100.0f)*idx);
		y0 = rect.bottom() - 5;//rect.top() + ((rect.bottom() - rect.top())/2) - 5;
		x1 = x0;
		y1 = y0 + 5;

		if (idx % 5 == 0)
			y0 -= 2;

		if (idx % 10 == 0)
			y0 -= 3;

		line.setLine(x0,y0,x1,y1);
		painter.drawLine(line);
	}

	DrawForeground(painter);

// 	if (GetFocus() == this)
// 	{
// 		dc.DrawFocusRect(&rect);
// 	}
}


void CCRampControl::DrawHandle(QPainter& painter, const HandleData& data)
{
	QPen pen;
	pen.setWidth(1);
	pen.setColor(QColor(0,0,0));
	painter.setPen(pen);

	QRect rect= this->rect();

	QLine line;
	float x0,y0,x1,y1;

	const float pixelsPercent = ((float)rect.right() - (float)rect.left()) / 100.0f;

	x0 = (int)(pixelsPercent * data.percentage);
	data.flags & hfSelected && m_drawPercentage ? y0 = rect.top() + 14 : y0 = rect.top();
	x1 = x0;
	y1 = rect.bottom();

	CPen linebrush;
	if ( data.flags & hfHotitem )
	{
		pen.setColor(QColor(255,0,0));
		painter.setPen(pen);
	}

	line.setLine(x0,y0,x1,y1);
	painter.drawLine(line);


	x0 = x0 - 3;
	x1 = x1 + 4;
	y1 = y1;
	y0 = y1 - 7;

	pen.setColor(QColor(255,255,255));
	painter.setPen(pen);

	QBrush qBrush;
	qBrush.setColor(QColor(255,255,255));
	qBrush.setStyle(Qt::SolidPattern);
	painter.setBrush(qBrush);

	QRect qRect(x0,y0,x1 - x0,y1 - y0);
	painter.drawRect(qRect);

	if ( data.flags & hfSelected )
	{
		pen.setColor(QColor(255,0,0));
		painter.setPen(pen);
		painter.drawRect(qRect);

		if ( m_drawPercentage )
		{
			const QString str = QString("%1").arg((int)data.percentage);;

			x0 = (int)(pixelsPercent * data.percentage) - 7;
			x1 = x1 + 5;
			y1 = rect.top();
			y0 = rect.top()+10;

			const QRectF qqRect(x0,y0,x1-x0,y1-y0);
			painter.setBackgroundMode(Qt::TransparentMode);
			painter.drawText(qqRect,Qt::AlignCenter | Qt::TextSingleLine | Qt::TextDontClip,str,NULL);
			painter.setBackgroundMode(Qt::OpaqueMode);
		}
	}
}


void CCRampControl::paintEvent(QPaintEvent * ev)
{
	QWidget::paintEvent(ev);

	QPainter painter(this);
	//----------------------------------------------------------------------------
	
	DrawRamp(painter);
	const int handleCount = m_handles.size();
	for (int idx = 0; idx < handleCount; ++idx)
	{
		DrawHandle(painter, m_handles[idx]);
	}

	//----------------------------------------------------------------------------
}

void CCRampControl::OnLButtonDblClk(QPoint point)
{
	float percent = DeterminePercentage(point);
	if ( !HitAnyHandles(percent) )
	{	
		if ( AddHandle(percent) )
			OnLodAdded((int)percent);
	}
	update();
}

void CCRampControl::mousePressEvent(QMouseEvent *ev)
{
	Qt::MouseButton mouseButton = ev->button();
	QPoint point = ev->pos();
	if (mouseButton == Qt::LeftButton)
	{
		OnLButtonDown(point);
	} 

}

void CCRampControl::mouseReleaseEvent(QMouseEvent *ev)
{
	Qt::MouseButton mouseButton = ev->button();
	QPoint point = ev->pos();
	if (mouseButton == Qt::LeftButton)
	{
		OnLButtonUp(point);
	} 

}

void CCRampControl::mouseMoveEvent(QMouseEvent *ev)
{
	QPoint point = ev->pos();
	OnMouseMove(point);
}

void CCRampControl::mouseDoubleClickEvent(QMouseEvent *ev)
{
	Qt::MouseButton mouseButton = ev->button();
	QPoint point = ev->pos();
	if (mouseButton == Qt::LeftButton)
	{
		OnLButtonDblClk(point);
	} 
}

void CCRampControl::keyPressEvent(QKeyEvent *ev)
{
	OnKeyDown(ev->nativeScanCode(),ev->count());
}

void CCRampControl::keyReleaseEvent(QKeyEvent *ev)
{
	OnKeyUp(ev->nativeScanCode(),ev->count());
}

void CCRampControl::OnLButtonDown(QPoint point)
{
	float percent = DeterminePercentage(point);
	if ( HitAnyHandles(percent) && !(GetKeyState(VK_CONTROL) & 0x8000) )
	{
		ClearSelectedHandles();
		OnSelectionCleared();
	}

	m_ptLeftDown = point;
	m_bLeftDown = true;
	m_canMoveSelected = IsSelectedHandle(percent);
	update();
}

void CCRampControl::OnLButtonUp(QPoint point)
{
	float percent = DeterminePercentage(point);
	if ( m_movedHandles == false )
	{
		if ( !HitAnyHandles(percent) && !(GetKeyState(VK_CONTROL) & 0x8000) )
		{
			ClearSelectedHandles();
			OnSelectionCleared();
		}
		else if ( HitAnyHandles(percent) )
		{
			if ( IsSelectedHandle(percent) )
			{
				DeSelectHandle(percent);
			}
			else
			{
				if ( SelectHandle(percent) )
					OnLodSelected();
			}
		}
	}

	m_ptLeftDown = QPoint(0,0);
	m_bLeftDown = false;

	m_mouseMoving = false;
	m_canMoveSelected = false;
	m_movedHandles = false;
	//SetFocus();
	ReleaseCapture();
	update();
}

void CCRampControl::OnMouseMove(QPoint point)
{
	float abspercent = DeterminePercentage(point);
	SetHotItem(abspercent);
	update();

	const int handleCount = m_handles.size();
	if ( !m_bLeftDown || handleCount == 0 )
		return;

	if (m_movedHandles == false && !m_mouseMoving && !m_canMoveSelected)
		m_canMoveSelected = SelectHandle(abspercent);
	
	m_mouseMoving = true;

	if ( !m_canMoveSelected )
		return;
		
	//SetCapture();

	bool modified = false;
	
	QPoint movement = point - m_ptLeftDown;
	float percent = DeterminePercentage(movement);

	//early out if we hit the edge limits
	for (int idx = 0; idx < handleCount; ++idx)
	{
		if ( m_handles[idx].flags & hfSelected && ((movement.x() < 0 && m_handles[idx].percentage <= 1) || (movement.x() > 0 && m_handles[idx].percentage >= 99)))
		{
			return;
		}
	}

	if ( movement.x() > 0)
	{
		// move the hanldes clamping in direction where required
		for (int idx = handleCount-1; idx >= 0; --idx)
		{
			if ( m_handles[idx].flags & hfSelected )
			{
				float amountClamped = MoveHandle(m_handles[idx], percent);
				if ( amountClamped != 0.0f)
					percent += amountClamped;
				modified = true;
			}
		}
	}
	else if (movement.x() < 0)
	{
		// move the hanldes clamping in direction where required
		for (int idx = 0; idx < handleCount; ++idx)
		{
			if ( m_handles[idx].flags & hfSelected )
			{
				float amountClamped = MoveHandle(m_handles[idx], percent);
				if ( amountClamped != 0.0f)
					percent += amountClamped;
				modified = true;
			}
		}
	}
	m_ptLeftDown = point;
	
	if ( modified )
	{
		m_movedHandles = true;
		OnLodValueChanged();
		update();
		SortHandles();
	}
}

void CCRampControl::OnKeyUp(int nChar, int nRepCnt)
{

}

void CCRampControl::OnKeyDown(int nChar, int nRepCnt)
{
	if ( nChar == VK_DELETE )
	{
		RemoveSelectedHandles();
		OnLodDeleted();
	}

	if ( nChar == VK_LEFT )
	{
		SelectPreviousHandle();
		OnLodSelected();
	}

	if ( nChar == VK_RIGHT )
	{
		SelectNextHandle();
		OnLodSelected();
	}

	update();
}

void CCRampControl::OnKillFocus()
{
	SetHotItem(-1.0f);
	update();
}

void CCRampControl::contextMenuEvent(QContextMenuEvent * ev)
{
	m_menuPoint = ev->pos();;
	float percent = DeterminePercentage(m_menuPoint);
	bool hit = HitAnyHandles(percent);
	bool maxed = GetHandleCount() == GetMaxHandles();

	QAction*    Act_Add;
	QAction*    Act_Delete;
	QAction*    Act_Select;
	QAction*    Act_SelectAll;
	QAction*    Act_SelectNone;

	Act_Add     = new QAction(tr("Add"), this);
	Act_Add->setEnabled(!maxed);
	//Act_Add->setIcon(QIcon("Resources/logo.png"));
	Act_Delete     = new QAction(tr("Delete"), this);
	Act_Select     = new QAction(tr("Select"), this);
	Act_SelectAll     = new QAction(tr("Select All"), this);
	Act_SelectNone     = new QAction(tr("Select None"), this);

	connect(Act_Add, SIGNAL(triggered()), this, SLOT(ADDChange()));
	connect(Act_Delete, SIGNAL(triggered()), this, SLOT(DeleteChange()));
	connect(Act_Select, SIGNAL(triggered()), this, SLOT(SelectChange()));
	connect(Act_SelectAll, SIGNAL(triggered()), this, SLOT(SelectAllChange()));
	connect(Act_SelectNone, SIGNAL(triggered()), this, SLOT(SelectNoneChange()));

	QMenu* menu = new QMenu();
	menu->addAction(Act_Add);
	menu->addSeparator();
	menu->addAction(Act_Delete);
	menu->addAction(Act_Select);
	menu->addSeparator();
	menu->addAction(Act_SelectAll);
	menu->addAction(Act_SelectNone);
	menu->exec(ev->globalPos());
	delete menu;
	delete Act_Add;
	delete Act_Delete;
	delete Act_Select;
	delete Act_SelectAll;
	delete Act_SelectNone;
}

// void CCRampControl::OnContextMenu(QPoint point)
// {
// 	m_menuPoint = point;
// 	ScreenToClient(&m_menuPoint);
// 	float percent = DeterminePercentage(m_menuPoint);
// 	bool hit = HitAnyHandles(percent);
// 	bool maxed = GetHandleCount() == GetMaxHandles();
// 
// 	
// 	CMenu *menu = new CMenu;
// 	menu->CreatePopupMenu();
// 	menu->AppendMenu(!maxed ? MF_STRING : MF_STRING|MF_GRAYED, ID_MENU_ADD, "&Add");
// 	menu->AppendMenu(MF_SEPARATOR,0,"");
// 	menu->AppendMenu(hit ? MF_STRING : MF_STRING|MF_GRAYED, ID_MENU_DELETE, "&Delete");
// 	menu->AppendMenu(hit ? MF_STRING : MF_STRING|MF_GRAYED, ID_MENU_SELECT, "&Select");
// 	menu->AppendMenu(MF_SEPARATOR,0,"");
// 	menu->AppendMenu(MF_STRING, ID_MENU_SELECTALL, "Select A&ll");
// 	menu->AppendMenu(MF_STRING, ID_MENU_SELECTNONE, "Select &None");
// 	AddCustomMenuOptions(menu);
// 	menu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON,point.x,point.y,pWnd);
// 	delete menu;
// 
// }

void CCRampControl::ADDChange(bool checked)
{
	float percent = DeterminePercentage(m_menuPoint);
	if ( AddHandle(percent) )
		OnLodAdded((int)percent);
	update();
}

void CCRampControl::DeleteChange(bool checked)
{
	ClearSelectedHandles();
	float percent = DeterminePercentage(m_menuPoint);
	SelectHandle(percent);
	RemoveSelectedHandles();
	OnLodDeleted();
	update();
}

void CCRampControl::SelectChange(bool checked)
{
	ClearSelectedHandles();
	float percent = DeterminePercentage(m_menuPoint);
	SelectHandle(percent);
	OnLodSelected();
	update();
}

void CCRampControl::SelectAllChange(bool checked)
{
	SelectAllHandles();
	OnLodSelected();
	update();
}

void CCRampControl::SelectNoneChange(bool checked)
{
	ClearSelectedHandles();
	OnSelectionCleared();
	update();
}

void CCRampControl::OnSize(int nType, int cx, int cy)
{
	update();
}

float CCRampControl::DeterminePercentage(const QPoint& point)
{
	QRect rect= this->rect();
	float percent = ((float)(point.x() - rect.left()) / (float)(rect.right()-rect.left())) * 100.0f;
	return percent;
}

bool CCRampControl::AddHandle(const float percent)
{
	if (m_handles.size() >= m_max)
		return false;

	HandleData data;
	data.percentage = percent;
	data.flags = 0;
	m_handles.push_back(data);

	SortHandles();
	return true;
}

void CCRampControl::SortHandles()
{
	std::sort(m_handles.begin(),m_handles.end(),SortHandle);
}

bool CCRampControl::SortHandle(const CCRampControl::HandleData& dataA, const CCRampControl::HandleData& dataB)
{
	return dataA.percentage < dataB.percentage;
}

void CCRampControl::ClearSelectedHandles()
{
	const int handleCount = m_handles.size();
	for (int idx = 0; idx < handleCount; ++idx)
	{
		m_handles[idx].flags &= ~hfSelected;
	}
}

void CCRampControl::Reset()
{
	m_handles.clear();
}

bool CCRampControl::SelectHandle(const float percent)
{
	const int handleCount = m_handles.size();
	for (int idx = 0; idx < handleCount; ++idx)
	{
		if ( HitTestHandle(m_handles[idx],percent) && !(m_handles[idx].flags & hfSelected) )
		{
			m_handles[idx].flags |= hfSelected;
			return true;
		}
	}
	return false;
}

bool CCRampControl::DeSelectHandle(const float percent)
{
	const int handleCount = m_handles.size();
	for (int idx = 0; idx < handleCount; ++idx)
	{
		if ( HitTestHandle(m_handles[idx],percent) && (m_handles[idx].flags & hfSelected))
		{
			m_handles[idx].flags &= ~hfSelected;
			return true;
		}
	}
	return false;
}

bool CCRampControl::IsSelectedHandle(const float percent)
{
	const int handleCount = m_handles.size();
	for (int idx = 0; idx < handleCount; ++idx)
	{
		if ( HitTestHandle(m_handles[idx],percent) )
			if ( m_handles[idx].flags & hfSelected )
				return true;
	}
	return false;
}

void CCRampControl::SelectAllHandles()
{
	const int handleCount = m_handles.size();
	for (int idx = 0; idx < handleCount; ++idx)
	{
		m_handles[idx].flags |= hfSelected;
	}
}

float CCRampControl::MoveHandle(HandleData& data, const float percent)
{
	float clampedby = 0.0f;
	data.percentage += percent;
	if ( data.percentage < 0.0f )
	{
		clampedby = -data.percentage;
		data.percentage = 0.0f;
	}

	if ( data.percentage > 99.99f )
	{
		clampedby = -(data.percentage - 99.0f);
		data.percentage = 99.0f;
	}
	return clampedby;
}

bool CCRampControl::HitAnyHandles(const float percent)
{
	const int handleCount = m_handles.size();
	for (int idx = 0; idx < handleCount; ++idx)
	{
		if ( HitTestHandle(m_handles[idx],percent) )
			return true;
	}
	return false;
}

bool CCRampControl::HitTestHandle(const HandleData& data, const float percent)
{
	if ( percent > data.percentage-1 && percent < data.percentage+1 )
		return true;
	return false;
}

bool CCRampControl::CheckSelected(const CCRampControl::HandleData& data)
{
    return data.flags & CCRampControl::hfSelected;
}

void CCRampControl::RemoveSelectedHandles()
{
    m_handles.erase(remove_if(m_handles.begin(),m_handles.end(),CCRampControl::CheckSelected),m_handles.end());
	SortHandles();
}

void CCRampControl::SelectNextHandle()
{
	bool selected = false;
	const int count = m_handles.size();
	for ( int idx = count-1; idx >= 0; --idx )
	{
		if ( (m_handles[idx].flags & hfSelected) && ( idx+1 < count ) )
		{
			m_handles[idx+1].flags |= hfSelected;
			selected = true;
		}
		m_handles[idx].flags &= ~hfSelected;
	}

	if ( selected == false && count > 0 )
	{
		m_handles[0].flags |= hfSelected;
	}
}

void CCRampControl::SelectPreviousHandle()
{
	bool selected = false;
	const int count = m_handles.size();
	for ( int idx = 0; idx < count; ++idx )
	{
		if ( (m_handles[idx].flags & hfSelected) && ( idx-1 >= 0) )
		{
			m_handles[idx-1].flags |= hfSelected;
			selected = true;
		}
		m_handles[idx].flags &= ~hfSelected;
	}

	if ( selected == false && count > 0 )
	{
		m_handles[count-1].flags |= hfSelected;

	}
}

/*************************************************************************************/
const bool CCRampControl::GetSelectedData(CCRampControl::HandleData & data)
{
	const int handleCount = m_handles.size();
	for (int idx = 0; idx < handleCount; ++idx)
	{
		if ( m_handles[idx].flags & hfSelected )
		{
			data = m_handles[idx];
			return true;
		}
	}
	return false;
}

const int CCRampControl::GetHandleCount()
{
	return m_handles.size();
}

const CCRampControl::HandleData CCRampControl::GetHandleData(const int idx)
{
	return m_handles[idx];
}

const int CCRampControl::GetHotHandleIndex()
{
	const int count = m_handles.size();
	for ( int idx = 0; idx < count; ++idx )
	{
		if ( m_handles[idx].flags & hfHotitem )
		{
			return idx;
		}
	}
	return -1;
}

void CCRampControl::SetHotItem(const float percentage)
{
	const int count = m_handles.size();
	for ( int idx = 0; idx < count; ++idx )
	{
		if ( HitTestHandle(m_handles[idx],percentage) )
		{
			m_handles[idx].flags |= hfHotitem;
		}
		else
		{
			m_handles[idx].flags &= ~hfHotitem;
		}
	}
}
