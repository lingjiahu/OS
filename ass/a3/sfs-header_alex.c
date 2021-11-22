#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAXFILENAME 16+1+3 /* max file name + max extension size */
#define BLOCK_SIZE 1024
#define MAXFILESIZE (12*BLOCK_SIZE) + ((BLOCK_SIZE/sizeof(int))*BLOCK_SIZE)

#define MAGIC 0xACBD0005
#define EMPTY -1

/*
1. maximum number of files that can be created on the root directory
2. maximum number of files that can be opened
3. maximum number of i-nodes
*/
#define MAX_FD 100

#define ROOT_DIREC_INODE 0

typedef struct dir_entry {
    int inode_index;
    char name[MAXFILENAME+1];
    bool isUsed;
} dir_entry;

typedef struct inode {
    int mode;
    int link_cnt;
    int size;
    int data_ptrs[12];
    int ind_ptrs;
    bool isUsed;
} inode;

typedef struct superblock {
    int magic;
    int block_size;
    int fs_size;
    int inode_table_length; /* # of blocks used to allocate Inode Table */
    int root_dir_inode;
} superblock;

typedef struct file_descriptor {
    int inode_index;
    inode* i_node;
    int wr_ptr;
    int r_ptr;
    bool isUsed;
} file_descriptor;

void mksfs(int fresh);                              // creates the file system
int sfs_getnextfilename(char *fname);               // get the name of the next file in directory
int sfs_getfilesize(const char* path);             	    // get the size of the given file
int sfs_fopen(char *name);                          // opens the given file
int sfs_fclose(int fileID);                         // closes the given file
int sfs_frseek(int fileID, int loc);                // seek (Read) to the location from beginning
int sfs_fwseek(int fileID, int loc);                // seek (Write) to the location from beginning
int sfs_fwrite(int fileID, char *buf, int length);  // write buf characters into disk
int sfs_fread(int fileID, char *buf, int length);   // read characters from disk into buf
int sfs_remove(char *file);                         // removes a file from the filesystem

void write_on_disk();

void init_open_fd_table ();
void init_inode_table ();
void update_superblock (int inode_t_len);
void init_root_table ();

void set_fd(int i, int inode_index, inode* i_node, int wr_ptr, int r_ptr);
int get_fd();
void rm_fd(int i);

void set_dir_entry(int i, int inode_index, char* name);
int get_dir_entry();
void rm_dir_entry(int i);

void set_inode(int i, int mode, int link_cnt, int size, int data_ptrs[12], int ind_ptrs);
int get_inode();
void rm_inode(int i);

int get_file(const char* name);
