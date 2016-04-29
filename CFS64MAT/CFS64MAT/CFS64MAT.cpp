// CFS64MAT.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "RawDisk.h"
#include "MBR.h"
#include "CFS64.h"
#include "dircopy.h"


int _tmain(int argc, _TCHAR* argv[])
{
	bool drivetoformat = false;
	union {
		TCHAR driveletter;
		TCHAR drivewithcolon[3];
	};
	TCHAR* dirtocopy = 0;
	drivewithcolon[1] = _T(':');
	drivewithcolon[2] = NULL;
	if (argc > 1)
	{
		for (int n = 1; n < argc; n++)
		{
			if (argv[n][0] == _T('-'))
			{
				if (_tcscmp(argv[n], _T("-d")) == 0)
				{
					if (argc < n+2)
					{
						tcout << _T("Error: no directory specified with switch -d") << endl;
						return -1;
					}
					else
					{
						dirtocopy = argv[++n];
					}
				}
				else
				{
					tcout << _T("Error: Unknown Parameter \"") << argv[n] << _T("\"") << endl;
					return -1;
				}
			}
			else
			{
				driveletter = argv[n][0];
				drivetoformat = true;
			}
		}
	}
	if (!drivetoformat)
	{
		tcout << _T("Enter drive to format: ");
		tcin >> driveletter;
	}
	tcout << _T("Formatting drive ") << driveletter << endl;
	CRawDisk* rawdisk = NULL;
	CMBR* mbr = NULL;
	CCFS64* fs = NULL;
	tstring str(_T("\\\\.\\"));
	str.append(drivewithcolon);
	try {
		rawdisk = new CRawDisk(str.c_str());
		fs = new CCFS64(rawdisk);
		mbr = new CMBR(str.c_str(), rawdisk->getHandle());
		tcout << mbr->getPartitionSize() << endl;
		fs->format(mbr->getPartitionSize()*512);
		if (dirtocopy)
		{
			dircopy(dirtocopy, fs);
		}
		tcout << _T("Partition on ") << mbr->getDevicePath() << _T(" with size ") << (double)mbr->getDiskSize() / (1024 * 1024 * 1024) << _T("GB") << endl;
		//mbr->setType();
	}
	catch (char* e)
	{
		cout << "Error: " << e << "\n";
		LPTSTR message;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&message, 0, NULL);
		tcout << message;
		cout << " (" << GetLastError() << ")\n";
	}
	tcout << _T("DONE\n");
	cin.ignore();
	return 0;
}

