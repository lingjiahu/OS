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
int RDIdx;                     // keep track of current position in directory for sfs_getnextfilename()
int RDCnt;                     // number of existing files
int openFileTable[NUM_INODES]; // entry of open file table is the read/ write pointer, index = inode number
char freebitmap[NUM_BLOCKS];   // a byte corresponds to a block, 1: free; 0: occupied

void initSuperBlock()
{
    strcpy(superBlock.magic, "0xACBD0005");
    superBlock.blockSize = BLOCK_SIZE;
    superBlock.FSSize = NUM_BLOCKS * BLOCK_SIZE;
    superBlock.iNodeTableLen = NUM_INODES * INODE_SIZE / BLOCK_SIZE + 1; // number of blocks for i node table
    superBlock.rootDir = 0;
}

void initINodeTable()
{
    for (int i = 0; i < NUM_INODES; i++)
    {
        INode inode;
        iNodeTable[i] = inode;
    }
}

// root directory: table of directory entries
// root directory is pointed to by an i node
// the i node is pointed to by the super block
void initRootDir()
{
    for (int i = 0; i < NUM_INODES - 1; i++)
    {
        iNodeTable[0].size = 0;
        rootDir[i].occupied = false;
    }
}

void initFreeBitMap()
{
    for (int i = 0; i < NUM_BLOCKS; i++)
    {
        // 1: free; 0: occupied
        freebitmap[i] = 1;
    }
}

void initOpenFileTable()
{
    for (int i = 0; i < NUM_INODES; i++)
    {
        openFileTable[i] = -1;
    }
}

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

        initFreeBitMap();
        freebitmap[NUM_BLOCKS - 1] = 0;

        initSuperBlock();
        write_blocks(0, 1, &superBlock); // write super block (1 block) to the first block of the disk
        freebitmap[0] = 0;

        initINodeTable();
        write_blocks(1, superBlock.iNodeTableLen, iNodeTable);
        for (int i = 1; i <= superBlock.iNodeTableLen; i++)
        {
            freebitmap[i] = 0;
        }

        initRootDir();
        write_blocks(superBlock.iNodeTableLen + 1, RD_BLOCKS, rootDir);
        for (int i = superBlock.iNodeTableLen + 1; i < (RD_BLOCKS - (superBlock.iNodeTableLen + 1)); i++)
        {
            freebitmap[i] = 0;
        }

        initOpenFileTable();

        write_blocks(NUM_BLOCKS - 1, 1, freebitmap);
    }
    else // open file system from disk
    {
        init_disk("disk_emu", BLOCK_SIZE, NUM_BLOCKS);

        read_blocks(0, 1, &superBlock); // read super block
        read_blocks(1, superBlock.iNodeTableLen, iNodeTable);
        read_blocks(superBlock.iNodeTableLen + 1, RD_BLOCKS, rootDir);
        read_blocks(NUM_BLOCKS - 1, 1, freebitmap);
    }
}

/*
copy the name of the next file in directory to fname
can be used to loop through a directory
function remebers current positon in directory at each call

input:
    fname: store the name of the next file in directory,
            store "\n" if there is no file to be returned.
return: 1 if there is a new file
        0 if all files have been returned

int sfs_getnextfilename(char *fname)
{
    RDIdx++;

    if (iNodeTable[0].size == 0)     // rd is empty
    {
        return 0;
    } else if ()    // no more file
    {
    }
}
*/

/*
get size of a given file

input: path of a file
return: size in bytes of a given file

int sfs_getfilesize(const char *path)
{
}
*/

/*
open a file in append mode with file discriptor at the end of the file
if file does not exits, create a new file with size = 0

input: 
    name: name of file to open
return: 
    index of the newly opened file in the file descriptor table on success,
    -1 otherwise
*/
int sfs_fopen(char *name)
{
    if (!name || strlen(name) > MAXFILENAME) // name != NULL && length < MAXFILENAME
    {
        return -1;
    }

    int fileIdx = -1; // index of file in root directory
    // search for file in root directory
    for (int i = 0; i < NUM_INODES - 1; i++)
    {
        if (rootDir[i].occupied)
        {
            if (strcmp(rootDir[i].fileName, name) == 0)
            {
                fileIdx = i;
            }
        }
    }

    if (fileIdx > -1) // open existing file in append mode with fd at EOF
    {
        // check if file is open TODO: whats the expected behavior
        for (int i = 0; i < NUM_INODES - 1; i++)
        {
            if (openFileTable[i] != -1)
            {
                return -1;
            }
        }

        // root directory is full - not supposed to happen, check just in case
        if (RDCnt == NUM_INODES - 1)
        {
            return -1;
        }

        int inode = rootDir[fileIdx].iNode;    // i node of file
        int filesize = iNodeTable[inode].size; // TODO: size of file == size of inode

        // get first available slot in open file table
        int openFileTableIdx;
        for (int i = 0; i < NUM_INODES - 1; i++)
        {
            if (openFileTable[i] == -1)
            {
                openFileTableIdx = i;
            }
        }

        openFileTable[openFileTableIdx] = filesize; // set fd to the end of the file
        return openFileTableIdx;
    }
    else // file does not exist, create a new file
    {
        // get first available slot in i node table
        int iNodeNum;
        for (int i = 0; i < NUM_INODES; i++)
        {
            if (!iNodeTable[i].occupied)
            {
                iNodeNum = i;
            }
        }

        // create directory entry
        DirectoryEntry dirEntry;
        dirEntry.iNode = iNodeNum;
        strcpy(dirEntry.fileName, name);
        dirEntry.occupied = true;

        // add directory entry to root directory
        rootDir[RDCnt] = dirEntry;
        RDCnt++;
        iNodeTable[0].size++;

        // get first available slot in open file table
        int openFileTableIdx;
        for (int i = 0; i < NUM_INODES - 1; i++)
        {
            if (openFileTable[i] == -1)
            {
                openFileTableIdx = i;
            }
        }

        openFileTable[openFileTableIdx] = 0;

        // write i node table & root dir
        write_blocks(1, superBlock.iNodeTableLen, iNodeTable);
        write_blocks(superBlock.iNodeTableLen + 1, RD_BLOCKS, rootDir);
        return openFileTableIdx;
    }
}

/*
close a file by removing its corresponding entry in the file descriptor table

input: 
    fileID: id of a file in file descriptor table
return: 
    0 on success
    -1 otherwise
*/
int sfs_fclose(int fileID)
{
    // check if file is open, if not, return -1
    if (openFileTable[fileID] == -1)
    {
        return -1;
    }

    openFileTable[fileID] = -1;
    return 0;
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
from the current file pointer

input:
    fileID: id of file to be read from, i node number
    buf: data to be read to
    length: max number of bytes to be read
return: 
    number of bytes read
*/
int sfs_fread(int fileID, char *buf, int length)
{
    int bytesRead = 0;
    // if file is not open, return 0 bytes read
    int rwPtr = openFileTable[fileID];
    if (rwPtr == -1)
    {
        return bytesRead;
    }

    INode inode = iNodeTable[fileID];

    // if file is empty or rwPtr at EOF
    if (inode.size == 0 || inode.size <= rwPtr)
    {
        return bytesRead;
    }

    INode inode = iNodeTable[fileID];

    // calculate number of blocks to be read
    int startBlock = rwPtr / BLOCK_SIZE;
    int startIdx = rwPtr % BLOCK_SIZE;
    int numBlocks = length / BLOCK_SIZE + 1; // number of blocks to be read
    int numBlocksRead = 0;

    // calculate bytes to read
    if (rwPtr + length > inode.size)    // current read/ write pointer location + length exceeds file size
    {
        bytesRead = length - rwPtr;
    } else
    {
        bytesRead = length;
    }

    char *blockBuffer[inode.size]; // store bytes read from start block to end block
    // read blocks within directPtr
    for (int i = startBlock; i < 12; i++)
    {
        read_blocks(inode.directPtr[startBlock], 1, blockBuffer + numBlocksRead * BLOCK_SIZE);
        numBlocks--;
        numBlocksRead++;
    }

    if (numBlocks >= 12) // read indirect blocks
    {
        // read indirect block
        int indBuffer[BLOCK_SIZE]; // store indirect block
        read_blocks(inode.indirectPtr, 1, indBuffer);
        for (int i = 0; i < numBlocks; i++)
        {
            read_blocks(indBuffer[i], 1, blockBuffer + numBlocksRead * BLOCK_SIZE);
            numBlocksRead++;
        }
    }

    memcpy(buf, blockBuffer + startIdx, bytesRead); // copy to buf
    // update rwPtr
    int newRWPtr = rwPtr + bytesRead;
    openFileTable[fileID] = newRWPtr;
    return bytesRead;
}

/*
move read/ write pointer to a given location

input:
    fileID: id of file to be read from
    loc: location of relocation
return:
    0 on success
    -1 otherwise
*/
int sfs_fseek(int fileID, int loc)
{
    // check if file is open, if not, return -1
    if (openFileTable[fileID] == -1)
    {
        return -1;
    }

    // check if loc is within the valid range, if not, return -1
    int filesize = iNodeTable[fileID].size;
    if (loc < 0 || loc >= filesize)
    {
        return -1;
    }
    openFileTable[fileID] = loc; // update read/ write pointer
    return 0;
}

/*
remove file from directory entry
release file allocation table entries and data blocks used by the file

input: 
    file: name of file to be removed
return:
    0 on success
    -1 otheriwse
*/
int sfs_remove(char *file)
{
    // find file in root directory
    DirectoryEntry dirEntry;
    int rootDirIdx = -1;
    for (int i = 0; i < NUM_INODES - 1; i++)
    {
        if (strcmp(rootDir[i].fileName, file) == 0)
        {
            dirEntry = rootDir[i];
            rootDirIdx = i;
        }
    }

    if (rootDirIdx == -1) // file is not found
    {
        return -1;
    }

    int inodeNum = dirEntry.iNode;
    if (openFileTable[inodeNum] != -1) // file is open
    {
        return -1;
    }

    INode iNode = iNodeTable[inodeNum];
    iNode.occupied = false; // remove from inode table

    rootDir[rootDirIdx].occupied = false; // remove from root dir
    // rearrange root dir - no gap in root dir
    for (int i = rootDirIdx; i < RDCnt; i++)
    {
        rootDir[i] = rootDir[i + 1];
    }

    // reset relevant bits to 1 in freebitmap
    int numBlocks = iNode.size / BLOCK_SIZE;
    for (int i = 0; i < 12 && i < numBlocks; i++) // free bits used for direct pointers
    {
        freebitmap[iNode.directPtr[i]] = 1;
    }
    if (numBlocks > 12) // indirect pointer is used, max: BLOCK_SIZE/sizeof(int)
    {
        int indirectPtrs[BLOCK_SIZE / sizeof(int)]; // TODO:256?
        read_blocks(iNode.indirectPtr, 1, indirectPtrs);
        // free blocks pointed by inodes inside the block pointed by indirectPtr
        for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++)
        {
            freebitmap[indirectPtrs[i]] = 1;
        }
        // free block pointed by indirectPtr
        freebitmap[iNode.indirectPtr] = 1;
    }

    // write to disk: i node table, root dir, freebitmap
    write_blocks(superBlock.iNodeTableLen + 1, RD_BLOCKS, rootDir);
    write_blocks(superBlock.iNodeTableLen + 1, RD_BLOCKS, rootDir);
    write_blocks(NUM_BLOCKS - 1, 1, freebitmap);
    return 0;
}
