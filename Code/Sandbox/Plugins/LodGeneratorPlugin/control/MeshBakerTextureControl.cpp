
#include "StdAfx.h"
#include <algorithm>
#include "MeshBakerTextureControl.h"
#include <QPainter>
#include <QMouseEvent>
#include <QAction>

CCMeshBakerTextureControl::CCMeshBakerTextureControl():m_bitmap(NULL),m_pToolTip(NULL),m_bHovering(false),m_bShowAlpha(true)
{

};

CCMeshBakerTextureControl::CCMeshBakerTextureControl(QWidget* qWidget):QWidget(qWidget),m_bitmap(NULL),m_pToolTip(NULL),m_bHovering(false),m_bShowAlpha(true)
{

};

CCMeshBakerTextureControl::~CCMeshBakerTextureControl()
{
	if (m_bitmap)
		delete m_bitmap;
};

void CCMeshBakerTextureControl::paintEvent(QPaintEvent * ev)
{
	QWidget::paintEvent(ev);

	QPainter painter(this);
	QRect rect= this->rect();
	//----------------------------------------------------------------------------

	if(m_bitmap != NULL)
		painter.drawImage(rect,*m_bitmap);

	//----------------------------------------------------------------------------
}

void CCMeshBakerTextureControl::SetTexture(ITexture *pTex, bool bShowAlpha)
{
	m_pCurrentTex=pTex;
	m_bShowAlpha=bShowAlpha;
	UpdateBitmap();
	update();
}

void CCMeshBakerTextureControl::UpdateBitmap()
{
	if (m_pCurrentTex)
	{
		int width=m_pCurrentTex->GetWidth();
		int height=m_pCurrentTex->GetHeight();

		byte* pStorage = new byte[width*height*4];
		bool bSaved=false;
		byte *pData=m_pCurrentTex->GetData32(0,0,pStorage);
		if (pData)
		{
			// Copy to temporary bitmap
			if (m_bShowAlpha)
			{
				for (int idx=0; idx<height*width*4; idx+=4)
				{
					pData[idx+0]=pData[idx+1]=pData[idx+2]=pData[idx+3];
					pData[idx+3]=255;
				}
			}
			else
			{
				for (int idx=0; idx<height*width*4; idx+=4)
				{
					byte red=pData[idx+0];
					pData[idx+0]=pData[idx+2];
					pData[idx+2]=red;
					pData[idx+3]=255;
				}
			}

			// Scale this into a compatible bitmap
			if (m_bitmap)
				delete m_bitmap;
			m_bitmap = new QImage(pStorage,width,height,QImage::Format_RGBA8888);
		}
	}
}


void CCMeshBakerTextureControl::enterEvent(QEvent * ev)
{
	if (!m_bHovering && m_pCurrentTex)
	{
		QRect rc = this->rect();
		QPoint gPos = this->mapToGlobal(QPoint(rc.left(),rc.top()));		
		rc = QRect(gPos.rx(),gPos.ry(),this->width(),this->height());

		int height=m_pCurrentTex->GetHeight()/2;
		int width=m_pCurrentTex->GetWidth()/2;

		POINT pos;
		pos.x = rc.left();
		pos.y = rc.top();
		HMONITOR hMon = MonitorFromPoint(pos, MONITOR_DEFAULTTONULL);
		if(!hMon)
		{
			GetCursorPos(&pos);
			hMon = MonitorFromPoint(pos, MONITOR_DEFAULTTOPRIMARY);
		}
		MONITORINFO mi = {0};
		mi.cbSize = sizeof(MONITORINFO);
		GetMonitorInfo(hMon, &mi);
		if (height>mi.rcMonitor.bottom-mi.rcMonitor.top)
		{
			float scale=(mi.rcMonitor.bottom-mi.rcMonitor.top)/(float)height;
			height=mi.rcMonitor.bottom-mi.rcMonitor.top;
			width=(int)floorf(width*scale+0.5f);
		}

		int gap=10;
		rc.setTop((rc.top()+rc.bottom()-height)/2);
		int leftOverlap=mi.rcMonitor.left-(rc.left()-gap-width);
		int rightOverlap=mi.rcMonitor.right-(rc.right()+gap+width);
		if (leftOverlap<=0 || leftOverlap<rightOverlap)
		{
			rc.setLeft(rc.left()- gap - width);
			rc.setRight(rc.left() + width);
		}
		else
		{
			rc.setLeft(rc.right() + gap);
			rc.setRight(rc.left() + width);
		}
		rc.setBottom(rc.top() + height);

		if (rc.top() < mi.rcMonitor.top)
		{
			rc.setTop(mi.rcMonitor.top);
			rc.setBottom(rc.top()+height);
		}
		else if (rc.bottom() > mi.rcMonitor.bottom)
		{
			rc.setBottom(mi.rcMonitor.bottom);
			rc.setTop(rc.bottom()-height);
		}

		m_bHovering=true;
		m_pToolTip=new CCMeshBakerTextureControl(NULL);
		m_pToolTip->SetTexture(m_pCurrentTex, m_bShowAlpha);
		//m_pToolTip->MoveWindow(rc, TRUE);
		m_pToolTip->setGeometry(rc);
		//m_pToolTip->setGeometry(QRect(0,0,512,512));
		m_pToolTip->hide();	
		m_pToolTip->setWindowFlags(m_pToolTip->windowFlags() | Qt::WindowStaysOnTopHint | Qt::WindowTransparentForInput | Qt::FramelessWindowHint);
		m_pToolTip->show();
	}
}

void CCMeshBakerTextureControl::leaveEvent(QEvent * ev)
{
	if (m_bHovering)
	{
		delete m_pToolTip;
		m_pToolTip=NULL;
		m_bHovering=false;
	}
}