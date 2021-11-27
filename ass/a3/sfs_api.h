#ifndef SFS_API_H
#define SFS_API_H

// You can add more into this file.

void mksfs(int);

int sfs_getnextfilename(char*);

int sfs_getfilesize(const char*);

int sfs_fopen(char*);

int sfs_fclose(int);

int sfs_fwrite(int, const char*, int);

int sfs_fread(int, char*, int);

int sfs_fseek(int, int);

int sfs_remove(char*);

#define MAXFILENAME 20  // filename (16) + dot (1) + extension (3)
#define NUM_BLOCKS 1024
#define BLOCK_SIZE 1024
#define NUM_INODES 
#define INODE_SIZE
#define MAXFILESIZE (12*BLOCK_SIZE) + ((BLOCK_SIZE/sizeof(int))*BLOCK_SIZE)

typedef struct
{
    int magic;
    int blockSize;
    int FSSize;
    int iNodeTableLen;
    int rootDir;
} SuperBlock;

typedef struct
{
    int size;
    int directPtr[12];
    int indirectPtr;
} INode;

typedef struct 
{
    int iNode;
    char fileName[MAXFILENAME];
} DirectoryEntry;

// entry in open file descriptor table
typedef struct 
{
    int iNode;
    int rwPtr;
} OpenFile;


#endif
