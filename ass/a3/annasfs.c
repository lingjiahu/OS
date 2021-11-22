#include "sfs_api.h"

void write_directory()
{
  write_file(&root_dir, (void*)&directory, sizeof(file_t) * NUM_INODES);
  root_dir.rwptr = 0;
}

void write_free_blocks()
{
  write_blocks(1, FREE_BLOCKS_STORAGE, free_blocks);
}

void write_inode_table()
{
  write_blocks(1 + FREE_BLOCKS_STORAGE, sb.inode_table_len, table);
}

void init_superblock()
{
  sb.magic = 0xACBD0005;
  sb.block_size = BLOCK_SZ;
  sb.fs_size = NUM_BLOCKS * BLOCK_SZ;
  sb.inode_table_len = NUM_INODE_BLOCKS;
  sb.root_dir_inode = 0;
}

void init_free_blocks()
{
  for (uint64_t i = 0; i < NUM_BLOCKS; i++) {
    free_blocks[i] = i >= (1 + FREE_BLOCKS_STORAGE + sb.inode_table_len);
  }
}

void mksfs(int format)
{
  // Set up root directory file descriptor.
  root_dir.inode = sb.root_dir_inode;
  root_dir.rwptr = 0;

  if (format) {
    printf("creating new file system: %s\n", DISK);

    // Initialize disk.
    init_fresh_disk(DISK, BLOCK_SZ, NUM_BLOCKS);
    // TODO check if disk opened successfully.

    // Create and write super block.
    init_superblock();
    write_blocks(0, 1, &sb);

    // Initialize free block list.
    init_free_blocks();

    // Write inode table.
    table[sb.root_dir_inode].link_cnt = 2;
    write_inode_table();

    // Write root directory.
    file_t root_file;
    root_file.inode = sb.root_dir_inode;
    strncpy(root_file.filename, ".", MAXFILENAME);
    directory[sb.root_dir_inode] = root_file;
    write_directory();

    // Write free blocks.
    write_free_blocks();
  } else {
    printf("reusing file system: %s\n", DISK);

    // Initialize disk.
    init_disk(DISK, BLOCK_SZ, NUM_BLOCKS);
    // TODO check if disk opened successfully.

    // Open super block.
    read_blocks(0, 1, &sb);
    if (sb.block_size != BLOCK_SZ) {
      fprintf(stderr, "block size mismatch: expected %d, got %lu\n",
          BLOCK_SZ, sb.block_size);
      exit(1);
    }

    // Read free blocks list.
    read_blocks(1, FREE_BLOCKS_STORAGE, free_blocks);

    // Read inode table.
    read_blocks(1 + FREE_BLOCKS_STORAGE, sb.inode_table_len, table);

    // Read root directory.
    read_file(&root_dir, (void*)&directory, sizeof(file_t) * NUM_INODES);
  }
}

int sfs_getnextfilename(char* fname)
{
  // Look for first unseen file.
  for (int i = 0; i < NUM_INODES; i++) {
    // Check if not seen and exists.
    if (!seen_files[i] && directory[i].inode) {
      // Copy name into buffer, and mark as seen.
      strcpy(fname, directory[i].filename);
      seen_files[i] = 1;

      // Notify that there is a new file.
      return 1;
    }
  }

  // Reset files seen.
  for (int i = 0; i < NUM_INODES; i++) {
    seen_files[i] = 0;
  }

  // No new file.
  return 0;
}

int sfs_getfilesize(const char* path)
{
  if (strlen(path) > MAXFILENAME) {
    // File name too long.
    errno = ENAMETOOLONG;
    perror("getfilesize()");
    return -(errno);
  }

  // Get matching inode.
  uint64_t inode = get_file(path);
  if (inode == -1) {
    // No such file.
    errno = ENOENT;
    perror("getfilesize()");
    return -(errno);
  }

  return table[inode].size;
}

int sfs_fopen(char* name)
{
  if (strlen(name) > MAXFILENAME) {
    // File name too long.
    errno = ENAMETOOLONG;
    perror("fopen()");
    return -errno;
  }

  // Get matching inode.
  uint64_t inode = get_file(name);
  if (inode == -1) {
    // Get a free inode.
    inode = yield_inode();
    if (inode < 0) {
      // No more inodes.
      return inode;
    }

    // Create new file.
    file_t f;
    strncpy(f.filename, name, MAXFILENAME);
    f.inode = inode;
    inode_t n = table[inode];
    n.link_cnt += 1;

    // Add file to directory.
    directory[inode] = f;
    write_directory();
  }

  // Get file descriptor number.
  int fd = -1;
  for (int i = 0; i < NUM_FILE_DESCRIPTORS; i++) {
    if (fdt[i].inode == inode) {
      fd = i;
      break;
    }
  }
  if (fd == -1) {
    // Get unused file descriptor number.
    fd = yield_fd();
    if (fd < 0) {
      // No more file descriptors.
      return fd;
    }

    // Set file descriptor.
    fdt[fd].inode = inode;
    fdt[fd].rwptr = table[inode].size;
  }

  // Write to disk.
  write_free_blocks();
  write_inode_table();

  // Return file descriptor.
  return fd;
}

int sfs_fclose(int fd)
{
  if (fd >= NUM_FILE_DESCRIPTORS) {
    // File descriptor out of bounds.
    errno = ENFILE;
    perror("fclose()");
    return 2;
  }

  if (fdt[fd].inode) {
    fdt[fd].inode = 0;
    fdt[fd].rwptr = 0;
    return 0;
  }

  // Bad file number.
  errno = EBADF;
  perror("fclose()");
  return 1;
}

int sfs_fread(int fd, char* buf, int length)
{
  if (fd >= NUM_FILE_DESCRIPTORS) {
    // File descriptor out of bounds.
    errno = ENFILE;
    perror("fread()");
    return 0;
  }

  if (fdt[fd].inode != 0) {
    return read_file(&fdt[fd], (void*)buf, length);
  }

  // Bad file number.
  errno = EBADF;
  perror("fread()");
  return 0;
}

int sfs_fwrite(int fd, const char* buf, int length)
{
  if (fd >= NUM_FILE_DESCRIPTORS) {
    // File descriptor out of bounds.
    errno = ENFILE;
    perror("fwrite()");
    return 0;
  }

  if (fdt[fd].inode != 0) {
    return write_file(&fdt[fd], (void*)buf, length);
  }

  // Bad file number.
  errno = EBADF;
  perror("fwrite()");
  return 0;
}

int sfs_fseek(int fd, int loc)
{
  if (fd >= NUM_FILE_DESCRIPTORS) {
    // File descriptor out of bounds.
    errno = ENFILE;
    perror("fseek()");
    return errno;
  }

  if (fdt[fd].inode) {
    if (table[fdt[fd].inode].size < loc) {
      // Trying to seek too far.
      errno = ESPIPE;
      perror("fseek()");
      return errno;
    }

    // Set pointer.
    fdt[fd].rwptr = loc;
    return 0;
  }

  // Bad file number.
  errno = EBADF;
  perror("fseek()");
  return errno;
}

int sfs_remove(char* file)
{
  if (strlen(file) > MAXFILENAME) {
    // File name too long.
    errno = ENAMETOOLONG;
    perror("remove()");
    return errno;
  }

  uint64_t inode = get_file(file);
  if (inode == -1) {
    // No such file.
    errno = ENOENT;
    perror("remove()");
    return errno;
  }

  // Yield from table.
  inode_t* n = &table[inode];

  // Create empty block.
  uint8_t* empty_block = (uint8_t*)malloc(BLOCK_SZ);
  clear_block(empty_block, 0, BLOCK_SZ);

  // Erase from disk.
  int num_blocks = n->size / BLOCK_SZ + 1;
  for (int i = 0; i < num_blocks; i++) {
    int block_index = get_block_at(n, i, false);
    if (block_index < 0) {
      continue;
    }
    write_blocks(block_index, 1, empty_block);
    free_blocks[block_index] = 0;
  }

  // Release from memory.
  free(empty_block);

  // Release from inode table.
  n->size = 0;
  n->link_cnt = 0;

  // Release from directory.
  for (int i = 0; i < MAXFILENAME; i++) {
    directory[inode].filename[i] = 0;
  }
  directory[inode].inode = 0;

  // Save changes to disk.
  write_directory();
  write_inode_table();
  write_free_blocks();

  // Release from file descriptor table.
  for (int i = 0; i < NUM_INODES; i++) {
    if (fdt[i].inode == inode) {
      sfs_fclose(i);
    }
  }

  // All good.
  return 0;
}