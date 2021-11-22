#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAXFILENAME 16+1+3 /* max file name + max extension size */
#define BLOCK_SIZE 1024
#define MAXFILESIZE (12*BLOCK_SIZE) + ((BLOCK_SIZE/sizeof(int))*BLOCK_SIZE)

// create the file system
// fresh == 1: new; else: old
void mksfs(int fresh)
{ 

}

// 
int sfs_getnextfilename(char* fname)
{

}

int sfs_getfilesize(const char* path)
{

}

int sfs_fopen(char* name)
{

}

int sfs_fclose(int fildID)
{

}

int sfs_fwrite(int fildID, const char* buf, int length)
{

}

int sfs_fread(int fildID, char* buf, int length)
{

}

int sfs_fseek(int fildID, int loc)
{

}

int sfs_remove(char* file)
{

}
