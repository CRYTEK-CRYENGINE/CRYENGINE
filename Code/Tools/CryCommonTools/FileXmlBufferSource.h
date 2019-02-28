// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FILEXMLBUFFERSOURCE_H__
#define __FILEXMLBUFFERSOURCE_H__

class FileXmlBufferSource : public IXmlBufferSource
{
public:
	FileXmlBufferSource(const char* path)
	{
		file = std::fopen(path, "r");
	}
	~FileXmlBufferSource()
	{
		if (file)
			std::fclose(file);
	}

	virtual int Read(void* buffer, int size) const
	{
		if (!file)
			return 0;
		return std::fread(buffer, 1, size, file);
	}

private:
	mutable std::FILE* file;
};

#endif //__FILEXMLBUFFERSOURCE_H__
