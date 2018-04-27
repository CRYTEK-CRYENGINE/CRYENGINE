// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace AssetLoader
{

struct IFileReader
{
public:
	typedef std::function<void(const char* pBuffer, size_t numberOfBytes, void* pUserData)> CompletionHandler;

public:
	virtual bool ReadFileAsync(const char* szFilePath, const CompletionHandler& completionHandler, void* pUserData) = 0;
	virtual void WaitForCompletion() = 0;
	virtual ~IFileReader() {};
};

std::unique_ptr<IFileReader> CreateFileReader();

}
