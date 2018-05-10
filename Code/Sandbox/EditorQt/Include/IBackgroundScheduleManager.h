// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//
// IBackgroundScheduleManager manages schedule of group of larger operations that should be run in sequence
//
// Schedules are derived from IBackgroundSchedule and consist of a list of IBackgroundScheduleItems.
// Each schedule item is executed IN ORDER for the previous item to complete first.
// Each IBackgroundScheduleItems consists of list of user defined work via IBackgroundScheduleItemsWork classes.
// Each schedule item work is executed IN PARALEL (they are all started when the item starts).
//
// Whenever a work item fails to complete the other work items are stopped, the schedule item is marked as "failed"
// and so is the whole schedule.
//
// All logic is performed on the main thread although schedule items are free to use threads.
// It is recommended to use IBackgroundTaskManager for dispatching a task list for every schedule work item.
//
// All objects in the schedule system are reference counted.
//

// State of the whole schedule
enum EScheduleState
{
	// Item has not started yet but is on the list
	eScheduleState_Pending,

	// We are processing this item
	eScheduleState_Processing,

	// We are stopping the schedule
	eSccheduleState_Stopping,

	// Schedule item has failed
	eScheduleState_Failed,

	// Schedule item was canceled
	eScheduleState_Canceled,

	// Schedule item has completed it's work
	eScheduleState_Completed,
};

// State of the single schedule item
enum EScheduleItemState
{
	// Item has not started yet but is on the list
	eScheduleItemState_Pending,

	// We are processing this item
	eScheduleItemState_Processing,

	// We are stopping this item
	eScheduleItemState_Stopping,

	// Schedule item has failed
	eScheduleItemState_Failed,

	// Schedule item has completed it's work
	eScheduleItemState_Completed,
};

// Work item status
enum EScheduleWorkItemStatus
{
	// Work is still not finished
	eScheduleWorkItemStatus_NotFinished,

	// Work has failed
	eScheduleWorkItemStatus_Failed,

	// Work has finished
	eScheduleWorkItemStatus_Finished,
};

struct IBackgroundScheduleItemWork
{
	// Get human readable description
	virtual const char* GetDescription() const = 0;

	// Get work item progress
	virtual float GetProgress() const = 0;

	// Called when the schedule item containing this work piece has started
	// If the work cannot be started for any reason return false.
	virtual bool OnStart() = 0;

	// Called when the schedule item containing this work piece has been canceled or failed externally
	// Not called when schedule item completed without errors.
	// If the work cannot be stopped this frame return false.
	virtual bool OnStop() = 0;

	// Called every frame to advance and check the work state
	// Should return one of the EBackgroundScheduleWorkItemStatus value
	virtual EScheduleWorkItemStatus OnUpdate() = 0;

	// Reference counting
	virtual void AddRef() = 0;
	virtual void Release() = 0;

protected:
	virtual ~IBackgroundScheduleItemWork() {};
};

struct IBackgroundScheduleItem
{
	// Get name of the schedule (debug & display)
	virtual const char* GetDescription() const = 0;

	// Get interal state
	virtual EScheduleItemState GetState() const = 0;

	// Get overall progress of this schedule item
	virtual const float GetProgress() const = 0;

	// Get number of work items in this schedule item
	virtual const uint32 GetNumWorkItems() const = 0;

	// Get n-th work item from the schedule item
	virtual IBackgroundScheduleItemWork* GetWorkItem(const uint32 index) const = 0;

	// Add work item to the schedule item
	virtual void AddWorkItem(IBackgroundScheduleItemWork* pWork) = 0;

	// Reference counting
	virtual void AddRef() = 0;
	virtual void Release() = 0;

protected:
	virtual ~IBackgroundScheduleItem() {};
};

struct IBackgroundSchedule
{
	// Get name of the schedule (debug & display)
	virtual const char* GetDescription() const = 0;

	// Get overall progress of the whole schedule
	virtual float GetProgress() const = 0;

	// Get item being currently processed
	virtual IBackgroundScheduleItem* GetProcessedItem() const = 0;

	// Get number of items in the schedule
	virtual const uint32 GetNumItems() const = 0;

	// Get single schedule item
	virtual IBackgroundScheduleItem* GetItem(const uint32 index) const = 0;

	// Get schedule item
	virtual EScheduleState GetState() const = 0;

	// Cancel the whole schedule
	virtual void Cancel() = 0;

	// Is the schedule canceled ?
	virtual bool IsCanceled() const = 0;

	// Add schedule item at the end of the list
	virtual void AddItem(IBackgroundScheduleItem* pItem) = 0;

	// Reference counting
	virtual void AddRef() = 0;
	virtual void Release() = 0;

protected:
	virtual ~IBackgroundSchedule() {};
};

struct IBackgroundScheduleManager
{
	virtual ~IBackgroundScheduleManager() {};

	// Create empty schedule
	virtual IBackgroundSchedule* CreateSchedule(const char* szName) = 0;

	// Create empty schedule item
	virtual IBackgroundScheduleItem* CreateScheduleItem(const char* szName) = 0;

	// Issue a schedule to the list (will start processing it)
	virtual void SubmitSchedule(IBackgroundSchedule* pSchedule) = 0;

	// Get number of schedules on the list
	virtual const uint32 GetNumSchedules() const = 0;

	// Get n-th schedule
	virtual IBackgroundSchedule* GetSchedule(const uint32 index) const = 0;

	// Advance work on the schedules
	virtual void Update() = 0;
};

