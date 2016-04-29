#include "stdafx.h"
#include "dircopy.h"
#include "CFS64.h"

static CCFS64* thefs = 0;

void ListDirCallback(const TCHAR* rootpath, const TCHAR* filename, bool isDir)
{
	filedesc* newfdesc = new filedesc;
	tstring srootpath = rootpath;
	if (rootpath[_tcslen(rootpath) - 1] != _T('\\'))
		srootpath.append(_T("\\"));
	tstring CFSpath = filename;
	CFSpath.erase(0, srootpath.length());
	newfdesc->filepath = CFSpath.c_str();
	isDir ? newfdesc->type = 0xD1 : newfdesc->type = 0xF1;
	HANDLE file = NULL;
	//Now actually copy the dir
	if (isDir)
	{
		file = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	}
	else
	{
		file = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	if (file == INVALID_HANDLE_VALUE)
		throw "Cannot open file";
	thefs->addFile(newfdesc, file);
	CloseHandle(file);
	delete newfdesc;
}

bool ListDirectoryContents(const TCHAR* sDir, const TCHAR* sRootDir = 0)
{
	WIN32_FIND_DATA fdFile;
	HANDLE hFind = NULL;
	TCHAR sPath[2048];
	//Specify a file mask. *.* = We want everything! 
	wsprintf(sPath, L"%s\\*.*", sDir);

	if (!sRootDir)
		sRootDir = sDir;

	if ((hFind = FindFirstFile(sPath, &fdFile)) == INVALID_HANDLE_VALUE)
	{
		_tprintf(L"Path not found: [%s]\n", sDir);
		return false;
	}

	do
	{
		//Find first file will always return "."
		//    and ".." as the first two directories. 
		if (_tcscmp(fdFile.cFileName, L".") != 0
			&& _tcscmp(fdFile.cFileName, L"..") != 0)
		{
			//Build up our file path using the passed in 
			//  [sDir] and the file/foldername we just found: 
			wsprintf(sPath, L"%s\\%s", sDir, fdFile.cFileName);

			//Is the entity a File or Folder? 
			if (fdFile.dwFileAttributes &FILE_ATTRIBUTE_DIRECTORY)
			{
				ListDirCallback(sRootDir, sPath, true);
				ListDirectoryContents(sPath, sRootDir); //Recursion, I love it! 
			}
			else{
				ListDirCallback(sRootDir, sPath, false);
			}
		}
	} while (FindNextFile(hFind, &fdFile)); //Find the next file. 

	FindClose(hFind); //Always, Always, clean things up! 

	return true;
}

void dircopy(TCHAR* directory, CCFS64* cfs)
{
	thefs = cfs;
	ListDirectoryContents(directory);
}
