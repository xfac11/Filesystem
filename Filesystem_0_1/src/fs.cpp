#include <iostream>
#include "fs.h"

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    CWD = ROOT_BLOCK;
    loadCWD();
    loadFAT();
}

FS::~FS()
{

}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
    std::cout << "FS::format()\n";
    uint8_t buffer[BLOCK_SIZE];
    memset(buffer, 0, BLOCK_SIZE);
    //set all data to 0 in the disk

    for (size_t i = 0; i < size_t(disk.get_no_blocks()); i++)
    {
      if(disk.write(i, buffer) > 0)
      {
        return -1;
      }
    }
    //Marks all blocks as free except root and FAT block.
    fat[ROOT_BLOCK] = FAT_EOF;
    fat[FAT_BLOCK] = FAT_EOF;
    for (size_t i = 2; i < BLOCK_SIZE/2; i++)
    {
      fat[i] = FAT_FREE;
    }
    CWD = ROOT_BLOCK;
    memset(&currentDir, 0, sizeof(currentDir));

    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{

  loadCWD();// load the current working directory to currentDir

  // Locate free entry and check if name already exist.
  int freeEntry = locateFreeEntry(filepath);
  if(freeEntry == -1)
  {
    return -1;//No free entry found or filepath already exist
  }

  //This filepath does not exist so create it now
  std::string data;
  std::getline(std::cin, data); // get input until enter key is pressed
  int firstBlock = saveDataToDisk(data); // save the data that was typed to disk
  if(firstBlock == -1)
  {
    return -1;//Not enough blocks found
  }

  dir_entry newFile;
  strcpy(newFile.file_name, filepath.c_str());
  newFile.type = TYPE_FILE;
  newFile.size = data.size();
  newFile.first_blk = firstBlock;
  currentDir[freeEntry] = newFile;

  saveFAT();//save fat to disk
  saveCWD();//save CWD to disk
  std::cout << "FS::create(" << filepath << ")\n";
  return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
  int indexIntoCWD = locateFile(filepath);
  if(indexIntoCWD == -1)
  {
    return -1; // could not find the filename filepath
  }
  std::string data = readFAT(currentDir[indexIntoCWD].first_blk);
  std::cout << data << std::endl;
  std::cout << "FS::cat(" << filepath << ")\n";
  return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
  std::string name;
  for (int i = 0; i < ENTRY_COUNT; i++)
  {
    name = currentDir[i].file_name;
    std::cout << name << "  " << currentDir[i].size << std::endl;
  }
    std::cout << "FS::ls()\n";
    return 0;
}

// cp <sourcefilepath> <destfilepath> makes an exact copy of the file
// <sourcefilepath> to a new file <destfilepath>
int
FS::cp(std::string sourcefilepath, std::string destfilepath)
{
  int sourceIndex = locateFile(sourcefilepath);
  if(sourceIndex == -1)
  {
    return -1;// Cannot find the file named sourcefilepath
  }
  std::string data = readFAT(currentDir[sourceIndex].first_blk);

    std::cout << "FS::cp(" << sourcefilepath << "," << destfilepath << ")\n";
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    std::cout << "FS::pwd()\n";
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}
int FS::saveFAT()
{
  uint8_t buffer[BLOCK_SIZE];
  memcpy(buffer, fat, BLOCK_SIZE/2);// cpy the fat into the buffer
  if(disk.write(FAT_BLOCK, buffer) == -1)
  {
    std::cout <<"Failed when writing the FAT to disk" << std::endl;
    return -1;
  }
  return 0;
}
int FS::loadFAT()
{
  uint8_t buffer[BLOCK_SIZE];
  if(disk.read(FAT_BLOCK, buffer) == -1)
  {
    std::cout << "Failed when reading the FAT on disk" << std::endl;
    return -1;
  }// read index FAT_BLOCK into buffer
  memcpy(fat, buffer, BLOCK_SIZE/2); // cpy the buffer into the fat
  return 0;
}
int FS::getFreeBlock()// doesn't mark the block as not free
{
  int startIndex = 2;// 0 is root and 1 is FAT so no need to check there
  for (int i = startIndex; i < BLOCK_SIZE/2; i++)
  {
    if(fat[i] == FAT_FREE)
    {
      return i;//free block found so return it
    }
  }
  std::cout << "No free block found" << std::endl;
  return -1; // no free block found
}
void FS::printFAT()
{
  std::cout << "Printing FAT:" << std::endl;
  for (int i = 0; i < BLOCK_SIZE/2; i++)
  {
    std::cout << "[" << i << "]";
    std::cout << "=" << fat[i] << " ";
    if(i % 9 == 0) { std::cout << std::endl; }
  }
  std::cout << std::endl;
}
int FS::getFreeBlock(int nrOfBlocks, int* blocks)// doesn't mark the block as not free
{
  int startIndex = 2;// 0 is root and 1 is FAT so no need to check there
  int blocksFound = 0;
  for (int i = startIndex; i < BLOCK_SIZE/2 && nrOfBlocks != blocksFound; i++)
  {
    if(fat[i] == FAT_FREE)
    {
      blocks[blocksFound] = i;
      blocksFound++;
      if(blocksFound == nrOfBlocks)
      {
        return 0;// found the number of blocks
      }
    }
  }
  std::cout << "No free block found" << std::endl;
  return -1; // no free block found
}
int FS::loadCWD()
{
  uint8_t buffer[BLOCK_SIZE];
  if(disk.read(CWD, buffer) == -1)
  {
    std::cout << "Failed when reading the CWD on disk" << std::endl;
    return -1;
  }
  memcpy(currentDir, buffer, sizeof(currentDir));
  return 0;
}
int FS::saveCWD()
{
  uint8_t buffer[BLOCK_SIZE];
  memcpy(buffer, currentDir, sizeof(currentDir));// cpy the fat into the buffer
  if(disk.write(CWD, buffer) == -1)
  {
    std::cout <<"Failed when writing the CWD to disk" << std::endl;
    return -1;
  }
  return 0;
}
std::string FS::readFAT(int startBlock)
{
  loadFAT();
  std::string data;
  uint8_t buffer[BLOCK_SIZE];
  memset(buffer, 0, BLOCK_SIZE);
  int16_t currentBlock = startBlock;
  if(fat[currentBlock] == FAT_FREE)
  {
    std::cout << "Fat is corrupted or entered wrong startblock" << std::endl;
    return data;
  }
  disk.read(currentBlock, buffer);
  data.assign(buffer, buffer + sizeof(buffer));
  while (fat[currentBlock] != FAT_EOF)
  {
    currentBlock = fat[currentBlock];
    if(fat[currentBlock] == FAT_FREE)
    {
      std::cout << "Fat is corrupted or entered wrong startblock" << std::endl;
      return data;
    }
    memset(buffer, 0, BLOCK_SIZE);
    std::string temp;
    disk.read(currentBlock, buffer);
    temp.assign(buffer, buffer + sizeof(buffer));
    data += temp;
  }
  return data;
}
int FS::locateFile(std::string filepath)
{
  //return the index to the fileentry located in CWD
  for (int i = 0; i < ENTRY_COUNT; i++)
  {
    std::string nameInDir;
    nameInDir = currentDir[i].file_name;
    if(nameInDir == filepath && int(currentDir[i].type) == TYPE_FILE)
    {
      return i;
    }
  }
  std::cout << "File: " << filepath << " could not be found " << std::endl;
  return -1;
}
int FS::saveDataToDisk(const std::string& data)
{
  uint8_t buffer[BLOCK_SIZE];
  memset(buffer, 0, sizeof(buffer));
  uint32_t fileSize = data.size();
  int nrOfBlocks = (int(fileSize) / BLOCK_SIZE) + 1; // example: (60/4096) + 1 = 1 block. (9053/4096) + 1 = 2 blocks
  int dataCopied = 0;
  int* blocksArray = new int[nrOfBlocks];
  if(getFreeBlock(nrOfBlocks, blocksArray) == -1)
  {
    return -1;//Not enough blocks found.
  }

  for (int i = 0; i < nrOfBlocks; i++)
  {
    if((dataCopied + BLOCK_SIZE) < int(fileSize))// will the data that we are copying be larger than filesize.
    {
      memcpy(buffer, data.c_str() + dataCopied, BLOCK_SIZE);
      /*copy data to buffer. Move in the dataarray by the bytes that we have actually copied.
      example: 10000 bytes to copy. 0, 4096. After this it goes to the else.*/
      dataCopied += BLOCK_SIZE;//Increase dataCopied with the bytes we copied.
    }
    else
    {
      //copy the remaining bytes
      // example cont: The remaining is 8192. So move in the dataarray 8192 and copy the remaining bytes 10000-8192=1808.
      memcpy(buffer, data.c_str() + dataCopied, int(fileSize) - dataCopied);
    }
    if(i == nrOfBlocks - 1)
    {
      fat[blocksArray[i]] = FAT_EOF;
    }
    else
    {
      fat[blocksArray[i]] = blocksArray[i+1];
    }
    disk.write(blocksArray[i], buffer);
    memset(buffer, 0, BLOCK_SIZE);// safe to reset the data in buffer to 0
  }
  return blocksArray[0]; // returns first_blk
}
int FS::locateFreeEntry(const std::string& name)
{
  int freeEntry = -1;
  for (int i = 0; i < ENTRY_COUNT; i++)
  {
    std::string nameInDir = currentDir[i].file_name;
    std::string empty = "";
    if(nameInDir == name && int(currentDir[i].type) == TYPE_FILE)
    {
      std::cout << "Filepath: " << name << " already exist" << std::endl;
      return -1;
    }
    else if(nameInDir == empty && freeEntry == -1)//check for empty entry for new file and save the index.
    {
      freeEntry = i;
    }
  }
  return freeEntry;
}
