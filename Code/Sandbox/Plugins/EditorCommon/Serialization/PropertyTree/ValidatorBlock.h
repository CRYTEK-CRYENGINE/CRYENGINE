// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/yasli/Config.h>
#include <CrySerialization/yasli/TypeID.h>
#include <vector>
#include <algorithm>

enum ValidatorEntryType
{
	VALIDATOR_ENTRY_WARNING,
	VALIDATOR_ENTRY_ERROR
};

struct ValidatorEntry
{
	const void* handle;
	yasli::TypeID typeId;

	ValidatorEntryType type;
	yasli::string message;

	bool operator<(const ValidatorEntry& rhs) const {
		if (handle != rhs.handle)
			return handle < rhs.handle; 
		return typeId < rhs.typeId;
	}

	ValidatorEntry(ValidatorEntryType type, const void* handle, const yasli::TypeID& typeId, const char* message)
	: type(type)
	, handle(handle)
	, message(message)
	, typeId(typeId)
	{
	}

	ValidatorEntry()
	: handle()
	, type(VALIDATOR_ENTRY_WARNING)
	{
	}
};
typedef std::vector<ValidatorEntry> ValidatorEntries;

class ValidatorBlock
{
public:
	ValidatorBlock()
	: enabled_(false)
	{
	}

	void clear()
	{
		entries_.clear();
		used_.clear();
	}

	void addEntry(const ValidatorEntry& entry)
	{
		ValidatorEntries::iterator it = std::upper_bound(entries_.begin(), entries_.end(), entry);
		used_.insert(used_.begin() + (it - entries_.begin()), false);
		entries_.insert(it, entry);
		enabled_ = true;
	}

	bool isEnabled() const { return enabled_; }

	const ValidatorEntry* getEntry(int index, int count) const {
		if (size_t(index) >= entries_.size())
			return 0;
		if (size_t(index + count) > entries_.size())
			return 0;
		return &entries_[index];
	}

	bool findHandleEntries(int* outIndex, int* outCount, const void* handle, const yasli::TypeID& typeId)
	{
		if (handle == 0)
			return false;
		ValidatorEntry e;
		e.handle = handle;
		e.typeId = typeId;
		ValidatorEntries::iterator begin = std::lower_bound(entries_.begin(), entries_.end(), e);
		ValidatorEntries::iterator end = std::upper_bound(entries_.begin(), entries_.end(), e);
		if (begin != end)
		{
			*outIndex = int(begin - entries_.begin());
			*outCount = int(end - begin);
			return true;
		}
		return false;
	}

	void markAsUsed(int start, int count)
	{
		if (start < 0)
			return;
		if (start + count > entries_.size())
			return;
		for(int i = start; i < start + count; ++i) {
			used_[i] = true;
		}
	}

	void mergeUnusedItemsWithRootItems(int* firstUnusedItem, int* count, const void* newHandle, const yasli::TypeID& typeId) 
	{
		size_t numItems = used_.size();
		for (size_t i = 0; i < numItems; ++i) {
			if (entries_[i].handle == newHandle) {
				entries_.push_back(entries_[i]);
				used_.push_back(true);
				entries_[i].typeId = yasli::TypeID();
			}
			if (!used_[i]) {
				entries_.push_back(entries_[i]);
				used_.push_back(true);
				entries_.back().handle = newHandle;
				entries_.back().typeId = typeId;
			}
		}
		*firstUnusedItem = (int)numItems;
		*count = int(entries_.size() - numItems);
	}

	bool containsWarnings() const
	{
		for (size_t i = 0; i < entries_.size(); ++i)
		{
			if (entries_[i].type == VALIDATOR_ENTRY_WARNING)
				return true;
		}
		return false;
	}

	bool containsErrors() const
	{
		for (size_t i = 0; i < entries_.size(); ++i)
		{
			if (entries_[i].type == VALIDATOR_ENTRY_ERROR)
				return true;
		}
		return false;
	}

private:
	ValidatorEntries entries_;
	std::vector<bool> used_;
	bool enabled_;
};

