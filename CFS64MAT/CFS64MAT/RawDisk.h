#pragma once
#include "stdafx.h"
class CRawDisk
{
public:
	CRawDisk(const TCHAR* deviceName);
	CRawDisk(TCHAR driveLetter);
	virtual void read(UINT64 sector, LPVOID buffer, size_t length);
	virtual void write(UINT64 sector, LPVOID buffer, size_t length);
	virtual void setSectorSize(unsigned int size){ sector_size = size;sector_2pow=log2(size); }
	HANDLE getHandle(){ return m_diskFile; }
	virtual ~CRawDisk();
protected:
	unsigned int sector_2pow = 9;
	unsigned int sector_size = (1<<sector_2pow);
	UINT64 offsetFromPhysBegin;
	HANDLE m_diskFile;
};

