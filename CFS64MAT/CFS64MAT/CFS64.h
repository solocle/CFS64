#pragma once
#include "stdafx.h"
#include "RawDisk.h"
#include "MBR.h"
#include "teratime.h"
#include "CFS64_filedesc.h"

class CCFS64
{
public:
	CCFS64(CRawDisk* disk);
	virtual ~CCFS64();
	virtual void format(uint64_t size);
	virtual void addFile(filedesc* desc, HANDLE hFile);
protected:
	virtual void readCluster(uint64_t cluster, LPVOID* buffer, bool autocreate = true);
	virtual void writeCluster(uint64_t cluster, LPVOID* buffer, bool setCAT = true, bool autodelete = true, bool freeing = false, bool FSfull = true);
	virtual void setClusterUsed(uint64_t cluster, bool freeing = false, bool FSFull = true);
	virtual bool isClusterUsed(uint64_t cluster);
	virtual bool isFSClusterUsed(uint64_t cluster);
	virtual bool checkChecksum(LPVOID structure);
	virtual void setChecksum(LPVOID structure);
	virtual uint64_t findFreeCluster();
	virtual uint64_t findFreeFSClust();
#pragma pack(push,1)
	//MASTER table
	struct CFS64_Master {
		uint8_t bldrjmp[2];		//JMP +0x40 (0x42)
		teratime creation;
		uint32_t creator;
		uint64_t disksize;		//Clusters
		uint16_t bytespercluster;
		uint8_t tablecopies;
		uint16_t majorversion;
		uint8_t minorversion;
		uint64_t FStabpointer;
		uint64_t roottablepointer;
		char FSID[8];			//CFS64
		char16_t vollabel[11];
		uint8_t bootloader[436];
		uint16_t bootsig;		//0xAA55
	};
	enum CreatorID {
		CREATOR_ID_CHAIOS,
		CREATOR_ID_WINFORMATTER,	//us
		CREATOR_ID_LINUX,
		CREATOR_ID_HANDCODED = 0xCAFEBABE,
		CREATOR_ID_THRID_PARTY = 0xDEADBEEF,
		CREATOR_ID_INCOMPATIBLE = 0xFFFFFFFF
	};
	static_assert(sizeof(CFS64_Master) == 512,"CFS64 Master table size error");
	//File tables
	struct FSPointer {
		uint64_t Cluster;
		uint16_t byte;
	};
	struct CFS64_FileEntry {
		uint8_t type;		//0xF1 for file, 0xD1 for directory,... 0 for free
		uint8_t checksum;
		wchar_t fileName[241];
		uint64_t size;		//Size for files, TODO: number of items (CURRENTLY NULL) for directories.
		FSPointer attrbiutes;
		FSPointer entries;
	};
	enum FileEntryType {
		SecurityInode = 0x50,
		DirectoryInode = 0xD0,
		DirectoryEntry = 0xD1,
		AttributeInode = 0xA0,
		ExtendedAttributeHeader = 0xA8,
		ExtendedAttributeInode = 0xAF,
		FileInode = 0xF0,
		FileEntry = 0xF1
	};
	static_assert(sizeof(CFS64_FileEntry) == 512, "CFS64 File table size error");
	struct CFS64_FileInode {
		uint8_t type;		//0xF1 for directory
		uint8_t checksum;
		uint64_t clusters[61];
		FSPointer backlink;
		FSPointer nextInode;
		WORD reserved;
	};
	static_assert(sizeof(CFS64_FileInode) == 512, "CFS64 File inode size error");
	struct CFS64_DirInode {
		uint8_t type;		//0xD1 for directory
		uint8_t checksum;
		FSPointer entries[49];
		FSPointer backlink;
		FSPointer nextInode;
	};
	static_assert(sizeof(CFS64_DirInode) == 512, "CFS64 Dir inode size error");
	//Now attributes
	struct CFS64_SecurityEntry {
		UUID userID;
		bool isgroup;
		struct {
			bool canRead : 1;
			bool canWrite : 1;
			bool canExecute : 1;
		}permissions;
	};
	static_assert(sizeof(CFS64_SecurityEntry) == 18, "CFS64 security entry size error");
	struct CFS64_Flags {
		uint8_t DeleteOnly : 1;
		uint8_t Hidden : 1;
		uint8_t ContentsHidden : 1;
		uint8_t NonPrivelegedRead : 1;
		uint8_t ModifyOnly : 1;
		uint8_t NonPrivilegedWrite : 1;
		uint8_t NonPrivilegedExecute : 1;
		uint8_t SystemFile : 1;
	};
	struct CFS64_Attributes {
		uint8_t type;
		uint8_t checksum;
		UUID ownerID;
		teratime created;
		teratime modified;
		teratime accessed;
		CFS64_Flags flags;
		CFS64_SecurityEntry securityentry[24];
		FSPointer securityInode;
		FSPointer xattr;
		FSPointer backlink;
		uint8_t reserved[7];
	};
	static_assert(sizeof(CFS64_Attributes) == 512, "CFS64 Attributes inode size error");
	struct CFS64_Security {
		uint8_t type;
		uint8_t checksum;
		CFS64_SecurityEntry entries[27];
		FSPointer backlink;
		FSPointer nextentry;
		DWORD reserved;
	};
	static_assert(sizeof(CFS64_Security) == 512, "CFS64 security size error");
	struct CFS64_xattr_entry {
		uint8_t type;
		uint8_t checksum;
		char name[256];		//type of attribute, so we don't need uniode
		uint8_t data[224];
		FSPointer inode;
		FSPointer backlink;
		FSPointer nextentry;
	};
	static_assert(sizeof(CFS64_xattr_entry) == 512, "CFS64 xattr size error");
	struct CFS64_xattr_inode {
		uint8_t type;
		uint8_t checksum;
		uint8_t data[490];
		FSPointer backlink;
		FSPointer nextentry;
	};
	static_assert(sizeof(CFS64_xattr_inode) == 512, "CFS64 xattr size error");
#pragma pack(pop)
	CRawDisk* m_pDisk;
	CFS64_Master* master;
	CFS64_FileEntry* root;
	wchar_t* rootName = L"\\\0ROOT: CFS64 root directory. DO NOT DESTROY, THIS WILL WIPE YOUR DRIVE. עם ישראל חי";
protected:
	virtual CFS64_FileEntry* openEntry(CFS64_FileEntry* curdir, TCHAR* name);
	virtual void addInodeEntry(CFS64_FileEntry* dir, FSPointer entry);
	virtual void addInodeEntry(CFS64_FileEntry* file, uint64_t cluster);
	virtual void setFileSize(CFS64_FileEntry* file, uint64_t bytes);
	virtual FSPointer createFSstructure(LPVOID data);
private:
	//Special values for this formatter. Might be specified later, and will be changed by derived classes
	const uint16_t bytespercluster = 4096;
	const uint16_t majorid = 1;
	const uint8_t minorid = 1;
	const uint8_t tablecopies = 1;
};

