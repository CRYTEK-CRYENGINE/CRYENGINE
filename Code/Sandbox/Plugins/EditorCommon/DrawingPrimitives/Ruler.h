// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Range.h>
#include <CryMovie/AnimTime.h>

#include <functional>
#include <vector>
#include <QRect>

#include <CryMovie/AnimTime.h>

class QPainter;
class QPalette;

namespace DrawingPrimitives
{
struct SRulerOptions;
typedef std::function<void ()> TDrawCallback;

struct SRulerOptions
{
	QRect         m_rect;
	TRange<CTimeValue> m_visibleRange;
	TRange<CTimeValue> m_rulerRange;
	const TRange<CTimeValue>* m_pInnerRange;
	int           m_markHeight;
	int           m_shadowSize;
	int           m_ticksYOffset;

	TDrawCallback m_drawBackgroundCallback;

	SRulerOptions()
		: m_pInnerRange(nullptr)
		, m_markHeight(0)
		, m_shadowSize(0)
		, m_ticksYOffset(0)
	{}
};

struct STick
{
	CTimeValue m_value;
	int   m_position;		// In pixels
	bool  m_bTenth;
	bool  m_bIsOuterTick;

	STick()
		: m_position(0)
		, m_value(0)
		, m_bTenth(false)
		, m_bIsOuterTick(false)
	{}
};

typedef SRulerOptions      STickOptions;

typedef std::vector<STick> Ticks;

void CalculateTicks(uint size, TRange<CTimeValue> visibleRange, TRange<CTimeValue> rulerRange, int* pRulerPrecision, TRange<int32>* pScreenRulerRange, Ticks& ticksOut, const TRange<CTimeValue>* innerRange = nullptr);
void DrawTicks(const std::vector<STick>& ticks, QPainter& painter, const QPalette& palette, const STickOptions& options);
void DrawTicks(QPainter& painter, const QPalette& palette, const STickOptions& options);
void DrawRuler(QPainter& painter, const SRulerOptions& options, int* pRulerPrecision);

class CRuler
{
public:
	struct SOptions
	{
		// NOTE: Range here was in 'Ticks'
		TRange<CTimeValue>      innerRange;
		TRange<CTimeValue>      visibleRange;
		TRange<CTimeValue>      rulerRange;

		QRect                   rect;
		int32                   markHeight;
		int32                   ticksYOffset;
		CTimeValue              secPerFrame; // Seconds/frame
		SAnimData::EDisplayMode timeUnit;

		SOptions()
			: markHeight(0)
			, ticksYOffset(0)
			, secPerFrame(0)
			, timeUnit(SAnimData::EDisplayMode::Time)
		{}
	};

	struct STick
	{
		CTimeValue value;
		int32     position; // In pixels
		bool      bTenth;
		bool      bIsOuterTick;

		STick()
			: position(0)
			, bTenth(false)
			, bIsOuterTick(false)
		{}
	};

	CRuler()
		: m_decimalDigits(0)
	{}

	SOptions&                 GetOptions() { return m_options; }

	void                      CalculateMarkers(TRange<int32>* pScreenRulerRange = nullptr);
	void                      Draw(QPainter& painter, const QPalette& palette);

	const std::vector<STick>& GetTicks() const { return m_ticks; }

protected:
	void     GenerateFormat(char* szFormatOut);
	QString& ToString(const STick& tick, QString& strOut, const char* szFormat);

private:
	SOptions           m_options;
	std::vector<STick> m_ticks;
	int32              m_decimalDigits;
};
}

