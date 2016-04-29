#include "stdafx.h"
#include "RawDisk.h"


CRawDisk::CRawDisk(const TCHAR* devicename)
{
	m_diskFile = CreateFile(devicename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_DEVICE, NULL);
	if (m_diskFile == INVALID_HANDLE_VALUE)
	{
		throw "Error creating disk file (" __FILE__ ")";
	}
}

CRawDisk::CRawDisk(TCHAR driveletter)
{ 
	tstring str = _T("\\\\.\\");
	str.append({ driveletter, 0 });
	str.append(_T(":"));
	CRawDisk(str.c_str());
}


CRawDisk::~CRawDisk()
{
	CloseHandle(m_diskFile);
}

void CRawDisk::read(UINT64 sector, LPVOID buffer, size_t length)
{
	//sector += offsetFromPhysBegin;
	LONG highbyte = (sector>>(32-sector_2pow));
	if (SetFilePointer(m_diskFile, (sector << sector_2pow), &highbyte, FILE_BEGIN) == INVALID_SET_FILE_POINTER && GetLastError() != 0)
	{
		throw "Error seeking to location on disk";
	}
	DWORD read = 0;
	if (!ReadFile(m_diskFile, buffer, length, &read, NULL))
	{
		throw "Error reading disk";
	}
}

void CRawDisk::write(UINT64 sector, LPVOID buffer, size_t length)
{
	//sector += offsetFromPhysBegin;
	LONG highbyte = (sector >> (32 - sector_2pow));
	if (SetFilePointer(m_diskFile, (sector << sector_2pow), &highbyte, FILE_BEGIN) == INVALID_SET_FILE_POINTER && GetLastError() != 0)
	{
		throw "Error seeking to location on disk";
	}
	DWORD written = 0;
	if (!WriteFile(m_diskFile, buffer, length, &written, NULL))
	{
		throw "Error writing disk";
	}
}
