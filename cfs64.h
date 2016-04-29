/*  cfs64.h - CFS64 filesystem
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000,2001,2002,2003,2004,2005,2007,2008,2009  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#ifndef GRUB_CFS64_H
#define GRUB_CFS64_H
 
#include <grub/types.h>

 typedef grub_uint64_t teratime;
 typedef grub_uint16_t wchar_t;
 typedef grub_uint16_t char16_t;

//MASTER table
struct CFS64_Master {
	grub_uint8_t bldrjmp[2];		//JMP +0x40 (0x42)
	teratime creation;
	grub_uint32_t creator;
	grub_uint64_t disksize;		//Clusters
	grub_uint16_t bytespercluster;
	grub_uint8_t tablecopies;
	grub_uint16_t majorversion;
	grub_uint8_t minorversion;
	grub_uint64_t FStabpointer;
	grub_uint64_t roottablepointer;
	char FSID[8];			//CFS64
	char16_t vollabel[11];
	grub_uint8_t bootloader[436];
	grub_uint16_t bootsig;		//0xAA55
}__attribute__ ((packed));

const char* CFS64_Master_FMT = "b2qdqwbwbq2b8w11b436w";

enum CreatorID {
	CREATOR_ID_CHAIOS,
	CREATOR_ID_WINFORMATTER,	//us
	CREATOR_ID_LINUX,
	CREATOR_ID_HANDCODED = 0xCAFEBABE,
	CREATOR_ID_THIRD_PARTY = 0xDEADBEEF,
	CREATOR_ID_INCOMPATIBLE = 0xFFFFFFFF
};

//File tables
struct FSPointer {
	grub_uint64_t Cluster;
	grub_uint16_t byte;
}__attribute__ ((packed));

const char* FSPointer_FMT = "qw";

struct CFS64_FileEntry {
	grub_uint8_t type;		//0xF1 for file, 0xD1 for directory,... 0 for free
	grub_uint8_t checksum;
	wchar_t fileName[241];
	grub_uint64_t size;
	struct FSPointer attrbiutes;
	struct FSPointer entries;
}__attribute__ ((packed));

const char* CFS64_FileEntry_FMT = "b2w241qf2";

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

struct CFS64_FileInode {
	grub_uint8_t type;		//0xF1 for directory
	grub_uint8_t checksum;
	grub_uint64_t clusters[61];
	struct FSPointer backlink;
	struct FSPointer nextInode;
	grub_uint16_t reserved;
}__attribute__ ((packed));

const char* CFS64_FileInode_FMT = "b2q61f2w";

struct CFS64_DirInode {
	grub_uint8_t type;		//0xD1 for directory
	grub_uint8_t checksum;
	struct FSPointer entries[49];
	struct FSPointer backlink;
	struct FSPointer nextInode;
}__attribute__ ((packed));

const char* CFS64_DirInode_FMT = "b2f51";

//Now attributes

struct UUID {
    grub_uint32_t Data1;
    grub_uint16_t Data2;
    grub_uint16_t Data3;
    grub_uint8_t Data4[ 8 ];
}__attribute__ ((packed));

struct CFS64_SecurityEntry {
	struct UUID userID;
	grub_uint8_t isgroup;
	struct {
		grub_uint8_t canRead : 1;
		grub_uint8_t canWrite : 1;
		grub_uint8_t canExecute : 1;
	}permissions;
}__attribute__ ((packed));

struct CFS64_Flags {
	grub_uint8_t DeleteOnly : 1;
	grub_uint8_t Hidden : 1;
	grub_uint8_t ContentsHidden : 1;
	grub_uint8_t NonPrivelegedRead : 1;
	grub_uint8_t ModifyOnly : 1;
	grub_uint8_t NonPrivilegedWrite : 1;
	grub_uint8_t NonPrivilegedExecute : 1;
	grub_uint8_t SystemFile : 1;
}__attribute__ ((packed));

struct CFS64_Attributes {
	grub_uint8_t type;
	grub_uint8_t checksum;
	struct UUID ownerID;
	teratime created;
	teratime modified;
	teratime accessed;
	struct CFS64_Flags flags;
	struct CFS64_SecurityEntry securityentry[24];
	struct FSPointer securityInode;
	struct FSPointer xattr;
	struct FSPointer backlink;
	grub_uint8_t reserved[7];
}__attribute__ ((packed));

struct CFS64_Security {
	grub_uint8_t type;
	grub_uint8_t checksum;
	struct CFS64_SecurityEntry entries[27];
	struct FSPointer backlink;
	struct FSPointer nextentry;
	grub_uint32_t reserved;
}__attribute__ ((packed));

struct CFS64_xattr_entry {
	grub_uint8_t type;
	grub_uint8_t checksum;
	char name[256];		//type of attribute, so we don't need unicode
	grub_uint8_t data[224];
	struct FSPointer inode;
	struct FSPointer backlink;
	struct FSPointer nextentry;
}__attribute__ ((packed));

struct CFS64_xattr_inode {
	grub_uint8_t type;
	grub_uint8_t checksum;
	grub_uint8_t data[490];
	struct FSPointer backlink;
	struct FSPointer nextentry;
}__attribute__ ((packed));

#endif
