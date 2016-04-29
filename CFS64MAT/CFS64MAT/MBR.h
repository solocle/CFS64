#pragma once
#include "stdafx.h"
#include "RawDisk.h"
class CMBR
{
public:
	CMBR(const TCHAR* deviceName, HANDLE hDevice = NULL);
	CMBR(TCHAR driveLetter);
	virtual ~CMBR();
	virtual void getPartitonList();
	virtual void setType();
	virtual UINT64 getDiskSize();
	virtual UINT64 getPartitionSize(){ return m_partsize; }
	const TCHAR* getDevicePath(){ return m_physicalpath.c_str(); }
public:
	struct PartitionEntry {
		BYTE type;
		DWORD attributes;
		UINT64 LBA;
		UINT64 length;
	};
protected:
	CRawDisk* m_pPhysRawDisk;
	tstring m_physicalpath;
	const TCHAR* m_devicepath;
	HANDLE m_hDevice; 
	DISK_GEOMETRY geometry;
	uint64_t m_partsize;
#pragma pack (push,1)
	struct sectcyl {
		WORD startsector : 6;
		WORD startcylind : 10;
	};
	struct PartitonTableEntry {
		BYTE bootable;
		BYTE starthead;
		sectcyl startsectcyl;
		BYTE sysid;
		BYTE endhead;
		sectcyl endsectcyl;
		DWORD LBA;
		DWORD length;
	};
	struct MBR {
		BYTE bootcode[436];
		BYTE diskid[10];
		PartitonTableEntry entries[4];
		WORD signature;
	};

	//GPT stuff

	struct GPTheader {
		char sig[8];	//"EFI PART"
		WORD revisionLow;
		WORD revisionHigh;
		DWORD headerSize;
		DWORD crc32;
		DWORD reserved;
		UINT64 currentLBA;
		UINT64 backupLBA;
		UINT64 firstUsableLBA;
		UINT64 lastUsableLBA;
		GUID diskGuid;
		UINT64 partitionArrayLBA;
		DWORD partitionArrayLength;
		DWORD partitionEntrySize;
		DWORD partitionArrayCRC;
	};

	struct GPTentry {
		GUID type;
		GUID id;
		UINT64 firstLBA;
		UINT64 lastLBA;
		UINT64 attributes;
		char16_t name[36];
	};

#pragma pack(pop)
};

