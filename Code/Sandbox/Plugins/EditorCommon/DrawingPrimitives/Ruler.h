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
	Range         m_visibleRange;
	Range         m_rulerRange;
	const Range*  m_pInnerRange;
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
	int   m_position;
	float m_value;
	bool  m_bTenth;
	bool  m_bIsOuterTick;

	STick()
		: m_position(0)
		, m_value(.0f)
		, m_bTenth(false)
		, m_bIsOuterTick(false)
	{}
};

typedef SRulerOptions      STickOptions;

typedef std::vector<STick> Ticks;

void CalculateTicks(uint size, Range visibleRange, Range rulerRange, int* pRulerPrecision, Range* pScreenRulerRange, Ticks& ticksOut, const Range* innerRange = nullptr);
void DrawTicks(const std::vector<STick>& ticks, QPainter& painter, const QPalette& palette, const STickOptions& options);
void DrawTicks(QPainter& painter, const QPalette& palette, const STickOptions& options);
void DrawRuler(QPainter& painter, const SRulerOptions& options, int* pRulerPrecision);

class CRuler
{
public:
	struct SOptions
	{
		TRange<SAnimTime>       innerRange;
		TRange<SAnimTime>       visibleRange;
		TRange<SAnimTime>       rulerRange;

		QRect                   rect;
		int32                   markHeight;
		int32                   ticksYOffset;
		int32                   ticksPerFrame;
		SAnimTime::EDisplayMode timeUnit;

		SOptions()
			: markHeight(0)
			, ticksYOffset(0)
			, ticksPerFrame(0)
			, timeUnit(SAnimTime::EDisplayMode::Time)
		{}
	};

	struct STick
	{
		SAnimTime value;
		int32     position;
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

