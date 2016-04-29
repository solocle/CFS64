#include "stdafx.h"
#include "CFS64.h"
#include "teratime.h"


CCFS64::CCFS64(CRawDisk* disk)
{
	m_pDisk = disk;
}


CCFS64::~CCFS64()
{
	delete master;
	delete root;
}

void CCFS64::readCluster(uint64_t cluster, LPVOID* buffer, bool autocreate)
{
	if (autocreate)
		*buffer = new BYTE[master->bytespercluster];
	m_pDisk->read(cluster*(master->bytespercluster / 512),*buffer,master->bytespercluster);
}

void CCFS64::writeCluster(uint64_t cluster, LPVOID* buffer, bool setCAT, bool autodelete, bool freeing, bool FSfull)
{
	m_pDisk->write(cluster*(master->bytespercluster / 512), *buffer, master->bytespercluster);
	if (autodelete)
		delete[] * buffer;
	if (setCAT)
		setClusterUsed(cluster,freeing,FSfull);
}

void CCFS64::setClusterUsed(uint64_t cluster, bool freeing, bool FSfull)
{
	uint64_t CATclus = 1 + (cluster / (8 * bytespercluster));
	uint64_t CATbyte = (cluster % (8 * bytespercluster)) / 8;
	uint64_t CATbit = (cluster % (8 * bytespercluster)) % 8;
	BYTE* buffer;
	readCluster(CATclus, (LPVOID*)&buffer);
	freeing ? (buffer[CATbyte] &= ~(1 << CATbit)) : (buffer[CATbyte] |= (1 << CATbit));
	writeCluster(CATclus, (LPVOID*)&buffer, false);
	if (FSfull)
	{
		uint64_t FSTclus = (cluster / (8 * bytespercluster)) + master->FStabpointer;
		uint64_t FSTbyte = (cluster % (8 * bytespercluster)) / 8;
		uint64_t FSTbit = (cluster % (8 * bytespercluster)) % 8;
		readCluster(FSTclus, (LPVOID*)&buffer);
		freeing ? (buffer[FSTbyte] &= ~(1 << FSTbit)) : (buffer[FSTbyte] |= (1 << FSTbit));
		writeCluster(FSTclus, (LPVOID*)&buffer, false);
	}
}

bool CCFS64::isFSClusterUsed(uint64_t cluster)
{
	uint64_t FSTclus = (cluster / (8 * bytespercluster)) + master->FStabpointer;
	uint64_t FSTbyte = (cluster % (8 * bytespercluster)) / 8;
	uint64_t FSTbit = (cluster % (8 * bytespercluster)) % 8;
	BYTE* buffer;
	readCluster(FSTclus, (LPVOID*)&buffer);
	bool isUsed = (buffer[FSTbyte] & (1 << FSTbit));
	delete[] buffer;
	return isUsed;
}

bool CCFS64::isClusterUsed(uint64_t cluster)
{
	uint64_t CATclus = 1 + (cluster / (8 * bytespercluster));
	uint64_t CATbyte = (cluster % (8 * bytespercluster)) / 8;
	uint64_t CATbit = (cluster % (8 * bytespercluster)) % 8;
	BYTE* buffer;
	readCluster(CATclus, (LPVOID*)&buffer);
	bool isUsed = (buffer[CATbyte] & (1 << CATbit));
	delete[] buffer;
	return isUsed;
}

uint64_t CCFS64::findFreeCluster()
{
	uint64_t* cluster = 0;
	uint64_t clustAddr = 1;
	int n = 0;
	bool freeFound = false;
	while (clustAddr < master->FStabpointer)
	{
		delete[] cluster;
		readCluster(clustAddr, (LPVOID*)&cluster);
		
		for (; n < master->bytespercluster / 8; n++)
		{
			if (cluster[n] != UINT64_MAX)
			{
				freeFound = true;
				break;
			}
		}
		if (freeFound)
			break;
		clustAddr++;
	}
	if (!freeFound)
		return 0;
	int bit = 0;
	while (cluster[n] & (1i64 << bit))
		bit++;
	delete[] cluster;
	return (((clustAddr - 1)*master->bytespercluster + n * 8) * 8 + bit);
	
}
uint64_t CCFS64::findFreeFSClust()
{
	uint64_t* cluster = 0;
	uint64_t clustAddr = master->FStabpointer;
	int n = 0;
	bool freeFound = false;
	while (clustAddr < master->roottablepointer)
	{
		if (cluster)
			delete[] cluster;
		readCluster(clustAddr, (LPVOID*)&cluster);

		for (; n < master->bytespercluster / 8; n++)
		{
			if (cluster[n] != UINT64_MAX)
			{
				freeFound = true;
				break;
			}
		}
		if (freeFound)
			break;
		clustAddr++;
	}
	if (!freeFound)
		return 0;
	int bit = 0;
	while (cluster[n] & (1i64 << bit))
		bit++;
	delete[] cluster;
	return (((clustAddr - master->FStabpointer)*master->bytespercluster + n * 8) * 8 + bit);
}

bool CCFS64::checkChecksum(LPVOID structure)
{
	BYTE* data = (BYTE*)structure;
	BYTE addition = 0;
	for (int n = 0; n < 512; n++)
		addition += data[n];
	if (addition == 0)
		return true;
	else
		return false;
}

void CCFS64::setChecksum(LPVOID structure)
{
	BYTE* data = (BYTE*)structure;
	BYTE addition = 0;
	data[1] = 0;
	for (int n = 0; n < 512; n++)
		addition += data[n];
	//Now we can work out what checksum must be
	data[1] = 0 - addition;
}

CCFS64::CFS64_FileEntry* CCFS64::openEntry(CFS64_FileEntry* curdir, TCHAR* name)
{
	if (!curdir)
		return 0;
	if (curdir->type != 0xD1)
		return 0;
	if (curdir->entries.Cluster == 0)		//Empty directory
		return 0;
	CFS64_DirInode* dirinode = 0;
	BYTE* inodecluster = 0;
	CFS64_FileEntry* entry = 0;
	BYTE* entrycluster = 0;
	readCluster(curdir->entries.Cluster, (LPVOID*)&inodecluster);
	dirinode = (CFS64_DirInode*)&inodecluster[curdir->entries.byte];
	//We need to be recursive because of indirect inodes
	int n = 0;
	do
	{
		if (!checkChecksum(dirinode))
			throw "Checksum invalid on directory inode";
		for (n = 0; n < 49 && dirinode->entries[n].Cluster; n++)
		{
			readCluster(dirinode->entries[n].Cluster, (LPVOID*)&entrycluster);
			entry = (CFS64_FileEntry*)&entrycluster[dirinode->entries[n].byte];
			if (!checkChecksum(entry))
				throw "Checksum invalid on directory entry";
			if (_tcscmp((wchar_t*)entry->fileName, name) == 0)
			{
				//We have our entry, return it
				delete[] inodecluster;
				CFS64_FileEntry* entrytoreturn = new CFS64_FileEntry;
				memcpy(entrytoreturn, entry, sizeof(CFS64_FileEntry));
				delete[] entrycluster;
				return entrytoreturn;
			}
			else
				delete[] entrycluster;
		}
		//Have we reached the last entry, or do we need to read next inode?
		if (n == 49)
		{
			//We need next inode
			if (dirinode->nextInode.Cluster == 0)		//We don't have a next inode
				break;
			//Now read the next inode
			//Temporary buffer, then delete inode to prevent memory leaks
			FSPointer nextinode = dirinode->nextInode;
			delete[] inodecluster;
			//Now read the next inode
			readCluster(nextinode.Cluster, (LPVOID*)&inodecluster);
			dirinode = (CFS64_DirInode*)&inodecluster[nextinode.byte];
		}
	} while (n == 49);
	//Not found
	return 0;
}

CCFS64::FSPointer CCFS64::createFSstructure(LPVOID data)
{
	FSPointer pointer = { 0 };
	CFS64_FileEntry* accessor = 0;
	BYTE* buf = 0;
	int n = 0;
	bool isLastEntry = false;
	do
	{
		pointer.Cluster = findFreeFSClust();
		if (pointer.Cluster == 0)
		{
			throw "Cannot create structure";
		}
		readCluster(pointer.Cluster, (LPVOID*)&buf);
		if (!isClusterUsed(pointer.Cluster))
			memset(buf,0,master->bytespercluster);
		accessor = (CFS64_FileEntry*)buf;
		for (n=0; n < master->bytespercluster / 512; n++)
		{
			if (accessor[n].type == 0)
				break;
			/*
			if (!checkChecksum(accessor))
				throw "Checksum invalid on free entry search";
			*/
		}
		if (n == master->bytespercluster / 512)
		{
			delete[] buf;
			setClusterUsed(pointer.Cluster);
			continue;
		}
		if (n == master->bytespercluster / 512 - 1)
		{
			isLastEntry = true;
		}
	} while (0);
	pointer.byte = n * 512;
	setChecksum(data);		//We do this here, it makes it easier
	memcpy(&buf[pointer.byte], data, 512);
	writeCluster(pointer.Cluster, (LPVOID*)&buf, true, true, false, isLastEntry);
	return pointer;
}

void CCFS64::addInodeEntry(CFS64_FileEntry* dir, FSPointer entry)
{
	int n = 0;
	if (!dir)
		return;
	if (dir->entries.Cluster == 0)
		return;
	if (dir->type != 0xD1)
		return;
	CFS64_DirInode* dirinode = 0;
	BYTE* inodecluster = 0;
	FSPointer inodeptr = dir->entries;
	do {
		if (inodecluster)
			delete[] inodecluster;
		readCluster(inodeptr.Cluster, (LPVOID*)&inodecluster);
		dirinode = (CFS64_DirInode*)&inodecluster[inodeptr.byte];
		if (!checkChecksum(dirinode))
			throw "Checksum invalid on directory inode";
		bool isLastEntry = false;
		for (n = 0; n < 49; n++)
		{
			if (dirinode->entries[n].Cluster == 0)
				break;
		}
		if (n == 49)		//We need to read next inode
		{
			if (dirinode->nextInode.Cluster == 0)		//No next inode, create it
			{
				CFS64_DirInode* newinode = new CFS64_DirInode;
				memset(newinode, 0, 512);
				newinode->backlink = inodeptr;
				newinode->type = DirectoryInode;
				FSPointer oldptr = inodeptr;
				inodeptr = createFSstructure(newinode);
				//Update the directory inode
				dirinode->nextInode = inodeptr;
				setChecksum(dirinode);
				//Write it to the disk
				BYTE* temp;
				readCluster(oldptr.Cluster, (LPVOID*)&temp);
				memcpy(&temp[oldptr.byte], dirinode, sizeof(CFS64_DirInode));
				writeCluster(oldptr.Cluster, (LPVOID*)&temp, false);
				delete newinode;
			}
			else
			{
				//Just read next inode address
				inodeptr = dirinode->nextInode;
			}
		}
	} while (n == 49);
	//We have a free entry, now enter our data
	dirinode->entries[n] = entry;
	setChecksum(dirinode);		//Needed explicitly because of direct write
	writeCluster(inodeptr.Cluster, (LPVOID*)&inodecluster, true, true, false, inodeptr.byte == (master->bytespercluster - 512));
}

void CCFS64::addInodeEntry(CFS64_FileEntry* file, uint64_t cluster)
{
	int n = 0;
	if (!file)
		return;
	if (file->entries.Cluster == 0)
		return;
	if (file->type != 0xF1)
		return;
	CFS64_FileInode* fileinode = 0;
	BYTE* inodecluster = 0;
	FSPointer inodeptr = file->entries;
	do {
		if (inodecluster)
			delete[] inodecluster;
		readCluster(inodeptr.Cluster, (LPVOID*)&inodecluster);
		fileinode = (CFS64_FileInode*)&inodecluster[inodeptr.byte];
		if (!checkChecksum(fileinode))
			throw "Checksum invalid on file inode";
		bool isLastEntry = false;
		for (n = 0; n < 61; n++)
		{
			if (fileinode->clusters[n] == 0)
				break;
		}
		if (n == 61)		//We need to read next inode
		{
			if (fileinode->nextInode.Cluster == 0)		//No next inode, create it
			{
				CFS64_FileInode* newinode = new CFS64_FileInode;
				memset(newinode, 0, 512);
				newinode->backlink = inodeptr;
				newinode->type = FileInode;
				FSPointer oldptr = inodeptr;
				inodeptr = createFSstructure(newinode);
				//Now we need to update the current inode to have the next inode
				fileinode->nextInode = inodeptr;
				//And we need to sort out the checksum again
				setChecksum(fileinode);
				//Write it to the disk
				BYTE* temp;
				readCluster(oldptr.Cluster, (LPVOID*)&temp);
				memcpy(&temp[oldptr.byte], fileinode, sizeof(CFS64_FileInode));
				writeCluster(oldptr.Cluster, (LPVOID*)&temp, false);
				delete newinode;
			}
			else
			{
				//Just read next inode address
				inodeptr = fileinode->nextInode;
			}
		}
	} while (n == 61);
	//We have a free entry, now enter our data
	fileinode->clusters[n] = cluster;
	setChecksum(fileinode);		//Again, needed explicitly
	writeCluster(inodeptr.Cluster, (LPVOID*)&inodecluster, true, true, false, inodeptr.byte == (master->bytespercluster - 512));
}

void CCFS64::setFileSize(CFS64_FileEntry* file, uint64_t bytes)
{
	int n = 0;
	if (!file)
		return;
	if (file->entries.Cluster == 0)
		return;
	if (file->type != 0xF1)
		return;
	CFS64_FileInode* fileinode = 0;
	BYTE* inodecluster = 0;
	FSPointer inodeptr = file->entries;
	do {
		if (inodecluster)
			delete[] inodecluster;
		readCluster(inodeptr.Cluster, (LPVOID*)&inodecluster);
		fileinode = (CFS64_FileInode*)&inodecluster[inodeptr.byte];
		if (!checkChecksum(fileinode))
			throw "Checksum invalid on file inode";
		bool isLastEntry = false;
		for (n = 0; n < 61; n++)
		{
			if (fileinode->clusters[n] == 0)
				break;
		}
		if (n == 61)		//We need to read next inode
		{
			if (fileinode->nextInode.Cluster == 0)		//No next inode
			{
				break;
			}
			else
			{
				//Just read next inode address
				inodeptr = fileinode->nextInode;
			}
		}
	} while (n == 61);
	fileinode->nextInode.byte = bytes % master->bytespercluster;
	setChecksum(fileinode);		//Again, needed explicitly
	writeCluster(inodeptr.Cluster, (LPVOID*)&inodecluster, true, true, false, inodeptr.byte == (master->bytespercluster - 512));
}


void CCFS64::addFile(filedesc* file, HANDLE hFile)
{
	//Adds a file or directory
	tcout << _T("Creating CFS64 file at \\") << file->filepath << endl;
	//Start with the root directory
	CFS64_FileEntry* dir = root;
	if (!checkChecksum(root))
		throw "Checksum invalid on root directory";
	BYTE* entrycluster = 0;	//The file entry cluster
	CFS64_FileEntry* entry = 0;
	//Read the directory inodes...
	TCHAR* pathpart = new TCHAR[file->filepath.length()];
	memset(pathpart, 0, (file->filepath.length()+1) * 2);
	file->filepath.copy(pathpart, file->filepath.length(), 0);
	while (_tcschr(pathpart, _T('\\')))
	{
		TCHAR* directory = pathpart;
		pathpart = _tcschr(pathpart, _T('\\'));
		*pathpart++ = 0;
		//Now we have the directory isolated. We will read for the directory
		//Read dir inode
		entry = openEntry(dir, directory);
		if (!entry)
			throw "File not found";
		if (dir != root)
			delete dir;
		dir = entry;
	}
	//Create entry
	tcout << _T("Creating ") << pathpart << endl;
	entry = new CFS64_FileEntry;
	memset(entry, 0, sizeof(CFS64_FileEntry));
	memcpy(entry->fileName, pathpart, _tcslen(pathpart)*2);
	entry->type = file->type;
	FSPointer entryaddr = createFSstructure(entry);
	//Create the inode
	if (file->type == 0xD1)
	{
		entry->size = NULL;
		CFS64_DirInode* inode = new CFS64_DirInode;
		memset(inode, 0, sizeof(CFS64_DirInode));
		inode->backlink = entryaddr;
		inode->type = DirectoryInode;
		CFS64_Attributes* attr = new CFS64_Attributes;
		memset(attr, 0, sizeof(attr));

		PSID pOwner = 0;
		PISID& pSOwner = (PISID&)pOwner;
		PSID pGroup = 0;
		PISID& pSGroup = (PISID&)pGroup;
		PACL pDacl = 0;		
		PSECURITY_DESCRIPTOR pSD = NULL;
		if (DWORD error = GetSecurityInfo(hFile, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&pOwner,&pGroup,&pDacl,NULL,&pSD) != ERROR_SUCCESS)
		{
			throw "Cannot access file securitty attributes";
		}
		FILE_NAME_INFO* info = (FILE_NAME_INFO*)malloc(4096);
		memset(info, 0, 4096);
		if (!GetFileInformationByHandleEx(hFile, FILE_INFO_BY_HANDLE_CLASS::FileNameInfo, info, 4096))
		{
			free(info);
			throw "Cannot get filename";
		}
		DWORD attributes = GetFileAttributes(info->FileName);
		free(info);
		if (attributes == INVALID_FILE_ATTRIBUTES)
			throw "Cannot get file attributes";
		if (attributes & FILE_ATTRIBUTE_HIDDEN)
			attr->flags.Hidden = 1;
		if (attributes & FILE_ATTRIBUTE_READONLY)
			attr->flags.DeleteOnly = attr->flags.ModifyOnly = 1;
		if (attributes & FILE_ATTRIBUTE_SYSTEM)
			attr->flags.SystemFile = 1;
		attr->ownerID.Data1 = 'SNIW';
		memcpy(&attr->ownerID.Data2, pSOwner, 12);
		attr->securityentry[0].isgroup = true;
		attr->securityentry[0].permissions.canExecute = attr->securityentry[0].permissions.canRead = attr->securityentry[0].permissions.canWrite = true;
		attr->securityentry[0].userID.Data1 = 'SNIW';
		memcpy(&attr->securityentry[0].userID.Data2, pSOwner, 12);
		attr->backlink = entryaddr;
		attr->accessed = attr->modified = attr->created = CTeratime::get_ttime();
		attr->type = AttributeInode;

		FSPointer inodeaddr = createFSstructure(inode);
		FSPointer attraddr = createFSstructure(attr);
		entry->entries = inodeaddr;
		entry->attrbiutes = attraddr;
		setChecksum(entry);		//Again, exlpicit
		BYTE* tempentry = 0;
		readCluster(entryaddr.Cluster, (LPVOID*)&tempentry);
		memcpy(&tempentry[entryaddr.byte], entry, 512);
		//Finalize write
		writeCluster(entryaddr.Cluster, (LPVOID*)&tempentry, false);
		delete inode;
		delete attr;
	}
	else if (file->type == 0xF1)
	{
		//Work out size
		union {
			UINT64 filesize;
			struct {
				DWORD filesizelow;
				DWORD filesizehigh;
			};
		};
		filesizelow = GetFileSize(hFile, &filesizehigh);
		entry->size = filesize;
		CFS64_FileInode* inode = new CFS64_FileInode;
		memset(inode, 0, 512);
		inode->backlink = entryaddr;
		inode->type = FileInode;
		CFS64_Attributes* attr = new CFS64_Attributes;
		memset(attr, 0, 512);

		PSID pOwner = 0;
		PISID& pSOwner = (PISID&)pOwner;
		PSID pGroup = 0;
		PISID& pSGroup = (PISID&)pGroup;
		PACL pDacl = 0;
		PSECURITY_DESCRIPTOR pSD = NULL;
		if (DWORD error = GetSecurityInfo(hFile, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &pOwner, &pGroup, &pDacl, NULL, &pSD) != ERROR_SUCCESS)
		{
			throw "Cannot access file securitty attributes";
		}
		FILE_NAME_INFO* info = (FILE_NAME_INFO*)malloc(4096);
		memset(info, 0, 4096);
		if (!GetFileInformationByHandleEx(hFile, FILE_INFO_BY_HANDLE_CLASS::FileNameInfo, info, 4096))
		{
			free(info);
			throw "Cannot get filename";
		}
		DWORD attributes = GetFileAttributes(info->FileName);
		free(info);
		if (attributes == INVALID_FILE_ATTRIBUTES)
			throw "Cannot get file attributes";
		if (attributes & FILE_ATTRIBUTE_HIDDEN)
			attr->flags.Hidden = 1;
		if (attributes & FILE_ATTRIBUTE_READONLY)
			attr->flags.DeleteOnly = attr->flags.ModifyOnly = 1;
		if (attributes & FILE_ATTRIBUTE_SYSTEM)
			attr->flags.SystemFile = 1;
		attr->ownerID.Data1 = 'SNIW';
		memcpy(&attr->ownerID.Data2, pSOwner, 12);
		attr->securityentry[0].isgroup = true;
		attr->securityentry[0].permissions.canExecute = attr->securityentry[0].permissions.canRead = attr->securityentry[0].permissions.canWrite = true;
		attr->securityentry[0].userID.Data1 = 'SNIW';
		memcpy(&attr->securityentry[0].userID.Data2, pSOwner, 12);
		attr->backlink = entryaddr;
		attr->accessed = attr->modified = attr->created = CTeratime::get_ttime();
		attr->type = AttributeInode;

		FSPointer inodeaddr = createFSstructure(inode);
		FSPointer attraddr = createFSstructure(attr);
		entry->entries = inodeaddr;
		entry->attrbiutes = attraddr;
		setChecksum(entry);		//...
		BYTE* tempentry = 0;
		readCluster(entryaddr.Cluster, (LPVOID*)&tempentry);
		memcpy(&tempentry[entryaddr.byte], entry, 512);
		//Finalize write
		writeCluster(entryaddr.Cluster, (LPVOID*)&tempentry, false, false);
		//Now we can actually start copying the file
		if (SetFilePointer(hFile, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER && GetLastError() != 0)
		{
			throw "Error seeking to location in file";
		}
		BYTE* filedata = new BYTE[master->bytespercluster];
		DWORD numbytesread = 0;
		for (int n = 0; n < (filesize + master->bytespercluster-1) / master->bytespercluster; n++)
		{
			memset(filedata, 0, master->bytespercluster);
			ReadFile(hFile, (LPVOID)filedata, master->bytespercluster, &numbytesread, NULL);
			uint64_t cluster = findFreeCluster();
			writeCluster(cluster, (LPVOID*)&filedata, true, false);
			//Now deal with the inode
			addInodeEntry(entry, cluster);
		}
		setFileSize(entry,filesize);
		delete inode;
	}
	//Write the entry
	//Now add our entry to the directory
	addInodeEntry(dir, entryaddr);
	//cleanup
	delete entry;
}

void CCFS64::format(uint64_t size)
{
	//CREATE MASTER TABLE
	master = new CFS64_Master;
	memset(master, 0, 512);
	master->bytespercluster = bytespercluster;
	master->creation = CTeratime::get_ttime();
	master->creator = CREATOR_ID_WINFORMATTER;
	master->disksize = size/master->bytespercluster;
	memcpy(master->FSID, "CFS64", 5);
	master->majorversion = majorid;
	master->minorversion = minorid;		//1.01
	master->FStabpointer = 1 + ((size + (master->bytespercluster*master->bytespercluster * 8 - 1)) / 
		(master->bytespercluster*master->bytespercluster * 8));		//Round up, above CAT, cluster pointer
	master->roottablepointer = master->FStabpointer + ((size + (master->bytespercluster*master->bytespercluster * 8 - 1)) /
		(master->bytespercluster*master->bytespercluster * 8));		//Round up, above FSTab, cluster pointer
	master->tablecopies = tablecopies;
	wchar_t* defvollabel = L"ChaiOS";
	memcpy(master->vollabel,defvollabel,6*2);
	//Clear CAT
	LPVOID zeros = new BYTE[master->bytespercluster];
	memset(zeros, 0, master->bytespercluster);
	for (int n = 0; n < master->FStabpointer; n++)
	{
		writeCluster(n, &zeros, false, false);
		writeCluster(n+master->FStabpointer, &zeros, false, false);
	}
	for (int n = 0; n < master->FStabpointer; n++)
	{
		setClusterUsed(n);
		setClusterUsed(n + master->FStabpointer);
	}
	//now write MASTER table
	LPVOID cluster = 0;
	readCluster(0, &cluster);
	memcpy(cluster, master, 512);
	writeCluster(0, &cluster);
	//CREATE ROOT TABLE
	root = new CFS64_FileEntry;
	memset(root, 0, sizeof(CFS64_FileEntry));
	memcpy(root->fileName, rootName, (wcslen(rootName+2)+2) * 2);
	root->type = DirectoryEntry;
	root->entries.Cluster = master->roottablepointer;
	root->entries.byte = 512;
	setChecksum(root);
	readCluster(master->roottablepointer, &cluster);
	memcpy(cluster, zeros, master->bytespercluster);
	memcpy(cluster, root, sizeof(CFS64_FileEntry));
	CFS64_DirInode* rootinode = new CFS64_DirInode;
	memset(rootinode, 0, sizeof(CFS64_FileEntry));
	rootinode->backlink.byte = 0;
	rootinode->backlink.Cluster = master->roottablepointer;
	rootinode->type = DirectoryInode;
	setChecksum(rootinode);
	memcpy((LPVOID)((BYTE*)cluster + 512), rootinode, 512);
	delete rootinode;
	writeCluster(master->roottablepointer, &cluster, true, true, false, false);

	delete [] zeros;
}