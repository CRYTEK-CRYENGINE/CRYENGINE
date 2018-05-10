// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Timeline.h"
#include "QtUtil.h"
#include "DrawingPrimitives/TimeSlider.h"
#include "CryIcon.h"
#include "EditorStyleHelper.h"
#include "ICommandManager.h"
#include "IEditor.h"

#include "QSearchBox.h"

#include <EditorFramework/Events.h>

#include <vector>
#include <algorithm>
#include <CryCore/Containers/VectorSet.h>
#include <CryString/StringUtils.h>

#include <QMenu>
#include <QPainter>
#include <QPaintEvent>
#include <QStyleOption>
#include <QLineEdit>
#include <QScrollBar>
#include <QApplication>

#ifdef min
	#undef min
#endif

#ifdef max
	#undef max
#endif

#define ENABLE_PROFILING 0

int STimelineViewState::ScrollOffset(float origin) const
{
	return int((origin / visibleDistance + 0.5f) * widthPixels);
}

int STimelineViewState::TimeToLayout(float time) const
{
	return int((time / visibleDistance) * widthPixels + 0.5f);
}

int STimelineViewState::TimeToLocal(float time) const
{
	return TimeToLayout(time) + treeWidth + scrollPixels.x();
}

float STimelineViewState::LayoutToTime(int x) const
{
	return (float(x) - 0.5f) / widthPixels * visibleDistance;
}

float STimelineViewState::LocalToTime(int x) const
{
	return LayoutToTime(x - treeWidth - scrollPixels.x());
}

QPoint STimelineViewState::LocalToLayout(const QPoint& p) const
{
	return p - scrollPixels - QPoint(treeWidth, 0);
}

QPoint STimelineViewState::LayoutToLocal(const QPoint& p) const
{
	return p + scrollPixels + QPoint(treeWidth, 0);
}

enum
{
	THUMB_WIDTH                = 12,
	THUMB_HEIGHT               = 24,

	RULER_HEIGHT               = 20,
	RULER_MARK_HEIGHT          = 8,

	TRACK_MARK_HEIGHT          = 6,

	DEFAULT_KEY_SIZE           = 16,
	VERTICAL_PADDING           = 6,
	TRACK_DESCRIPTION_INDENT   = 8,

	SELECTION_WIDTH            = 4,

	MAX_PUSH_OUT               = VERTICAL_PADDING * 2,
	PUSH_OUT_DISTANCE          = 3,

	SPLITTER_WIDTH             = 4,

	DEFAULT_TREE_WIDTH         = 300,
	TREE_LEFT_MARGIN           = 6,
	TREE_INDENT_MULTIPLIER     = 8,
	TREE_BRANCH_INDICATOR_SIZE = 8,

	MIN_WIDTH_TREE             = 50,
	MIN_WIDTH_SEQ              = 50
};

struct STimelineContentElementRef
{
	STimelineContentElementRef()
		: pTrack(nullptr)
		, index(0)
	{
	}

	STimelineContentElementRef(STimelineTrack* pTrack, size_t index)
		: pTrack(pTrack)
		, index(index)
	{
	}

	STimelineElement& GetElement() const
	{
		return pTrack->elements[index];
	}

	bool IsValid() const { return pTrack != 0 && index < pTrack->elements.size(); }

	bool operator<(const STimelineContentElementRef& rhs) const
	{
		if (pTrack == rhs.pTrack)
			return index < rhs.index;
		else
		{
			if (!pTrack && rhs.pTrack)
				return true;
			else if (pTrack && !rhs.pTrack)
				return false;
			else if (pTrack && rhs.pTrack)
				return pTrack->name < rhs.pTrack->name;
			return false;
		}
	}

	STimelineTrack* pTrack;
	size_t          index;
};

struct SElementLayout
{
	STimelineElement::EType                 type;
	int                                     caps;
	float                                   pushOutDistance;
	QRect                                   rect;
	ColorB                                  color;
	string                                  description;

	STimelineContentElementRef              elementRef;
	std::vector<STimelineContentElementRef> subElements;

	bool IsHighlighted() const
	{
		if ((elementRef.pTrack->caps & STimelineTrack::CAP_COMPOUND_TRACK) == 0)
		{
			return elementRef.GetElement().highlighted;
		}
		else
		{
			bool bHighlighted = false;
			const size_t numSubElements = subElements.size();

			for (size_t i = 0; i < numSubElements; ++i)
			{
				bHighlighted = bHighlighted || subElements[i].GetElement().highlighted;
			}

			return bHighlighted;
		}
	}

	void SetHighlighted(const bool bHighlighted) const
	{
		if ((elementRef.pTrack->caps & STimelineTrack::CAP_COMPOUND_TRACK) == 0)
		{
			elementRef.GetElement().highlighted = bHighlighted;
		}
		else
		{
			const size_t numSubElements = subElements.size();
			for (size_t i = 0; i < numSubElements; ++i)
			{
				subElements[i].GetElement().highlighted = bHighlighted;
			}
		}
	}
    
	bool IsSelected() const
	{
		if ((elementRef.pTrack->caps & STimelineTrack::CAP_COMPOUND_TRACK) == 0)
		{
			return elementRef.GetElement().selected;
		}
		else
		{
			bool bSelected = false;
			const size_t numSubElements = subElements.size();

			for (size_t i = 0; i < numSubElements; ++i)
			{
				bSelected = bSelected || subElements[i].GetElement().selected;
			}

			return bSelected;
		}
	}

	void SetSelected(const bool bSelected) const
	{
		if ((elementRef.pTrack->caps & STimelineTrack::CAP_COMPOUND_TRACK) == 0)
		{
			elementRef.GetElement().selected = bSelected;
		}
		else
		{
			const size_t numSubElements = subElements.size();
			for (size_t i = 0; i < numSubElements; ++i)
			{
				subElements[i].GetElement().selected = bSelected;
			}
		}
	}

	SElementLayout()
		: pushOutDistance(0.0f)
		, caps(0)
		, type(STimelineElement::KEY)
	{
	}
};

struct STrackLayout;
typedef std::vector<STrackLayout> STrackLayouts;

struct STrackLayout
{
	QRect                       rect;
	int                         indent;

	STimelineTrack*             pTimelineTrack;

	std::vector<SElementLayout> elements;
	STrackLayouts               tracks;

	STrackLayout()
		: indent(0)
		, pTimelineTrack(0)
	{
	}
};

struct STimelineLayout
{
	int           thumbPositionX;
	STrackLayouts tracks;
	SAnimTime     minStartTime;
	SAnimTime     maxEndTime;
	QSize         size;

	STimelineLayout()
		: thumbPositionX(0)
		, minStartTime(0.0f)
		, maxEndTime(1.0f)
		, size(1, 1)
	{
	}
};

namespace
{
SElementLayout& AddElementToTrackLayout(STimelineTrack& track, STrackLayout& trackLayout, const STimelineElement& element,
                                        const STimelineViewState& viewState, uint keySize, int treeWidth, int& currentTop, size_t elementIndex)
{
	trackLayout.elements.push_back(SElementLayout());
	SElementLayout& elementl = trackLayout.elements.back();
	elementl.color = element.color;
	elementl.type = element.type;
	elementl.caps = element.caps;
	elementl.description = element.description;
	elementl.elementRef.pTrack = &track;
	elementl.elementRef.index = elementIndex;

	if (element.type == STimelineElement::KEY)
	{
		const int left = (viewState.TimeToLayout(element.start.ToFloat()) - keySize / 2);
		const int top = currentTop + ((track.height - keySize) / 2);
		elementl.rect = QRect(left, top, keySize, keySize);
	}
	else
	{
		const int left = viewState.TimeToLayout(element.start.ToFloat());
		const int right = viewState.TimeToLayout(element.end.ToFloat());
		elementl.rect = QRect(left, currentTop + VERTICAL_PADDING, right - left, track.height - VERTICAL_PADDING * 2);
	}

	return elementl;
}

void AddCompoundElementsToTrackLayout(STimelineTrack& track, STimelineLayout* layout, const STimelineViewState& viewState, int trackId, uint keySize, int treeWidth, int& currentTop)
{
	const size_t numSubTracks = track.tracks.size();
	SAnimTime currentElementTime = SAnimTime::Min();
	size_t* pCurrentSubTrackIndices = static_cast<size_t*>(alloca(sizeof(size_t) * numSubTracks));
	memset(pCurrentSubTrackIndices, 0, sizeof(size_t) * numSubTracks);

	while (true)
	{
		bool bElementFound = false;
		SAnimTime minElementTime = SAnimTime::Max();

		// First search for minimum element time for current track positions
		for (size_t i = 0; i < numSubTracks; ++i)
		{
			const STimelineTrack& subTrack = *track.tracks[i];
			const STimelineElements& elements = subTrack.elements;
			const size_t numTrackElements = elements.size();
			const size_t index = pCurrentSubTrackIndices[i];

			if (index < numTrackElements)
			{
				const SAnimTime elementTime = elements[index].start;
				minElementTime = min(elementTime, minElementTime);
				bElementFound = true;
			}
		}

		if (!bElementFound)
		{
			break;
		}

		STimelineElement compoundElement;
		compoundElement.start = minElementTime;
		compoundElement.end = minElementTime;

		// If elements were found create a compound element
		STrackLayout& trackLayout = layout->tracks[trackId];
		SElementLayout& compoundElementLayout = AddElementToTrackLayout(track, trackLayout, compoundElement, viewState, keySize, treeWidth, currentTop, 0);
		currentElementTime = minElementTime;

		compoundElementLayout.description = "(";

		// Advance track positions and add elements IDs to compound element if times match
		for (size_t i = 0; i < numSubTracks; ++i)
		{
			STimelineTrack* pSubTrack = track.tracks[i];
			const STimelineElements& elements = pSubTrack->elements;
			const size_t numTrackElements = elements.size();
			size_t& index = pCurrentSubTrackIndices[i];

			if (index < numTrackElements)
			{
				const SAnimTime elementTime = elements[index].start;

				if (elementTime == minElementTime)
				{
					STimelineContentElementRef ref;
					ref.pTrack = pSubTrack;
					ref.index = index;
					compoundElementLayout.subElements.push_back(ref);
					compoundElementLayout.description += elements[index].description;
					++index;
				}
				else
				{
					compoundElementLayout.description += "-";
				}

				if ((i + 1) < numSubTracks)
				{
					compoundElementLayout.description += ", ";
				}
			}
		}

		compoundElementLayout.description += ")";
	}
}

bool FilterTracks(const STimelineTrack& track, std::unordered_set<const STimelineTrack*>& invisibleTracks, const char* filterString)
{
	bool bAnyChildVisible = false;
	const bool bNameMatchesFilter = CryStringUtils::stristr(track.name, filterString) != nullptr;

	if (!bNameMatchesFilter)
	{
		const size_t numChildTracks = track.tracks.size();
		for (size_t i = 0; i < numChildTracks; ++i)
		{
			bAnyChildVisible = FilterTracks(*track.tracks[i], invisibleTracks, filterString) || bAnyChildVisible;
		}
	}

	if (!bNameMatchesFilter && !bAnyChildVisible)
	{
		invisibleTracks.insert(&track);
	}

	return bNameMatchesFilter || bAnyChildVisible;
}

void ClampVisibleDistanceToTotalDuration(STimelineViewState& viewState, const STimelineLayout* const layout, uint timelinePadding)
{
	const float totalDuration = (layout->maxEndTime - layout->minStartTime).ToFloat();
	const float padding = (float(timelinePadding) - 0.5f) / viewState.widthPixels * totalDuration;

	viewState.visibleDistance = clamp_tpl(viewState.visibleDistance, 0.01f, totalDuration + 2.0f * padding);
}

void CalculateMinMaxTime(STimelineLayout* layout, const STimelineTrack& parentTrack)
{
	layout->minStartTime = min(layout->minStartTime, parentTrack.startTime);
	layout->maxEndTime = max(layout->maxEndTime, parentTrack.endTime);

	const size_t numTracks = parentTrack.tracks.size();
	for (size_t i = 0; i < numTracks; ++i)
	{
		STimelineTrack& track = *parentTrack.tracks[i];
		CalculateMinMaxTime(layout, track);
	}
}

void CalculateMinMaxTimeWithDefaults(STimelineLayout* layout, const STimelineTrack& parentTrack)
{
	if (!parentTrack.tracks.empty())
	{
		layout->minStartTime = SAnimTime::Max();
		layout->maxEndTime = SAnimTime::Min();
	}
	else
	{
		layout->minStartTime = SAnimTime(0.0f);
		layout->maxEndTime = SAnimTime(1.0f);
	}

	CalculateMinMaxTime(layout, parentTrack);
}

void CalculateTrackLayout(STimelineLayout* layout, int& currentTop, int currentIndent, STimelineTrack& parentTrack, const STimelineViewState& viewState,
                          float thumbTime, uint keySize, int treeWidth, const std::unordered_set<const STimelineTrack*>& invisibleTracks)
{
	const size_t numTracks = parentTrack.tracks.size();
	for (size_t i = 0; i < numTracks; ++i)
	{
		STimelineTrack& track = *parentTrack.tracks[i];

		if (stl::find(invisibleTracks, &track))
		{
			continue;
		}

		layout->tracks.push_back(STrackLayout());
		STrackLayout& trackLayout = layout->tracks.back();
		trackLayout.elements.reserve(track.elements.size());
		trackLayout.indent = currentIndent;
		trackLayout.pTimelineTrack = &track;

		const bool bIsCompositeTrack = (track.caps & STimelineTrack::CAP_COMPOUND_TRACK) != 0;

		if (bIsCompositeTrack)
		{
			const int trackLayoutId = layout->tracks.size() - 1;
			AddCompoundElementsToTrackLayout(track, layout, viewState, trackLayoutId, keySize, treeWidth, currentTop);
		}
		else
		{
			for (size_t i = 0; i < track.elements.size(); ++i)
			{
				const STimelineElement& element = track.elements[i];
				AddElementToTrackLayout(track, trackLayout, element, viewState, keySize, treeWidth, currentTop, i);
			}
		}

		int left = viewState.TimeToLayout(track.startTime.ToFloat());
		int right = viewState.TimeToLayout(track.endTime.ToFloat());
		trackLayout.rect = QRect(left, currentTop, right - left, track.height);
		currentTop += track.height;

		if (track.expanded)
		{
			CalculateTrackLayout(layout, currentTop, currentIndent + 1, track, viewState, thumbTime, keySize, treeWidth, invisibleTracks);
		}
	}
}

void ApplyPushOut(STimelineLayout* layout, uint keySize)
{
	float maxPushOut = 0.0f;

	const size_t numLayoutTracks = layout->tracks.size();
	for (size_t i = 0; i < numLayoutTracks; ++i)
	{
		STrackLayout& track = layout->tracks[i];

		const size_t numElements = track.elements.size();
		for (size_t i = 0; i < numElements; ++i)
		{
			if (track.elements[i].type != STimelineElement::KEY)
			{
				continue;
			}

			for (size_t j = 0; j < numElements; ++j)
			{
				if ((track.elements[j].type != STimelineElement::KEY) || (j == i))
				{
					continue;
				}

				float distance = track.elements[j].rect.left() - track.elements[i].rect.left();
				float delta = clamp_tpl(1.0f - fabsf(distance) / keySize, 0.0f, 1.0f);

				if (delta == 0.0f)
				{
					continue;
				}

				float& pushOutDistance = track.elements[i].pushOutDistance;
				pushOutDistance += (i < j) ? -delta : delta;

				if (fabsf(pushOutDistance) > maxPushOut)
				{
					maxPushOut = fabsf(pushOutDistance);
				}
			}
		}
	}

	float maxPushOutNormalized = float(MAX_PUSH_OUT) / PUSH_OUT_DISTANCE;
	float pushOutScale = 1.0f;

	if (maxPushOut > maxPushOutNormalized && maxPushOut > 0.0f)
	{
		pushOutScale = maxPushOutNormalized / maxPushOut;
	}

	for (size_t i = 0; i < numLayoutTracks; ++i)
	{
		STrackLayout& track = layout->tracks[i];

		for (size_t j = 0; j < track.elements.size(); ++j)
		{
			track.elements[j].rect.translate(QPoint(0, int(pushOutScale * track.elements[j].pushOutDistance * PUSH_OUT_DISTANCE)));
		}
	}
}

void UpdateScrollPixels(STimelineLayout* layout, STimelineViewState& viewState)
{
	if (viewState.visibleDistance != 0)
	{
		SAnimTime totalDuration = layout->maxEndTime - layout->minStartTime;
		viewState.scrollPixels = QPoint(viewState.ScrollOffset(viewState.viewOrigin), 0);
		viewState.maxScrollX = int(viewState.widthPixels * totalDuration.ToFloat() / viewState.visibleDistance) - viewState.widthPixels;
	}
	else
	{
		viewState.scrollPixels = QPoint(0, 0);
		viewState.maxScrollX = 0;
	}
}

void CalculateLayout(STimelineLayout* layout, STimelineContent& content, STimelineViewState& viewState, const QLineEdit* pFilterLineEdit, float thumbTime, uint keySize, bool treeVisible, bool mainTrackTimeRange)
{
	if (!mainTrackTimeRange)
	{
		CalculateMinMaxTimeWithDefaults(layout, content.track);
	}
	else
	{
		layout->minStartTime = content.track.startTime;
		layout->maxEndTime = content.track.endTime;
	}

	layout->thumbPositionX = viewState.TimeToLayout(thumbTime);     // has to be after calculating the visibleDistance

	int currentTop = RULER_HEIGHT + VERTICAL_PADDING;
	const int treeWidth = treeVisible ? viewState.treeWidth : 0;

	std::unordered_set<const STimelineTrack*> invisibleTracks;
	if (pFilterLineEdit && !pFilterLineEdit->text().isEmpty())
	{
		FilterTracks(content.track, invisibleTracks, QtUtil::ToString(pFilterLineEdit->text()));
	}

	CalculateTrackLayout(layout, currentTop, 0, content.track, viewState, thumbTime, keySize, treeWidth, invisibleTracks);

	layout->size = QSize(viewState.TimeToLayout(layout->maxEndTime.ToFloat()), currentTop + VERTICAL_PADDING);
}

STrackLayout* HitTestTrack(STrackLayouts& tracks, const QPoint& point, bool bIgnoreTrackRange)
{
	auto findIter = std::upper_bound(tracks.begin(), tracks.end(), point.y(), [&](int y, const STrackLayout& track)
		{
			return y < track.rect.bottom();
	  });

	if (findIter != tracks.end() && (!bIgnoreTrackRange || findIter->rect.contains(point)))
	{
		return &(*findIter);
	}
	return nullptr;
}

template<class Func>
void ForEachTrack(STimelineTrack& track, Func& fun)
{
	fun(track);

	for (size_t i = 0; i < track.tracks.size(); ++i)
	{
		STimelineTrack& subTrack = *track.tracks[i];
		ForEachTrack(subTrack, fun);
	}
}

template<class Func>
void ForEachTrack(STimelineTrack& track, std::vector<STimelineTrack*>& parents, Func& fun)
{
	fun(track);

	for (size_t i = 0; i < track.tracks.size(); ++i)
	{
		parents.push_back(&track);
		STimelineTrack& subTrack = *track.tracks[i];
		ForEachTrack(subTrack, fun);
		parents.pop_back();
	}
}

template<class Func>
void ForEachElement(STimelineTrack& track, Func& fun)
{
	ForEachTrack(track, [&](STimelineTrack& subTrack)
		{
			for (size_t i = 0; i < subTrack.elements.size(); ++i)
			{
			  fun(subTrack, subTrack.elements[i]);
			}
	  });
}

template<class Func>
void ForEachElementWithIndex(STimelineTrack& track, Func& fun)
{
	ForEachTrack(track, [&](STimelineTrack& subTrack)
		{
			for (size_t i = 0; i < subTrack.elements.size(); ++i)
			{
			  fun(subTrack, subTrack.elements[i], i);
			}
	  });
}

void ClearTrackSelection(STimelineTrack& track)
{
	ForEachTrack(track, [](STimelineTrack& track)
		{
			track.selected = false;
	  });
}

void GetSelectedTracks(STimelineTrack& track, std::vector<STimelineTrack*>& tracks)
{
	ForEachTrack(track, [&](STimelineTrack& track)
		{
			if (track.selected)
			{
			  tracks.push_back(&track);
			}
	  });
}

void ClearElementSelection(STimelineTrack& track)
{
	ForEachElement(track, [](STimelineTrack& track, STimelineElement& element)
		{
			track.keySelectionChanged = track.keySelectionChanged || element.selected;
			element.selected = false;
	  });
}

void SetSelectedElementTimes(STimelineTrack& track, const std::vector<SAnimTime>& times)
{
	auto iter = times.begin();
	ForEachElement(track, [&](STimelineTrack& track, STimelineElement& element)
		{
			if (element.selected)
			{
			  track.modified = true;
			  const SAnimTime length = element.end - element.start;
			  element.start = *(iter++);
			  element.end = element.start + length;
			}
	  });
}

float FindNearestKeyTime(std::vector<SAnimTime>& vec, SAnimTime value, SAnimTime selectedValue, bool forwardSearch)
{
	if (forwardSearch)
	{
		auto const it = std::upper_bound(vec.begin(), vec.end(), value);

		if (it == vec.end() && selectedValue > vec.back())
		{
			return vec.back();
		}

		return *it;

	}
	else
	{
		auto it = std::lower_bound(vec.begin(), vec.end(), value);

		if (it == vec.end() && selectedValue > vec.back())
		{
			return vec.back();
		}
		else if (it != vec.begin() && *it > selectedValue)
		{
			--it;
		}

		return *it;
	}
}

std::vector<SAnimTime> GetSelectedElementTimes(STimelineTrack& track)
{
	std::vector<SAnimTime> times;
	ForEachElement(track, [&](STimelineTrack& track, STimelineElement& element) { if (element.selected) { times.push_back(element.start); } });
	return times;
}

std::vector<SAnimTime> GetAllElementTimes(STimelineTrack& track)
{
	std::vector<SAnimTime> times;
	ForEachElement(track, [&](STimelineTrack& track, STimelineElement& element) { if (!track.disabled && !element.selected && !element.deleted) times.push_back(element.start); });

	for (size_t i = 0; i < track.tracks.size(); ++i)
	{
		if (track.tracks[i]->elements.size() > 0)
		{
			std::vector<SAnimTime> subItems = GetAllElementTimes((STimelineTrack&)track.tracks[i]);

			times.insert(times.end(), subItems.begin(), subItems.end());
		}
	}

	return times;
}

VectorSet<SAnimTime> GetSelectedElementsTimeSet(STimelineTrack& track)
{
	VectorSet<SAnimTime> timeSet;
	ForEachElement(track, [&](STimelineTrack& track, STimelineElement& element) { if (element.selected) { timeSet.insert(element.start); } });
	return timeSet;
}

typedef std::vector<std::pair<STimelineTrack*, STimelineElement*>> TSelectedElements;
TSelectedElements GetSelectedElements(STimelineTrack& track)
{
	TSelectedElements elements;

	ForEachElement(track, [&](STimelineTrack& track, STimelineElement& element)
		{
			if (element.selected)
			{
			  elements.push_back(std::make_pair(&track, &element));
			}
	  });

	return elements;
}

void UpdateTruncatedDurations(STimelineTrack* track)
{
	size_t numElements = track->elements.size();
	std::vector<int> sortedTrackIndices(numElements);
	for (int i = 0; i < numElements; ++i)
		sortedTrackIndices[i] = i;
	std::sort(sortedTrackIndices.begin(), sortedTrackIndices.end(), [&](int a, int b) { return track->elements[a].start < track->elements[b].start; });
	for (size_t i = 1; i < numElements; ++i)
	{
		STimelineElement& previous = track->elements[sortedTrackIndices[i - 1]];
		const STimelineElement& e = track->elements[sortedTrackIndices[i]];
		SAnimTime maxEndTime = e.start;
		if (e.blendInDuration > SAnimTime(0.0f))
			maxEndTime += SAnimTime(std::min((e.end - e.start).GetTicks(), e.blendInDuration.GetTicks()));
		if (previous.end > maxEndTime)
			previous.truncatedDuration = SAnimTime(maxEndTime - previous.start);
		else
			previous.truncatedDuration = SAnimTime(previous.end - previous.start);
		previous.nextKeyTime = e.start;
		previous.nextBlendInTime = maxEndTime;
	}
	if (numElements > 0)
	{
		STimelineElement& last = track->elements[sortedTrackIndices.back()];
		last.truncatedDuration = last.end - last.start;
		last.nextKeyTime = last.end;
		last.nextBlendInTime = last.end;
	}
}

void MoveSelectedElements(STimelineTrack& track, SAnimTime delta)
{
	ForEachElement(track, [delta](STimelineTrack& track, STimelineElement& element)
		{
			if (element.selected)
			{
			  track.modified = true;
			  element.start += delta;
			  if (element.end != SAnimTime::Max())
					element.end += delta;
			}
	  });

	if (track.caps & track.CAP_CLIP_TRUNCATED_BY_NEXT_KEY)
		UpdateTruncatedDurations(&track);
}

void ScaleSelectedElements(STimelineTrack& track, SAnimTime cursorTime, SAnimTime start, SAnimTime delta)
{
	SAnimTime currentDistance = start - cursorTime;
	SAnimTime newDistance = currentDistance + delta;

	if (std::abs((float)currentDistance) < std::numeric_limits<float>::epsilon())
		return;

	float scale = float(newDistance) / float(currentDistance);

	ForEachElement(track, [&](STimelineTrack& track, STimelineElement& element)
		{
			if (element.selected)
			{
			  currentDistance = element.start - cursorTime;
			  newDistance = scale * currentDistance;

			  track.modified = true;
			  element.start = cursorTime + newDistance;
			}
	  });

	if (track.caps & track.CAP_CLIP_TRUNCATED_BY_NEXT_KEY)
		UpdateTruncatedDurations(&track);
}

void DeletedMarkedElements(STimelineTrack& track)
{
	ForEachTrack(track, [&](STimelineTrack& track)
		{
			for (auto iter = track.elements.begin(); iter != track.elements.end(); )
			{
			  if (iter->deleted)
			  {
			    iter = track.elements.erase(iter);
			  }
			  else
			  {
			    ++iter;
			  }
			}
	  });
}

void SelectElementsInRect(const STrackLayouts& tracks, const QRect& rect, bool bToggleSelected = false, bool bSelect = false, bool bDeselect = false)
{
	for (size_t j = 0; j < tracks.size(); ++j)
	{
		const STrackLayout& track = tracks[j];

		for (size_t i = 0; i < track.elements.size(); ++i)
		{
			const SElementLayout& element = track.elements[i];

			if ((element.caps & STimelineElement::CAP_SELECT) == 0)
			{
				continue;
			}

			STimelineTrack* pTimelineTrack = element.elementRef.pTrack;
			const bool bIsCompoundTrack = (pTimelineTrack->caps & STimelineTrack::CAP_COMPOUND_TRACK) != 0;

			if (element.rect.intersects(rect))
			{
				if (!bIsCompoundTrack)
				{
					if (bToggleSelected)
					{
						element.elementRef.GetElement().selected = !element.elementRef.GetElement().selected;
					}
					else if (bDeselect)
					{
						element.elementRef.GetElement().selected = false;
					}
					else
					{
						element.elementRef.GetElement().selected = true;
					}

					element.elementRef.pTrack->keySelectionChanged = true;

				}
				else
				{
					const size_t numSubElements = element.subElements.size();

					for (size_t k = 0; k < numSubElements; ++k)
					{
						if (bToggleSelected)
						{
							element.subElements[k].GetElement().selected = !element.subElements[k].GetElement().selected;
						}
						else if (bDeselect)
						{
							element.subElements[k].GetElement().selected = false;
						}
						else if (!element.subElements[k].GetElement().selected)
						{
							element.subElements[k].GetElement().selected = true;
						}

						element.subElements[k].pTrack->keySelectionChanged = true;
					}
				}
			}
		}

		SelectElementsInRect(track.tracks, rect, bToggleSelected, bSelect, bDeselect);
	}
}

bool HitTestElements(STrackLayouts& tracks, const QRect& rect, SElementLayoutPtrs& out)
{
	bool bHit = false;

	for (size_t j = 0; j < tracks.size(); ++j)
	{
		STrackLayout& track = tracks[j];
		if (track.rect.intersects(rect))
		{
			for (size_t i = 0; i < track.elements.size(); ++i)
			{
				SElementLayout& element = track.elements[i];

				if (element.rect.intersects(rect))
				{
					out.push_back(&element);
					bHit = true;
				}
			}

			bHit = bHit || HitTestElements(track.tracks, rect, out);
		}
	}

	return bHit;
}

bool HitTestElements(STrackLayout& track, const QRect& rect, SElementLayoutPtrs& out)
{
	bool bHit = false;
	for (size_t i = 0; i < track.elements.size(); ++i)
	{
		SElementLayout& element = track.elements[i];

		if (element.rect.intersects(rect))
		{
			out.push_back(&element);
			bHit = true;
		}
	}

	return bHit || HitTestElements(track.tracks, rect, out);
}

enum EPass
{
	PASS_BACKGROUND,
	PASS_MAIN,
	NUM_PASSES
};

QBrush PickTrackBrush(const STrackLayout& track)
{
	LOADING_TIME_PROFILE_SECTION

	const QColor trackColor = GetStyleHelper()->timelineTrackColor();
	const QColor descriptionTrackColor = GetStyleHelper()->timelineDescriptionTrackColor();
	const QColor compositeTrackColor = GetStyleHelper()->timelineCompositeTrackColor();
	const QColor selectionColor = GetStyleHelper()->timelineSelectionColor();

	const bool bIsDescriptionTrack = (track.pTimelineTrack->caps & STimelineTrack::CAP_DESCRIPTION_TRACK) != 0;
	const bool bIsCompositeTrack = (track.pTimelineTrack->caps & STimelineTrack::CAP_COMPOUND_TRACK) != 0;

	const QColor color = bIsDescriptionTrack ? descriptionTrackColor : (bIsCompositeTrack ? compositeTrackColor : trackColor);
	return QBrush(track.pTimelineTrack->selected ? QtUtil::InterpolateColor(color, selectionColor, 0.3f) : color);
}

void UpdateClipLinesCache(STracksRenderCache& renderCache, const SElementLayout& element, const QRect& rect, const STimelineViewState& viewState)
{
	float duration = element.elementRef.GetElement().clipAnimDuration;

	float start = MIN(duration, MAX(0, element.elementRef.GetElement().clipAnimStart));
	float r = start / duration;

	int x0L = rect.left();
	int x0R = rect.right();

	int len = viewState.TimeToLayout(duration);

	int x1L = x0L - len*r;
	int x1R = x1L + len;

	x1R = (x1R >= x0R ? x1R : x0R);

	int y0 = rect.top();
	int y1 = y0 + rect.height();

	renderCache.unclippedClipLines.emplace_back(QLine(x1L, y0, x0L, y0));
	renderCache.unclippedClipLines.emplace_back(QLine(x1L, y0, x1L, y1));
	renderCache.unclippedClipLines.emplace_back(QLine(x1L, y1, x0L, y1));

	renderCache.unclippedClipLines.emplace_back(QLine(x1R, y0, x0R, y0));
	renderCache.unclippedClipLines.emplace_back(QLine(x1R, y0, x1R, y1));
	renderCache.unclippedClipLines.emplace_back(QLine(x1R, y1, x0R, y1));
}

void UpdateTracksRenderCache(STracksRenderCache& renderCache, QPainter& painter, const STimelineLayout& layout, const STimelineViewState& viewState, const QRect& trackRect, float timeUnitScale, bool drawMarkers, bool bShowText, int scrollOffset)
{
	LOADING_TIME_PROFILE_SECTION

	renderCache.Clear();

	const STrackLayouts& tracks = layout.tracks;

	const int32 trackAreaLeft = viewState.LocalToLayout(QPoint(viewState.treeWidth, 0)).x();
	const int32 trackAreaRight = trackAreaLeft + trackRect.width();

	const Range visibleRange = Range(viewState.LocalToTime(viewState.treeWidth) * timeUnitScale, viewState.LocalToTime(trackRect.width()) * timeUnitScale);
	const Range rulerRange = Range(layout.minStartTime.ToFloat() * timeUnitScale, layout.maxEndTime.ToFloat() * timeUnitScale);

	const int32 highMarkersMod = TRACK_MARK_HEIGHT;
	const int32 lowMarkersMod = highMarkersMod / 2;

	const QRect markersRect = QRect(-viewState.scrollPixels.x(), 0, trackRect.width() - viewState.treeWidth, 0);

	const char* passName[] = {
		"PASS_BACKGROUND",
		"PASS_MAIN",
		"NUM_PASSES"
	};

	QRect outSideBackgroundRectLeft = QRect(0, 0, viewState.TimeToLayout(rulerRange.start), layout.size.height());
	outSideBackgroundRectLeft.setLeft(-viewState.scrollPixels.x());
	outSideBackgroundRectLeft.setRight(viewState.TimeToLayout(rulerRange.start));

	QRect outSideBackgroundRectRight = QRect(viewState.TimeToLayout(rulerRange.end), 0, trackRect.width() - viewState.scrollPixels.x(), layout.size.height());

	renderCache.outsideTrackBGRects.emplace_back(outSideBackgroundRectLeft);
	renderCache.outsideTrackBGRects.emplace_back(outSideBackgroundRectRight);

	std::vector<std::vector<SElementLayout>> trackSortedElements(tracks.size());
	for (size_t j = 0; j < tracks.size(); ++j)
	{
		const STrackLayout& track = tracks[j];
		const uint32 numElements = track.elements.size();

		std::vector<SElementLayout>& layouts = trackSortedElements[j];

		std::copy(std::begin(track.elements), std::end(track.elements), std::back_inserter(layouts));
		std::sort(std::begin(layouts), std::end(layouts),
			[](const SElementLayout& a, const SElementLayout& b)
		{
			return a.rect.left() < b.rect.left();
		}
		);
	}

	for (int32 i = PASS_BACKGROUND; i <= PASS_MAIN; ++i)
	{
		const EPass pass = (EPass)i;

		LOADING_TIME_PROFILE_SECTION_NAMED(passName[i])

		for (size_t j = 0; j < tracks.size(); ++j)
		{
			const STrackLayout& track = tracks[j];

			// only cache the track if it's in view
			if (track.rect.top() - scrollOffset > trackRect.bottom() || track.rect.bottom() - scrollOffset < trackRect.top())
			{
				continue;
			}

			const uint32 numElements = track.elements.size();

			std::vector<SElementLayout>& sortedElements = trackSortedElements[j];
			switch (pass)
			{
			case PASS_BACKGROUND:
				{
					QRect backgroundRect = track.rect;
					backgroundRect.setLeft(-viewState.scrollPixels.x());
					backgroundRect.setWidth(trackRect.width());

					if (track.pTimelineTrack->selected)
					{
						renderCache.seleTrackBGRects.emplace_back(backgroundRect);
					}
					else if ((track.pTimelineTrack->caps & STimelineTrack::CAP_DESCRIPTION_TRACK) != 0)
					{
						renderCache.descTrackBGRects.emplace_back(backgroundRect);
					}
					else
					{
						renderCache.compTrackBGRects.emplace_back(backgroundRect);
					}

					if (track.pTimelineTrack->disabled)
					{
						renderCache.disbTrackBGRects.emplace_back(backgroundRect);
					}

					const int32 lineY = track.rect.bottom() + 1;
					renderCache.bottomLines.emplace_back(QLine(QPoint(backgroundRect.left(), lineY), QPoint(backgroundRect.right(), lineY)));

					if (drawMarkers && (track.pTimelineTrack->caps & STimelineTrack::CAP_DESCRIPTION_TRACK) == 0)
					{
						QRect rect = markersRect;
						rect.setTop(track.rect.top());
						rect.setBottom(track.rect.bottom());

						const int32 height = rect.height();
						const int32 top = rect.top();

						const int32 lowY = top + height - lowMarkersMod;
						const int32 highY = top + height - highMarkersMod;

						if (renderCache.pTimeMarkers)
						{
							for (const DrawingPrimitives::CRuler::STick& tick : * renderCache.pTimeMarkers)
							{
								const int32 x = tick.position + rect.left();
								const QLine line = QLine(QPoint(x, !tick.bTenth ? lowY : highY), QPoint(x, top + height));
								if (!tick.bIsOuterTick)
									renderCache.innerTickLines.emplace_back(line);
							}
						}
						else
						{
							for (const DrawingPrimitives::STick& tick : renderCache.timeMarkers)
							{
								const int32 x = tick.m_position + rect.left();
								const QLine line = QLine(QPoint(x, tick.m_bTenth ? lowY : highY), QPoint(x, top + height));
								if (!tick.m_bIsOuterTick)
									renderCache.innerTickLines.emplace_back(line);
							}
						}
					}

					if ((track.pTimelineTrack->caps & STimelineTrack::CAP_TOGGLE_TRACK) != 0)
					{
						QRect toggleRect = track.rect;
						toggleRect.setTop(toggleRect.top() + 2);
						toggleRect.setBottom(toggleRect.bottom() - 2);

						const uint32 drawStart = track.pTimelineTrack->toggleDefaultState ? 0 : 1;
						for (uint32 i = drawStart; i <= numElements; i += 2)
						{
							toggleRect.setLeft((i == 0) ? (-viewState.scrollPixels.x()) : sortedElements[i - 1].rect.right());
							toggleRect.setRight((i == numElements) ? (-viewState.scrollPixels.x() + trackRect.width()) : sortedElements[i].rect.left());

							renderCache.toggleRects.emplace_back(toggleRect);
						}
					}
				}
				break;
			default:
				{
					for (auto itr = sortedElements.begin(); itr != sortedElements.end(); ++itr)
					{
						const SElementLayout& element = *itr;
						const QRect& rect = element.rect;

						if (rect.right() < trackAreaLeft)
							continue;
						else if (rect.left() > trackAreaRight)
							break;

						switch (element.type)
						{
						case STimelineElement::KEY:
							{
								CryIconColorMap cryIconColorMap;
								cryIconColorMap[QIcon::Normal] = QColor(element.color.r, element.color.g, element.color.b, element.color.a);

								if (element.IsHighlighted())
								{
									QPixmap pixmap = CryIcon("icons:Trackview/Trackview_Key_Highlight.ico", std::move(cryIconColorMap)).pixmap(rect.width(), rect.height());
									renderCache.highlightedKeyIcons.emplace_back(QRenderIcon(rect, std::move(pixmap)));
								}
								else if (element.IsSelected())
								{
									QPixmap pixmap = CryIcon("icons:Trackview/Trackview_Key_Active.ico", std::move(cryIconColorMap)).pixmap(rect.width(), rect.height());
									renderCache.selectedKeyIcons.emplace_back(QRenderIcon(rect, std::move(pixmap)));
								}
								else
								{
									QPixmap pixmap = CryIcon("icons:Trackview/Trackview_Key_normal.ico", std::move(cryIconColorMap)).pixmap(rect.width(), rect.height());
									renderCache.defaultKeyIcons.emplace_back(QRenderIcon(rect, std::move(pixmap)));
								}

								if (bShowText)
								{
									QRect textRect = track.rect;
									textRect.moveLeft(rect.right() + TRACK_DESCRIPTION_INDENT);
									textRect.setTop(textRect.top() + 1);

									auto nextItr = itr + 1;
									if (nextItr != sortedElements.end())
									{
										textRect.setRight(nextItr->rect.left() - 6);
									}

									const int32 width = textRect.width();
									if (width > 5)
									{
										const QString elidedText = painter.fontMetrics().elidedText(element.description.c_str(), Qt::ElideRight, textRect.width());
										renderCache.text.emplace_back(QRenderText(textRect, elidedText));
									}
								}
							}
							break;
						default:
							{
								// HACK: Temp solution to ensure super small durations get
								//			 rendered in reasonable size.
								if (rect.right() - rect.left() <= 1)
								{
									const_cast<QRect&>(rect).setRight(rect.right() + 1);
									const_cast<QRect&>(rect).setLeft(rect.left() - 1);
								}

								// calculate lines, to designate cropped animation length
								if ((element.elementRef.GetElement().clipAnimStart) || (element.elementRef.GetElement().clipAnimEnd) && (!element.IsSelected()))
								{
									UpdateClipLinesCache(renderCache, element, rect, viewState);
								}
								
								if (!element.IsSelected())
								{
									renderCache.defaultClipRects.emplace_back(rect);
								}
								else
								{
									renderCache.selectedClipRects.emplace_back(rect);
								}
							}
						}
					}
				}
			}
		}       // switch
	}
}

void DrawSelectionLines(QPainter& painter, const STimelineViewState& viewState, const Range& visibleRange, STimelineContent& content, int rulerPrecision, int width, int height, float time, float timeUnitScale, bool hasFocus)
{
	LOADING_TIME_PROFILE_SECTION

	const VectorSet<SAnimTime> times = GetSelectedElementsTimeSet(content.track);
	const QColor indicatorColor = (hasFocus ? GetStyleHelper()->timelineSelectionLinesFocused() : GetStyleHelper()->timelineSelectionLines());

	painter.setPen(indicatorColor);
	for (auto iter = times.begin(); iter != times.end(); ++iter)
	{
		const float linePos = iter->ToFloat();
		if (linePos >= visibleRange.start && linePos <= visibleRange.end)
		{
			const int32 indicatorX = int32(viewState.TimeToLocal(iter->ToFloat()) + 0.5f);
			painter.drawLine(QPoint(indicatorX, 0), QPoint(indicatorX, height));
		}
	}
}

void UpdateTreeRenderCache(STreeRenderCache& renderCache, const QRect& treeRect, QWidget* timeline, const STimelineContent& content, const STrackLayouts& tracks, const STimelineViewState& viewState, int scrollOffset)
{
	LOADING_TIME_PROFILE_SECTION

	renderCache.Clear();

	for (size_t i = 0; i < tracks.size(); ++i)
	{
		const STrackLayout& track = tracks[i];

		// only cache the track if it's in view
		if (track.rect.top() - scrollOffset > treeRect.bottom() || track.rect.bottom() - scrollOffset < treeRect.top())
		{
			continue;
		}

		const QRect backgroundRect(1, track.rect.top() + 1, viewState.treeWidth - SPLITTER_WIDTH - 1, track.rect.height() - 1);

		if (track.pTimelineTrack->selected)
		{
			renderCache.seleTrackBGRects.emplace_back(backgroundRect);
		}
		else if ((track.pTimelineTrack->caps & STimelineTrack::CAP_DESCRIPTION_TRACK) != 0)
		{
			renderCache.descTrackBGRects.emplace_back(backgroundRect);
		}
		else
		{
			renderCache.compTrackBGRects.emplace_back(backgroundRect);
		}

		if (track.pTimelineTrack->disabled)
		{
			renderCache.disbTrackBGRects.emplace_back(backgroundRect);
		}

		const int32 branchLeft = TREE_LEFT_MARGIN + track.indent * TREE_INDENT_MULTIPLIER;

		if (track.pTimelineTrack->tracks.size() > 0)
		{
			QStyleOptionViewItem opt;
			opt.rect = QRect(branchLeft, track.rect.top() + 1, TREE_BRANCH_INDICATOR_SIZE, track.rect.height() - 2);
			opt.state = QStyle::State_Enabled | QStyle::State_Children;
			opt.state |= track.pTimelineTrack->expanded ? QStyle::State_Open : QStyle::State_None;

			renderCache.primitives.emplace_back(opt);
		}

		const int32 iconLeft = branchLeft + TREE_BRANCH_INDICATOR_SIZE + 4;
		const int32 textLeft = track.pTimelineTrack->HasIcon() ? (iconLeft + track.rect.height() + 2) : iconLeft;
		const int32 textWidth = std::max(treeRect.width() - textLeft - 4, 0);
		const QRect textRect(textLeft, track.rect.top() + 1, textWidth, track.rect.height() - 2);

		renderCache.text.emplace_back(QRenderText(textRect, QString(track.pTimelineTrack->name)));

		if (track.pTimelineTrack->HasIcon())
		{
			const QRect iconRect(iconLeft, track.rect.top() + 1, track.rect.height() - 2, track.rect.height() - 2);
			if (track.pTimelineTrack->selected)
			{
				CryIconColorMap cryIconColorMap;
				cryIconColorMap[QIcon::Normal] = GetStyleHelper()->alternateSelectedIconTint();
				renderCache.icons.emplace_back(QRenderIcon(iconRect, CryIcon(QString(track.pTimelineTrack->icon), cryIconColorMap).pixmap(iconRect.width(), iconRect.height())));
			}
			else
				renderCache.icons.emplace_back(QRenderIcon(iconRect, CryIcon(QString(track.pTimelineTrack->icon)).pixmap(iconRect.width(), iconRect.height())));
		}

		const int32 lineY = track.rect.bottom() + 1;
		renderCache.trackLines.emplace_back(QLine(QPoint(0, lineY), QPoint(treeRect.width(), lineY)));
	}
}

void PaintRenderCacheRects(QPainter& painter, const std::vector<QRect>& rectVector)
{
	painter.drawRects(rectVector.data(), rectVector.size());
}

void PaintRenderCacheLines(QPainter& painter, const std::vector<QLine>& lineVector)
{
	painter.drawLines(lineVector.data(), lineVector.size());
}
}

struct CTimeline::SMouseHandler
{
	virtual void mousePressEvent(QMouseEvent* ev)       {}
	virtual void mouseDoubleClickEvent(QMouseEvent* ev) {}
	virtual void mouseMoveEvent(QMouseEvent* ev)        {}
	virtual void mouseReleaseEvent(QMouseEvent* ev)     {}
	virtual void focusOutEvent(QFocusEvent* ev)         {}
	virtual void paintOver(QPainter& painter)           {}
};

struct CTimeline::SSelectionHandler : SMouseHandler
{
	CTimeline*        m_timeline;
	QPoint            m_startPoint;
	QRect             m_rect;
	bool              m_add;
	TSelectedElements m_oldSelectedElements;

	SSelectionHandler(CTimeline* timeline, bool add)
		: m_timeline(timeline)
		, m_add(add)
	{
		if (m_timeline->m_pContent)
		{
			m_oldSelectedElements = GetSelectedElements(m_timeline->m_pContent->track);
		}
	}

	void mousePressEvent(QMouseEvent* ev) override
	{
		const int scroll = m_timeline->m_scrollBar ? m_timeline->m_scrollBar->value() : 0;
		const QPoint pos(ev->pos().x(), ev->pos().y() + scroll);
		m_startPoint = m_timeline->m_viewState.LocalToLayout(pos);
		m_rect = QRect(m_startPoint, m_startPoint + QPoint(1, 1));
	}

	void mouseMoveEvent(QMouseEvent* ev) override
	{
		const int scroll = m_timeline->m_scrollBar ? m_timeline->m_scrollBar->value() : 0;
		const QPoint pos(ev->pos().x(), ev->pos().y() + scroll);
		m_rect = QRect(m_startPoint, m_timeline->m_viewState.LocalToLayout(pos) + QPoint(1, 1));

		SElementLayoutPtrs hitElements;
		m_timeline->ResolveHitElements(m_rect, hitElements);
		m_timeline->UpdateHighligted(hitElements);
	}

	void mouseReleaseEvent(QMouseEvent* ev) override
	{
		const bool bToggleSelected = (ev->modifiers() & Qt::CTRL) != 0;
		const bool bSelect = (ev->modifiers() & Qt::SHIFT) != 0;
		const bool bDeselect = (ev->modifiers() & Qt::ALT) != 0;

		Apply(false, bToggleSelected, bSelect, bDeselect);
	}

	void Apply(bool continuous, bool bToggleSelected = false, bool bSelect = false, bool bDeselect = false)
	{
		if (m_timeline->m_pContent)
		{
			const TSelectedElements selectedElements = GetSelectedElements(m_timeline->m_pContent->track);

			if (!bToggleSelected && !bSelect && !bDeselect)
			{
				ClearElementSelection(m_timeline->m_pContent->track);
			}

			SelectElementsInRect(m_timeline->m_layout->tracks, m_rect, bToggleSelected, bSelect, bDeselect);

			const TSelectedElements newSelectedElements = GetSelectedElements(m_timeline->m_pContent->track);
			if ((continuous && selectedElements != newSelectedElements)
			    || (!continuous && m_oldSelectedElements != newSelectedElements))
			{
				m_timeline->SignalSelectionChanged(continuous);
			}
		}
	}

	void paintOver(QPainter& painter) override
	{
		painter.save();
		QColor highlightColor = GetStyleHelper()->highlightColor();
		QColor highlightColorA = QColor(highlightColor.red(), highlightColor.green(), highlightColor.blue(), 128);
		painter.setPen(QPen(highlightColor));
		painter.setBrush(QBrush(highlightColorA));
		painter.drawRect(QRectF(m_rect));
		painter.restore();
	}
};

static STimelineElement* NextSelectedElement(const SElementLayoutPtrs& array, STimelineElement* nextToValue, STimelineElement* defaultValue)
{
	for (size_t i = 0; i < array.size(); ++i)
		if (&array[i]->elementRef.GetElement() == nextToValue)
			return &array[(i + 1) % array.size()]->elementRef.GetElement();
	return defaultValue;
}

struct CTimeline::SMoveHandler : SMouseHandler
{
	CTimeline*              m_timeline;
	const STimelineElement* m_pClickedElement;
	QPoint                  m_startPoint;
	bool                    m_cycleSelection;
	SAnimTime               m_startTime;
	SAnimTime               m_clickTime;
	std::vector<SAnimTime>  m_elementTimes;

	SMoveHandler(CTimeline* timeline, bool cycleSelection)
		: m_timeline(timeline)
		, m_pClickedElement(nullptr)
		, m_cycleSelection(cycleSelection) {}

	void mousePressEvent(QMouseEvent* ev) override
	{
		const int scroll = m_timeline->m_scrollBar ? m_timeline->m_scrollBar->value() : 0;
		const QPoint currentPos(ev->pos().x(), ev->pos().y() + scroll);

		m_startPoint = m_timeline->m_viewState.LocalToLayout(QPoint(currentPos));

		m_clickTime = SAnimTime(float(m_startPoint.x()) / m_timeline->m_viewState.widthPixels * m_timeline->m_viewState.visibleDistance);
		m_startTime = m_clickTime;
		SAnimTime dt = m_timeline->m_pContent->track.endTime;
		SAnimTime clickedElementDelta = dt;

		const TSelectedElements selectedElements = GetSelectedElements(m_timeline->m_pContent->track);

		for (size_t i = 0; i < selectedElements.size(); ++i)
		{
			const STimelineElement& element = *selectedElements[i].second;
			clickedElementDelta = element.start - m_clickTime;
			if (abs(clickedElementDelta.GetTicks()) < abs(dt.GetTicks()))
			{
				m_pClickedElement = &element;
				dt = clickedElementDelta;
			}
		}

		if (m_timeline->m_snapKeys)
		{
			// Get all not selected elements from non-disabled tracks
			m_elementTimes = GetAllElementTimes(m_timeline->m_pContent->track);

			std::sort(m_elementTimes.begin(), m_elementTimes.end());

			// Remove exact time duplicates
			m_elementTimes.erase(unique(m_elementTimes.begin(), m_elementTimes.end()), m_elementTimes.end());
		}
		else
		{
			m_elementTimes = GetSelectedElementTimes(m_timeline->m_pContent->track);
		}
	}

	void mouseMoveEvent(QMouseEvent* ev) override
	{
		if (m_timeline->m_viewState.widthPixels == 0)
		{
			return;
		}

		const TSelectedElements selectedElements = GetSelectedElements(m_timeline->m_pContent->track);

		const int scroll = m_timeline->m_scrollBar ? m_timeline->m_scrollBar->value() : 0;
		const QPoint currentPos(ev->pos().x(), ev->pos().y() + scroll);
		int delta = m_timeline->m_viewState.LocalToLayout(currentPos).x() - m_startPoint.x();

		const bool bToggleSelected = (ev->modifiers() & Qt::SHIFT) != 0;

		SAnimTime deltaTime = SAnimTime(float(delta) / m_timeline->m_viewState.widthPixels * m_timeline->m_viewState.visibleDistance);
		SAnimTime endTime = m_timeline->m_pContent->track.endTime;		
		
		SAnimTime currentTime = m_timeline->m_viewState.LayoutToTime(m_timeline->m_viewState.LocalToLayout(currentPos).x());

		bool bNeedMoveElements = false;
		SAnimTime moveElementsDeltaTime;

		if (m_pClickedElement && (m_timeline->m_snapKeys || m_timeline->m_snapTime || bToggleSelected))
		{
			SAnimTime startElementTime = m_pClickedElement->start;
			SAnimTime nearestKeyTime(startElementTime);

			if (m_timeline->m_snapKeys)
			{
				if (deltaTime > SAnimTime(0.0f))
				{
					nearestKeyTime = FindNearestKeyTime(m_elementTimes, startElementTime, currentTime, true);
				}
				else if (deltaTime < SAnimTime(0.0f))
				{
					nearestKeyTime = FindNearestKeyTime(m_elementTimes, startElementTime, currentTime, false);
				}
			}
			else if (m_timeline->m_snapTime)
			{
				nearestKeyTime = currentTime.SnapToNearest(m_timeline->m_frameRate);
			}
			// User drags keys when SHIFT key is pressed.  It snaps selected keys to Timeline ruler markings.
			else if (bToggleSelected)
			{
				if (deltaTime > SAnimTime(0.0f))
				{
					nearestKeyTime = FindNearestKeyTime(m_timeline->GetTickTimePositions(), startElementTime, currentTime, true);
				}
				else if (deltaTime < SAnimTime(0.0f))
				{
					nearestKeyTime = FindNearestKeyTime(m_timeline->GetTickTimePositions(), startElementTime, currentTime, false);
				}
			}

			if (nearestKeyTime != startElementTime)
			{
				bNeedMoveElements = true;
				moveElementsDeltaTime.SetTime(nearestKeyTime - startElementTime);
				m_startTime = nearestKeyTime;
				m_startPoint.setX(m_timeline->m_viewState.LocalToLayout(currentPos).x());
				m_timeline->ContentChanged(true);
			}
		}
		// No snapping
		else
		{
			bNeedMoveElements = true;
			moveElementsDeltaTime.SetTime( deltaTime );
			m_startTime = currentTime;
			m_startPoint.setX(m_timeline->m_viewState.LocalToLayout(currentPos).x());
			m_timeline->ContentChanged(true);
		}

		if (!m_timeline->m_allowOutOfRangeKeys && bNeedMoveElements)
		{
			bool bAtLeastOneSelectedElementIsOutOfRange = false;

			for (size_t i = 0; i < selectedElements.size(); ++i)
			{
				const STimelineTrack& track = *selectedElements[i].first;
				const STimelineElement& element = *selectedElements[i].second;
				if (element.start + deltaTime < track.startTime)
				{
					bAtLeastOneSelectedElementIsOutOfRange = true;
					break;
				}
				if ( (element.type == element.CLIP ? element.end : element.start) + deltaTime > track.endTime)
				{
					bAtLeastOneSelectedElementIsOutOfRange = true;
					break;
				}
			}

			if (bAtLeastOneSelectedElementIsOutOfRange)
			{
				bNeedMoveElements = false;
				moveElementsDeltaTime.SetTime(0);
			}
		}

		if (bNeedMoveElements)
		{
			MoveElements(m_timeline->m_pContent->track, moveElementsDeltaTime);
			m_timeline->UpdateLayout();
		}

		m_timeline->setCursor(Qt::SizeHorCursor);
		m_cycleSelection = false;
	}

	virtual void MoveElements(STimelineTrack& track, SAnimTime delta) = 0;

	void         focusOutEvent(QFocusEvent* ev)
	{
		SetSelectedElementTimes(m_timeline->m_pContent->track, m_elementTimes);
		m_timeline->UpdateLayout();
	}

	void mouseReleaseEvent(QMouseEvent* ev)
	{
		if (m_cycleSelection)
		{
			SElementLayoutPtrs hitElements;

			const int scroll = m_timeline->m_scrollBar ? m_timeline->m_scrollBar->value() : 0;
			const QPoint currentPos(ev->pos().x(), ev->pos().y() + scroll);

			QPoint posInLayoutSpace = m_timeline->m_viewState.LocalToLayout(currentPos);
			bool bHit = HitTestElements(m_timeline->m_layout->tracks, QRect(posInLayoutSpace - QPoint(2, 2), posInLayoutSpace + QPoint(2, 2)), hitElements);

			if (!hitElements.empty() && !hitElements.back()->elementRef.pTrack->elements.empty())
			{
				TSelectedElements selectedElements = GetSelectedElements(m_timeline->m_pContent->track);
				if (selectedElements.size() == 1)
				{
					STimelineElement* lastSelection = selectedElements[0].second;

					ClearElementSelection(m_timeline->m_pContent->track);
					NextSelectedElement(hitElements, lastSelection, &hitElements.back()->elementRef.GetElement())->selected = true;
					m_timeline->SignalSelectionChanged(false);
				}
				else
				{
					ClearElementSelection(m_timeline->m_pContent->track);
					hitElements.back()->elementRef.GetElement().selected = true;
					m_timeline->SignalSelectionChanged(false);
				}
			}
		}

		if (m_clickTime != m_startTime)
			m_timeline->ContentChanged(false);
	}
};

struct CTimeline::SShiftHandler : public CTimeline::SMoveHandler
{
	SShiftHandler(CTimeline* timeline, bool cycleSelection) : SMoveHandler(timeline, cycleSelection) {}

	virtual void MoveElements(STimelineTrack& track, SAnimTime delta) override
	{
		MoveSelectedElements(m_timeline->m_pContent->track, delta);
	}
};

struct CTimeline::SScaleHandler : public CTimeline::SMoveHandler
{
	SScaleHandler(CTimeline* timeline, bool cycleSelection) : SMoveHandler(timeline, cycleSelection) {}

	virtual void MoveElements(STimelineTrack& track, SAnimTime delta) override
	{
		SAnimTime cursorTime = m_timeline->Time();
		SAnimTime start = m_startTime;

		ScaleSelectedElements(m_timeline->m_pContent->track, cursorTime, start, delta);
	}
};

struct CTimeline::SPanHandler : SMouseHandler
{
	CTimeline* m_timeline;
	QPoint     m_startPoint;
	float      m_startOrigin;
	int        m_startScroll;

	SPanHandler(CTimeline* timeline) : m_timeline(timeline)
	{
	}

	void mousePressEvent(QMouseEvent* ev) override
	{
		m_startPoint = QPoint(int(ev->x()), int(ev->y()));
		m_startOrigin = m_timeline->m_viewState.viewOrigin;
		m_startScroll = m_timeline->m_scrollBar ? m_timeline->m_scrollBar->value() : 0;

		m_timeline->setCursor(Qt::SizeAllCursor);
	}

	void mouseMoveEvent(QMouseEvent* ev) override
	{
		const QPoint pos(int(ev->x()), int(ev->y()));
		const QPoint posDif(pos - m_startPoint);

		float deltaX = 0.0f;
		if (m_timeline->m_viewState.widthPixels != 0)
		{
			deltaX = posDif.x() * m_timeline->m_viewState.visibleDistance / m_timeline->m_viewState.widthPixels;
		}

		if (m_timeline->m_scrollBar && !m_timeline->m_scrollBar->isHidden())
		{
			m_timeline->m_scrollBar->setValue(m_startScroll - (int)posDif.y());
		}

		m_timeline->m_viewState.viewOrigin = m_startOrigin + deltaX;
		m_timeline->ClampViewOrigin();

		m_timeline->SignalPan();
	}

	void mouseReleaseEvent(QMouseEvent* ev) override
	{
		m_timeline->setCursor(Qt::ArrowCursor);
	}
};

struct CTimeline::SScrubHandler : SMouseHandler
{
	CTimeline* m_timeline;
	SScrubHandler(CTimeline* timeline) : m_timeline(timeline) {}
	SAnimTime  m_startThumbPosition;
	QPoint     m_startPoint;

	void SetThumbPositionX(int positionX)
	{
		int nThumbEndPadding = (THUMB_WIDTH / 2) + 2;
		float fVisualRange = float(m_timeline->m_viewState.widthPixels - nThumbEndPadding * 2);
		SAnimTime time = SAnimTime(m_timeline->m_viewState.LayoutToTime(positionX));

		m_timeline->ClampAndSetTime(time, false);
	}

	void mousePressEvent(QMouseEvent* ev) override
	{
		QPoint point = QPoint(ev->pos().x(), ev->pos().y());

		QPoint posInLayout = m_timeline->m_viewState.LocalToLayout(point);

		int thumbPositionX = m_timeline->m_viewState.TimeToLayout(m_timeline->m_time.ToFloat());
		QRect thumbRect(thumbPositionX - THUMB_WIDTH / 2, 0, THUMB_WIDTH, THUMB_HEIGHT);

		if (!thumbRect.contains(posInLayout))
		{
			SetThumbPositionX(m_timeline->m_viewState.LocalToLayout(point).x());
		}

		m_startThumbPosition = m_timeline->m_time;
		m_startPoint = point;
	}

	void Apply(QMouseEvent* ev, bool continuous)
	{
		QPoint point = QPoint(ev->pos().x(), ev->pos().y());

		bool shift = ev->modifiers().testFlag(Qt::ShiftModifier);
		bool control = ev->modifiers().testFlag(Qt::ControlModifier);

		float delta = 0.0f;

		if (m_timeline->m_viewState.widthPixels != 0)
		{
			delta = (point.x() - m_startPoint.x()) * m_timeline->m_viewState.visibleDistance / m_timeline->m_viewState.widthPixels;
		}

		if (shift)
		{
			delta *= 0.01f;
		}

		if (control)
		{
			delta *= 0.1f;
		}

		m_timeline->ClampAndSetTime(m_startThumbPosition + SAnimTime(delta), true);
	}

	void mouseMoveEvent(QMouseEvent* ev) override
	{
		Apply(ev, true);
	}

	void mouseReleaseEvent(QMouseEvent* ev) override
	{
		Apply(ev, false);
	}
};

struct CTimeline::SSplitterHandler : SMouseHandler
{
	CTimeline* m_timeline;
	int        m_offset;
	bool       m_movedSlider;

	SSplitterHandler(CTimeline* timeline) : m_timeline(timeline), m_offset(0), m_movedSlider(false) {}

	void mousePressEvent(QMouseEvent* ev) override
	{
		m_offset = m_timeline->m_viewState.treeWidth - ev->pos().x();
		m_timeline->m_splitterState = ESplitterState::Moving;
	}

	void mouseReleaseEvent(QMouseEvent* ev) override
	{
		if (!m_movedSlider)
		{
			STimelineViewState& viewState = m_timeline->m_viewState;

			if (viewState.treeWidth == SPLITTER_WIDTH)
			{
				viewState.treeWidth = viewState.treeLastOpenedWidth;
			}
			else
			{
				viewState.treeLastOpenedWidth = viewState.treeWidth;
				viewState.treeWidth = SPLITTER_WIDTH;
			}

			m_timeline->m_splitterState = ESplitterState::Normal;

			m_timeline->UpdateLayout();
			m_timeline->update();
		}
	}

	void mouseMoveEvent(QMouseEvent* ev) override
	{
		m_timeline->setCursor(QCursor(Qt::SplitHCursor));
		uint treeWidth = clamp_tpl<int>(ev->pos().x(), SPLITTER_WIDTH, m_timeline->width()) + m_offset;
		m_timeline->OnSplitterMoved(treeWidth);
		m_movedSlider = true;
	}
};

struct CTimeline::STreeMouseHandler : SMouseHandler
{
	QPoint     m_dragStartPos;
	CTimeline* m_timeline;
	bool       m_bIsDragging;
	bool       m_bClearSelectionOnMouseRelease;

	STreeMouseHandler(CTimeline* timeline)
		: m_timeline(timeline)
		, m_dragStartPos(QPoint(0, 0))
		, m_bIsDragging(false)
		, m_bClearSelectionOnMouseRelease(false)
	{
	}

	void mousePressEvent(QMouseEvent* ev) override
	{
		const bool bToggleSelected = (ev->modifiers() & Qt::CTRL) != 0;
		const bool bSelect = (ev->modifiers() & Qt::SHIFT) != 0;

		const int scroll = m_timeline->m_scrollBar ? m_timeline->m_scrollBar->value() : 0;
		const QPoint pos(ev->pos().x(), ev->pos().y() + scroll);

		STrackLayout* pTrackLayout = m_timeline->GetTrackLayoutFromPos(pos);

		if (!pTrackLayout)
		{
			if (!bSelect && !bToggleSelected)
			{
				ClearTrackSelection(m_timeline->m_pContent->track);
			}
		}
		else
		{
			const int left = TREE_LEFT_MARGIN + pTrackLayout->indent * TREE_INDENT_MULTIPLIER;
			const int right = left + TREE_BRANCH_INDICATOR_SIZE;

			const int x = pos.x();

			if (x >= left && x <= right)
			{
				m_timeline->OnTrackToggled(pos);
			}
			else
			{
				const bool bPreviousState = pTrackLayout->pTimelineTrack->selected;

				if (!bToggleSelected && !bPreviousState)
				{
					ClearTrackSelection(m_timeline->m_pContent->track);
				}
				else if (!bToggleSelected)
				{
					std::vector<STimelineTrack*> selectedTracks;
					GetSelectedTracks(m_timeline->m_pContent->track, selectedTracks);
					if (selectedTracks.size() > 1)
					{
						m_bClearSelectionOnMouseRelease = true;
					}
				}

				if (bToggleSelected)
				{
					pTrackLayout->pTimelineTrack->selected = !bPreviousState;
				}
				else if (bSelect)
				{
					STrackLayouts& tracks = m_timeline->m_layout->tracks;

					auto startFindIter = std::find_if(tracks.begin(), tracks.end(), [=](const STrackLayout& track) { return (pTrackLayout == &track); });
					auto endFindIter = std::find_if(tracks.begin(), tracks.end(), [=](const STrackLayout& track) { return (m_timeline->m_pLastSelectedTrack == &track); });

					if (startFindIter != tracks.end() && endFindIter != tracks.end())
					{
						if (startFindIter > endFindIter)
						{
							std::swap(startFindIter, endFindIter);
						}

						for (auto iter = startFindIter; iter <= endFindIter; ++iter)
						{
							iter->pTimelineTrack->selected = true;
						}
					}
				}
				else
				{
					pTrackLayout->pTimelineTrack->selected = true;
				}

				if (!bSelect && pTrackLayout->pTimelineTrack->selected)
				{
					m_timeline->m_pLastSelectedTrack = pTrackLayout;
				}
			}
		}

		m_timeline->SignalTrackSelectionChanged();

		if (ev->button() == Qt::LeftButton)
		{
			m_dragStartPos = pos;
		}
	}

	void mouseMoveEvent(QMouseEvent* ev) override
	{
		if ((ev->buttons() & Qt::LeftButton))
		{
			if (!m_bIsDragging && (ev->pos() - m_dragStartPos).manhattanLength() > QApplication::startDragDistance())
			{
				std::vector<STimelineTrack*> selectedTracks;
				GetSelectedTracks(m_timeline->m_pContent->track, selectedTracks);

				for (size_t i = 0; i < selectedTracks.size(); ++i)
				{
					m_timeline->SignalTracksBeginDrag(selectedTracks[i], m_bIsDragging);
				}
			}
		}
	}

	void mouseReleaseEvent(QMouseEvent* ev) override
	{
		if (m_bIsDragging)
		{
			m_bIsDragging = false;
			bool bHasValidTarget = false;
			const int scroll = m_timeline->m_scrollBar ? m_timeline->m_scrollBar->value() : 0;
			const QPoint pos(ev->pos().x(), ev->pos().y() + scroll);

			STrackLayout* pTrackLayout = m_timeline->GetTrackLayoutFromPos(pos);
			STimelineTrack* pTargetTrack = pTrackLayout ? pTrackLayout->pTimelineTrack : &m_timeline->m_pContent->track;

			if (pTargetTrack)
			{
				m_timeline->SignalTracksDragging(pTargetTrack, bHasValidTarget);
				if (bHasValidTarget)
				{
					m_timeline->SignalTracksDragged(pTargetTrack);
				}
			}
		}
		else if (m_bClearSelectionOnMouseRelease)
		{
			m_bClearSelectionOnMouseRelease = false;
			ClearTrackSelection(m_timeline->m_pContent->track);
			mousePressEvent(ev);
		}
	}

	void mouseDoubleClickEvent(QMouseEvent* ev) override
	{
		if (ev->modifiers() == 0)
		{
			m_timeline->OnTrackToggled(ev->pos());
		}
	}
};

// ---------------------------------------------------------------------------

CTimeline::CTimeline(QWidget* parent)
	: QWidget(parent)
	, m_cycled(true)
	, m_sizeToContent(false)
	, m_snapTime(false)
	, m_snapKeys(false)
	, m_scaleKeys(false)
	, m_treeVisible(false)
	, m_selIndicators(false)
	, m_verticalScrollbarVisible(false)
	, m_drawMarkers(false)
	, m_clampToViewOrigin(true)
	, m_allowOutOfRangeKeys(false)
	, m_useAdvancedRuler(false)
	, m_useMainTrackTimeRange(false)
	, m_layout(new STimelineLayout)
	, m_keySize(DEFAULT_KEY_SIZE)
	, m_cornerWidget(nullptr)
	, m_scrollBar(nullptr)
	, m_cornerWidgetWidth(0)
	, m_timelinePadding(120)
	, m_pContent(nullptr)
	, m_timeUnitScale(1.0f)
	, m_frameRate(SAnimTime::eFrameRate_30fps)
	, m_timeUnit(SAnimTime::EDisplayMode::Time)
	, m_time(0.0f)
	, m_splitterState(ESplitterState::Normal)
	, m_pLastSelectedTrack(nullptr)
	, m_mouseMenuStartPos(0, 0)
	, m_bShowKeyText(true)
	, m_pFilterLineEdit(nullptr)
	, m_searchWidget(nullptr)
	, m_updateTracksRenderCache(ERenderCacheUpdate::Once)
	, m_updateTreeRenderCache(ERenderCacheUpdate::Once)
{
	setMinimumWidth(MIN_WIDTH_TREE + MIN_WIDTH_SEQ);
	// TODO: We should force the parent to have a min width as well.

	setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
	setFocusPolicy(Qt::WheelFocus);
	setMouseTracking(true);

	m_viewState.visibleDistance = 1.0f;

	auto invalidateTracks = [this]()
	{
		InvalidateTracks();
	};

	QObject::connect(this, &CTimeline::SignalSelectionChanged, invalidateTracks);
	QObject::connect(this, &CTimeline::SignalViewOptionChanged, invalidateTracks);

	auto invalidateTreeTracksAndTimeMarkers = [this]()
	{
		InvalidateTree();
		InvalidateTracks();
		InvalidateTracksTimeMarkers();
	};

	QObject::connect(this, &CTimeline::SignalContentChanged, invalidateTreeTracksAndTimeMarkers);
	QObject::connect(this, &CTimeline::SignalZoom, invalidateTreeTracksAndTimeMarkers);
	QObject::connect(this, &CTimeline::SignalPan, invalidateTreeTracksAndTimeMarkers);
	QObject::connect(this, &CTimeline::SignalTimeUnitChanged, invalidateTreeTracksAndTimeMarkers);

	auto invalidateTreeAndTracks = [this]()
	{
		InvalidateTree();
		InvalidateTracks();
	};

	QObject::connect(this, &CTimeline::SignalTrackSelectionChanged, invalidateTreeAndTracks);
	QObject::connect(this, &CTimeline::SignalTracksBeginDrag, invalidateTreeAndTracks);
	QObject::connect(this, &CTimeline::SignalTracksDragged, invalidateTreeAndTracks);

	QObject::connect(this, &CTimeline::SignalUndo, invalidateTreeAndTracks);
	QObject::connect(this, &CTimeline::SignalRedo, invalidateTreeAndTracks);
	QObject::connect(this, &CTimeline::SignalPaste, invalidateTreeAndTracks);
	QObject::connect(this, &CTimeline::SignalTrackToggled, invalidateTreeAndTracks);
	QObject::connect(this, &CTimeline::SignalLayoutChanged, invalidateTreeAndTracks);

	setContextMenuPolicy(Qt::CustomContextMenu);
	QObject::connect(this, &QWidget::customContextMenuRequested, this, &CTimeline::OnContextMenu);

	m_highlightedTimer.setSingleShot(true);
	m_highlightedConnection = QObject::connect(&m_highlightedTimer, &QTimer::timeout, [this]() { UpdateHightlightedInternal(); });
	QObject::connect(this, &QObject::destroyed, [this]() { disconnect(m_highlightedConnection); });
}

CTimeline::~CTimeline()
{
}

void CTimeline::customEvent(QEvent* pEvent)
{
	if (pEvent->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(pEvent);
		const string& command = commandEvent->GetCommand();
		const QPoint mousePos = mapToGlobal(m_lastMouseMoveEventPos);

		if (command == "general.delete")
		{
			OnMenuDelete();
			commandEvent->setAccepted(true);
		}
		else if (command == "general.copy")
		{
			SignalCopy(GetTimeFromPos(mousePos), GetTrackFromPos(mousePos));
			commandEvent->setAccepted(true);
		}
		else if (command == "general.cut")
		{
			SignalCopy(GetTimeFromPos(mousePos), GetTrackFromPos(mousePos));//TODO !
			commandEvent->setAccepted(true);
		}
		else if (command == "general.paste")
		{
			SignalPaste(GetTimeFromPos(mousePos), GetTrackFromPos(mousePos));
			commandEvent->setAccepted(true);
		}
		else if (command == "general.undo")
		{
			//uses global undo, do not accept
			SignalUndo();
		}
		else if (command == "general.redo")
		{
			//uses global undo, do not accept
			SignalRedo();
		}
		else if (command == "general.duplicate")
		{
			OnMenuDuplicate();
			commandEvent->setAccepted(true);
		}
	}
	else
	{
		QWidget::customEvent(pEvent);
	}
}

void CTimeline::CalculateRulerMarkerTimes()
{
	m_tickTimePositions.clear();

	for (const DrawingPrimitives::CRuler::STick& tick : * m_tracksRenderCache.pTimeMarkers)
	{
		if (!tick.bIsOuterTick)
		{
			m_tickTimePositions.push_back(tick.value);
		}
	}
}

void CTimeline::paintEvent(QPaintEvent* ev)
{
#if ENABLE_PROFILING
	gEnv->pSystem->StartBootProfilerSessionFrames("TimelinePainting");
#endif
	LOADING_TIME_PROFILE_SECTION;

	if (m_viewState.widthPixels <= 0)
	{
		return;
	}

	const QPoint mousePos = mapFromGlobal(QCursor::pos());

	QPainter painter(this);
	painter.save();
	painter.translate(0.5f, 0.5f);

	UpdateScrollPixels(m_layout.get(), m_viewState);

	const int scrollOffset = m_scrollBar ? m_scrollBar->value() : 0;
	const QPoint localToLayoutTranslate = m_viewState.LayoutToLocal(QPoint(0, -scrollOffset));

	int rulerPrecision = 0;

	painter.translate(localToLayoutTranslate);
	painter.setRenderHint(QPainter::Antialiasing);

	const Range innerRange = Range(m_layout->minStartTime.ToFloat() * m_timeUnitScale, m_layout->maxEndTime.ToFloat() * m_timeUnitScale);
	const Range visibleRange = Range(m_viewState.LocalToTime(m_viewState.treeWidth) * m_timeUnitScale, m_viewState.LocalToTime(size().width()) * m_timeUnitScale);
	const QRect rulerRect = QRect(m_viewState.treeWidth, -1, size().width() - m_viewState.treeWidth, RULER_HEIGHT + 2);
	if (m_useAdvancedRuler)
	{
		DrawingPrimitives::CRuler::SOptions& options = m_timelineDrawer.GetOptions();
		options.markHeight = RULER_MARK_HEIGHT;
		options.rect = rulerRect;
		options.visibleRange = TRange<SAnimTime>(visibleRange.start, visibleRange.end);
		options.rulerRange = options.visibleRange;
		options.innerRange = TRange<SAnimTime>(innerRange.start, innerRange.end);
		;
		options.ticksPerFrame = SAnimTime::numTicksPerSecond / SAnimTime::GetFrameRateValue(m_frameRate);
		options.timeUnit = m_timeUnit;

		m_timelineDrawer.CalculateMarkers();
		CalculateRulerMarkerTimes();
	}

	if (m_updateTracksRenderCache == ERenderCacheUpdate::Once || m_updateTracksRenderCache == ERenderCacheUpdate::Continuous)
	{
		const int32 tracksWidth = width() - m_viewState.treeWidth - SPLITTER_WIDTH;
		if (!m_useAdvancedRuler && m_tracksRenderCache.timeMarkers.empty())
		{
			UpdateTracksTimeMarkers(width() - 1);
		}

		QRect trackRect(0, 0, tracksWidth, height());
		UpdateTracksRenderCache(m_tracksRenderCache, painter, *m_layout, m_viewState, trackRect, m_timeUnitScale, m_drawMarkers, m_bShowKeyText, scrollOffset);

		if (m_updateTracksRenderCache == ERenderCacheUpdate::Once)
		{
			m_updateTracksRenderCache = ERenderCacheUpdate::None;
		}
	}

	const QColor trackColor = GetStyleHelper()->timelineTrackColor();
	const QColor descriptionTrackColor = GetStyleHelper()->timelineDescriptionTrackColor();
	const QColor compositeTrackColor = GetStyleHelper()->timelineCompositeTrackColor();
	const QColor selectionColor = GetStyleHelper()->timelineSelectionColor();
	const QPen descriptionTextPen = QPen(GetStyleHelper()->timelineDescriptionTextColor());

	{
		LOADING_TIME_PROFILE_SECTION_NAMED("Draw calls")

		if (m_tracksRenderCache.descTrackBGRects.size())
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Desc tracks")

			const QBrush brush = QBrush(descriptionTrackColor);
			painter.setPen(Qt::NoPen);
			painter.setBrush(brush);
			PaintRenderCacheRects(painter, m_tracksRenderCache.descTrackBGRects);
		}

		if (m_tracksRenderCache.compTrackBGRects.size())
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Compound tracks")

			const QBrush brush = QBrush(compositeTrackColor);
			painter.setPen(Qt::NoPen);
			painter.setBrush(brush);
			PaintRenderCacheRects(painter, m_tracksRenderCache.compTrackBGRects);
		}

		if (m_tracksRenderCache.innerTickLines.size())
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Inner tick lines")

			painter.setPen(QPen(GetStyleHelper()->timelineInnerTickLines()));
			painter.setBrush(Qt::NoBrush);
			PaintRenderCacheLines(painter, m_tracksRenderCache.innerTickLines);
		}

		if (m_tracksRenderCache.outsideTrackBGRects.size())
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Background Track Area")

			const QBrush brush = QBrush(GetStyleHelper()->timelineOutsideTrackColor());
			painter.setPen(Qt::NoPen);
			painter.setBrush(brush);
			PaintRenderCacheRects(painter, m_tracksRenderCache.outsideTrackBGRects);
		}

		if (m_tracksRenderCache.bottomLines.size())
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Bottom lines")

			painter.setPen(QPen(GetStyleHelper()->timelineBottomLines()));
			painter.setBrush(Qt::NoBrush);
			PaintRenderCacheLines(painter, m_tracksRenderCache.bottomLines);
		}

		if (m_tracksRenderCache.seleTrackBGRects.size())
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Selected tracks")

			const QBrush brush = QBrush(selectionColor);
			painter.setPen(Qt::NoPen);
			painter.setBrush(brush);
			PaintRenderCacheRects(painter, m_tracksRenderCache.seleTrackBGRects);
		}

		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Text")

			painter.setPen(descriptionTextPen);
			for (const QRenderText& txt : m_tracksRenderCache.text)
			{
				painter.drawStaticText(QPoint(txt.rect.x(), txt.rect.y() + (txt.rect.height() - fontMetrics().height()) / 2), txt.text);
			}
		}

		if (m_tracksRenderCache.toggleRects.size())
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Toggle rects")

			painter.setBrush(QBrush(GetStyleHelper()->timelineToggleColor()));
			PaintRenderCacheRects(painter, m_tracksRenderCache.toggleRects);
		}

		if (m_tracksRenderCache.selectedClipRects.size())
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Selected clips")

			painter.setPen(QPen(hasFocus() ? GetStyleHelper()->timelineSelectedClipFocused() : GetStyleHelper()->timelineSelectedClip()));
			painter.setBrush(QBrush(hasFocus() ? GetStyleHelper()->timelineSelectedClipFocused() : GetStyleHelper()->timelineSelectedClip()));
			PaintRenderCacheRects(painter, m_tracksRenderCache.selectedClipRects);
		}

		if (m_tracksRenderCache.unclippedClipLines.size())
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Unclip")

			painter.setPen(QPen(GetStyleHelper()->timelineUnclipPen()));
			PaintRenderCacheLines(painter, m_tracksRenderCache.unclippedClipLines);
		}

		if (m_tracksRenderCache.defaultClipRects.size())
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Clips")

			painter.setPen(QPen(GetStyleHelper()->timelineClipPen()));
			painter.setBrush(QBrush(GetStyleHelper()->timelineClipBrush()));
			PaintRenderCacheRects(painter, m_tracksRenderCache.defaultClipRects);
		}

		if (!m_tracksRenderCache.defaultKeyIcons.empty())
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Keys")

			painter.setPen(Qt::NoPen);
			painter.setBrush(Qt::NoBrush);

			for (const QRenderIcon& renderIcon : m_tracksRenderCache.defaultKeyIcons)
			{
 				painter.drawPixmap(renderIcon.rect, renderIcon.icon);
			}
		}

		if (!m_tracksRenderCache.selectedKeyIcons.empty())
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Selected keys")

			painter.setPen(Qt::NoPen);
			painter.setBrush(Qt::NoBrush);

			for (const QRenderIcon& renderIcon : m_tracksRenderCache.selectedKeyIcons)
			{
				painter.drawPixmap(renderIcon.rect, renderIcon.icon);
			}
		}

		if (m_tracksRenderCache.disbTrackBGRects.size())
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Disabled tracks")

			const QBrush brush = QBrush(GetStyleHelper()->timelineDisabledColor());
			painter.setPen(Qt::NoPen);
			painter.setBrush(brush);
			PaintRenderCacheRects(painter, m_tracksRenderCache.disbTrackBGRects);
		}
	}

	painter.translate(-localToLayoutTranslate);

	if (!m_useAdvancedRuler)
	{
		DrawingPrimitives::SRulerOptions rulerOptions;
		rulerOptions.m_rect = rulerRect;
		rulerOptions.m_visibleRange = visibleRange;
		rulerOptions.m_rulerRange = visibleRange;
		rulerOptions.m_pInnerRange = &innerRange;
		rulerOptions.m_markHeight = RULER_MARK_HEIGHT;
		rulerOptions.m_ticksYOffset = 0;
		DrawingPrimitives::DrawRuler(painter, rulerOptions, &rulerPrecision);
	}
	else
	{
		m_timelineDrawer.Draw(painter, GetStyleHelper()->palette());
	}

	for (auto& element : m_headerElements)
	{
		if (element.visible && visibleRange.IsInside(element.time.ToFloat()))
		{
			QRect rect = rulerRect;
			int timePx = m_viewState.TimeToLocal(element.time.ToFloat());
			QPixmap pixmap(element.pixmap.c_str());

			if (element.alignment & Qt::AlignLeft)
			{
				rect.setRight(timePx);
				rect.setLeft(timePx - pixmap.size().width());
			}
			else if (element.alignment & Qt::AlignRight)
			{
				rect.setLeft(timePx);
				rect.setRight(timePx + pixmap.size().width());
			}
			else
			{
				rect.setLeft(timePx - pixmap.size().width() / 2);
				rect.setRight(timePx + pixmap.size().width() / 2);
			}

			if (element.alignment & Qt::AlignTop)
			{
				rect.setBottom(rect.top() + pixmap.size().height());
			}
			else if (element.alignment & Qt::AlignBottom)
			{
				rect.setTop(rect.bottom() - pixmap.size().height());
			}
			else
			{
				const int vmid = rect.top() + rect.height() / 2;
				rect.setTop(vmid - pixmap.size().height() / 2);
				rect.setBottom(vmid + pixmap.size().height() / 2);
			}

			painter.drawPixmap(rect, pixmap);
		}
	}

	if (m_pContent && isEnabled())
	{
		const float sliderPos = m_time.ToFloat() * m_timeUnitScale;
		if (sliderPos >= visibleRange.start && sliderPos <= visibleRange.end)
		{
			DrawingPrimitives::STimeSliderOptions timeSliderOptions;
			timeSliderOptions.m_rect = rect();
			timeSliderOptions.m_precision = rulerPrecision;
			timeSliderOptions.m_position = m_viewState.TimeToLocal(m_time.ToFloat());
			timeSliderOptions.m_time = m_time.ToFloat() * m_timeUnitScale;
			timeSliderOptions.m_bHasFocus = hasFocus();
			DrawingPrimitives::DrawTimeSlider(painter, timeSliderOptions);
		}

		DrawSelectionLines(painter, m_viewState, visibleRange, *m_pContent, rulerPrecision, width(), height(), m_time.ToFloat(), m_timeUnitScale, hasFocus());
	}

	painter.translate(localToLayoutTranslate);

	if (m_mouseHandler)
	{
		m_mouseHandler->paintOver(painter);
	}

	// draw highlighted after selection rectangle
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("Draw calls")

		if (!m_tracksRenderCache.highlightedKeyIcons.empty())
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Highlighted keys")

			painter.setPen(Qt::NoPen);
			painter.setBrush(Qt::NoBrush);

			for (const QRenderIcon& renderIcon : m_tracksRenderCache.highlightedKeyIcons)
			{
				painter.drawPixmap(renderIcon.rect, renderIcon.icon);
			}
		}
	}

    ///////////////////////////////////////////////////////////

	painter.translate(-localToLayoutTranslate);
	painter.restore();

	if (m_treeVisible)
	{
		if (m_pContent)
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Desc tree")

			int searchHeight = m_searchWidget ? m_searchWidget->height() + 1 : 0;
			QRect treeRect(0, searchHeight, m_viewState.treeWidth - SPLITTER_WIDTH + 1, height() - searchHeight);
			if (m_updateTreeRenderCache == ERenderCacheUpdate::Once || m_updateTreeRenderCache == ERenderCacheUpdate::Continuous)
			{
				UpdateTreeRenderCache(m_treeRenderCache, treeRect, this, *m_pContent, m_layout->tracks, m_viewState, scrollOffset);

				if (m_updateTreeRenderCache == ERenderCacheUpdate::Once)
				{
					m_updateTreeRenderCache = ERenderCacheUpdate::None;
				}
			}

			painter.save();

			painter.setClipRect(treeRect);
			painter.setClipping(true);

			painter.translate(0, -scrollOffset);

			QTextOption textOption;
			textOption.setWrapMode(QTextOption::NoWrap);

			QStyleOptionFrame opt;
			opt.palette = GetStyleHelper()->palette();
			opt.state = QStyle::State_Enabled;
			opt.rect = QRect(treeRect.left(), treeRect.top() - 1, treeRect.width(), treeRect.height() + 2);
			style()->drawPrimitive(QStyle::PE_Frame, &opt, &painter, this);

			if (m_treeRenderCache.descTrackBGRects.size())
			{
				LOADING_TIME_PROFILE_SECTION_NAMED("Desc tree tracks")

				const QBrush brush = QBrush(descriptionTrackColor);
				painter.setPen(Qt::NoPen);
				painter.setBrush(brush);
				PaintRenderCacheRects(painter, m_treeRenderCache.descTrackBGRects);
			}

			if (m_treeRenderCache.compTrackBGRects.size())
			{
				LOADING_TIME_PROFILE_SECTION_NAMED("Compound tree tracks")

				const QBrush brush = QBrush(compositeTrackColor);
				painter.setPen(Qt::NoPen);
				painter.setBrush(brush);
				PaintRenderCacheRects(painter, m_treeRenderCache.compTrackBGRects);
			}

			if (m_treeRenderCache.seleTrackBGRects.size())
			{
				LOADING_TIME_PROFILE_SECTION_NAMED("Selected tree tracks")

				const QBrush brush = QBrush(selectionColor);
				painter.setPen(Qt::NoPen);
				painter.setBrush(brush);
				PaintRenderCacheRects(painter, m_treeRenderCache.seleTrackBGRects);
			}

			if (m_treeRenderCache.trackLines.size())
			{
				LOADING_TIME_PROFILE_SECTION_NAMED("Tree lines")

				painter.setPen(QPen(GetStyleHelper()->timelineTreeLines()));
				PaintRenderCacheLines(painter, m_treeRenderCache.trackLines);
			}

			if (m_treeRenderCache.text.size())
			{
				LOADING_TIME_PROFILE_SECTION_NAMED("Tree text")
				painter.setPen(QPen(GetStyleHelper()->timelineTreeText()));

				for (const QRenderText& txt : m_treeRenderCache.text)
				{
					painter.drawStaticText(QPoint(txt.rect.x(), txt.rect.y() + (txt.rect.height() - fontMetrics().height()) / 2), txt.text);
				}
			}

			if (m_treeRenderCache.primitives.size())
			{
				LOADING_TIME_PROFILE_SECTION_NAMED("Tree primitives")

				for (const QStyleOptionViewItem& styleOption : m_treeRenderCache.primitives)
				{
					style()->drawPrimitive(QStyle::PE_IndicatorBranch, &styleOption, &painter, this);
				}
			}

			if (m_treeRenderCache.icons.size())
			{
				LOADING_TIME_PROFILE_SECTION_NAMED("Tree icons")

				for (const QRenderIcon& icon : m_treeRenderCache.icons)
				{
					painter.drawPixmap(icon.rect, icon.icon);
				}
			}

			if (m_tracksRenderCache.disbTrackBGRects.size())
			{
				LOADING_TIME_PROFILE_SECTION_NAMED("Disabled tracks")

				const QBrush brush = QBrush(GetStyleHelper()->timelineDisabledColor());
				painter.setPen(Qt::NoPen);
				painter.setBrush(brush);
				PaintRenderCacheRects(painter, m_tracksRenderCache.disbTrackBGRects);
			}

			painter.restore();
		}

		const QRect splitterRect(m_viewState.treeWidth - SPLITTER_WIDTH, 0, SPLITTER_WIDTH, height());
		QColor color;
		switch (m_splitterState)
		{
		default:
		case ESplitterState::Normal:
			color = GetStyleHelper()->timelineSplitterNormal();
			break;
		case ESplitterState::Selected:
			color = GetStyleHelper()->timelineSplitterSelected();
			break;
		case ESplitterState::Moving:
			color = GetStyleHelper()->timelineSplitterMoving();
			break;
		}

		painter.fillRect(splitterRect, color);
	}

#if ENABLE_PROFILING
	gEnv->pSystem->StopBootProfilerSessionFrames();
#endif
}

void CTimeline::UpdateHightlightedInternal()
{
	using UpdateFunc = std::function<void(const STrackLayout&, const SElementLayoutPtrs&, bool&)>;
	static UpdateFunc UpdateHighlightedElementsRecursevely = [&](const STrackLayout& trackLayout, const SElementLayoutPtrs& highlightedElements, bool& highlightedStateChanged)
	{
		for (auto& element : trackLayout.elements)
		{
			bool isHighlighted = element.IsHighlighted();
			bool inHighlightedElements = std::find(std::begin(highlightedElements), std::end(highlightedElements), &element) != std::end(highlightedElements);
			bool stateChanged  = (inHighlightedElements && (!isHighlighted)) || ((!inHighlightedElements) && isHighlighted);

			highlightedStateChanged = highlightedStateChanged || stateChanged;
			element.SetHighlighted(inHighlightedElements);
		}

		for (auto& track : trackLayout.tracks)
		{
			UpdateHighlightedElementsRecursevely(track, highlightedElements, highlightedStateChanged);
		}
	};

	bool higlightedStateChanged = false;
	for (auto& track : m_layout->tracks)
	{
		UpdateHighlightedElementsRecursevely(track, m_highlightedElements, higlightedStateChanged);
	}

	if (higlightedStateChanged)
	{
		InvalidateTracks();
		repaint();
	}

	m_highlightedElements.clear();
}

void CTimeline::UpdateTracksTimeMarkers(int32 width)
{
	const Range rulerRange = Range(m_layout->minStartTime.ToFloat() * m_timeUnitScale, m_layout->maxEndTime.ToFloat() * m_timeUnitScale);
	const Range visibleRange = Range(m_viewState.LocalToTime(m_viewState.treeWidth) * m_timeUnitScale, m_viewState.LocalToTime(width) * m_timeUnitScale);

	const QRect rect = QRect(-m_viewState.scrollPixels.x(), 0, width - m_viewState.treeWidth, 0);

	DrawingPrimitives::CalculateTicks(rect.width(), visibleRange, visibleRange, nullptr, nullptr, m_tracksRenderCache.timeMarkers, &rulerRange);
}

void CTimeline::ClampViewOrigin(bool force)
{
	if (force || m_clampToViewOrigin)
	{
		float zoomOffset = m_viewState.visibleDistance * 0.5f;

		const float padding = m_viewState.LayoutToTime(m_timelinePadding);
		float maxViewOrigin = m_layout->minStartTime.ToFloat() - zoomOffset + padding;
		float minViewOrigin = std::min(m_viewState.visibleDistance - m_layout->maxEndTime.ToFloat() - zoomOffset - padding, maxViewOrigin);

		m_viewState.viewOrigin = clamp_tpl(m_viewState.viewOrigin, minViewOrigin, maxViewOrigin);
	}
}

void CTimeline::keyPressEvent(QKeyEvent* ev)
{
	//TODO : figure out why were are doing this, it should not be necessary
	const QPoint mousePos = mapFromGlobal(QCursor::pos());
	QMouseEvent mouseEvent(QEvent::MouseMove, mousePos, Qt::NoButton, Qt::NoButton, ev->modifiers());
	mouseMoveEvent(&mouseEvent);
	int k = ev->key() | ev->modifiers();
	ev->setAccepted(HandleKeyEvent(k));
}

void CTimeline::keyReleaseEvent(QKeyEvent* ev)
{
	//TODO : figure out why were are doing this, it should not be necessary
	const QPoint mousePos = mapFromGlobal(QCursor::pos());
	QMouseEvent mouseEvent(QEvent::MouseMove, mousePos, Qt::NoButton, Qt::NoButton, ev->modifiers());
	mouseMoveEvent(&mouseEvent);
}

bool CTimeline::HandleKeyEvent(int k)
{
	QKeySequence key(k);
	const QPoint mousePos = QCursor::pos();

	if (key == QKeySequence(Qt::Key_Home))
	{
		m_time = SAnimTime(0);
		update();
		SignalScrub(false);
		return true;
	}
	if (key == QKeySequence(Qt::Key_End))
	{
		SAnimTime endTime = SAnimTime(0);
		for (size_t i = 0; i < m_pContent->track.tracks.size(); ++i)
		{
			endTime = std::max(endTime, m_pContent->track.tracks[i]->endTime);
		}
		m_time = endTime;
		update();
		SignalScrub(false);
		return true;
	}
	if (key == QKeySequence(Qt::Key_X) || key == QKeySequence(Qt::Key_PageUp))
	{
		OnMenuPreviousKey();
		return true;
	}
	if (key == QKeySequence(Qt::Key_C) || key == QKeySequence(Qt::Key_PageDown))
	{
		OnMenuNextKey();
		return true;
	}

	if (k == Qt::Key_Comma || k == Qt::Key_Left)
	{
		OnMenuPreviousFrame();
		return true;
	}

	if (k == Qt::Key_Period || k == Qt::Key_Right)
	{
		OnMenuNextFrame();
		return true;
	}

	if (key == QKeySequence(Qt::Key_Space))
	{
		OnMenuPlay();
		return true;
	}

	if (k >= Qt::Key_0 && k <= Qt::Key_9)
	{
		int number = k - int(Qt::Key_0);
		SignalNumberHotkey(number);
		return true;
	}
	return false;
}

void CTimeline::mousePressEvent(QMouseEvent* ev)
{
	setFocus();

	const bool bInTreeArea = m_treeVisible && (ev->x() <= m_viewState.treeWidth);

	const int scroll = m_scrollBar ? m_scrollBar->value() : 0;
	m_lastMousePressEventPos = QPoint(ev->pos().x(), ev->pos().y() + scroll);

	if (ev->button() == Qt::LeftButton)
	{
		QPoint posInLayout = m_viewState.LocalToLayout(ev->pos());

		if (bInTreeArea)
		{
			const bool bOverSplitter = ev->x() >= (m_viewState.treeWidth - SPLITTER_WIDTH);

			if (bOverSplitter)
			{
				m_mouseHandler.reset(new SSplitterHandler(this));
				m_mouseHandler->mousePressEvent(ev);
				update();
			}
			else
			{
				m_mouseHandler.reset(new STreeMouseHandler(this));
				m_mouseHandler->mousePressEvent(ev);
				update();
			}
		}
		else if (ev->y() < RULER_HEIGHT)
		{
			//TODO : if selecting a timeline header element, create another mouse handler for them.

			m_mouseHandler.reset(new SScrubHandler(this));
			m_mouseHandler->mousePressEvent(ev);
			update();
		}
		else
		{
			QPoint posInLayoutSpace = m_viewState.LocalToLayout(ev->pos());
			if (m_scrollBar)
			{
				posInLayoutSpace.setY(posInLayoutSpace.y() + m_scrollBar->value());
			}

			bool bHit = false;
			SElementLayoutPtrs hitElements;
			if (STrackLayout* pTrackLayout = GetTrackLayoutFromPos(posInLayoutSpace))
			{
				bHit = HitTestElements(*pTrackLayout, QRect(posInLayoutSpace - QPoint(2, 2), posInLayoutSpace + QPoint(2, 2)), hitElements);
			}

			if (ev->modifiers() & Qt::SHIFT || ev->modifiers() & Qt::CTRL || ev->modifiers() & Qt::ALT)
			{
				if (bHit)
				{
					if (ev->modifiers() & Qt::CTRL)
					{
						hitElements.back()->SetSelected(!hitElements.back()->IsSelected());
					}
					else if (ev->modifiers() & Qt::SHIFT)
					{
						hitElements.back()->SetSelected(true);
					}
					else if (ev->modifiers() & Qt::ALT)
					{
						hitElements.back()->SetSelected(false);
					}

					m_mouseHandler.reset(new SShiftHandler(this, false));
					m_mouseHandler->mousePressEvent(ev);
					update();
				}
				else
				{
					m_mouseHandler.reset(new SSelectionHandler(this, true));
					m_mouseHandler->mousePressEvent(ev);
				}
			}
			else
			{
				if (bHit)
				{
					bool useExistingSelection = std::any_of(hitElements.begin(), hitElements.end(), [](const SElementLayout* element) { return element->IsSelected(); });

					if (!useExistingSelection)
					{
						const TSelectedElements selectedElements = GetSelectedElements(m_pContent->track);

						ClearElementSelection(m_pContent->track);
						hitElements.back()->SetSelected(true);

						if (selectedElements != GetSelectedElements(m_pContent->track))
						{
							SignalSelectionChanged(false);
						}
					}
					else
					{
						SignalSelectionRefresh();
					}

					bool cycleSelection = useExistingSelection;
					if ((ev->modifiers() & Qt::AltModifier) || m_scaleKeys)
						m_mouseHandler.reset(new SScaleHandler(this, cycleSelection));
					else
						m_mouseHandler.reset(new SShiftHandler(this, cycleSelection));

					m_mouseHandler->mousePressEvent(ev);
					update();
				}
				else
				{
					const TSelectedElements selectedElements = GetSelectedElements(m_pContent->track);
					if (!selectedElements.empty())
					{
						ClearElementSelection(m_pContent->track);
						SignalSelectionChanged(false);
					}

					m_mouseHandler.reset(new SSelectionHandler(this, false));
					m_mouseHandler->mousePressEvent(ev);
					update();
				}
			}
		}
	}
	else if (ev->button() == Qt::MiddleButton)
	{
		if (!bInTreeArea)
		{
			m_mouseHandler.reset(new SPanHandler(this));
			m_mouseHandler->mousePressEvent(ev);
			update();
		}
	}
}

void CTimeline::AddKeyToTrack(STimelineTrack& track, SAnimTime time)
{
	if (m_snapKeys || m_snapTime)
	{
		time = time.SnapToNearest(m_frameRate);
	}

	track.modified = true;
	track.elements.push_back(track.defaultElement);
	track.elements.back().added = true;
	SAnimTime length = track.defaultElement.end - track.defaultElement.start;
	track.elements.back().start = time;
	track.elements.back().end = length;
	track.elements.back().selected = true;
}

void CTimeline::mouseDoubleClickEvent(QMouseEvent* ev)
{
	if (ev->button() == Qt::LeftButton)
	{
		const bool bInTreeArea = m_treeVisible && (ev->x() <= m_viewState.treeWidth);
		const int scroll = m_scrollBar ? m_scrollBar->value() : 0;
		m_lastMousePressEventPos = QPoint(ev->pos().x(), ev->pos().y() + scroll);

		if (bInTreeArea)
		{
			const bool bOverSplitter = ev->x() >= (m_viewState.treeWidth - SPLITTER_WIDTH);

			if (!bOverSplitter)
			{
				m_mouseHandler.reset(new STreeMouseHandler(this));
				m_mouseHandler->mouseDoubleClickEvent(ev);
				update();
			}
		}

		QPoint layoutPoint = m_viewState.LocalToLayout(m_lastMousePressEventPos);
		STrackLayout* track = HitTestTrack(m_layout->tracks, layoutPoint, m_allowOutOfRangeKeys);

		if (track)
		{
			SElementLayoutPtrs hitElements;
			const bool bHit = HitTestElements(m_layout->tracks, QRect(layoutPoint - QPoint(2, 2), layoutPoint + QPoint(2, 2)), hitElements);

			if (!bHit)
			{
				float time = m_viewState.LayoutToTime(layoutPoint.x());
				STimelineTrack& timelineTrack = *track->pTimelineTrack;

				if ((timelineTrack.caps & STimelineTrack::CAP_COMPOUND_TRACK) != 0)
				{
					const size_t numSubTracks = timelineTrack.tracks.size();
					for (size_t i = 0; i < numSubTracks; ++i)
					{
						STimelineTrack& subTrack = *timelineTrack.tracks[i];
						AddKeyToTrack(subTrack, SAnimTime(time));
					}
				}
				else if ((timelineTrack.caps & STimelineTrack::CAP_DESCRIPTION_TRACK) == 0)
				{
					AddKeyToTrack(timelineTrack, SAnimTime(time));
				}
				else
				{
					return;
				}

				ContentChanged(false);
				m_mouseHandler.reset();
				mouseMoveEvent(ev);
			}
		}
	}
}

void CTimeline::UpdateCursor(QMouseEvent* ev, const SElementLayoutPtrs& hitElements)
{
	const bool bOverSelected = !hitElements.empty() && hitElements.back()->IsSelected();
	const bool bInTreeArea = m_treeVisible && (ev->x() <= m_viewState.treeWidth);

	bool shift = ev->modifiers().testFlag(Qt::ShiftModifier);
	bool control = ev->modifiers().testFlag(Qt::ControlModifier);

	ESplitterState splitterState = m_splitterState;

	if (m_mouseHandler)
	{
		m_mouseHandler->mouseMoveEvent(ev);
		update();
	}
	else if (m_treeVisible && (ev->x() <= m_viewState.treeWidth) && (ev->x() >= (m_viewState.treeWidth - SPLITTER_WIDTH)))
	{
		splitterState = ESplitterState::Selected;
		setCursor(QCursor(Qt::SplitHCursor));
	}
	else if (!bInTreeArea && bOverSelected && !(shift || control))
	{
		splitterState = ESplitterState::Normal;
		setCursor(QCursor(Qt::SizeHorCursor));
	}
	else
	{
		splitterState = ESplitterState::Normal;
		setCursor(QCursor());
	}

	if (m_splitterState != splitterState)
	{
		m_splitterState = splitterState;
		update();
	}
}

void CTimeline::UpdateHighligted(const SElementLayoutPtrs& hitElements)
{    
	for (auto& element : hitElements)
	{
		bool inHighlightedElements = std::find(std::begin(m_highlightedElements), std::end(m_highlightedElements), element) != std::end(m_highlightedElements);
		if (!inHighlightedElements)
		{
			m_highlightedElements.push_back(element);
		}
	}

	m_highlightedTimer.start(0);
}

void CTimeline::mouseMoveEvent(QMouseEvent* ev)
{
	m_lastMouseMoveEventPos = ev->pos();

	const int scroll = m_scrollBar ? m_scrollBar->value() : 0;
	const QPoint pos(ev->pos().x(), ev->pos().y() + scroll);

	SElementLayoutPtrs hitElements;
	ResolveHitElements(pos, hitElements);

	UpdateCursor(ev, hitElements);
	UpdateHighligted(hitElements);
}

void CTimeline::mouseReleaseEvent(QMouseEvent* ev)
{
	if (ev->button() == Qt::LeftButton || ev->button() == Qt::MiddleButton)
	{
		if (m_mouseHandler.get())
		{
			m_mouseHandler->mouseReleaseEvent(ev);
			m_mouseHandler.reset();
			update();
		}
	}

	const int scroll = m_scrollBar ? m_scrollBar->value() : 0;
	const QPoint pos(ev->pos().x(), ev->pos().y() + scroll);

	SElementLayoutPtrs hitElements;

	ResolveHitElements(pos, hitElements);
	UpdateCursor(ev, hitElements);
}

void CTimeline::focusOutEvent(QFocusEvent* ev)
{
	if (m_mouseHandler.get())
	{
		m_mouseHandler->focusOutEvent(ev);
		m_mouseHandler.reset();
	}

	update();
}

void CTimeline::wheelEvent(QWheelEvent* ev)
{
	const bool bInTreeArea = m_treeVisible && (ev->x() <= m_viewState.treeWidth);

	//Only zoom in with alt, scroll with normal mousewheel
	if (m_scrollBar && (bInTreeArea || ev->modifiers() & (Qt::AltModifier | Qt::ControlModifier)))
	{
		//Not using QApplication::sendEvent() to avoid recursion
		//If the scrollbar is at the end, it will not handle the event which will land in this method again due to hierarchy
		m_scrollBar->event(ev);
	}
	else
	{
		int pixelDelta = ev->pixelDelta().manhattanLength();

		if (pixelDelta == 0)
		{
			pixelDelta = ev->delta();
		}

		float currentTime = m_viewState.LocalToTime(ev->x());
		ZoomContinuous((float)pixelDelta);
		float newTime = m_viewState.LocalToTime(ev->x());

		m_viewState.viewOrigin += newTime - currentTime;
		UpdateLayout();
		update();
		SignalPan();
	}
}

void CTimeline::ZoomContinuous(float delta)
{
	if (delta != 0)
	{
		const float step = std::abs(delta) / 100.0f;
		if (0 < delta)
			m_viewState.visibleDistance /= step;
		else
			m_viewState.visibleDistance *= step;

		if (m_timeUnit == SAnimTime::EDisplayMode::Frames)
		{
			m_viewState.visibleDistance = max(m_viewState.visibleDistance, 0.35f);
		}

		ClampVisibleDistanceToTotalDuration(m_viewState, m_layout.get(), m_timelinePadding);

		UpdateLayout();
		update();
		SignalZoom();
	}
}

void CTimeline::ZoomStep(int steps)
{
	ZoomContinuous(float(120 * steps));
}

QSize CTimeline::sizeHint() const
{
	return QSize(m_layout->size);
}

void CTimeline::focusTrack(STimelineTrack* pTrack)
{
	std::vector<STimelineTrack*> parents;
	ForEachTrack(m_pContent->track, parents, [&](STimelineTrack& track)
	{
		track.selected = (pTrack == &track);

		if (track.selected)
		{
		  for (int i = 0; i < parents.size(); ++i)
		  {
		    STimelineTrack* pParentTrack = parents[i];
		    pParentTrack->expanded = true;

		    track.expanded = true;
		  }
		}
	});

	UpdateLayout();

	if (m_scrollBar)
	{
		for (STrackLayout& layout : m_layout->tracks)
		{
			if (layout.pTimelineTrack == pTrack)
			{
				m_scrollBar->setValue(layout.rect.top() - (RULER_HEIGHT + VERTICAL_PADDING));
				break;
			}
		}
	}

	update();
}

void CTimeline::resizeEvent(QResizeEvent* ev)
{
	UpdateLayout();
}

SAnimTime CTimeline::ClampAndSnapTime(SAnimTime time, bool snapToFrames) const
{
	SAnimTime minTime = m_layout->minStartTime;
	SAnimTime maxTime = m_layout->maxEndTime;
	SAnimTime unclampedTime = time;
	SAnimTime deltaTime = maxTime - minTime;

	if (m_cycled)
	{
		while (unclampedTime < minTime)
		{
			unclampedTime += deltaTime;
		}

		unclampedTime = ((unclampedTime - minTime) % deltaTime) + minTime;
	}

	SAnimTime clampedTime = clamp_tpl(unclampedTime, minTime, maxTime);

	if (!snapToFrames)
	{
		return clampedTime;
	}
	else
	{
		return clampedTime.SnapToNearest(m_frameRate);
	}
}

void CTimeline::ClampAndSetTime(SAnimTime time, bool scrubThrough)
{
	SAnimTime newTime = ClampAndSnapTime(time, m_snapTime);

	if (newTime != m_time)
	{
		m_time = newTime;
		UpdateLayout();
		update();
		SignalScrub(scrubThrough);
	}
}

void CTimeline::SetTimeUnitScale(float scale)
{
	m_timeUnitScale = scale;
	update();
}

void CTimeline::SetTime(SAnimTime time)
{
	m_time = time;
	update();
}

void CTimeline::SetCycled(bool cycled)
{
	m_cycled = cycled;
}

void CTimeline::SetContent(STimelineContent* pContent)
{
	m_pContent = pContent;
	ForEachTrack(m_pContent->track, [&](STimelineTrack& track)
	{
		UpdateTruncatedDurations(&track);
	}
	             );

	UpdateLayout(true);
	update();
}

void CTimeline::ContentUpdated()
{
	UpdateLayout();
	InvalidateTree();
	InvalidateTracks();
	update();
}

void CTimeline::ShowKeyText(bool bShow)
{
	if (m_bShowKeyText != bShow)
	{
		m_bShowKeyText = bShow;
		SignalViewOptionChanged();
		update();
	}
}

void CTimeline::UpdateLayout(bool forceClamp)
{
	m_layout->tracks.clear();

	QWidget* pParent = static_cast<QWidget*>(parent());
	m_viewState.widthPixels = width();

	if (m_treeVisible)
	{
		m_viewState.widthPixels -= m_viewState.treeWidth;
		if (m_viewState.widthPixels <= 0)
		{
			return;
		}
	}

	if (m_verticalScrollbarVisible)
	{
		if (!m_scrollBar)
		{
			m_scrollBar = new QScrollBar(Qt::Vertical, this);
			connect(m_scrollBar, SIGNAL(valueChanged(int)), this, SLOT(OnVerticalScroll(int)));
		}

		const uint scrollbarWidth = style()->pixelMetric(QStyle::PM_ScrollBarExtent, 0, this);
		m_scrollBar->setGeometry(width() - scrollbarWidth, 0, scrollbarWidth, height());

		m_viewState.widthPixels -= scrollbarWidth;
		if (m_viewState.widthPixels <= 0)
		{
			return;
		}
	}
	else if (!m_verticalScrollbarVisible && m_scrollBar)
	{
		SAFE_DELETE(m_scrollBar);
	}

	if (m_pContent)
	{
		ForEachTrack(m_pContent->track, [&](STimelineTrack& track)
		{
			if (track.caps & track.CAP_CLIP_TRUNCATED_BY_NEXT_KEY)
				UpdateTruncatedDurations(&track);
		}
		             );

		CalculateLayout(m_layout.get(), *m_pContent, m_viewState, m_pFilterLineEdit, m_time.ToFloat(), m_keySize, m_treeVisible, m_useMainTrackTimeRange);
		ApplyPushOut(m_layout.get(), m_keySize);
	}

	ClampViewOrigin(forceClamp); // has to be after updated after the viewstate/layout
	ClampVisibleDistanceToTotalDuration(m_viewState, m_layout.get(), m_timelinePadding);

	if (m_scrollBar)
	{
		const int timelineTrackCount = m_layout->tracks.size();
		const int timelineHeight = rect().height();
		const int scrollBarRange = m_layout->size.height() - timelineHeight;
		const int singleStep = scrollBarRange / (timelineTrackCount > 0 ? timelineTrackCount : 1);
		m_scrollBar->setSingleStep(singleStep);
		m_scrollBar->setPageStep(singleStep * 20);

		if (scrollBarRange > 0)
		{
			m_scrollBar->setRange(0, scrollBarRange);
			m_scrollBar->show();
		}
		else
		{
			m_scrollBar->setValue(0);
			m_scrollBar->hide();
		}
	}

	if (m_sizeToContent)
	{
		setMaximumHeight(m_layout->size.height());
		setMinimumHeight(m_layout->size.height());
	}
	else
	{
		setMinimumHeight(RULER_HEIGHT + 1);
		setMaximumHeight(QWIDGETSIZE_MAX);
	}

	if (m_treeVisible)
	{
		int searchAreaWidth = m_viewState.treeWidth - SPLITTER_WIDTH - m_cornerWidgetWidth;
		int searchAreaHeight = RULER_HEIGHT + VERTICAL_PADDING;

		if (!m_pFilterLineEdit)
		{
			if (!m_searchWidget)
				m_searchWidget = new QWidget(this);

			m_searchWidget->setAutoFillBackground(true);

			QPalette searchBKGColor(palette());
			searchBKGColor.setColor(QPalette::Background, GetStyleHelper()->timelineOutsideTrackColor());

			auto searchBox = new QSearchBox(m_searchWidget);
			searchBox->setParent(m_searchWidget);
			searchBox->setAutoFillBackground(true);
			searchBox->setPalette(searchBKGColor);
			searchBox->EnableContinuousSearch(true);
			searchBox->SetSearchFunction(std::function<void(const QString&)>([this](const QString&) { OnFilterChanged(); }));

			m_pFilterLineEdit = searchBox;

			if (m_cornerWidget)
			{
				m_cornerWidget->setPalette(searchBKGColor);
				m_cornerWidget->setAutoFillBackground(true);
			}
		}

		if (m_pFilterLineEdit)
		{
			m_searchWidget->resize(searchAreaWidth, searchAreaHeight);
			m_pFilterLineEdit->resize(searchAreaWidth, searchAreaHeight);
		}

		const uint cornerWidgetWidth = m_cornerWidget ? m_cornerWidgetWidth : 0;

		if (m_cornerWidget)
		{
			m_cornerWidget->setGeometry(searchAreaWidth, 0, cornerWidgetWidth, searchAreaHeight);
		}
	}
	else if (!m_treeVisible && m_pFilterLineEdit)
	{
		if (m_searchWidget)
		{
			SAFE_DELETE(m_searchWidget);
		}

		SAFE_DELETE(m_pFilterLineEdit);
	}

	UpdateScrollPixels(m_layout.get(), m_viewState);
}

void CTimeline::SetSizeToContent(bool sizeToContent)
{
	m_sizeToContent = sizeToContent;

	UpdateLayout();
}

void CTimeline::ContentChanged(bool continuous)
{
	SignalContentChanged(continuous);

	DeletedMarkedElements(m_pContent->track);

	if (!continuous)
	{
		ForEachElement(m_pContent->track, [](STimelineTrack& track, STimelineElement& element)
		{
			track.modified = false;
			element.added = false;
		});
	}

	UpdateLayout();
	update();
}

void CTimeline::OnMenuSelectionToCursor()
{
	TSelectedElements elements = GetSelectedElements(m_pContent->track);

	for (size_t i = 0; i < elements.size(); ++i)
	{
		STimelineTrack& track = *elements[i].first;
		STimelineElement& element = *elements[i].second;
		SAnimTime length = element.end - element.start;
		element.start = m_time;
		element.end = element.start + length;
		if (element.type == element.CLIP)
			if (length > track.endTime)
				element.start = track.endTime - length;
		if (element.start < track.startTime)
			element.start = track.startTime;
	}

	ContentChanged(false);
}

void CTimeline::OnMenuDuplicate()
{
	TSelectedElements selectedElements = GetSelectedElements(m_pContent->track);
	if (selectedElements.empty())
		return;

	typedef std::vector<std::pair<STimelineTrack*, STimelineElement>> TTrackElements;

	TTrackElements elements;

	ForEachElement(m_pContent->track, [&](STimelineTrack& track, STimelineElement& element)
	{
		if (element.selected)
		{
		  elements.push_back(std::make_pair(&track, element));
		  element.selected = false;
		}
	});

	for (size_t i = 0; i < elements.size(); ++i)
	{
		STimelineTrack* track = elements[i].first;
		const STimelineElement& element = elements[i].second;
		track->elements.push_back(element);
		STimelineElement& e = track->elements.back();
		e.userId = 0;
		e.added = true;
		e.sideLoadChanged = true;
		e.selected = true;
	}

	ContentChanged(false);
	SignalSelectionChanged(false);
}

void CTimeline::OnMenuCopy()
{
	SignalCopy(GetTimeFromPos(m_mouseMenuStartPos), GetTrackFromPos(m_mouseMenuStartPos));
}

void CTimeline::OnMenuCut()
{
	//TODO :!
}

void CTimeline::OnMenuPaste()
{
	SignalPaste(GetTimeFromPos(m_mouseMenuStartPos), GetTrackFromPos(m_mouseMenuStartPos));
}

void CTimeline::OnMenuDelete()
{
	ForEachElement(m_pContent->track, [](STimelineTrack& track, STimelineElement& element)
	{
		if (element.selected)
		{
		  track.modified = true;
		  element.deleted = true;
		}
	});

	ContentChanged(false);
}

void CTimeline::OnMenuPlay()
{
	SignalPlay();
}

typedef std::vector<std::pair<SAnimTime, STimelineContentElementRef>> TimeToId;
static void GetAllTimes(TimeToId* times, const STimelineTrack& track)
{
	for (size_t i = 0; i < track.elements.size(); ++i)
	{
		const STimelineElement& element = track.elements[i];

	}

	for (size_t i = 0; i < track.tracks.size(); ++i)
	{
		GetAllTimes(times, *track.tracks[i]);
	}
}

static void GetAllTimes(TimeToId* times, STimelineContent& content)
{
	ForEachTrack(content.track, [&](STimelineTrack& track)
	{
		times->push_back(std::make_pair(track.startTime, STimelineContentElementRef()));
		times->push_back(std::make_pair(track.endTime, STimelineContentElementRef()));
	});

	ForEachElementWithIndex(content.track, [=](STimelineTrack& track, STimelineElement& element, size_t i)
	{
		STimelineContentElementRef ref(&track, i);
		times->push_back(std::make_pair(element.start, ref));
		if (element.type == STimelineElement::CLIP)
			times->push_back(std::make_pair(element.end, ref));
	});

	std::sort(times->begin(), times->end());
}

static STimelineContentElementRef SelectedIdAtTime(const std::vector<STimelineContentElementRef>& selection, const STimelineContent& content, SAnimTime time)
{
	for (size_t i = 0; i < selection.size(); ++i)
	{
		const STimelineContentElementRef& id = selection[i];
		const STimelineElement& element = id.GetElement();
		if (element.start == time || element.end == time)
			return id;
	}
	return STimelineContentElementRef();
}

void CTimeline::OnMenuPreviousKey()
{
	if (!m_pContent)
		return;
	TimeToId times;
	GetAllTimes(&times, *m_pContent);

	std::vector<STimelineContentElementRef> selection;
	ForEachElementWithIndex(m_pContent->track, [&](STimelineTrack& t, STimelineElement& e, size_t i) { if (e.selected) selection.push_back(STimelineContentElementRef(&t, i)); });

	STimelineContentElementRef selectedId = SelectedIdAtTime(selection, *m_pContent, m_time);

	TimeToId::iterator it = std::lower_bound(times.begin(), times.end(), std::make_pair(m_time, selectedId));
	if (it != times.end())
	{
		if (it != times.begin())
			--it;

		ClearElementSelection(m_pContent->track);
		if (it->second.IsValid())
			it->second.GetElement().selected = true;

		m_time = it->first;

		SignalSelectionChanged(false);
		SignalScrub(false);
		update();
	}
}

void CTimeline::OnMenuNextKey()
{
	if (!m_pContent)
		return;
	TimeToId times;
	GetAllTimes(&times, *m_pContent);

	std::vector<STimelineContentElementRef> selection;
	ForEachElementWithIndex(m_pContent->track, [&](STimelineTrack& t, STimelineElement& e, size_t i) { if (e.selected) selection.push_back(STimelineContentElementRef(&t, i)); });

	STimelineContentElementRef selectedId = SelectedIdAtTime(selection, *m_pContent, m_time);

	TimeToId::iterator it = std::upper_bound(times.begin(), times.end(), std::make_pair(m_time, selectedId));
	if (it != times.end())
	{
		ClearElementSelection(m_pContent->track);
		if (it->second.IsValid())
			it->second.GetElement().selected = true;

		m_time = it->first;

		SignalSelectionChanged(false);
		SignalScrub(false);
		update();
	}
}

void CTimeline::OnMenuPreviousFrame()
{
	m_time = ClampAndSnapTime(m_time.StepToPrev(m_frameRate), true);
	SignalScrub(false);
	update();
}

void CTimeline::OnMenuNextFrame()
{
	m_time = ClampAndSnapTime(m_time.StepToNext(m_frameRate), true);
	SignalScrub(false);
	update();
}

void CTimeline::OnFilterChanged()
{
	UpdateLayout();
	update();
}

void CTimeline::OnVerticalScroll(int value)
{
	InvalidateTree();
	InvalidateTracks();
	update();
}

void CTimeline::OnContextMenu(const QPoint& pos)
{
	const bool bInTreeArea = m_treeVisible && (pos.x() <= m_viewState.treeWidth);

	if (bInTreeArea)
	{
		std::vector<STimelineTrack*> selectedTracks;
		GetSelectedTracks(m_pContent->track, selectedTracks);

		const int scroll = m_scrollBar ? m_scrollBar->value() : 0;
		const QPoint posInTree(pos.x(), pos.y() + scroll);
		STrackLayout* pLayout = GetTrackLayoutFromPos(posInTree);
		if (pLayout)
		{
			if (!stl::find(selectedTracks, pLayout->pTimelineTrack))
			{
				ClearTrackSelection(m_pContent->track);
				pLayout->pTimelineTrack->selected = true;
			}
		}

		SignalTreeContextMenu(mapToGlobal(pos));
	}
	else
	{
		QMenu menu;
		bool hasSelection = false;
		ForEachElement(m_pContent->track, [&](STimelineTrack& t, STimelineElement& e) { if (e.selected) hasSelection = true; });

		menu.addAction("Move Selection to Cursor", this, SLOT(OnMenuSelectionToCursor()))->setEnabled(hasSelection);

		auto duplicateAction = GetIEditor()->GetICommandManager()->GetAction("general.duplicate");
		duplicateAction->setEnabled(hasSelection);
		menu.addAction(duplicateAction);
		menu.addSeparator();
		menu.addAction(GetIEditor()->GetICommandManager()->GetAction("general.delete"));
		menu.addSeparator();
		menu.addAction("Play / Pause", this, SLOT(OnMenuPlay()), QKeySequence("Space"));
		menu.addAction("Previous Frame", this, SLOT(OnMenuPreviousFrame()), QKeySequence(","));
		menu.addAction("Next Frame", this, SLOT(OnMenuNextFrame()), QKeySequence("."));
		menu.addAction("Jump to Previous Event", this, SLOT(OnMenuPreviousKey()), QKeySequence("X"));
		menu.addAction("Jump to Next Event", this, SLOT(OnMenuNextKey()), QKeySequence("C"));
		menu.addSeparator();
		menu.addAction(GetIEditor()->GetICommandManager()->GetAction("general.copy"));
		menu.addAction(GetIEditor()->GetICommandManager()->GetAction("general.cut"));
		menu.addAction(GetIEditor()->GetICommandManager()->GetAction("general.paste"));

		menu.exec(mapToGlobal(pos), duplicateAction);
	}
}

void CTimeline::SetTreeVisible(bool visible)
{
	m_treeVisible = visible;
	m_viewState.treeWidth = visible ? DEFAULT_TREE_WIDTH : 0;

	UpdateLayout();
	update();
}

STrackLayout* CTimeline::GetTrackLayoutFromPos(const QPoint& pos) const
{
	if (pos.y() < RULER_HEIGHT)
	{
		return nullptr;
	}

	STrackLayouts& tracks = m_layout->tracks;

	auto findIter = std::upper_bound(tracks.begin(), tracks.end(), pos.y(), [&](int y, const STrackLayout& track)
	{
		return y < track.rect.bottom();
	});

	if (findIter != tracks.end() && pos.y() <= findIter->rect.bottom())
	{
		return &(*findIter);
	}

	return nullptr;
}

void CTimeline::ResolveHitElements(const QRect& rect, SElementLayoutPtrs& hitElements)
{
	hitElements.clear();     
	for (auto& pTrackLayout : m_layout->tracks)
	{
		HitTestElements(pTrackLayout, rect, hitElements);
	}
}


void CTimeline::ResolveHitElements(const QPoint& pos, SElementLayoutPtrs& hitElements)
{
	hitElements.clear();  

	QPoint posInLayoutSpace = m_viewState.LocalToLayout(pos);
	if (STrackLayout* pTrackLayout = GetTrackLayoutFromPos(pos))
	{
		HitTestElements(*pTrackLayout, QRect(posInLayoutSpace - QPoint(2, 2), posInLayoutSpace + QPoint(2, 2)), hitElements);
	}
}

void CTimeline::SetCustomTreeCornerWidget(QWidget* pWidget, uint width)
{
	SAFE_DELETE(m_cornerWidget);

	m_cornerWidget = pWidget;
	m_cornerWidgetWidth = width;

	if (m_cornerWidget)
	{
		m_cornerWidget->setCursor(QCursor());
	}

	UpdateLayout();
	update();
}

void CTimeline::SetVerticalScrollbarVisible(bool bVisible)
{
	m_verticalScrollbarVisible = bVisible;
	UpdateLayout();
	update();
}

void CTimeline::SetDrawTrackTimeMarkers(bool bDrawMarkers)
{
	m_drawMarkers = bDrawMarkers;
	update();
}

void CTimeline::SetVisibleDistance(float distance)
{
	m_viewState.visibleDistance = distance;

	UpdateLayout();
	update();
}

Range CTimeline::GetVisibleTimeRange() const
{
	return Range(m_viewState.LocalToTime(m_viewState.treeWidth) * m_timeUnitScale,
	             m_viewState.LocalToTime(size().width()) * m_timeUnitScale);
}

Range CTimeline::GetVisibleTimeRangeFull() const
{
	return Range(m_viewState.LocalToTime(0) * m_timeUnitScale,
	             m_viewState.LocalToTime(size().width()) * m_timeUnitScale);
}

void CTimeline::ZoomToTimeRange(const float start, const float end)
{
	m_viewState.scrollPixels.setX(m_viewState.TimeToLocal(start));
	m_viewState.visibleDistance = (end - start);

	if (m_timeUnit == SAnimTime::EDisplayMode::Frames)
	{
		m_viewState.visibleDistance = max(m_viewState.visibleDistance, 0.35f);
	}
}

void CTimeline::ClearElementSelections()
{
	ClearElementSelection(m_pContent->track);
	SignalSelectionChanged(false);
}

STimelineTrack* CTimeline::GetTrackFromPos(const QPoint& pos) const
{
	const QPoint mousePos = mapFromGlobal(pos);
	const int scroll = m_scrollBar ? m_scrollBar->value() : 0;
	const QPoint timelinePos(mousePos.x(), mousePos.y() + scroll);

	STrackLayout* pTrackLayout = GetTrackLayoutFromPos(timelinePos);
	return pTrackLayout ? pTrackLayout->pTimelineTrack : nullptr;
}

SAnimTime CTimeline::GetLastMousePressEventTime() const
{
	QPoint layoutPoint = m_viewState.LocalToLayout(m_lastMousePressEventPos);
	return SAnimTime(m_viewState.LayoutToTime(layoutPoint.x()));
}

SAnimTime CTimeline::GetTimeFromPos(QPoint p) const
{
	QPoint layoutPoint = m_viewState.LocalToLayout(mapFromGlobal(p));
	return SAnimTime(m_viewState.LayoutToTime(layoutPoint.x()));
}

void CTimeline::OnTrackToggled(QPoint pos)
{
	STrackLayout* pTrackLayout = GetTrackLayoutFromPos(pos);
	if (pTrackLayout && pTrackLayout->pTimelineTrack)
	{
		pTrackLayout->pTimelineTrack->expanded = !pTrackLayout->pTimelineTrack->expanded;
		UpdateLayout();
		SignalTrackToggled(*pTrackLayout->pTimelineTrack);
		update();
	}
}

void CTimeline::OnSplitterMoved(uint32 newTreeWidth)
{
	m_viewState.treeWidth = newTreeWidth;
	m_viewState.treeLastOpenedWidth = newTreeWidth;
	UpdateLayout();
	update();

	SignalLayoutChanged();
}

void CTimeline::OnLayoutChange()
{
	InvalidateTracksTimeMarkers();
	UpdateLayout();
	update();

	SignalLayoutChanged();
}

