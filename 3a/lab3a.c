/* NAME: Michael Huo, Tygan Zeng */
/* EMAIL: EMAIL,  */
/* ID: UID,  */
#include <sys/types.h>
#include "ext2_fs.h" //include this after stdint.h to avoid typedef issues
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
typedef struct ext2_super_block super_block;
typedef struct ext2_group_desc group_desc;
typedef struct ext2_dir_entry dir_entry;
typedef struct ext2_inode inode;

super_block* sb; //access super block globally
group_desc* group; //assume we only need to deal with single groups
int fd;

void handle_segfault(int sig){
    fprintf(stderr, "Exit status 4: Caught segmentation fault. %d\n", sig);
    exit(4);
}


unsigned long calculate_offset(unsigned block_num) {
    return block_num * (EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size);
}

void read_super_block() {
    sb = malloc(sizeof(super_block));
    if (!sb) {
        fprintf(stderr, "error in buffer allocation\n");
        exit(2);
    }

    pread(fd, sb, sizeof(super_block), 1024); //read super block

    if (sb->s_magic != EXT2_SUPER_MAGIC) {
        fprintf(stderr, "super block magic value check failed\n");
        exit(1);
    }
    
    unsigned int bsize = EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size;
    fprintf(stderr, "size of block according to superblock: %d\n", bsize);
    fprintf(stdout, "SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n",
            sb->s_blocks_count, sb->s_inodes_count, bsize, sb->s_inode_size,
            sb->s_blocks_per_group, sb->s_inodes_per_group, sb->s_first_ino);
}

void read_group() {
    group = malloc(sizeof(group_desc));
    if (!group) {
        fprintf(stderr, "error in buffer allocation\n");
        exit(2);
    }

    pread(fd, group, sizeof(group_desc), calculate_offset(sb->s_first_data_block + 1));
    fprintf(stdout,"GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n", 0,
            sb->s_blocks_count, sb->s_inodes_count, group->bg_free_blocks_count,
            group->bg_free_inodes_count, group->bg_block_bitmap, group->bg_inode_bitmap, group->bg_inode_table);
}

void scan_free_blocks() {
    unsigned int bsize = EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size;
    unsigned char *buffer = malloc(bsize);
    if (!buffer) {
        fprintf(stderr, "error in buffer allocation\n");
        exit(2);
    }

    pread(fd, buffer, bsize, calculate_offset(group->bg_block_bitmap));

    unsigned int byte, offset;
    for (unsigned i = 0; i < sb->s_blocks_per_group; ++i) {
        byte = i / 8; // 8 blocks represented per byte
        offset = i % 8; //offset from that byte
        if (!(buffer[byte] >> offset & 0x1)) { //find the right bit, if it's 0 the if statement is true
            fprintf(stdout, "BFREE,%u\n", i + 1); //I guess block # starts at 1
        }
    }
    free(buffer);
}

void scan_free_inodes() {
    unsigned int bsize = EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size;
    unsigned char *buffer = malloc(bsize);
    if (!buffer) {
        fprintf(stderr, "error in buffer allocation\n");
        exit(2);
    }

    pread(fd, buffer, bsize, calculate_offset(group->bg_inode_bitmap));

    unsigned int byte, offset;
    for (unsigned i = 0; i < sb->s_inodes_per_group; ++i) {
        byte = i / 8; // 8 blocks represented per byte
        offset = i % 8; //offset from that byte
        if (!(buffer[byte] >> offset & 0x1)) { //find the right bit, if it's 0 the if statement is true
            fprintf(stdout, "IFREE,%u\n", i + 1); //I guess block # starts at 1
        }
    }
    free(buffer);
}

void read_directory_entry(unsigned inum, unsigned offset){
    
    dir_entry entry;
    unsigned bytes_read = 0;

    while(bytes_read < 1024){
        if (pread(fd, &entry, sizeof(dir_entry), calculate_offset(offset) + bytes_read) < 0){
            fprintf(stderr, "ERROR: failed to read\n");
            exit(1);
        }
        if(entry.inode != 0){
            
            fprintf(stdout, "DIRENT,%d,%d,%d,%d,%d,'", inum, bytes_read, entry.inode, entry.rec_len, entry.name_len);
            for(unsigned i = 0; i < entry.name_len; i++){
                fprintf(stdout, "%c", entry.name[i]);
            }
            fprintf(stdout,"'\n");
        }
        bytes_read += entry.rec_len;
    }
}


static void format_inode(inode node, unsigned inum) { //auxiliary function to print inode information
    char type, timestr_c[20], timestr_m[20], timestr_a[20];
    //the value of S_IFLNK is 0xA000 which overlaps bits with regular file (0x8000), so we can't simply
    //rely on the value itself as a bitmask
    //there is also an overlap of bits with socket and directory, so we have to do this for all types
    if ((node.i_mode & 0xF000) == S_IFLNK)
        type = 's';
    else if ((node.i_mode & 0xF000) == S_IFREG)
        type = 'f';
    else if ((node.i_mode & 0xF000) == S_IFDIR)
        type = 'd';
    else
        type = '?';

    time_t ctime = (time_t) node.i_ctime;
    time_t mtime = (time_t) node.i_mtime;
    time_t atime = (time_t) node.i_atime;
    struct tm *tmp;

    tmp = gmtime(&ctime);
    strftime(timestr_c, sizeof(timestr_c), "%m/%d/%y %H:%M:%S", tmp);

    tmp = gmtime(&mtime);
    strftime(timestr_m, sizeof(timestr_m), "%m/%d/%y %H:%M:%S", tmp);

    tmp = gmtime(&atime);
    strftime(timestr_a, sizeof(timestr_a), "%m/%d/%y %H:%M:%S", tmp);

    unsigned long long filesize = node.i_dir_acl; //upper bits
    filesize = (filesize << 32) | node.i_size; // add lower bits

    fprintf(stdout, "INODE,%u,%c,%o,%hu,%hu,%hu,%s,%s,%s,%llu,%u", //print everything up to the block pointers
            inum, type, node.i_mode & 0x0FFF, node.i_uid, node.i_gid, node.i_links_count, timestr_c,
            timestr_m, timestr_a, filesize, node.i_blocks);

    if (type != 's' || filesize > 60) { //normal case, where file is not a symlink where size <= 60 bytes
        for(int i = 0; i < 15; ++i)
            fprintf(stdout, ",%u", node.i_block[i]);
    }

    fprintf(stdout, "\n"); //terminate inode description with a newline regardless of file type

    if(type == 'd'){
        for(unsigned entry_number = 0; entry_number < 12; entry_number++){
            if(node.i_block[entry_number] != 0){
                read_directory_entry(inum, node.i_block[entry_number]);
            }
        }
    }
    unsigned bsize = EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size;
    if(node.i_block[12] != 0){
        __u32* block_pointers = malloc(bsize);
        pread(fd, block_pointers, bsize, calculate_offset(node.i_block[12]));

        for(unsigned ptr = 0; ptr < bsize; ptr += 4){
            if(*(block_pointers + ptr/4) != 0){
                if(type == 'd'){
                    read_directory_entry(inum, *(block_pointers + ptr/4));
                }
                int logical_offset = (ptr/4);
                fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n", inum, 1, 12 + logical_offset, node.i_block[12], *(block_pointers + ptr/4));
            }
        }
        free(block_pointers);
    } 

    if(node.i_block[13] != 0){
        __u32* indirect_pointers = malloc(bsize);
        pread(fd, indirect_pointers, bsize, calculate_offset(node.i_block[13]));
        for(unsigned indirect_offset = 0; indirect_offset < bsize; indirect_offset += 4){
            if(*(indirect_pointers + indirect_offset/4) != 0){
                int indirect_logical_offset = (indirect_offset/4) * 256;
                fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n", inum, 2, 12 + indirect_logical_offset + 256, node.i_block[13], *(indirect_pointers + indirect_offset/4));
                __u32* block_pointers = malloc(bsize);
                pread(fd, block_pointers, bsize, calculate_offset(*(indirect_pointers + indirect_offset/4)));
                for(unsigned ptr = 0; ptr < bsize; ptr += 4){
                    if(*(block_pointers + ptr/4) != 0){
                        if(type == 'd'){
                            read_directory_entry(inum, *(block_pointers + ptr/4));
                        }
                        int logical_offset = (ptr/4);
                        fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n", inum, 1, 12 + indirect_logical_offset + 256 + logical_offset, *(indirect_pointers + indirect_offset/4), *(block_pointers + ptr/4));
                    }
                }
                free(block_pointers);
            }
        }
        free(indirect_pointers);
    }

    if(node.i_block[14] != 0){
        __u32* double_indirect_pointers = malloc(bsize);
        pread(fd, double_indirect_pointers, bsize, calculate_offset(node.i_block[14]));
        for(unsigned double_indirect_offset = 0; double_indirect_offset < bsize; double_indirect_offset += 4){
            
            if(*(double_indirect_pointers + double_indirect_offset/4) != 0){
                int double_indirect_logical_offset = (double_indirect_offset/4) * 65536;
                fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n", inum, 3, 12 + double_indirect_logical_offset + 256 + 65536, node.i_block[14], *(double_indirect_pointers + double_indirect_offset/4));
                
                __u32* indirect_pointers = malloc(bsize);
                pread(fd, indirect_pointers, bsize, calculate_offset(*(double_indirect_pointers + double_indirect_offset/4)));
                for(unsigned indirect_offset = 0; indirect_offset < bsize; indirect_offset += 4){
                    if(*(indirect_pointers + indirect_offset/4) != 0){
                        int indirect_logical_offset = (indirect_offset/4) * 256;
                        fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n", inum, 2, 12 + double_indirect_logical_offset + 256 + 65536 + indirect_logical_offset, *(double_indirect_pointers + double_indirect_offset/4), *(indirect_pointers + indirect_offset/4));
                        __u32* block_pointers = malloc(bsize);
                        pread(fd, block_pointers, bsize, calculate_offset(*(indirect_pointers + indirect_offset/4)));
                        for(unsigned ptr = 0; ptr < bsize; ptr += 4){
                            if(*(block_pointers + ptr/4) != 0){
                                if(type == 'd'){
                                    read_directory_entry(inum, *(block_pointers + ptr/4));
                                }
                                int logical_offset = (ptr/4);
                                fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n", inum, 1, 12 + double_indirect_logical_offset + 256 + 65536 + indirect_logical_offset + logical_offset, *(indirect_pointers + indirect_offset/4), *(block_pointers + ptr/4));
                            }
                        }
                        free(block_pointers);
                    }
                }
                free(indirect_pointers);
            }
        }
        free(double_indirect_pointers);
    }

}

void scan_inodes() {
    unsigned int bsize = EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size;
    unsigned int inodes_per_block = bsize/sb->s_inode_size;
    unsigned int inode_table_blocks = sb->s_inodes_per_group/inodes_per_block;
    inode *buffer = malloc(bsize);
    if (!buffer) {
        fprintf(stderr, "error in buffer allocation\n");
        exit(2);
    }

    inode node;
    for (unsigned table_number = 0; table_number < inode_table_blocks; ++table_number) {
        if (pread(fd, buffer, bsize, calculate_offset(group->bg_inode_table + table_number)) != bsize) {
            fprintf(stderr, "error reading from inode table, block %u\n", table_number);
            exit(2);
        }
        for (unsigned offset = 0; offset < inodes_per_block; ++offset) { //go through each inode in the block
            node = buffer[offset];
            if (node.i_mode && node.i_links_count) { //if inode is allocated
                format_inode(node, table_number * inodes_per_block + offset + 1);
            }
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 2) { //./lab3a [img name]
        fprintf(stderr, "usage: %s [image]\n", argv[0]);
        exit(1);
    }
    signal(SIGSEGV, handle_segfault);
    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "%s: error opening image file %s: %s\n", argv[0], argv[1], strerror(errno));
        exit(1);
    }

    //fprintf(stderr, "size of super block struct: %lu\n", sizeof(super_block)); //block size ?
    
    read_super_block();
    read_group();
    scan_free_blocks();
    scan_free_inodes();
    scan_inodes();
    exit(0);
}