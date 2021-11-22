
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "bitmap.h"
#include "sfs_api.h"
#include "disk_emu.h"

dir_entry root[MAX_FD]; /* root directory table */
inode inode_t[MAX_FD]; /* inode table */
file_descriptor open_fd_t[MAX_FD]; /* open file descriptor table */
int free_bitmap[NB_ROWS] = {[0 ... NB_ROWS - 1] = UINT8_MAX}; /* free bit map */

superblock sb;

int indirect_block[BLOCK_SIZE / sizeof(int)]; /* indirect pointer block that is pointing to indirect blocks */

int dir_walker = 0; /* pointer to keep track of curr location in directory */
int count = 0;
int numBlcksInodeT, numBlcksRoot;

/* initialize all elements in indirect block with EMPTY value */
void init_ind_block()
{
	for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++)
	{
		indirect_block[i] = EMPTY;
	}
}

/* write on disk all modifications */
void write_on_disk()
{
	int counter = 0;

	write_blocks(counter, 1, &sb);
	counter++;
	write_blocks(counter, numBlcksInodeT, &inode_t);
	counter += numBlcksInodeT;
	write_blocks(counter, numBlcksRoot, &root);

	write_blocks(CONST_BM_POS, 1, &free_bitmap);
}

/* initialize open file descriptor table with maximum number of files that can be opened */
void init_open_fd_table()
{
	for (int i = 0; i < MAX_FD; i++)
	{
		rm_fd(i);
	}
}

/* initialize inode table with maximum number of files that can be created */
void init_inode_table()
{
	for (int i = 0; i < MAX_FD; i++)
	{
		rm_inode(i);
	}
}

/* initialize super block which contains inode number that points to the root */
void update_superblock(int inode_t_len)
{
	sb.magic = MAGIC;
	sb.block_size = BLOCK_SIZE;
	sb.fs_size = NB_BLOCKS * BLOCK_SIZE;
	sb.inode_table_length = inode_t_len;
	sb.root_dir_inode = ROOT_DIREC_INODE;
}

/* initialize root directory */
void init_root_table()
{
	for (int i = 0; i < MAX_FD; i++)
	{
		rm_dir_entry(i);
	}
}

/* create a file descriptor in Open File Descriptor Table with these attributes
first parameter is the fileID */
void set_fd(int i, int inode_index, inode *i_node, int wr_ptr, int r_ptr)
{
	open_fd_t[i].inode_index = inode_index;
	open_fd_t[i].i_node = i_node;
	open_fd_t[i].wr_ptr = wr_ptr;
	open_fd_t[i].r_ptr = r_ptr;

	open_fd_t[i].isUsed = true;
}

/* get the first available spot in Open File Descriptor Table and return the index of it */
int get_fd()
{
	int fileID = -1;
	int i = 0;
	while (open_fd_t[i].isUsed != false && i < MAX_FD)
	{
		i++;
	}
	fileID = i;
	if (fileID < 0 || i == MAX_FD)
	{
		return -1;
	}
	else
	{
		return fileID;
	}
}

/* remove specified file descriptor from Open File Descriptor Table */
void rm_fd(int i)
{
	open_fd_t[i].inode_index = EMPTY;
	open_fd_t[i].i_node = NULL;
	open_fd_t[i].wr_ptr = EMPTY;
	open_fd_t[i].r_ptr = EMPTY;
	open_fd_t[i].isUsed = false;
}

/* create a directory entry with those specific attributes
first parameter is the index in the Root Directory */
void set_dir_entry(int i, int inode_index, char *name)
{
	root[i].inode_index = inode_index;
	memcpy(root[i].name, name, strlen(name) + 1);
	root[i].isUsed = true;
}

/* find the instance of the first available space in Root Directory and return the index of it */
int get_dir_entry()
{
	int index = -1;
	int i = 0;
	while (root[i].isUsed != false && i < MAX_FD)
	{
		i++;
	}
	index = i;
	if (index < 0 || i == MAX_FD)
	{
		return -1;
	}
	else
	{
		return index;
	}
}

/* remove the specified directory entry from Root Directory */
void rm_dir_entry(int i)
{
	root[i].inode_index = EMPTY;
	memset(root[i].name, 0, sizeof(root[i].name));
	root[i].isUsed = false;
}

/* create an inode with those specific attributes
first parameter is the index in the Inode Table */
void set_inode(int i, int mode, int link_cnt, int size, int data_ptrs[12], int ind_ptrs)
{
	inode_t[i].mode = mode;
	inode_t[i].link_cnt = link_cnt;
	inode_t[i].size = size;
	inode_t[i].ind_ptrs = ind_ptrs;

	for (int j = 0; j < 12; j++)
	{
		inode_t[i].data_ptrs[j] = data_ptrs[j];
	}

	inode_t[i].isUsed = true;
}

/* find the first available spot in Inode Table and return the index of it */
int get_inode()
{
	int index = -1;
	int i = 0;
	while (inode_t[i].isUsed != false && i < MAX_FD)
	{
		i++;
	}
	index = i;
	if (index < 0 || i == MAX_FD)
	{
		return -1;
	}
	else
	{
		return index;
	}
}

/* remove inode i from the Inode Table*/
void rm_inode(int i)
{
	inode_t[i].mode = EMPTY;
	inode_t[i].link_cnt = EMPTY;
	inode_t[i].size = EMPTY;
	inode_t[i].ind_ptrs = EMPTY;

	for (int j = 0; j < 12; j++)
	{
		inode_t[i].data_ptrs[j] = EMPTY;
	}

	inode_t[i].isUsed = false;
}

/* set index in bitmap to used */
void set_index(int i)
{
	int index = i / ROW_SIZE;
	int bit = i % ROW_SIZE;

	int temp = free_bitmap[index] & ~(1 << (7 - bit));
	free_bitmap[index] = temp;
}

/* find the first free data block and return the index */
int get_index()
{
	int index = -1;
	int i = 0;
	/* find first non-full row in free bit map*/
	while (free_bitmap[i] == 0 && i < NB_ROWS)
	{
		i++;
	}
	/* all data blocks are used */
	if (i == 128)
	{
		return -1;
	}
	else
	{
		int n = free_bitmap[i];
		int b;
		/* get the leftmost 1 bit */
		for (b = 0; n > 0; b++)
		{
			n = n / 2;
		}
		index = ROW_SIZE - b;
	}
	/* something went wrong... */
	if (index < 0)
	{
		return -1;
	}
	else
	{
		return index + (i * ROW_SIZE);
	}
}

/* frees index in bitmap */
void rm_index(int i)
{
	int index = i / ROW_SIZE;
	int bit = i % ROW_SIZE;

	int temp = free_bitmap[index] | (1 << (7 - bit));
	free_bitmap[index] = temp;
}

/* find given file in Root Directory and return its position in the Directory Table*/
int get_file(const char *name)
{
	for (int i = 0; i < MAX_FD; i++)
	{
		if (root[i].isUsed == true)
		{
			if (strcmp(root[i].name, name) == 0)
			{
				return i;
			}
		}
	}
	return -1;
}

/* creates the file system */
void mksfs(int fresh)
{
	int counter = 0;

	if (fresh)
	{
		init_fresh_disk("disk.sfs", BLOCK_SIZE, NB_BLOCKS);

		init_root_table();
		init_inode_table();
		init_open_fd_table();

		/* calculate number of blocks needed to allocate Inode Table */
		int remainder = (sizeof(inode_t) % BLOCK_SIZE);
		if (remainder == 0)
		{
			numBlcksInodeT = (sizeof(inode_t) / BLOCK_SIZE);
		}
		else
		{
			numBlcksInodeT = ((int)(sizeof(inode_t) / BLOCK_SIZE)) + 1;
		}
		/* calculate number of blocks needed to allocate Root Directory */
		remainder = (sizeof(root) % BLOCK_SIZE);
		if (remainder == 0)
		{
			numBlcksRoot = (sizeof(root) / BLOCK_SIZE);
		}
		else
		{
			numBlcksRoot = ((int)(sizeof(root) / BLOCK_SIZE)) + 1;
		}

		/* initialize and write super block on disk*/
		update_superblock(numBlcksInodeT);

		write_blocks(counter, 1, &sb);
		/* also update free bit map */
		set_index(counter);
		counter++;

		/* write Inode Table on disk */
		write_blocks(counter, numBlcksInodeT, &inode_t);
		/* also update free bit map*/
		for (int i = counter; i < numBlcksInodeT + counter; i++)
		{
			set_index(i);
		}
		counter += numBlcksInodeT;

		int data_ptrs[12];
		for (int i = 0; i < numBlcksRoot; i++, counter++)
		{
			data_ptrs[i] = counter;
			/* update bitmap */
			set_index(counter);
		}
		/* create inode pointing to root directory */
		set_inode(ROOT_DIREC_INODE, 0, numBlcksRoot, 0, data_ptrs, EMPTY);

		/* write Root Directory on disk */
		write_blocks(inode_t[ROOT_DIREC_INODE].data_ptrs[0], numBlcksRoot, &root);

		/* write free bitmap on disk */
		write_blocks(CONST_BM_POS, 1, &free_bitmap);
	}
	else
	{
		init_disk("disk.sfs", BLOCK_SIZE, NB_BLOCKS);

		/* read first block from disk and write into super block*/
		read_blocks(counter, 1, &sb);
		counter++;

		numBlcksInodeT = sb.inode_table_length;
		/* read Inode Table from disk*/
		read_blocks(counter, numBlcksInodeT, &inode_t);
		counter += numBlcksInodeT;

		numBlcksRoot = inode_t[ROOT_DIREC_INODE].link_cnt;
		/* read Root Directory from disk*/
		read_blocks(counter, numBlcksRoot, &root);

		/* read free bitmap from disk */
		read_blocks(CONST_BM_POS, 1, &free_bitmap);
	}
}

/* get the name of the next file in directory */
int sfs_getnextfilename(char *fname)
{
	/* if empty directory */
	if (inode_t[ROOT_DIREC_INODE].size == 0) {
		return 0;
	}
	else {
		/* if reached end of Root Directory */
		if ( count == inode_t[ROOT_DIREC_INODE].size ) {
			dir_walker = 0;
			count = 0;
			return 0;
		}
		else {
			/* find the most recent used direc entry */
			while ( root[dir_walker].isUsed != true) {
				dir_walker++;
				if ( dir_walker == MAX_FD ){
					return 0;
				}
			}
			strcpy(fname, root[dir_walker].name);
			dir_walker++;
			count++;
			return 1;
		}
	}
}

/* get the size of the given file */
int sfs_getfilesize(const char *path)
{
	int dir_index = get_file(path);
	/* if file doesn't exist, return ERROR */
	if (dir_index == -1)
	{
		return -1;
	}
	else
	{
		int inode_index = root[dir_index].inode_index;
		return inode_t[inode_index].size;
	}
}

/* opens the given file */
int sfs_fopen(char *name)
{
	int dir_index = -1;
	int inode_index = -1;
	int fileID = -1;

	/* if name is invalid, return ERROR */
	if (strlen(name) > MAXFILENAME || strlen(name) < 0)
	{
		return -1;
	}

	dir_index = get_file(name);

	/* file doesn't exist, need to create it */
	if (dir_index < 0)
	{
		//printf("FILE DOESNT EXIST\n");
		dir_index = get_dir_entry();
		/* if no more space in Root Directory */
		if (dir_index < 0)
		{
			return -1;
		}

		inode_index = get_inode();
		/* if no more space in Inode Table */
		if (inode_index < 0)
		{
			return -1;
		}

		fileID = get_fd();
		if (fileID < 0)
		{
			/* if no more space in Open File Descriptor Table */
			return -1;
		}
		set_dir_entry(dir_index, inode_index, name);

		inode_t[ROOT_DIREC_INODE].size++;
		/* since we're adding a new file in the Root Directory, must augment its size */

		int data_ptrs[12];
		for (int i = 0; i < 12; i++)
		{
			data_ptrs[i] = EMPTY;
		}
		set_inode(inode_index, 0, 0, 0, data_ptrs, EMPTY);
		set_fd(fileID, inode_index, &(inode_t[inode_index]), 0, 0);
		write_on_disk();
	}
	/* file exists already */
	else
	{
		//printf("FILE EXISTS ALREADY \n");
		inode_index = root[dir_index].inode_index;

		/* make sure file isn't already opened */
		for (int i = 0; i < MAX_FD; i++)
		{
			if (open_fd_t[i].isUsed == true)
			{
				if (open_fd_t[i].inode_index == inode_index)
				{
					/* if it is already opened return ERROR */
					return -1;
				}
			}
		}
		/* file isn't already opened */
		fileID = get_fd();
		if (fileID < 0)
		{
			/* if no more space in Open File Descriptor Table */
			return -1;
		}
		/* when opening an existing file
		read pointer points at the beginning of the file
		write pointer points at the end of the file */
		set_fd(fileID, inode_index, &(inode_t[inode_index]), inode_t[inode_index].size, 0);
	}
	return fileID;
}

/* closes given file */
int sfs_fclose(int fileID)
{
	/* check first if valid file ID */
	if (fileID > MAX_FD || fileID < 0 || open_fd_t[fileID].isUsed == false)
	{
		return -1;
	}
	else
	{
		rm_fd(fileID);
		return 0;
	}
}

/* seek (Read) to the location from beginning */
int sfs_frseek(int fileID, int loc)
{
	/* check first if valid file ID */
	if (fileID > MAX_FD || fileID < 0 || open_fd_t[fileID].isUsed == false)
	{
		return -1;
	}
	int file_size = open_fd_t[fileID].i_node->size;
	if (loc > file_size || loc < 0)
	{
		/* if location out of boundaries, return ERROR*/
		return -1;
	}
	else
	{
		open_fd_t[fileID].r_ptr = loc;
		return 0;
	}
}

/* seek (Write) to the location from beginning */
int sfs_fwseek(int fileID, int loc)
{
	/* check first if valid file ID */
	if (fileID > MAX_FD || fileID < 0 || open_fd_t[fileID].isUsed == false)
	{
		return -1;
	}
	int file_size = open_fd_t[fileID].i_node->size;
	if (loc > file_size || loc < 0)
	{
		/* if location out of boundaries, return ERROR*/
		return -1;
	}
	else
	{
		open_fd_t[fileID].wr_ptr = loc;
		return 0;
	}
}

/* write buf characters into disk */
int sfs_fwrite(int fileID, char *buf, int length)
{
	//printf("Orginal length = %d\n", length);

	/* check first if valid file ID */
	if (fileID > MAX_FD || fileID < 0 || open_fd_t[fileID].isUsed == false)
	{
		return -1;
	}

	int bm_index, ind_block_index, new_size;
	int inode_index = open_fd_t[fileID].inode_index;
	int file_size = inode_t[inode_index].size;

	int w_ptr = open_fd_t[fileID].wr_ptr;
	int bytesToWrite = length;
	int newBlocks; /* number of new data blocks we're going to need to write buf into file */

	/* if writing buf into the file, we'll reach MAXFILESIZE
	then will only write into the file until reached MAXFILESIZE */
	if (w_ptr + length > MAXFILESIZE)
	{
		new_size = MAXFILESIZE;
		bytesToWrite = MAXFILESIZE - w_ptr;
		if (bytesToWrite == 0)
		{
			/* no more space in file */
			/* return 0 byte written */
			return 0;
		}
		newBlocks = bytesToWrite / BLOCK_SIZE;
		if (bytesToWrite % BLOCK_SIZE != 0)
		{
			newBlocks++;
		}
	}
	else if (w_ptr + length <= file_size)
	{
		/* if only overwritting whats already written in file */
		new_size = file_size;
		newBlocks = 0;
	}
	else
	{
		/* if writing length would increase the file size */
		new_size = w_ptr + length;
		newBlocks = new_size / BLOCK_SIZE;
		if (new_size % BLOCK_SIZE != 0)
		{
			newBlocks++;
		}
		newBlocks = newBlocks - inode_t[inode_index].link_cnt;
	}

	void *ind = malloc(BLOCK_SIZE);
	/* initialize indirect pointer block if we need it */
	if (new_size > (12 * BLOCK_SIZE) && inode_t[inode_index].ind_ptrs == EMPTY)
	{
		//printf("INDIRECT POINTER DOESN'T EXIST AND WE NEED\n");
		init_ind_block(); /* initiliaze the indirect block */
		ind_block_index = get_index();
		if (ind_block_index < 0)
		{
			/* no more available blocks to allocate the indirect block */
			new_size = 12 * BLOCK_SIZE;
			bytesToWrite = new_size - w_ptr;
			newBlocks = bytesToWrite / BLOCK_SIZE;
			if (bytesToWrite % BLOCK_SIZE != 0)
			{
				newBlocks++;
			}
		}
		else
		{
			open_fd_t[fileID].i_node->ind_ptrs = ind_block_index;
			/* mark bitmap index as used */
			set_index(ind_block_index);
		}
	}
	else if (new_size > (12 * BLOCK_SIZE) && inode_t[inode_index].ind_ptrs != EMPTY)
	{
		/* if indirect pointer block already exists, retrieve it */
		ind_block_index = inode_t[inode_index].ind_ptrs;
		read_blocks(ind_block_index, 1, ind);
		memcpy(indirect_block, ind, BLOCK_SIZE);
		//printf("INDIRECT POINTER EXITS AT INDEX %d\n", ind_block_index);
	}
	free(ind);

	/* if need to allocate new disk blocks */
	if (newBlocks > 0)
	{
		for (int i = inode_t[inode_index].link_cnt; i < inode_t[inode_index].link_cnt + newBlocks; i++)
		{
			bm_index = get_index();
			if (bm_index < 0)
			{
				/* no more available blocks */
				newBlocks = inode_t[inode_index].link_cnt - i;
				break;
			}
			/* modify file's inode to point to these blocks */
			if (i < 12)
			{
				inode_t[inode_index].data_ptrs[i] = bm_index;
			}
			else
			{
				indirect_block[i - 12] = bm_index;
			}
			/* mark bitmap index as used */
			set_index(bm_index);
		}
	}

	/* find which block and where in block to start reading */
	int curr_block = w_ptr / BLOCK_SIZE;
	int block_offset = w_ptr % BLOCK_SIZE;

	void *buffer = malloc((inode_t[inode_index].link_cnt + newBlocks) * BLOCK_SIZE);
	/* write into buffer what's already written in the file starting from the block we're interested in */
	int i = curr_block;
	while (i < inode_t[inode_index].link_cnt + newBlocks)
	{
		if (i < 12)
		{
			/* reading form direct blocks */
			read_blocks(inode_t[inode_index].data_ptrs[i], 1, buffer + ((i - curr_block) * BLOCK_SIZE));
		}
		else
		{
			/* reading from indirect pointers */
			read_blocks(indirect_block[i - 12], 1, buffer + ((i - curr_block) * BLOCK_SIZE));
		}
		i++;
	}

	/* copy onto buffer what we want to write into the file
    	on top of whats already written */
	memcpy(buffer + block_offset, buf, bytesToWrite);

	/* update inote's link count */
	inode_t[inode_index].link_cnt += newBlocks;
	/* start writing on disk */
	i = curr_block;
	while (i < inode_t[inode_index].link_cnt)
	{
		if (i < 12)
		{
			/* write into direct blocks */
			write_blocks(inode_t[inode_index].data_ptrs[i], 1, buffer);
		}
		else
		{
			/* write into indirect blocks */
			write_blocks(indirect_block[i - 12], 1, buffer + ((i - curr_block) * BLOCK_SIZE));
		}
		i++;
	}
	free(buffer);

	/* update inode's size */
	inode_t[inode_index].size = new_size;
	/* update writer pointer */
	open_fd_t[fileID].wr_ptr += bytesToWrite;
	/* if indirect pointers were used,
	write it into the disk */
	if (inode_t[inode_index].link_cnt > 12)
	{
		write_blocks(ind_block_index, 1, &indirect_block);
	}
	/* flush all modifications to disk */
	write_on_disk();
	/*return number of bytes written */
	return bytesToWrite;
}

/* read characters from disk into buf */
int sfs_fread(int fileID, char *buf, int length)
{
	/* check first if valid file ID */
	if (fileID > MAX_FD || fileID < 0 || open_fd_t[fileID].isUsed == false)
		return -1;

	int inode_index = open_fd_t[fileID].inode_index;
	int r_ptr = open_fd_t[fileID].r_ptr;
	int file_size = inode_t[inode_index].size;
	int bytesToRead = length;
	int blocksToRead = -1;

	/* if file empty or have to read 0 bytes, return 0 bytes read */
	if (file_size == 0 || length == 0)
	{
		return 0;
	}
	/* if reading the whole length will make read ptr go out of boundaries */
	if (r_ptr + length > file_size)
	{
		bytesToRead = file_size - r_ptr;
		blocksToRead = file_size / BLOCK_SIZE;
		if (file_size / BLOCK_SIZE != 0)
		{
			blocksToRead++;
		}
	}
	else
	{ /* if reading the whole length will not make read ptr go out of boundaries */
		blocksToRead = (r_ptr + length) / BLOCK_SIZE;
		if ((r_ptr + length) / BLOCK_SIZE != 0)
		{
			blocksToRead++;
		}
	}

	/* find which block and where in block to start reading */
	int curr_block = r_ptr / BLOCK_SIZE;
	int block_offset = r_ptr % BLOCK_SIZE;

	void *ind = malloc(BLOCK_SIZE);
	/* copy indirect block in case we need it */
	if (inode_t[inode_index].ind_ptrs != EMPTY)
	{
		read_blocks(inode_t[inode_index].ind_ptrs, 1, ind);
		memcpy(&indirect_block, ind, BLOCK_SIZE);
	}
	free(ind);

	void *buffer = malloc(inode_t[inode_index].link_cnt * BLOCK_SIZE);
	/* start reading */
	int i = curr_block;
	while (i < inode_t[inode_index].link_cnt)
	{
		if (i < 12)
		{
			/* reading form direct blocks */
			read_blocks(inode_t[inode_index].data_ptrs[i], 1, buffer + ((i - curr_block) * BLOCK_SIZE));
		}
		else
		{
			/* reading from indirect pointers */
			read_blocks(indirect_block[i - 12], 1, buffer + ((i - curr_block) * BLOCK_SIZE));
		}
		i++;
	}

	/* copy bytes from buffer starting from the offset to buf until the desired length */
	memcpy(buf, buffer + block_offset, bytesToRead);

	/* update the read pointer */
	open_fd_t[fileID].r_ptr += bytesToRead;
	/* return number of bytes read */
	return bytesToRead;
}

/* removes a file from the filesystem */
int sfs_remove(char *file)
{
	int index, i;
	int inode_index = -1;

	int dir_index = get_file(file);
	/* if no such file in Root Directory, return ERROR */
	if (dir_index < 0)
	{
		return -1;
	}
	else
	{
		inode_index = root[dir_index].inode_index;
	}
	/* make sure we aren't removing a file that's currently opened */
	for (int i = 0; i < MAX_FD; i++)
	{
		if (open_fd_t[i].isUsed == true)
		{
			if (open_fd_t[i].inode_index == inode_index)
			{
				/* if it is open, return ERROR */
				return -1;
			}
		}
	}

	/* release all data blocks used by the file */
	for (i = 0; i < inode_t[inode_index].link_cnt && i < 12; i++)
	{
		rm_index(inode_t[inode_index].data_ptrs[i]);
	}

	/* if file has indirect pointers */
	if (inode_t[inode_index].ind_ptrs != EMPTY)
	{
		index = inode_t[inode_index].ind_ptrs;
		/* release the block pointed by the indirect pointer*/
		rm_index(index);

		void *buffer = malloc(BLOCK_SIZE);
		read_blocks(index, 1, buffer);
		memcpy(&indirect_block, buffer, BLOCK_SIZE);

		/* release all blocks the indirect pointer was pointing to */
		for (int i = 12; i < inode_t[inode_index].link_cnt; i++)
		{
			rm_index(indirect_block[i - 12]);
		}
	}

	/* clear its inode entry */
	rm_inode(inode_index);
	/* clear its directory entry */
	rm_dir_entry(dir_index);
	/* decrement Root Directory size */
	inode_t[ROOT_DIREC_INODE].size -= 1;
	/* flush all modifications to disk */
	write_on_disk();

	return 0;
}
