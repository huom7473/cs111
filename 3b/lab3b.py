#!/usr/local/cs/bin/python3
# NAME: Michael Huo, Tygan Zeng
# EMAIL: EMAIL
# ID: UID
import sys
from collections import defaultdict

linesdict = defaultdict(list)

block_free_list_set = set()
inode_free_list_set = set()
block_allocated_set = set()
inode_allocated_set = dict() # keeps track of {inode #: csv link count} (not actual linkcount)
inode_actual_links = defaultdict(int) # keeps track of actual links, with default value at 0
block_printed_set = set()
block_dictionary = dict()
inode_children = defaultdict(set)

def generate_free_list():
    for i in linesdict['BFREE']:
        block_free_list_set.add(i[1])
    for i in linesdict['IFREE']:
        inode_free_list_set.add(i[1])
    

def check_unreferenced_blocks():
    for i in range(reserved_boundary, num_blocks):
        if (i not in block_free_list_set) and (i not in block_allocated_set):
            print(f"UNREFERENCED BLOCK {i}")
        if (i in block_free_list_set) and (i in block_allocated_set):
            print(f"ALLOCATED BLOCK {i} ON FREELIST")

def check_inodes():
    for i in range(1, inode_start):
        if i in inode_free_list_set:
            print(f"ALLOCATED INODE {i} ON FREELIST")
    for i in range(inode_start, num_inodes):
        if (i not in inode_free_list_set) and (i not in inode_allocated_set):
            print(f"UNALLOCATED INODE {i} NOT ON FREELIST")
        if (i in inode_free_list_set) and (i in inode_allocated_set):
            print(f"ALLOCATED INODE {i} ON FREELIST")

def scan_blocks():
    boundary = 0
    for i in linesdict['INODE']:
        double_offset = int(12 + block_size/4) # 4 is the size of a block pointer (unsigned 32 bit)
        triple_offset = int(double_offset + (block_size/4)**2)
        if i[2] == 'f' or i[2] == 'd':
            for j in range(15):
                if i[12+j] < 0 or i[12+j] > num_blocks:
                    if j < 12:
                        print(f"INVALID BLOCK {i[12+j]} IN INODE {i[1]} AT OFFSET {j}")
                    elif j == 12:
                        print(f"INVALID INDIRECT BLOCK {i[12+j]} IN INODE {i[1]} AT OFFSET {j}")
                    elif j == 13:
                        print(f"INVALID DOUBLE INDIRECT BLOCK {i[12+j]} IN INODE {i[1]} AT OFFSET {double_offset}")
                    elif j == 14:
                        print(f"INVALID TRIPLE INDIRECT BLOCK {i[12+j]} IN INODE {i[1]} AT OFFSET {triple_offset}")
                elif i[12+j] > 0 and i[12+j] < reserved_boundary:
                    if j < 12:
                        print(f"RESERVED BLOCK {i[12+j]} IN INODE {i[1]} AT OFFSET {j}")
                    elif j == 12:
                        print(f"RESERVED INDIRECT BLOCK {i[12+j]} IN INODE {i[1]} AT OFFSET {j}")
                    elif j == 13:
                        print(f"RESERVED DOUBLE INDIRECT BLOCK {i[12+j]} IN INODE {i[1]} AT OFFSET {double_offset}")
                    elif j == 14:
                        print(f"RESERVED TRIPLE INDIRECT BLOCK {i[12+j]} IN INODE {i[1]} AT OFFSET {triple_offset}")
                else:
                    block_allocated_set.add(i[12+j])

                if i[12+j] in block_dictionary:
                    if j < 12:
                        print(f"DUPLICATE BLOCK {i[12+j]} IN INODE {i[1]} AT OFFSET {j}")
                    elif j == 12:
                        print(f"DUPLICATE INDIRECT BLOCK {i[12+j]} IN INODE {i[1]} AT OFFSET {j}")
                    elif j == 13:
                        print(f"DUPLICATE DOUBLE INDIRECT BLOCK {i[12+j]} IN INODE {i[1]} AT OFFSET {double_offset}")
                    elif j == 14:
                        print(f"DUPLICATE TRIPLE INDIRECT BLOCK {i[12+j]} IN INODE {i[1]} AT OFFSET {triple_offset}")
                    block_printed_set.add(i[12+j])
                elif i[12+j] != 0:
                    if j < 12:
                        block_dictionary[i[12+j]] = (0, i[1], j)
                    elif j == 12: 
                        block_dictionary[i[12+j]] = (1, i[1], j)
                    elif j == 13:
                        block_dictionary[i[12+j]] = (2, i[1], 268)
                    elif j == 14:
                        block_dictionary[i[12+j]] = (3, i[1], 65804)
        
        inode_allocated_set[i[1]] = i[6]
                    
    for i in linesdict['INDIRECT']:
        if i[5] < 0 or i[5] > num_blocks: 
            print(i[2])
            if i[2] == '1':
                print(f"INVALID INDIRECT BLOCK {i[5]} IN INODE {i[1]} AT OFFSET {i[3]}")
            elif i[2] == '2':
                print(f"INVALID DOUBLE INDIRECT BLOCK {i[5]} IN INODE {i[1]} AT OFFSET {i[3]}")
            elif i[2] == '3':
                print(f"INVALID TRIPLE INDIRECT BLOCK {i[5]} IN INODE {i[1]} AT OFFSET {i[3]}")
        else:
            block_allocated_set.add(i[5])
        
        if i[5] in block_dictionary:
            if j < 12:
                print(f"DUPLICATE BLOCK {i[5]} IN INODE {i[1]} AT OFFSET {i[3]}")
            elif j == 12:
                print(f"DUPLICATE INDIRECT BLOCK {i[5]} IN INODE {i[1]} AT OFFSET {i[3]}")
            elif j == 13:
                print(f"DUPLICATE DOUBLE INDIRECT BLOCK {i[5]} IN INODE {i[1]} AT OFFSET {i[3]}")
            elif j == 14:
                print(f"DUPLICATE TRIPLE INDIRECT BLOCK {i[5]} IN INODE {i[1]} AT OFFSET {i[3]}")
            block_printed_set.add(i[5])
        elif i[5] != 0:
            if j < 12:
                block_dictionary[i[5]] = (0, i[1], i[3])
            elif j == 12: 
                block_dictionary[i[5]] = (1, i[1], i[3])
            elif j == 13:
                block_dictionary[i[5]] = (2, i[1], i[3])
            elif j == 14:
                block_dictionary[i[5]] = (3, i[1], i[3])
            

    for i in block_printed_set:
        if block_dictionary[i][0] == 0:
            print(f"DUPLICATE BLOCK {i} IN INODE {block_dictionary[i][1]} AT OFFSET {block_dictionary[i][2]}")
        elif block_dictionary[i][0] == 1:
            print(f"DUPLICATE INDIRECT BLOCK {i} IN INODE {block_dictionary[i][1]} AT OFFSET {block_dictionary[i][2]}")
        elif block_dictionary[i][0] == 2:
            print(f"DUPLICATE DOUBLE BLOCK {i} IN INODE {block_dictionary[i][1]} AT OFFSET {block_dictionary[i][2]}")
        elif block_dictionary[i][0] == 3:
            print(f"DUPLICATE TRIPLE BLOCK {i} IN INODE {block_dictionary[i][1]} AT OFFSET {block_dictionary[i][2]}")

def scan_dirents():
    dot_dict = defaultdict(lambda: [-1, -1])  # in the form of {inode #: [. inode #, .. inode #]}
    for dirent in linesdict['DIRENT']:
        if dirent[6] == "'.'": # record what each . and .. refer to
            dot_dict[dirent[1]][0] = dirent[3]
        elif dirent[6] == "'..'":
            dot_dict[dirent[1]][1] = dirent[3]

        parent_inode = dirent[1]
        referenced_inode = dirent[3]
        entry_name = dirent[6]
        # do not consider a directory to be a child of itself, and do not consider its parent to be its child, either
        if entry_name != "'..'" and entry_name != "'.'": 
            inode_children[parent_inode].add(referenced_inode) # associate directory inode number with a set containing its children
        inode_actual_links[referenced_inode] += 1

        # assuming that since invalid inodes should always be unallocated but not vise versa, invalid should take priority
        if referenced_inode < 1 or referenced_inode > inode_count:
            print(f"DIRECTORY INODE {parent_inode} NAME {entry_name} INVALID INODE {referenced_inode}")
        elif referenced_inode not in inode_allocated_set: 
            print(f"DIRECTORY INODE {parent_inode} NAME {entry_name} UNALLOCATED INODE {referenced_inode}")

    #print(dot_dict)
    for inode in dot_dict:
        if dot_dict[inode][0] != inode:
            print(f"DIRECTORY INODE {inode} NAME '.' LINK TO INODE {dot_dict[inode][0]} SHOULD BE {inode}")
        if inode == 2: # inode 2 is the root, so it's a special case
            if dot_dict[inode][1] != 2:
                print(f"DIRECTORY INODE 2 NAME '..' LINK TO INODE {dot_dict[inode][1]} SHOULD BE 2")
        elif inode not in inode_children[dot_dict[inode][1]]:
            correct_parent = None
            for d in inode_children: # doesn't handle a case where one directory entry is somehow contained by two inodes, but it should be fine
                if inode in inode_children[d]: # find correct parent
                    correct_parent = d
                    break
            print(f"DIRECTORY INODE {inode} NAME '..' LINK TO INODE {dot_dict[inode][1]} SHOULD BE {correct_parent}")
        
def check_directories():
    for inode in inode_allocated_set:
        if inode_allocated_set[inode] != inode_actual_links[inode]:
            print(f"INODE {inode} HAS {inode_actual_links[inode]} LINKS BUT LINKCOUNT IS {inode_allocated_set[inode]}")
        
def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} [file.csv]", file=sys.stderr)
        sys.exit(1)
    
    try:
        with open(sys.argv[1]) as csv:
            for line in csv:
                type = line.split(',')[0]
                temp = []
                for field in line.split(','):
                    try:
                        temp.append(int(field))
                    except ValueError:
                        temp.append(field.strip())
                linesdict[type].append(tuple(temp))
    except FileNotFoundError:
        print(f"{sys.argv[0]}: error opening file {sys.argv[1]}: file doesn't exist",
              file=sys.stderr)
        sys.exit(1)
    except Exception as e: # fallthrough case
        print(f"{sys.argv[0]}: {e}",
              file=sys.stderr)
        sys.exit(1)

    if 'SUPERBLOCK' not in linesdict:
        print(f"{sys.argv[0]}: no superblock entry found, analysis impossible. exiting",
              file=sys.stderr)
        sys.exit(1)
        
    global num_blocks
    num_blocks = linesdict['SUPERBLOCK'][0][1]
    
    global num_inodes
    num_inodes = linesdict['SUPERBLOCK'][0][2]
    
    global block_size
    block_size = linesdict['SUPERBLOCK'][0][3]
    inode_size = linesdict['SUPERBLOCK'][0][4]
    inodes_per_block = block_size/inode_size # both are powers of 2
    inodes_per_group = linesdict['SUPERBLOCK'][0][6]

    global inode_start
    inode_start = linesdict['SUPERBLOCK'][0][7]

    global inode_count
    inode_count = linesdict['SUPERBLOCK'][0][2]
    
    global reserved_boundary
    reserved_boundary = int(inodes_per_group/inodes_per_block) + linesdict['GROUP'][0][8]

    generate_free_list()
    scan_blocks()
    check_unreferenced_blocks()
    check_inodes()
    scan_dirents()
    check_directories()
    
if __name__ == "__main__":
    main()
