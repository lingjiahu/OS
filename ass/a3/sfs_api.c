/*
Implementation of a Simple File System, which can be mounted over an emulated disk system.
Guaranteed to work in Linux.
Single level directory - no subdirectories.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "sfs_api.h"
#include "disk_emu.h"

SuperBlock superBlock;
INode iNodeTable[NUM_INODES];
DirectoryEntry rootDir[NUM_INODES - 1];
int fileCnt; // keep track of current position in directory for sfs_getnextfilename()
OpenFile openFileTable[NUM_INODES];

void initSuperBlock()
{
    strcpy(superBlock.magic, "0xACBD0005");
    superBlock.blockSize = BLOCK_SIZE;
    superBlock.FSSize = NUM_BLOCKS;
    superBlock.iNodeTableLen = NUM_INODES * INODE_SIZE / BLOCK_SIZE;
    superBlock.rootDir = 0;
}

void initINodeTable()
{
    for (int i = 0; i < NUM_INODES; i++)
    {
        iNodeTable[i].occupied = false;
    }
}

// root directory: table of directory entries
// root directory is pointed to by an i node
// the i node is pointed to by the super block
void initRootDir()
{
    for (int i = 0; i < NUM_INODES-1; i++)
    {
        rootDir[i].occupied = false;
    }
}

// void initOpenFileTable()
// {
//     for (int i = 0; i < NUM_INODES; i++)
//     {
//         openFileTable[i].occupied = false;
//     } 
// }

/* 
format virtual disk & create a file system on top of the virtual disk

input: 
    fresh: whether file system should be created from scratch
        fresh == true: new file system
        fresh == false: open existing file system from disk
*/
void mksfs(int fresh)
{
    if (fresh) // create new file system
    {
        init_fresh_disk("disk_emu", BLOCK_SIZE, NUM_BLOCKS);
        initSuperBlock();
        write_blocks(0, 1, &superBlock); // write super block (1 block) to the first block of the disk

        initINodeTable();
        write_blocks(1, superBlock.iNodeTableLen, iNodeTable);

        initRootDir();
        write_blocks(superBlock.iNodeTableLen+1, , )

    }
    else // open file from disk
    {
        init_disk("disk_emu", BLOCK_SIZE, NUM_BLOCKS);
    }
}

/*
copy the name of the next file in directory to fname
can be used to loop through a directory
function remebers current positon in directory at each call TODO: global var?

input:
    fname: store the name of the next file in directory,
            store "\n" if there is no file to be returned.
return: 1 if there is a new file
        0 if all files have been returned
*/
int sfs_getnextfilename(char *fname)
{
}

/*
get size of a given file

input: path of a file
return: size in bytes of a given file
*/
int sfs_getfilesize(const char *path)
{
}

/*
open a file in append mode with file discriptor at the end of the file
if file does not exits, create a new file with size = 0

input: 
    name: name of file to open
return: index of the newly opened file in the file descriptor table
*/
int sfs_fopen(char *name)
{
}

/*
close a file by removing its corresponding entry in the file descriptor table

input: 
    fileID: id of a file in file descriptor table
return: 
    0 on success
    negative value (-1) otherwise
*/
int sfs_fclose(int fileID)
{
}

/*
write buf characters to disk
write data in buf to file from the current file pointer
increase the size of the file, the amount of increase depends on the current file pointer

input: 
    fileID: id of file to write to
    buf: data to be written
    length: size of buf
return: number of bytes written
*/
int sfs_fwrite(int fileID, const char *buf, int length)
{
}

/*
read characters from disk to buf
TODO: from the current file pointer?

input:
    fileID: id of file to be read from
    buf: data to be read to
    length: size of characters to be read
return: number of bytes read
*/
int sfs_fread(int fileID, char *buf, int length)
{
}

/*
move read/ write pointer to a given location

input:
    fileID: id of file to be read from
    loc: location of relocation
return:
    0 on success
    negative value (-1) otherwise
*/
int sfs_fseek(int fileID, int loc)
{
}

/*
remove file from directory entry
release file allocation table entries and data blocks used by the file

input: 
    file: name of file to be removed
return:
    TODO:?
*/
int sfs_remove(char *file)
{
}
