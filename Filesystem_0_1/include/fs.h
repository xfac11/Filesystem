#include <iostream>
#include <cstdint>
#include "disk.h"
#include <string.h>
#include <sstream>

#include <vector>
#ifndef __FS_H__
#define __FS_H__

#define ROOT_BLOCK 0
#define FAT_BLOCK 1
#define FAT_FREE 0
#define FAT_EOF -1

#define TYPE_FILE 0
#define TYPE_DIR 1
#define READ 0x04
#define WRITE 0x02
#define EXECUTE 0x01

#define ENTRY_COUNT 34
#define ENTRY_SIZE 120

struct dir_entry {
    char file_name[56]; // name of the file / sub-directory
    uint16_t first_blk; // index in the FAT for the first block of the file
    uint32_t size; // size of the file in bytes
    uint8_t type; // directory (1) or file (0)
    uint8_t access_rights; // read (0x04), write (0x02), execute (0x01)
};

class FS {
private:
    Disk disk;
    // size of a FAT entry is 2 bytes
    int16_t fat[BLOCK_SIZE/2];
    dir_entry currentDir[BLOCK_SIZE/ENTRY_SIZE];// cache for current working directory 4kB in memory
    int16_t CWD;// index for the block of the current working directory

    int saveFAT();
    int loadFAT();
    int loadCWD();
    int saveCWD();
    int getFreeBlock();
    int loadDirectory(const std::string& filepath, dir_entry* theDirectory, int type, std::string& lastName);
    int getFreeBlock(int nrOfBlocks, int* blocks);
    //Forces the startBlock to be the first block in blocks[] and then searches as normal
    //This creates a link between startblock and the other ones in blocks[]
    int getFreeBlock(int startBlock, int nrOfBlocks, int* blocks);
    std::string readFAT(int startBlock);
    int getLastBlock(int startBlock);
    //Saves the data on disk. Updates the FAT but not the CWD
    //Returns -1 if not enough blocks found or the first blockIndex for the data
    int saveDataToDisk(const std::string& data);
    int saveDataToDisk(int startBlock, const std::string& data);
    //Checks if name already exist and gives error otherwise returns a free entry
    int locateFreeEntry(dir_entry* directory, const std::string& name);
    //Checks if name exist and returns its index
    int locateEntry(dir_entry* directory, const std::string& filepath, int type);
    //creates an entry and insert it into currentdir[entryIndex]
    int createEntry(dir_entry* directory, const char* filepath, int entryIndex, uint16_t firstBlock, uint32_t size, uint8_t type, uint8_t access_rights);

    int createEntry(const char* filepath, int entryIndex, uint16_t firstBlock, uint32_t size, uint8_t type, uint8_t access_rights);
    void printFAT();
    std::string createString(uint8_t* buffer, int bufferSize);
public:
    FS();
    ~FS();
    // formats the disk, i.e., creates an empty file system
    int format();
    // create <filepath> creates a new file on the disk, the data content is
    // written on the following rows (ended with an empty row)
    int create(std::string filepath);
    // cat <filepath> reads the content of a file and prints it on the screen
    int cat(std::string filepath);
    // ls lists the content in the currect directory (files and sub-directories)
    int ls();

    // cp <sourcefilepath> <destfilepath> makes an exact copy of the file
    // <sourcefilepath> to a new file <destfilepath>
    int cp(std::string sourcefilepath, std::string destfilepath);
    // mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
    // or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
    int mv(std::string sourcepath, std::string destpath);
    // rm <filepath> removes / deletes the file <filepath>
    int rm(std::string filepath);
    // append <filepath1> <filepath2> appends the contents of file <filepath1> to
    // the end of file <filepath2>. The file <filepath1> is unchanged.
    int append(std::string filepath1, std::string filepath2);

    // mkdir <dirpath> creates a new sub-directory with the name <dirpath>
    // in the current directory
    int mkdir(std::string dirpath);
    // cd <dirpath> changes the current (working) directory to the directory named <dirpath>
    int cd(std::string dirpath);
    // pwd prints the full path, i.e., from the root directory, to the current
    // directory, including the currect directory name
    int pwd();

    // chmod <accessrights> <filepath> changes the access rights for the
    // file <filepath> to <accessrights>.
    int chmod(std::string accessrights, std::string filepath);
};

#endif // __FS_H__
