#include "stdafx.h"
#include "MBR.h"
#include <initguid.h>

extern uint32_t crc32(uint32_t crc, const void *buf, size_t size);
DEFINE_GUID(CFS64GUID, 0x22186590, 0xcae2, 0x4d34, 0xa4, 0x25, 0x33, 0x16, 0x36, 0x65, 0xcd, 0xba);
//{22186590-cae2-4d34-a425-33163665cdba}

CMBR::CMBR(const TCHAR* deviceName, HANDLE hDevice)
{
	m_pPhysRawDisk = NULL;
	VOLUME_DISK_EXTENTS extents;
	memset(&extents, 0, sizeof(extents));
	DWORD byteswritten = 0;
	if (!hDevice)
	{
		CRawDisk tempdisk(deviceName);
		hDevice = tempdisk.getHandle();
		if (!DeviceIoControl(hDevice, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, &extents, sizeof(extents), &byteswritten, NULL))
		{
			cout << "Error getting Physical Device (" << GetLastError() << "), aborting...\n";
			throw "Error in MBR (" __FILE__ ") attaining physcial device";
		}
	}
	else
	{
		if (!DeviceIoControl(hDevice, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, &extents, sizeof(extents), &byteswritten, NULL))
		{
			cout << "Error getting Physical Device (" << GetLastError() << "), aborting...\n";
			throw "Error in MBR (" __FILE__ ") attaining physcial device";
		}
	}
	m_hDevice = hDevice;
	if (!DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &geometry, sizeof(geometry), &byteswritten, NULL))
	{
		cout << "Error getting Physical Device (" << GetLastError() << "), aborting...\n";
		throw "Error in MBR (" __FILE__ ") attaining physcial device";
	}
	int nPhysDrive = extents.Extents[0].DiskNumber;
	tstring path = tstring(_T("\\\\.\\PhysicalDrive") + to_tstring(nPhysDrive));
	m_physicalpath = path;
	m_pPhysRawDisk = new CRawDisk(path.c_str());
	m_pPhysRawDisk->setSectorSize(geometry.BytesPerSector);
	PARTITION_INFORMATION_EX partinfo;
	DWORD read = 0;
	if (!DeviceIoControl(hDevice, IOCTL_DISK_GET_PARTITION_INFO_EX, NULL, 0, &partinfo, sizeof(partinfo), &read, NULL))
		throw "Cannot get partititon information";
	m_partsize = partinfo.PartitionLength.QuadPart / geometry.BytesPerSector;
}

CMBR::CMBR(TCHAR driveLetter)
{
	tstring str = _T("\\\\.\\");
	str.append({ driveLetter, 0 });
	str.append(_T(":"));
	CMBR(str.c_str());
}

UINT64 CMBR::getDiskSize()
{
	GET_LENGTH_INFORMATION len;
	DWORD junk = 0;
	if (!DeviceIoControl(m_pPhysRawDisk->getHandle(), IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &len, sizeof(len), &junk, NULL))
	{
		throw "Error getting disk size (" __FILE__ ")\n";
	}
	return len.Length.QuadPart;
}

void CMBR::getPartitonList()
{

}

void CMBR::setType()
{
#if 1
	PARTITION_INFORMATION_EX partinfo;
	DWORD read = 0;
	HANDLE hDevice = m_hDevice;
	if (!DeviceIoControl(hDevice, IOCTL_DISK_GET_PARTITION_INFO_EX, NULL, 0, &partinfo, sizeof(partinfo), &read, NULL))
		throw "Cannot get partititon information";
	m_partsize = partinfo.PartitionLength.QuadPart / geometry.BytesPerSector;
	if (partinfo.PartitionStyle == PARTITION_STYLE_MBR)
	{
		SET_PARTITION_INFORMATION_EX setinfo;
		setinfo.PartitionStyle = partinfo.PartitionStyle;
		setinfo.Mbr.PartitionType = 0x13;
		//if (!DeviceIoControl(hDevice, IOCTL_DISK_SET_PARTITION_INFO_EX, &setinfo, sizeof(setinfo), NULL, 0, &read, NULL))
		//throw "Could not set partition type";
	}
	else if (partinfo.PartitionStyle == PARTITION_STYLE_GPT)
	{
		SET_PARTITION_INFORMATION_EX setinfo;
		setinfo.PartitionStyle = partinfo.PartitionStyle;
		setinfo.Gpt = partinfo.Gpt;
		setinfo.Gpt.PartitionType = CFS64GUID;
		if (!DeviceIoControl(hDevice, IOCTL_DISK_SET_PARTITION_INFO_EX, &setinfo, sizeof(setinfo), NULL, 0, &read, NULL))
			throw "Could not set partition type";
	}
	else
		throw "Unknown partitioning scheme";
#else
	MBR thembr;
	UINT64 RECORDLBA = 0;
	try {
		m_pPhysRawDisk->read(0, &thembr, 512);
	}
	catch (char* e)
	{
		//Probably native 4K, we can continue;
		goto fullGPT;
	}
	//GPT support
	if (thembr.entries[0].sysid == 0xEE)
	{
	fullGPT:
		throw "GPT support is buggy";
		//full GPT, we just need to search GPT tables
		GPTheader* header = (GPTheader*)new BYTE[geometry.BytesPerSector];
		GPTheader& theheader = *header;
		m_pPhysRawDisk->read(1, &theheader, geometry.BytesPerSector);
		BYTE* fullarray = new BYTE[theheader.partitionArrayLength*theheader.partitionEntrySize + geometry.BytesPerSector];
		ZeroMemory(fullarray, theheader.partitionArrayLength*theheader.partitionEntrySize + geometry.BytesPerSector);
		m_pPhysRawDisk->read(theheader.partitionArrayLBA, fullarray, ((theheader.partitionArrayLength*theheader.partitionEntrySize + geometry.BytesPerSector - 1) / geometry.BytesPerSector) * geometry.BytesPerSector);
		GPTentry* theentry = (GPTentry*)fullarray;
		bool bFound = false;
		for (int n = 0; n < theheader.partitionArrayLength; n++)
		{
			theentry = (GPTentry*)&fullarray[n*theheader.partitionEntrySize];
			if (theentry->firstLBA <= extents.Extents[0].StartingOffset.QuadPart / geometry.BytesPerSector &&  theentry->lastLBA >= extents.Extents[0].StartingOffset.QuadPart / geometry.BytesPerSector)
			{
				//We found our entry, set it as CFS64
				bFound = true;
				theentry->type = CFS64GUID;
				break;
			}
		}
		if (!bFound)
			throw "Coud not find partition entry";
		DWORD initcrc = crc32(0, NULL, 0);
		theheader.partitionArrayCRC = crc32(initcrc, fullarray, theheader.partitionArrayLength*theheader.partitionEntrySize);
		m_pPhysRawDisk->write(theheader.partitionArrayLBA, fullarray, ((theheader.partitionArrayLength*theheader.partitionEntrySize + geometry.BytesPerSector - 1) / geometry.BytesPerSector) * geometry.BytesPerSector);
		m_pPhysRawDisk->write(theheader.lastUsableLBA + 1, fullarray, ((theheader.partitionArrayLength*theheader.partitionEntrySize + geometry.BytesPerSector - 1) / geometry.BytesPerSector) * geometry.BytesPerSector);
		m_pPhysRawDisk->write(1, &theheader, geometry.BytesPerSector);
		m_pPhysRawDisk->write(theheader.backupLBA, &theheader, geometry.BytesPerSector);
		delete[] fullarray;
		delete[] header;
		return;
	}
	//GPT Hybrid support
	if (thembr.entries[0].sysid == 0xED)
	{
		throw "GPT support is buggy";
		//We need to set type of GPT partition AND MBR partition (if it is there)
		GPTheader* header = (GPTheader*)new BYTE[geometry.BytesPerSector];
		GPTheader& theheader = *header;
		m_pPhysRawDisk->read(1, &theheader, geometry.BytesPerSector);
		BYTE* fullarray = new BYTE[theheader.partitionArrayLength*theheader.partitionEntrySize + geometry.BytesPerSector];
		ZeroMemory(fullarray, theheader.partitionArrayLength*theheader.partitionEntrySize + geometry.BytesPerSector);
		m_pPhysRawDisk->read(theheader.partitionArrayLBA, fullarray, ((theheader.partitionArrayLength*theheader.partitionEntrySize + geometry.BytesPerSector - 1) / geometry.BytesPerSector) * geometry.BytesPerSector);
		GPTentry* theentry = (GPTentry*)fullarray;
		bool bFound = false;
		for (int n = 0; n < theheader.partitionArrayLength; n++)
		{
			theentry = (GPTentry*)&fullarray[n*theheader.partitionEntrySize];
			if (theentry->firstLBA <= extents.Extents[0].StartingOffset.QuadPart / geometry.BytesPerSector &&  theentry->lastLBA >= extents.Extents[0].StartingOffset.QuadPart / geometry.BytesPerSector)
			{
				//We found our entry, set it as CFS64
				bFound = true;
				theentry->type = CFS64GUID;
				break;
			}
		}
		if (!bFound)
			throw "Coud not find partition entry";
		DWORD initcrc = crc32(0, NULL, 0);
		theheader.partitionArrayCRC = crc32(initcrc, fullarray, theheader.partitionArrayLength*theheader.partitionEntrySize);
		m_pPhysRawDisk->write(theheader.partitionArrayLBA, fullarray, ((theheader.partitionArrayLength*theheader.partitionEntrySize + geometry.BytesPerSector - 1) / geometry.BytesPerSector) * geometry.BytesPerSector);
		m_pPhysRawDisk->write(1, &theheader, geometry.BytesPerSector);
		delete[] fullarray;
		delete[] header;
		return;
	}
startsearch:
	for (int n = 0; n < 4; n++)
	{
		if (thembr.entries[n].LBA + RECORDLBA <= extents.Extents[0].StartingOffset.QuadPart / geometry.BytesPerSector && extents.Extents[0].StartingOffset.QuadPart / geometry.BytesPerSector < thembr.entries[n].LBA + RECORDLBA + thembr.entries[n].length)
		{
			if (thembr.entries[n].sysid == 0x0F)		//We ignore CHS extended partitions, so only check fro LBA
			{
				m_pPhysRawDisk->read(thembr.entries[n].LBA + RECORDLBA, &thembr, 512);
				RECORDLBA = thembr.entries[n].LBA + RECORDLBA;
				n = 0;
				goto startsearch;
			}
			else if (thembr.entries[n].sysid == 0xED)		//Skip GPT hybrid entry
			{
				continue;
			}
			else
			{
				//We have found our partition, so make it CFS
				thembr.entries[n].sysid = 0x13;
				m_pPhysRawDisk->write(RECORDLBA, &thembr, 512);
				tcout << _T("Partition with start ") << (double)(thembr.entries[n].LBA + RECORDLBA) / (1024 * 1024 * 1024 / 512) << _T("GB and length ") << (double)thembr.entries[n].length / (1024 * 1024 * 1024 / 512) << _T("GB set as CFS64\n");
				m_partsize = thembr.entries[n].length;
				break;
			}
		}
	}
#endif
}

CMBR::~CMBR()
{
	delete m_pPhysRawDisk;
}
