#pragma once

#include <QWidget>
#include <vector>

// RampControl

namespace Ui {
	class CCRampControl;
}

struct RampControlMeshInfo
{
public:
	RampControlMeshInfo()
	{
		nIndices = 0;
		nVertices = 0;
	}

	RampControlMeshInfo(int indices,int vertices):nIndices(indices),nVertices(vertices)
	{
	}

	int nIndices;
	int nVertices;
};

class CCRampControl : public QWidget
{
	Q_OBJECT

public:
	enum HandleFlags
	{
		hfSelected = 1<<0,
		hfHotitem = 1<<1,
	};

	struct HandleData
	{
		float percentage;
		int flags;
	};
	
public:
    CCRampControl();
    CCRampControl(QWidget* qWidget);
    virtual ~CCRampControl();

signals:
	void OnLodValueChanged();
	void OnLodDeleted();
	void OnLodAdded(float fPercentage);
	void OnLodSelected();
	void OnSelectionCleared();

public slots:
	void ADDChange(bool checked = false);
	void DeleteChange(bool checked = false);
	void SelectChange(bool checked = false);
	void SelectAllChange(bool checked = false);
	void SelectNoneChange(bool checked = false);

public:
    void OnLButtonDblClk(QPoint point);
    void OnLButtonDown(QPoint point);
    void OnLButtonUp(QPoint point);
    void OnMouseMove(QPoint point);
    void OnKeyDown(int nChar, int nRepCnt);
    void OnKeyUp(int nChar, int nRepCnt);
    void OnKillFocus();

    void OnSize(int nType, int cx, int cy);

	virtual float DeterminePercentage(const QPoint& point);
	virtual bool AddHandle(const float percent);
	virtual void SortHandles();
    static bool SortHandle(const CCRampControl::HandleData& dataA, const CCRampControl::HandleData& dataB);
	virtual void ClearSelectedHandles();
	virtual void Reset();
	virtual bool SelectHandle(const float percent);
	virtual bool DeSelectHandle(const float percent);
	virtual bool IsSelectedHandle(const float percent);
	virtual void SelectAllHandles();
	virtual float MoveHandle(HandleData& data, const float percent);
	virtual bool HitAnyHandles(const float percent);
	virtual bool HitTestHandle(const HandleData& data, const float percent);
	static bool CheckSelected(const HandleData& data);
	virtual void RemoveSelectedHandles();
	virtual void SelectNextHandle();
	virtual void SelectPreviousHandle();
	
	virtual const bool GetSelectedData(HandleData & data);
	virtual const int GetHandleCount();
	virtual const HandleData GetHandleData(const int idx);
	virtual const int GetHotHandleIndex();

	virtual void SetHotItem(const float percentage);

	virtual void SetMaxHandles(int max) { m_max = max; }
	virtual const int GetMaxHandles() { return m_max; }
	virtual void SetDrawPercentage(bool draw) { m_drawPercentage = draw; }

	void SetStats(RampControlMeshInfo stats);

private:
	void paintEvent(QPaintEvent *ev);
	void mousePressEvent(QMouseEvent *ev);
	void mouseReleaseEvent(QMouseEvent *ev);
	void mouseMoveEvent(QMouseEvent *ev);
	void mouseDoubleClickEvent(QMouseEvent *ev);
	void keyPressEvent(QKeyEvent *ev);
	void keyReleaseEvent(QKeyEvent *ev);
	void contextMenuEvent(QContextMenuEvent * ev);

	void DrawRamp(QPainter& painter);
	void DrawBackground(QPainter& painter);
	void DrawForeground(QPainter& painter);
	void DrawHandle(QPainter& painter, const HandleData& data);
	void DrawFrame(QPainter& painter);

protected:
	std::vector<HandleData> m_handles;
	QPoint m_ptLeftDown;
	bool m_bLeftDown;
	QPoint m_menuPoint;
    int m_max;
	bool m_movedHandles;
	bool m_canMoveSelected;
	bool m_mouseMoving;
	bool m_drawPercentage;

	float m_totalError;
	float m_selectedError;

	int indexCount;
	int vetexCount;

	RampControlMeshInfo m_currentStats;

};
