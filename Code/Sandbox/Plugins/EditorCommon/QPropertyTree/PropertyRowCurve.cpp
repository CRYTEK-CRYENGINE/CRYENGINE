// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PropertyRowCurve.h"
#include <CrySerialization/ClassFactory.h>
#include <Serialization/PropertyTree/PropertyTreeModel.h>
#include <Serialization/PropertyTree/PropertyRowNumber.h>
#include <Serialization/PropertyTree/IMenu.h>
#include <QMenu>
#include <QFileDialog>
#include <QPainter>
#include <QApplication>
#include <QDesktopWidget>
#include <QBoxLayout>
#include <QToolBar>
#include <QLabel>
#include "Serialization/QPropertyTree/QDrawContext.h"
#include <CurveEditor.h>
#include <CrySerialization/yasli/TextOArchive.h>
#include <CryMath/ISplines.h>
#include "EditorStyleHelper.h"
#include "EditorFramework/Events.h"
#include "EditorFramework/BroadcastManager.h"
#include <Controls/CurveEditorPanel.h>


//////////////////////////////////////////////////////////////////////////

static const ColorB curveColors[3] = { ColorB(255, 0, 0), ColorB(0, 255, 0), ColorB(0, 0, 255) };

void PropertyRowCurve::setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar)
{
	if (!multiCurve_)
	{
		// Single curve
		auto spline = (ISplineEvaluator*)ser.pointer();
		curves().resize(1);
		curves()[0].FromSpline(*spline);
		curves()[0].m_externData = this;
	}
	else
	{
		// Multiple curves
		auto multiSpline = (IMultiSplineEvaluator*)ser.pointer();
		curves().resize(multiSpline->GetNumSplines());
		for (int i = 0; i < curves().size(); ++i)
		{
			curves()[i].FromSpline(*multiSpline->GetSpline(i));
			curves()[i].m_externData = this;
			if (curves().size() > 1 && i <= 3)
				curves()[i].m_color = curveColors[i];
		}
	}

	const char* szCurveDomain = ar.context<const char>();
	if (szCurveDomain == nullptr)
		szCurveDomain = "";

	if (curveDomain_ != szCurveDomain)
		curveDomain_ = szCurveDomain;
}

bool PropertyRowCurve::assignTo(const Serialization::SStruct& ser) const
{
	if (!multiCurve_)
	{
		// Single curve
		if (curves().size() > 0)
		{
			auto spline = (ISplineEvaluator*)ser.pointer();
			spline->FromSpline(curves()[0]);
		}
	}
	else
	{
		// Multiple curves
		auto multiSpline = (IMultiSplineEvaluator*)ser.pointer();
		int count = min<int>(multiSpline->GetNumSplines(), curves().size());
		for (int i = 0; i < count; ++i)
			multiSpline->GetSpline(i)->FromSpline(curves()[i]);
	}
	return true;
}

void PropertyRowCurve::serializeValue(yasli::Archive& ar)
{
	ar(curves(), "Splines", "Splines");
}

yasli::string PropertyRowCurve::valueAsString() const
{
	string text;
	for (const auto& curve : curves())
	{
		if (!text.empty())
			text += "\n";
		text += curve.ToString();
	}
	return text;
}

bool PropertyRowCurve::onContextMenu(IMenu& menu, PropertyTree* tree)
{
	yasli::SharedPtr<PropertyRowCurve> selfPointer(this);
	CurveMenuHandler* handler = new CurveMenuHandler(tree, this);
	menu.addAction("Edit Curve", "", 0, handler, &CurveMenuHandler::onMenuEditCurve);
	tree->addMenuHandler(handler);
	return true;
}

void PropertyRowCurve::redraw(property_tree::IDrawContext& context)
{
	QDrawContext& qdc = static_cast<QDrawContext&>(context);
	QPainter& painter = *qdc.painter_;
	Rect rect = context.widgetRect;

	// Draw background box
	context.drawEntry(rect, "", false, false, 0);

	// Evaluate spline for every x in rect
	rect = context.widgetRect.adjusted(1, 1, -2, -2);
	const float xscale = painter.deviceTransform().m11();
	const size_t npoints = size_t(rect.width() * xscale);

	const float tInc = 1.0f / npoints;
	const float xInc = rect.width() * tInc;

	using TDrawSpline = spline::CSplineKeyInterpolator<spline::SplineKey<float>>;

	auto setPoint = [=](QPointF& point, TDrawSpline& spline, int p)
	{
		const float t = tInc * float(p);
		const float x = rect.left() + xInc * float(p);

		float val;
		spline.interpolate(t, val);

		point.rx() = x;
		point.ry() = rect.bottom() - (val * rect.height());
	};

	painter.save();
	painter.setClipRect(QRect(rect.x, rect.y, rect.w, rect.h));
	painter.setRenderHint(QPainter::Antialiasing, true);

	TDrawSpline splines[3];
	assert(curves().size() <= 3);
	for (int c = 0; c < curves().size(); ++c)
		splines[c].FromSpline(curves()[c]);

	if (curves().size() == 3 && name() == QString("ColorCurve"))
	{
		// Draw color gradient
		for (int p = 0; p <= npoints; p++)
		{
			const float t = tInc * float(p);
			const float x = rect.left() + xInc * float(p);

			ISplineEvaluator::ValueType vals[3];
			for (int i = 0; i < 3; ++i)
				splines[i].Interpolate(t, vals[i]);

			painter.setPen(QColor(vals[0][0] * 255, vals[1][0] * 255, vals[2][0] * 255, 255));
			painter.drawLine(QPointF(x, rect.bottom()), QPointF(x, rect.top()));
		}
	}
	else if (curves().size() == 2)
	{
		// Draw two curves, filled between
		std::vector<QPointF> points(npoints * 2 + 2);

		for (int p = 0; p <= npoints; p++)
		{
			setPoint(points[p], splines[0], p);
		}
		for (int p = npoints; p >= 0; p--)
		{
			setPoint(points[npoints * 2 + 1 - p], splines[1], p);
		}

		painter.setBrush(GetStyleHelper()->propertyRowCurveFill());
		painter.drawPolygon(points.data(), points.size());
	}
	else
	{
		// Draw separate curves
		for (int c = 0; c < curves().size(); ++c)
		{
			// Draw each spline, as a polyline per point
			std::vector<QPointF> points(npoints + 1);

			for (int p = 0; p < points.size(); p++)
			{
				setPoint(points[p], splines[c], p);
			}

			painter.drawPolyline(points.data(), points.size());
		}
	}

	painter.restore();
}

bool PropertyRowCurve::onActivate(const PropertyActivationEvent& e)
{
	if (e.reason == e.REASON_PRESS)
	{
		editCurve(e.tree);
		return true;
	}
	return false;
}

void PropertyRowCurve::editCurve(PropertyTree* tree)
{
	if (!tree_ && tree)
	{
		QObject::connect(&editorContent_, &SCurveEditorContent::SignalAboutToBeChanged, [this](CCurveEditor& editor)
		{
			for (auto& curve : curves())
			{
				if (PropertyRowCurve* pRow = static_cast<PropertyRowCurve*>(curve.m_externData))
				{
					if (auto* pTree = pRow->tree())
					{
						if (editor.IsPriorityCurve(curve))
							pTree->setSelectedRow(pRow);
						else if (pTree->selectedRow() == pRow)
							pTree->setSelectedRow(nullptr);
					}
				}
			}
		});

		QObject::connect(&editorContent_, &SCurveEditorContent::SignalChanged, [this](CCurveEditor& editor)
		{
			int32 rowCurve = 0;
			PropertyRowCurve* pLastRow = nullptr;

			for (auto& curve : curves())
			{
				if (PropertyRowCurve* pRow = static_cast<PropertyRowCurve*>(curve.m_externData))
				{
					if (pRow == pLastRow)
						rowCurve++;
					else
						rowCurve = 0;
					pLastRow = pRow;

					curve.ToSpline(pRow->curves()[rowCurve]);
					if (auto* pTree = pRow->tree())
					{
						pTree->model()->rowAboutToBeChanged(pRow);
						pTree->repaint();
						pTree->apply(true);
					}
				}
			}
		});
	}
	
	tree_ = tree;

	if (QWidget* widget = static_cast<QPropertyTree*>(tree))
	{
		CBroadcastManager* broadcastManager = CBroadcastManager::Get(widget);
		if (broadcastManager)
		{
			auto setupEditor = [this](CCurveEditorPanel& editorPanel)
			{
				CRY_ASSERT(curves().size() > 0);
				editorPanel.SetTitle(curveDomain_.c_str());

				CCurveEditor& editor = editorPanel.GetEditor();
				editor.SetTimeRange(0, 1, CCurveEditor::ELimit::Clamp);
				editor.SetValueRange(0, 1, CCurveEditor::ELimit::Snap);
				editor.ZoomToTimeRange(0, 1);
				editor.ZoomToValueRange(0, 1);

				editorPanel.SetEditorContent(&editorContent_);

				if (curves().size() > 1)
					editor.SetPriorityCurve(curves()[0]);

				editor.update();
			};

			EditCurveEvent editCurveEvent(setupEditor);
			broadcastManager->Broadcast(editCurveEvent);
		}
	}
}

struct PropertyRowCurves : PropertyRowCurve
{
	PropertyRowCurves()
		: PropertyRowCurve(true) {}
};

REGISTER_PROPERTY_ROW(ISplineEvaluator, PropertyRowCurve);

REGISTER_PROPERTY_ROW(IMultiSplineEvaluator, PropertyRowCurves);

