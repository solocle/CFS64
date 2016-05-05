/* cfs64.c - CFS64 filesystem */
/*
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

#include <grub/fs.h>
#include <grub/disk.h>
#include <grub/file.h>
#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/charset.h>
#include <grub/cfs64.h>
#include <grub/i18n.h>

GRUB_MOD_LICENSE ("GPLv3+");

static grub_dl_t my_mod;

#define UNUSED(x) (void)(x)

struct grub_cfs64_data {
	grub_disk_t disk;
	grub_uint64_t disksize;
	grub_uint64_t roottab;
	grub_uint16_t bytespercluster;
	grub_uint16_t sectorspercluster;
	grub_uint16_t sectorsize;
	char16_t label[12];
	struct UUID uuid;
};

struct grub_cfs64_file_data {
	struct grub_cfs64_data* data;
	struct CFS64_FileEntry* fileent;
	grub_off_t finode_start_off;
	struct CFS64_FileInode* finode;
};

static grub_uint32_t power2(grub_uint32_t logarithm2)
{
	grub_uint32_t val = 1;
	grub_uint32_t n = 0;
	for(;n<logarithm2;n++)
		val *= 2;
	return val;
}

static grub_size_t grub_wcslen(const char16_t* str)
{
	if(!str)
		return 0;
	grub_size_t result = 0;
	while(str[result++]);
	return result;
}

static grub_err_t grub_cfs64_endian_fixup(const char* fmt, void* data, const grub_size_t len)
{
#ifdef GRUB_CPU_WORDS_BIGENDIAN
	grub_uint8_t* bdata = 0;
	bdata = (grub_uint8_t*)data;
	grub_size_t count = 0;
	grub_size_t offset = 0;
	/*Convert from little endian to big endian*/
	while(fmt[count] && offset < len)
	{
		//Parse the format string
		//Count digits following the unit size and convert to usable value
		unsigned int ndigits = 0;
		unsigned int repeatcount = 1;
		while(fmt[count+1+ndigits] >= '0' && fmt[count+1+ndigits] <= '9')
		{
			if(ndigits == 0)		//Just to avoid corruption, by default we have 1
				repeatcount = 0;
			repeatcount *= 10;
			repeatcount += fmt[count+1+ndigits] - '0';
			ndigits++;
		}
		for(unsigned int n = 0; n<repeatcount; n++)
		{
			if(fmt[count] == 'q')
			{
				//Quadword
				grub_uint64_t* qdata = (grub_uint64_t*)&bdata[offset];
				*qadta = grub_le_to_cpu64(*qdata);
				offset += 8;
			}
			else if(fmt[count] == 'd')
			{
				//doubleword
				grub_uint32_t* ddata = (grub_uint32_t*)&bdata[offset];
				*dadta = grub_le_to_cpu32(*ddata);
				offset += 4;
			}
			else if(fmt[count] == 'w')
			{
				//word
				grub_uint16_t* ddata = (grub_uint16_t*)&bdata[offset];
				*wadta = grub_le_to_cpu16(*wdata);
				offset += 2;
			}
			else if(fmt[count] == 'b')
			{
				//byte
				//*badta = grub_le_to_cpu8(*bdata);		//Does nothing
				offset += 1;
			}
			else if(fmt[count] == 'f')
			{
				//FSPointer
				grub_cfs64_endian_fixup(FSPointer_FMT, bdata, 10);
				offset += 10;
			}
		}
		count += ndigits + 1;
	}
	return 0;
#else
	UNUSED(fmt);
	UNUSED(data);
	UNUSED(len);
	/*Everything is OK already*/
	return 0;
#endif
}

static grub_err_t grub_cfs64_read_FSPointer(const struct grub_cfs64_data* data,
					 struct FSPointer* fsptr,
					 grub_size_t size,
					 void *buf, const char* fmt)
{
	grub_err_t err = 0;
	if((err = grub_disk_read(data->disk, fsptr->Cluster*data->sectorspercluster + fsptr->byte/512, fsptr->byte % 512, size, buf)))
		return err;
	//Now do endian fixup
	if(fmt)
	{
		grub_cfs64_endian_fixup(fmt, buf, size);
	}
	return err;
}

static grub_err_t grub_cfs64_read_cluster(const struct grub_cfs64_data* data,
					 grub_uint64_t clust,
					 void *buf)
{
	grub_uint16_t bytespersector = data->bytespercluster / data->sectorspercluster;
	grub_uint64_t n = 0;
	grub_err_t err = 0;
	for(;n<data->sectorspercluster;n++)
	{
		if((err = grub_disk_read(data->disk, clust*data->sectorspercluster + n, 0, bytespersector, &((char*)buf)[n*bytespersector])))
			return err;
	}
	return 0;
}

static grub_uint8_t grub_cfs64_mount_debug = 0;

static struct grub_cfs64_data *
grub_cfs64_mount (grub_disk_t disk)
{
	struct grub_cfs64_data *data = 0;
	struct CFS64_Master* master = 0;
	struct CFS64_FileEntry* rootdir = 0;

	if(!disk)
		goto fail;
	/*Allocate a mount data structure*/
	data = (struct grub_cfs64_data *) grub_malloc (sizeof (*data));
	/*Error?*/	
	if(!data)
		goto fail;
	
	grub_memset(data,0,sizeof(*data));

	data->disk = disk;

	/*Now read in CFS64 master table*/	
	master = (struct CFS64_Master*)grub_malloc(sizeof(*master));
	if(!master)
		goto fail;
#if 0
	if(grub_cfs64_mount_debug != 0)		//DEBUGGING
	{
		grub_error(GRUB_ERR_BUG, "Debugging breakpoint");
		goto fail;
	}
#endif

	/*Actual read of master table*/
	if (grub_disk_read (disk, 0, 0, sizeof (*master), master))
    	goto fail;
	
	if(grub_memcmp((const char*)master->FSID, "CFS64", 5) != 0)
		goto fail;

	grub_cfs64_endian_fixup(CFS64_Master_FMT,master,sizeof(*master));	

	data->disksize = master->disksize;
	data->roottab = master->roottablepointer;
	data->bytespercluster = master->bytespercluster;
	data->sectorsize = power2(disk->log_sector_size);
	data->sectorspercluster = data->bytespercluster / data->sectorsize;
	/*Some sanity checks*/
	if(data->disksize == 0)
		goto fail;
	if(data->roottab == 0)
		goto fail;
	
	/*Grab the volume label*/
	grub_uint16_t vlabeln = 0;
	for(vlabeln = 0; vlabeln < sizeof(master->vollabel)/sizeof(*master->vollabel); vlabeln++)
		data->label[vlabeln] = master->vollabel[vlabeln];
	
	/*invent a UUID based on stuff likely to be unique*/
	data->uuid.Data1 = (grub_uint32_t)master->creation;
	data->uuid.Data2 = (grub_uint16_t)master->FStabpointer;
	data->uuid.Data3 = (grub_uint16_t)master->roottablepointer;
	grub_memcpy(data->uuid.Data4,data->label,8);
	
	/*check the root directory for sanity*/
	rootdir = (struct CFS64_FileEntry*)grub_malloc(sizeof(*rootdir));
	if(!rootdir)
		goto fail;
	if(grub_disk_read(disk, data->roottab*data->sectorspercluster, 0, sizeof(*rootdir), rootdir))
		goto fail;
	
	grub_cfs64_endian_fixup(CFS64_FileEntry_FMT,rootdir,sizeof(*rootdir));

	if((rootdir->type != DirectoryEntry))
		goto fail;
	
	/*Free temporary stuff*/
	grub_free(master);
	grub_free(rootdir);
	
	return data;

/*FAILURE HANDLING*/
fail:
	grub_free(data);
	grub_free(master);
	grub_free(rootdir);
	grub_error (GRUB_ERR_BAD_FS, "not a valid CFS64 filesystem");
	return 0;
}
static struct CFS64_FileEntry* get_file(struct grub_cfs64_data* data, struct CFS64_FileEntry* directory, const char* name, int (*hook) (const char *filename,
			   const struct grub_dirhook_info *info));
static struct CFS64_FileEntry* get_file(struct grub_cfs64_data* data, struct CFS64_FileEntry* directory, const char* name, int (*hook) (const char *filename,
			   const struct grub_dirhook_info *info))
{
	struct CFS64_FileEntry* retv = NULL;
	struct CFS64_FileEntry* temp = NULL;
	struct CFS64_DirInode* dirinode = NULL;
	
	//For code compatibility with copied code from old verison of dir()
	struct CFS64_FileEntry* dirent = directory;

	if(!directory || (directory->type != DirectoryEntry))
	{
		grub_error (GRUB_ERR_BAD_FILENAME, "not a valid path");
		goto error;
	}

	temp = grub_malloc(sizeof(*temp));
	if(!temp)
		goto error;
	dirinode = grub_malloc(sizeof(*dirinode));
	if(!dirinode)
		goto error;
	
	if(!directory->entries.Cluster)
	{
		grub_error (GRUB_ERR_BAD_FILENAME, "not a valid path");
		goto error;
	}
	if(grub_cfs64_read_FSPointer(data, &directory->entries, sizeof(*dirinode), dirinode, CFS64_DirInode_FMT))
	{
		goto error;
	}
	/* //Read in dir inode entries */
	struct FSPointer* currentptr = NULL;
	grub_uint8_t found = 0;
	grub_uint64_t lastptr = 0;
	do {
		currentptr = &dirinode->nextInode;
		lastptr = currentptr->Cluster;
		unsigned int fileptrindex = 0;
		/* //Search this inode */
		for(fileptrindex = 0;fileptrindex<(sizeof(dirinode->entries) / sizeof(dirinode->entries[0]));fileptrindex++)
		{
			if(!dirinode->entries[fileptrindex].Cluster)
				continue;
			/* //Read in file header temporarily */
			if(grub_cfs64_read_FSPointer(data, &dirinode->entries[fileptrindex], sizeof(*temp), temp, CFS64_FileEntry_FMT))
			{
				grub_free(temp);
				grub_free(dirent);
				grub_free(dirinode);
				goto error;
			}
			if(hook)
			{
				//We list all files. Note that, by not touching "found", we will not break
				/* //Now get the name... change memory resident name to native endian, then utf16 to utf8 */
				unsigned int namen = 0;
				for(namen = 0; namen < sizeof(temp->fileName)/sizeof(temp->fileName[0]); namen++)
					temp->fileName[namen] = temp->fileName[namen];
				grub_size_t lenutf8 = grub_wcslen(temp->fileName) * 4;
				if(lenutf8 == 0)
					continue;
				char* nameutf8 = grub_malloc(lenutf8 + 1);
				if(!nameutf8)
				{
					grub_free(temp);
					grub_free(dirent);
					grub_free(dirinode);
					goto error;
				}
				*grub_utf16_to_utf8((grub_uint8_t *)nameutf8,temp->fileName,grub_wcslen(temp->fileName)) = (grub_uint8_t)'\0';

				/* //Now the actual data */
				struct grub_dirhook_info info;
				if(temp->type == DirectoryEntry)
					info.dir = 1;
				else
					info.dir = 0;
				/*To save time we don't grab mtime*/
				info.case_insensitive = info.mtime = info.mtimeset = 0;
				hook(nameutf8, &info);
				grub_free(nameutf8);
			}
			else
			{
				/* //Now compare name to what we're looking for... change memory resident name to native endian, then utf16 to utf8 */
				unsigned int namen = 0;
				for(namen = 0; namen < sizeof(temp->fileName)/sizeof(*temp->fileName); namen++)
					temp->fileName[namen] = temp->fileName[namen];
				grub_size_t lenutf8 = grub_wcslen(temp->fileName) * 4;
				if(lenutf8 == 0)
					continue;
				char* nameutf8 = grub_malloc(lenutf8 + 1);
				if(!nameutf8)
				{
					grub_free(temp);
					grub_free(dirent);
					grub_free(dirinode);
					goto error;
				}
				*grub_utf16_to_utf8((grub_uint8_t *)nameutf8,temp->fileName,grub_wcslen(temp->fileName)) = (grub_uint8_t)'\0';
				/* //Now the actual comparison */
				if(grub_strcmp(nameutf8, name) == 0)
				{
					found = 1;grub_free(nameutf8);break;
				}
				else
					grub_free(nameutf8);
			}
		}
		if(found)
			break;
		if(lastptr)
		{
			if(grub_cfs64_read_FSPointer(data, &dirinode->nextInode, sizeof(*dirinode), dirinode, CFS64_DirInode_FMT))
			{
					grub_free(temp);
					grub_free(dirent);
					grub_free(dirinode);
					goto error;
			}
		}
	}while(lastptr);
	retv = temp;
error:
	grub_free(dirinode);
	if(!retv)
		grub_free(temp);
	return retv;
}

static struct CFS64_FileEntry* walk_path(struct grub_cfs64_data* data, struct CFS64_FileEntry* directory, const char* path)
{
	struct CFS64_FileEntry* curdir = directory;
	struct CFS64_FileEntry* temp =curdir;
	//Create editable version of path
	grub_size_t pathlen = grub_strlen(path);
	char* pathscratch = grub_malloc(pathlen+1);
	if(!pathscratch)
	{
		/* //grub_error (GRUB_ERR_BAD_FS, "not a valid CFS64 filesystem");		//TODO: Make this error more appropriate */
		goto error;
	}
	grub_memcpy(pathscratch, path, pathlen+1);
	//Now we walk the path
	char* nxtfile = pathscratch;
	char* slash = NULL;
	while((slash = grub_strchr(nxtfile, '/')) || (slash = grub_strchr(nxtfile, '\0')))
	{
		char slashval = *slash;
		*slash = 0;
		//grub_printf("%s",nxtfile);
		if(nxtfile[0])		//Basically if we have a filename between two slashes
		{
			temp = get_file(data, curdir, nxtfile, NULL);
			if(!temp)
				goto error;
			
		}

		if(temp)
		{
			if(curdir != directory)
				grub_free(curdir);
			curdir = temp;
			temp = NULL;
		}
		nxtfile = ++slash;
		
		if(slashval == '\0')		//Last file read
			break;
	}
	return curdir;
error:
	return NULL;
}

static grub_off_t grub_cfs64_filesize(struct grub_cfs64_data* data, struct CFS64_FileEntry* file)
{
#if 1
	UNUSED(data);
	return (grub_off_t)file->size;
#else
	if(file->type != FileEntry || !file->entries.Cluster)
		return 0;
	//Walk the inodes and add cluster size for each cluster.
	struct CFS64_FileInode* inode = NULL;
	inode = grub_malloc(sizeof(*inode));
	if(!inode)
		return 0;
	if(grub_cfs64_read_FSPointer(data, &file->entries, sizeof(*inode), inode))
	{
		grub_free(inode);
		return 0;
	}
	struct FSPointer* currentptr = NULL;
	grub_uint64_t lastptr = 0;
	grub_off_t filesize = 0;
	do {
		currentptr = &inode->nextInode;
		lastptr = currentptr->Cluster;
		unsigned int fileptrindex = 0;
		/* //Search this inode */
		for(fileptrindex = 0;fileptrindex<(sizeof(inode->clusters) / sizeof(inode->clusters[0]));fileptrindex++)
		{
			if(!inode->clusters[fileptrindex])
				continue;
			/* Add the cluster size for this */
			filesize += data->bytespercluster;
		}
		if(lastptr)
		{
			if(grub_cfs64_read_FSPointer(data, &inode->nextInode, sizeof(*inode), inode))
			{
					grub_free(inode);
					return 0;
			}
		}
	}while(lastptr);
	return filesize;
#endif
}

static grub_err_t
grub_cfs64_dir (grub_device_t device, const char *path,
	      int (*hook) (const char *filename,
			   const struct grub_dirhook_info *info))
{
	struct grub_cfs64_data* data = NULL;
	struct CFS64_FileEntry* temp = NULL;
	grub_disk_t disk = device->disk;

	grub_dl_ref(my_mod);

	data = grub_cfs64_mount(disk);
	if(data)
	{
		/* //Walk the path. */
		struct CFS64_FileEntry* dirent = 0;
		/* //First read in ROOT */
		dirent = grub_malloc(sizeof(*dirent));
		if(!dirent)
		{
			/* //grub_error (GRUB_ERR_BAD_FS, "not a valid CFS64 filesystem");		//TODO: Make this error more appropriate */
			goto error;
		}
			
		if(grub_disk_read(disk, data->roottab*data->sectorspercluster, 0, sizeof(*dirent), dirent))
		{
			grub_free(dirent);
			/* //grub_error (GRUB_ERR_BAD_FS, "not a valid CFS64 filesystem");		//TODO: Make this error more appropriate */
			goto error;
		}
		
		temp = walk_path(data, dirent, path);
		if(!temp)
			goto error;
		
		/* //If we haven't hit an error by now, we have the directory
		//So now we return all its contents */

		get_file(data, temp, NULL, hook);
#if 0		
		grub_free(dirent);
#endif		
	}
	else {
		goto error;
	}

error:

	grub_dl_unref(my_mod);
	grub_free(data);
	grub_free(temp);
	return grub_errno;
}

static grub_err_t
grub_cfs64_open (grub_file_t file, const char *name)
{
	struct grub_cfs64_data *data = NULL;
	struct grub_cfs64_file_data *fdat = NULL;
	struct CFS64_FileEntry* fileent = NULL;
	struct CFS64_FileInode* finode = NULL;
	struct CFS64_FileEntry* dirent = NULL;

	grub_dl_ref (my_mod);
	//Mount the disk
	grub_cfs64_mount_debug = 1; //DEBUGGING
	data = grub_cfs64_mount(file->device->disk);
	if(!data)
		goto fail;
	
	//Now we need to find our file
	/* //Walk the path. */
	
	/* //First read in ROOT */
	dirent = grub_malloc(sizeof(*dirent));
	if(!dirent)
	{
		/* //grub_error (GRUB_ERR_BAD_FS, "not a valid CFS64 filesystem");		//TODO: Make this error more appropriate */
		goto fail;
	}

	finode = grub_malloc(sizeof(*finode));
	if(!finode)
	{
		goto fail;
	}
		
	if(grub_disk_read(file->device->disk, data->roottab*data->sectorspercluster, 0, sizeof(*dirent), dirent))
	{
		/* //grub_error (GRUB_ERR_BAD_FS, "not a valid CFS64 filesystem");		//TODO: Make this error more appropriate */
		goto fail;
	}

	fileent = walk_path(data, dirent, name);
	if(!fileent)
		goto fail;
	//Just read the first inode
	//Now we have our file open, we store this in our struct and return
	fdat = grub_malloc(sizeof(*fdat));
	if(!fdat)
		goto fail;

	if(grub_cfs64_read_FSPointer(data, &fileent->entries, sizeof(*finode), finode, CFS64_FileInode_FMT))
	{
		/* //grub_error (GRUB_ERR_BAD_FS, "not a valid CFS64 filesystem");		//TODO: Make this error more appropriate */
		goto fail;
	}
	
	fdat->data = data;
	fdat->fileent = fileent;
	fdat->finode = finode;
	fdat->finode_start_off = 0;
	file->data = fdat;
	//Compute file size
	file->size = grub_cfs64_filesize(data, fileent);
#if 0
	grub_free(dirent);
#endif
	return GRUB_ERR_NONE;
fail:
	grub_free(data);
#if 1
	grub_free(dirent);
#endif
	grub_free(fileent);
	grub_free(finode);
	grub_free(fdat);
	grub_dl_unref (my_mod);
	return grub_errno;
}

static grub_ssize_t
grub_cfs64_read (grub_file_t file, char *buf, grub_size_t len)
{
	if(!file || !file->data)
		return 0;

	/*grub_printf("Length to read: %d", (int)len);*/

	grub_size_t bytesread = 0;
	
	struct grub_cfs64_data *data = NULL;
	struct grub_cfs64_file_data *fdat = NULL;

	//Working stuff
	fdat = file->data;
	data = fdat->data;

	//If inode is not read, read it (first inode)
	if(!fdat->finode)
	{
		fdat->finode_start_off = 0;
		fdat->finode = grub_malloc(sizeof(*fdat->finode));
		if(!fdat->finode)
			goto error;
		if(grub_cfs64_read_FSPointer(data, &fdat->fileent->entries, sizeof(*fdat->finode), fdat->finode, CFS64_FileInode_FMT))
		{
			/* //grub_error (GRUB_ERR_BAD_FS, "not a valid CFS64 filesystem");		//TODO: Make this error more appropriate */
			goto error;
		}		
	}

	grub_size_t clustersperinode = (sizeof(fdat->finode->clusters)/sizeof(fdat->finode->clusters[0]));
	grub_size_t bytesperinode = clustersperinode*data->bytespercluster;

	//Now we work out where to read
	//First, if we're ahead, work backwards
	
	while(fdat->finode_start_off >= file->offset + bytesperinode)
	{
		//Read the backlink
		struct CFS64_FileInode* oldinode = fdat->finode;
		if(grub_cfs64_read_FSPointer(data, &fdat->finode->backlink, sizeof(*fdat->finode), fdat->finode, CFS64_FileInode_FMT))
		{
			fdat->finode = 0;
			/* //grub_error (GRUB_ERR_BAD_FS, "not a valid CFS64 filesystem");		//TODO: Make this error more appropriate */
			goto error;
		}
		grub_free(oldinode);
		//Reduce offset to correspond
		fdat->finode_start_off -= bytesperinode;
	}
	//Now, if we're behind, read ahead
	while(file->offset < fdat->finode_start_off)
	{
		//Read the next inode
		struct CFS64_FileInode* oldinode = fdat->finode;
		if(grub_cfs64_read_FSPointer(data, &fdat->finode->nextInode, sizeof(*fdat->finode), fdat->finode, CFS64_FileInode_FMT))
		{
			fdat->finode = 0;
			/* //grub_error (GRUB_ERR_BAD_FS, "not a valid CFS64 filesystem");		//TODO: Make this error more appropriate */
			goto error;
		}
		grub_free(oldinode);
		//Add to offset to correspond
		fdat->finode_start_off += bytesperinode;
	}
	//Now, the start of the read is within the current inode. We start actually reading.
	grub_off_t offsetinode = file->offset - fdat->finode_start_off;
	grub_off_t startbyte = 0;
	grub_off_t startcluster = grub_divmod64(offsetinode, data->bytespercluster, &startbyte);

	/*grub_printf(", offset in inode: %d, file offset: %d\n", (int)offsetinode, (int)fdat->finode_start_off);*/

	//Temporary value
	grub_uint8_t* clusterval = grub_malloc(data->bytespercluster);
	grub_uint8_t* bytebuf = (grub_uint8_t*)buf;
	if(!clusterval)
		goto error;
	while(len > bytesread)
	{
		for(;(len > bytesread) && (startcluster < clustersperinode); startcluster++)
		{
			grub_size_t leninclust = data->bytespercluster - startbyte;
			if(leninclust > len-bytesread)
				leninclust = len-bytesread;
			/*grub_printf("Reading cluster at 0x%x (%d), with length: %d, offset: %d\n", (int)fdat->finode->clusters[startcluster], (int)startcluster, (int)leninclust, (int)startbyte);*/
			if(grub_cfs64_read_cluster(data, fdat->finode->clusters[startcluster], clusterval))
				goto error;
			/*grub_printf("Successfully read\n");*/
			//Now copy what we want
			grub_memcpy(&bytebuf[bytesread], &clusterval[startbyte], leninclust);
			startbyte = 0;
			bytesread += leninclust;
		}
		//Reset counters
		startcluster = offsetinode = startbyte = 0;
		//Read next inode
		if(len > bytesread)
		{
			//grub_printf("Reading in the next inode at %d:%d\n", (int)fdat->finode->nextInode.Cluster, (int)fdat->finode->nextInode.byte);
			if(grub_cfs64_read_FSPointer(data, &fdat->finode->nextInode, sizeof(*fdat->finode), fdat->finode, CFS64_FileInode_FMT))
			{
				fdat->finode = 0;
				/* //grub_error (GRUB_ERR_BAD_FS, "not a valid CFS64 filesystem");		//TODO: Make this error more appropriate */
				goto error;
			}
			fdat->finode_start_off += bytesperinode;
		}
	}

	grub_free(clusterval);
	return bytesread;
error:
	return -bytesread;
}

static grub_err_t
grub_cfs64_close (grub_file_t file)
{
	struct grub_cfs64_file_data *fdat = (struct grub_cfs64_file_data*)file->data;
	grub_free(fdat->fileent);
	grub_free(fdat->finode);
	grub_free(fdat->data);
	grub_free(fdat);

	grub_dl_unref (my_mod);
	return grub_errno;
}

static grub_err_t
grub_cfs64_label (grub_device_t device, char **label)
{
	struct grub_cfs64_data* data = 0;
	data = grub_cfs64_mount(device->disk);

	grub_dl_ref(my_mod);

	*label = 0;
	
	if(data)
	{
		char16_t lbl[12] = {0};
		grub_ssize_t lbllen = sizeof(lbl)*4;
		char* lbla = grub_malloc(lbllen);
		grub_memset(lbl,0,sizeof(lbl));
		grub_memcpy(lbl,data->label,sizeof(data->label));
		*grub_utf16_to_utf8((grub_uint8_t *)lbla,lbl,sizeof(lbl)) = (grub_uint8_t)'\0';
		*label = lbla;
	}

	grub_dl_unref(my_mod);

	grub_free(data);
	return grub_errno;
}

static grub_err_t
grub_cfs64_uuid (grub_device_t device, char **uuid)
{
	UNUSED(uuid);

	struct grub_cfs64_data* data = 0;
	data = grub_cfs64_mount(device->disk);

	grub_dl_ref(my_mod);

	if(data)
	{
		char* ptr;
		grub_uint64_t* data4u64 = (grub_uint64_t*)data->uuid.Data4;
		*uuid = grub_xasprintf("%08x-%04x-%04x-%08x%08x", data->uuid.Data1, data->uuid.Data2, data->uuid.Data3, (grub_uint32_t)*data4u64, (grub_uint32_t)(*data4u64>>32));
		for (ptr = *uuid; ptr && *ptr; ptr++)
	*ptr = grub_toupper (*ptr);
	}
	else
	{
		*uuid = NULL;
	}

	grub_dl_unref(my_mod);

	grub_free(data);
	return grub_errno;
}

static struct grub_fs grub_cfs64_fs =
  {
    .name = "CFS64",
    .dir = grub_cfs64_dir,
    .open = grub_cfs64_open,
    .read = grub_cfs64_read,
    .close = grub_cfs64_close,
    .label = grub_cfs64_label,
    .uuid = grub_cfs64_uuid,
#ifdef GRUB_UTIL
    .reserved_first_sector = 1,
    .blocklist_install = 1,
#endif
    .next = 0
  };


GRUB_MOD_INIT(cfs64)
{
  COMPILE_TIME_ASSERT (sizeof (struct CFS64_Master) == 512);
  grub_fs_register (&grub_cfs64_fs);
  my_mod = mod;
}

GRUB_MOD_FINI(cfs64)
{
  grub_fs_unregister (&grub_cfs64_fs);
}
