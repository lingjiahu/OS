

Design Choices:
According to post #433 on Ed, I chose to set NUM_BLOCKS to 1024.



inode:
indirect pointer: points to a block, the block contains BLOCK_SIZE/sizeof(int) i node numbers - pointing to another 1024 blocks


free bitmap:
char array of size NUM_BLOCKS, a char corresponds to a block in disk

