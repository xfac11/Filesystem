#include <iostream>
#include "fs.h"

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    CWD = ROOT_BLOCK;
    loadCWD();
    loadFAT();
    printFAT();
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
    memset(&fat, 0, sizeof(fat));
    fat[ROOT_BLOCK] = FAT_EOF;
    fat[FAT_BLOCK] = FAT_EOF;
    CWD = ROOT_BLOCK;
    memset(&currentDir, 0, sizeof(currentDir));
    saveFAT();
    saveCWD();
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{

  loadCWD();// load the current working directory to currentDir

  dir_entry loadedDir[ENTRY_COUNT];
  memset(loadedDir, 0, sizeof(loadedDir));
  std::string nameOfFile;
  int dirBlock = loadDirectory(filepath, loadedDir, TYPE_FILE, nameOfFile);

  // Locate free entry and check if name already exist.
  int freeEntry = locateFreeEntry(loadedDir, nameOfFile);
  if(freeEntry == -1)
  {
    return -1;//No free entry found or filepath already exist
  }
  //This filepath does not exist so create it now
  std::string data;
  std::string line;
  std::getline(std::cin, line); // get input until enter key is pressed twice
  while(line.size() > 0)
  {
    data+=line;
    line.clear();
    std::getline(std::cin, line);
  }
  std::cout << data.size() << std::endl;
  int firstBlock = saveDataToDisk(data);
  if(firstBlock == -1)
  {
    return -1;//Not enough blocks found
  }

  createEntry(loadedDir, nameOfFile.c_str(), freeEntry, firstBlock, data.size(), TYPE_FILE, 0);

  std::string testName = loadedDir[0].file_name;

//saves the loaded directory
//If it is the CWD we just copy the updated loadedDir to currentDir and save CWD
//Else we save it to the corresponding block and do not update or save CWD because it isn't effected by it
  if(int(CWD) == dirBlock)
  {
    memcpy(currentDir, loadedDir, sizeof(currentDir));
    saveCWD();
  }
  else
  {
    uint8_t buffer[BLOCK_SIZE];
    memset(buffer, 0 , BLOCK_SIZE);
    memcpy(buffer,loadedDir , sizeof(loadedDir));
    if(disk.write(dirBlock, buffer) == -1)//Save the directory
    {
      std::cout <<"Failed when writing the CWD to disk" << std::endl;
      return -1;
    }
  }

  saveFAT();//save FAT to disk
  std::cout << "FS::create(" << filepath << ")\n";
  return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
  dir_entry loadedDir[ENTRY_COUNT];
  memset(loadedDir, 0, sizeof(loadedDir));
  std::string nameOfFile;
  loadDirectory(filepath, loadedDir, TYPE_FILE, nameOfFile);

  if(locateEntry(loadedDir, nameOfFile, TYPE_DIR) > -1)
  {
    std::cout << "Error, filepath "<<  "'" <<filepath <<"'" << " is a directory" << std::endl;
    return -1;
  }
  int loadedIndex = locateEntry(loadedDir, nameOfFile, TYPE_FILE);
  if((READ & loadedDir[loadedIndex].access_rights) == false)
  {
    std::cout << nameOfFile << " doesn't have read access" << std::endl;
    return -1;
  }
  if(loadedIndex == -1)
  {
    return -1; // could not find the filename filepath
  }
  std::string data = readFAT(loadedDir[loadedIndex].first_blk);
  std::cout << data.size() << std::endl;
  std::cout << data << std::endl;
  std::cout << "FS::cat(" << filepath << ")\n";
  return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
  std::string green("\033[0;32m");
  std::string blue("\033[0;33m");
  std::string reset("\033[0m");
  std::string name;
  dir_entry entry;
  for (int i = 0; i < ENTRY_COUNT; i++)
  {
    entry = currentDir[i];
    name = entry.file_name;
    if(name == "")
      continue;
    std::cout << name << "  ";
    if(unsigned(entry.type) == 0)
    {
      std::cout << green << "file " << reset;
    }
    else
    {
      std::cout << blue << "dir " << reset;
    }
//RWX
    (entry.access_rights & READ) ? std::cout << "r" : std::cout << "-";
    (entry.access_rights & WRITE) ? std::cout << "w" : std::cout << "-";
    (entry.access_rights & EXECUTE) ? std::cout << "x" : std::cout << "-";

    std::cout << "  " << entry.size <<std::endl;
  }
    std::cout << "FS::ls()\n";
    return 0;
}

// cp <sourcefilepath> <destfilepath> makes an exact copy of the file
// <sourcefilepath> to a new file <destfilepath>
int
FS::cp(std::string sourcefilepath, std::string destfilepath)
{
  dir_entry sourceDirectory[ENTRY_COUNT];
  dir_entry destDirectory[ENTRY_COUNT];

//load the sourcefilepath directory and the destfilepath directory
  std::string nameOfFile;
  std::string nameOfCopy;
  loadDirectory(sourcefilepath, sourceDirectory, TYPE_FILE, nameOfFile);
  int destBlock = loadDirectory(destfilepath, destDirectory, TYPE_FILE, nameOfCopy);

  //First locate the file in sourceDirectory and if found copy the data.

  int sourceIndex = locateEntry(sourceDirectory, nameOfFile, TYPE_FILE);
  if((READ & sourceDirectory[sourceIndex].access_rights) == false)
  {
    std::cout << nameOfFile << " doesn't have read access" << std::endl;
    return -1;
  }
  if(sourceIndex == -1)
  {
    return -1;// Cannot find the file named sourcefilepath
  }
  int freeEntry = locateFreeEntry(destDirectory, nameOfCopy);
  if(freeEntry == -1)
  {
    return -1;//No free entry found or filepath already exist
  }
  std::string data = readFAT(sourceDirectory[sourceIndex].first_blk);

  int firstBlock = saveDataToDisk(data);
  if(firstBlock == -1)
  {
    return -1;//Not enough blocks found
  }

  createEntry(destDirectory, nameOfCopy.c_str(), freeEntry, firstBlock, uint32_t(data.size()), TYPE_FILE, 0);


//Save the destDirectory. The sourceDirectory hasn't been changed so no need to save it.
  if(int(CWD) == destBlock)
  {
    memcpy(currentDir, destDirectory, sizeof(currentDir));
    saveCWD();
  }
  else
  {
    uint8_t buffer[BLOCK_SIZE];
    memset(buffer, 0 , BLOCK_SIZE);
    memcpy(buffer,destDirectory , sizeof(destDirectory));
    if(disk.write(destBlock, buffer) == -1)//Save the directory
    {
      std::cout <<"Failed when writing the CWD to disk" << std::endl;
      return -1;
    }
  }

  saveFAT();//save FAT to disk
  std::cout << "FS::cp(" << sourcefilepath << "," << destfilepath << ")\n";
  return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
  loadCWD();

  dir_entry sourceDirectory[ENTRY_COUNT];
  dir_entry destDirectory[ENTRY_COUNT];

  std::string nameOfFile;
  std::string nameOfDirectory;
  uint8_t buffer[BLOCK_SIZE];


  int sourceBlock = loadDirectory(sourcepath, sourceDirectory, TYPE_FILE, nameOfFile);
  int destBlock = loadDirectory(destpath, destDirectory, TYPE_DIR, nameOfDirectory);

  int testIfDirectory = locateEntry(currentDir, nameOfDirectory, TYPE_DIR);//Check if it's a directory

  if(destpath.find('/') == std::string::npos && testIfDirectory == -1)//rename file
  {
    int destIndex = locateEntry(sourceDirectory, destpath, TYPE_FILE);//Check if exist
    int sourceIndex = locateEntry(sourceDirectory, nameOfFile, TYPE_FILE);//locate to change the name
    if(sourceIndex != -1 && destIndex == -1)
    {
      //rename file
      strcpy(sourceDirectory[sourceIndex].file_name, destpath.c_str());

      memset(buffer, 0 , BLOCK_SIZE);
      memcpy(buffer,sourceDirectory , sizeof(sourceDirectory));
      if(disk.write(sourceBlock, buffer) == -1)//Save the directory
      {
        std::cout <<"Failed when writing the CWD to disk" << std::endl;
        return -1;
      }

    }
  }
  else//move file
  {
    int sourceIndex = locateEntry(sourceDirectory, nameOfFile, TYPE_FILE);
    //move sourcepath file to destpath directory
    if(destBlock == -1)
    {
      return -1;//No directory found.
    }
    //Free entry from currentdir
    dir_entry sourceFile = sourceDirectory[sourceIndex];
    memset(&sourceDirectory[sourceIndex], 0, sizeof(dir_entry));
    //find free entry in destDir
    int freeEntry = locateFreeEntry(destDirectory, nameOfFile);
    destDirectory[freeEntry] = sourceFile;
    //Save destDirectory to disk
    memset(buffer, 0, BLOCK_SIZE);
    memcpy(buffer, destDirectory, sizeof(destDirectory));
    disk.write(destBlock, buffer);

    //Save sourceDirectory to disk
    memset(buffer, 0 , BLOCK_SIZE);
    memcpy(buffer,sourceDirectory , sizeof(sourceDirectory));
    if(disk.write(sourceBlock, buffer) == -1)//Save the directory
    {
      std::cout <<"Failed when writing the CWD to disk" << std::endl;
      return -1;
    }
  }

//If any of the two were CWD we update currentDir
  if(int(CWD) == sourceBlock)
  {
    memcpy(currentDir, sourceDirectory, sizeof(currentDir));
  }
  else if(int(CWD) == destBlock)
  {
    memcpy(currentDir, destDirectory, sizeof(currentDir));
  }


    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{

  dir_entry directory[ENTRY_COUNT];
  std::string nameOfFile;
  int dirBlock = loadDirectory(filepath, directory, TYPE_FILE, nameOfFile);

  int index = locateEntry(directory, nameOfFile, TYPE_FILE);
  if(index == -1)
  {
    return -1;
  }
  int firstBlock = directory[index].first_blk;
  memset(&directory[index], 0, sizeof(dir_entry));//removes entry


// Marks the blocks Free and write to disk with a 0 buffer
  int nextBlock = fat[firstBlock];
  uint8_t buffer[BLOCK_SIZE];
  memset(buffer, 0, BLOCK_SIZE);
  disk.write(firstBlock, buffer);
  fat[firstBlock] = FAT_FREE;
  while (nextBlock != FAT_EOF)
  {
    int index = nextBlock;
    disk.write(nextBlock, buffer);
    nextBlock = fat[nextBlock];
    fat[index] = FAT_FREE;
  }

  if(int(CWD) == dirBlock)
  {
    memcpy(currentDir, directory, sizeof(currentDir));
    saveCWD();
  }
  else
  {
    uint8_t buffer[BLOCK_SIZE];
    memset(buffer, 0 , BLOCK_SIZE);
    memcpy(buffer,directory , sizeof(directory));
    if(disk.write(dirBlock, buffer) == -1)//Save the directory
    {
      std::cout <<"Failed when writing the directory to disk" << std::endl;
      return -1;
    }
  }
  saveFAT();
    std::cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
  uint8_t buffer[BLOCK_SIZE];
  memset(buffer, 0, BLOCK_SIZE);

  std::string nameOfFile1;
  std::string nameOfFile2;
  dir_entry sourceDirectory[ENTRY_COUNT];
  dir_entry destDirectory[ENTRY_COUNT];

  loadDirectory(filepath1, sourceDirectory, TYPE_FILE, nameOfFile1);
  int destBlock = loadDirectory(filepath2, destDirectory, TYPE_FILE, nameOfFile2);
  int fileIndex1 = locateEntry(sourceDirectory, nameOfFile1, TYPE_FILE);
  int fileIndex2 = locateEntry(destDirectory, nameOfFile2, TYPE_FILE);

  int fileFirstBlk1 = sourceDirectory[fileIndex1].first_blk;
  if((READ & sourceDirectory[fileIndex1].access_rights) == false)
  {
    std::cout << filepath1 << " doesn't have read access" << std::endl;
    return -1;
  }
  std::string dataFile1 = readFAT(fileFirstBlk1);

  int fileLastBlk2 = getLastBlock(destDirectory[fileIndex2].first_blk);
  if((WRITE & destDirectory[fileIndex2].access_rights) == false)
  {
    std::cout << filepath2 << " doesn't have write access" << std::endl;
    return -1;
  }
  disk.read(fileLastBlk2, buffer);
  std::string dataLastBlk2;
  dataLastBlk2 = createString(buffer, BLOCK_SIZE);// create a string with uint8_t buffer
  dataLastBlk2 += dataFile1;//appending filepath1 data to the end of filepath2
  saveDataToDisk(fileLastBlk2, dataLastBlk2);
  destDirectory[fileIndex2].size = destDirectory[fileIndex2].size +  dataFile1.size();


  if(int(CWD) == destBlock)
  {
    memcpy(currentDir, destDirectory, sizeof(currentDir));
    saveCWD();
  }
  else
  {
    uint8_t buffer[BLOCK_SIZE];
    memset(buffer, 0 , BLOCK_SIZE);
    memcpy(buffer,destDirectory , sizeof(destDirectory));
    if(disk.write(destBlock, buffer) == -1)//Save the directory
    {
      std::cout <<"Failed when writing the directory to disk" << std::endl;
      return -1;
    }
  }
  saveFAT();
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
  //create the directory entry and update FAT
  dir_entry directory[ENTRY_COUNT];
  std::string nameOfDirectory;
  //TYPE_FILE so loadDirectory doesn't try to load a directory that doesn't exist yey
  int dirBlock = loadDirectory(dirpath, directory, TYPE_FILE, nameOfDirectory);
  int dirEntry = locateFreeEntry(directory, nameOfDirectory);
  if(dirEntry == -1)
  {
    return -1;//Name already exist
  }
  int freeBlock = getFreeBlock();
  createEntry(directory, nameOfDirectory.c_str(), dirEntry, freeBlock, 0, TYPE_DIR, 0);
  fat[freeBlock] = FAT_EOF;
  //create the new subDirectory with its first entry being the parent
  dir_entry subDirectory[ENTRY_COUNT];
  dir_entry parent;
  memset(&subDirectory, 0, sizeof(subDirectory));
  strcpy(parent.file_name, "..");
  parent.type = TYPE_DIR;
  parent.size = 0;
  parent.first_blk = dirBlock;
  parent.access_rights = 0;
  subDirectory[0] = parent;

  //Write the subDirectory to disk and save the FAT and loaded directory
  uint8_t buffer[BLOCK_SIZE];
  memcpy(buffer, subDirectory, sizeof(subDirectory));
  if(disk.write(freeBlock, buffer) == -1)
  {
    std::cout <<"Failed when writing the subDirectory to disk" << std::endl;
    return -1;
  }
  if(int(CWD) == dirBlock)
  {
    memcpy(currentDir, directory, sizeof(currentDir));
    saveCWD();
  }
  else
  {
    uint8_t buffer[BLOCK_SIZE];
    memset(buffer, 0 , BLOCK_SIZE);
    memcpy(buffer,directory , sizeof(directory));
    if(disk.write(dirBlock, buffer) == -1)//Save the directory
    {
      std::cout <<"Failed when writing the directory to disk" << std::endl;
      return -1;
    }
  }
  saveFAT();
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
  dir_entry directory[ENTRY_COUNT];
  std::string nameOfDir;
  int dirBlock = loadDirectory(dirpath, directory, TYPE_DIR, nameOfDir);
  if(dirBlock == -1)
  {
    return -1;// Entry not found
  }
  saveCWD();//Save the previous CWD
  CWD = dirBlock;
  loadCWD();//Load in the new CWD
    std::cout << "FS::cd(" << dirpath << ")\n";
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
  std::vector<std::string> fullPath;
  std::string currentAbsPath;
  uint8_t buffer[BLOCK_SIZE];
  memset(buffer, 0, BLOCK_SIZE);
  dir_entry parent[ENTRY_COUNT];
  if(int(CWD) == ROOT_BLOCK)
  {
    std::cout << "/" << std::endl;
    return 0;
  }
  int childBlock = int(CWD);
  int parentBlock = int(currentDir[0].first_blk);

  while(childBlock != ROOT_BLOCK)
  {
    memset(buffer, 0, BLOCK_SIZE);
    disk.read(parentBlock, buffer);
    memcpy(&parent, buffer, sizeof(parent));
    for (int i = 0; i < ENTRY_COUNT; i++)
    {
      if(int(parent[i].first_blk) == int(childBlock))
      {
        std::string name = createString((uint8_t*)parent[i].file_name, sizeof(parent[i].file_name));
        fullPath.push_back(name);
        break;
      }
    }
    childBlock = parentBlock;
    parentBlock = int(parent[0].first_blk);
  }
  fullPath.push_back("/");
  for (int i = fullPath.size() - 1; i > -1; i--)
  {
    currentAbsPath+=fullPath[i];
    if(i != 0 && i != int(fullPath.size()) - 1)
    {
      currentAbsPath+="/";
    }
  }

  std::cout << currentAbsPath << std::endl;
    std::cout << "FS::pwd()\n";
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
  std::string nameOfFile;
  dir_entry directory[ENTRY_COUNT];
  int dirBlock = loadDirectory(filepath, directory, TYPE_FILE, nameOfFile);
  if(dirBlock == -1)
  {
    //no directory found
    return -1;
  }
  int entryIndex = locateEntry(directory, nameOfFile, TYPE_FILE);
  if(entryIndex == -1)
  {
    return -1;
  }
  uint8_t rights;
  memcpy(&rights, accessrights.c_str(), sizeof(rights));
  directory[entryIndex].access_rights = rights;

  if(int(CWD) == dirBlock)
  {
    memcpy(currentDir, directory, sizeof(currentDir));
    saveCWD();
  }
  else
  {
    uint8_t buffer[BLOCK_SIZE];
    memset(buffer, 0 , BLOCK_SIZE);
    memcpy(buffer,directory , sizeof(directory));
    if(disk.write(dirBlock, buffer) == -1)//Save the directory
    {
      std::cout <<"Failed when writing the directory to disk" << std::endl;
      return -1;
    }
  }
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}
int FS::saveFAT()
{
  uint8_t buffer[BLOCK_SIZE];
  memcpy(&buffer, fat, sizeof(fat));// cpy the fat into the buffer
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
  memcpy(&fat, buffer, sizeof(fat)); // cpy the buffer into the fat
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
    }
  }
  if(blocksFound == nrOfBlocks)
  {
    return 0;// found the number of blocks
  }
  std::cout << "No free block found" << std::endl;
  return -1; // no free block found
}
int FS::getFreeBlock(int startBlock, int nrOfBlocks, int* blocks)// doesn't mark the block as not free
{
  int startIndex = 2;// 0 is root and 1 is FAT so no need to check there
  int blocksFound = 0;
  blocks[0] = startBlock;//first block is the startBlock
  blocksFound++;
  for (int i = startIndex; i < BLOCK_SIZE/2 && nrOfBlocks != blocksFound; i++)
  {
    if(fat[i] == FAT_FREE)
    {
      blocks[blocksFound] = i;
      blocksFound++;
    }
  }
  if(blocksFound == nrOfBlocks)
  {
    return 0;// found the number of blocks
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
  data += createString(buffer, BLOCK_SIZE);
  while (fat[currentBlock] != FAT_EOF)
  {
    currentBlock = fat[currentBlock];
    if(fat[currentBlock] == FAT_FREE)
    {
      std::cout << "Fat is corrupted or entered wrong startblock" << std::endl;
      return data;
    }
    memset(buffer, 0, BLOCK_SIZE);
    disk.read(currentBlock, buffer);
    data += createString(buffer, BLOCK_SIZE);
  }
  return data;
}
int FS::locateEntry(dir_entry* directory, const std::string& filepath, int type)
{
  //return the index to the fileentry located in directory
  for (int i = 0; i < ENTRY_COUNT; i++)
  {
    std::string nameInDir;
    nameInDir = directory[i].file_name;
    if(nameInDir == filepath && int(directory[i].type) == type)
    {
      return i;
    }
  }

  return -1;
}
int FS::saveDataToDisk(const std::string& data)
{
  uint8_t buffer[BLOCK_SIZE];
  memset(buffer, 0, sizeof(buffer));
  uint32_t fileSize = data.size();
  int nrOfBlocks = (int(fileSize - 1)/ BLOCK_SIZE) + 1; // example: (60-1/4096) + 1 = 1 block. (9053-1/4096) + 1 = 2 blocks
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
  int firstBlock = blocksArray[0];
  delete[] blocksArray;
  return firstBlock; // returns the first index to the FAT
}
int FS::saveDataToDisk(int startBlock, const std::string& data)
{
  uint8_t buffer[BLOCK_SIZE];
  memset(buffer, 0, sizeof(buffer));
  uint32_t fileSize = data.size();
  int nrOfBlocks = (int(fileSize) / BLOCK_SIZE) + 1; // example: (60/4096) + 1 = 1 block. (9053/4096) + 1 = 2 blocks
  int dataCopied = 0;
  int* blocksArray = new int[nrOfBlocks];
  if(getFreeBlock(startBlock, nrOfBlocks, blocksArray) == -1)
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
  int firstBlock = blocksArray[0];
  delete[] blocksArray;
  return firstBlock; // returns the first index to the FAT
}
int FS::locateFreeEntry(dir_entry* directory, const std::string& name)
{
  int freeEntry = -1;
  for (int i = 0; i < ENTRY_COUNT; i++)
  {
    std::string nameInDir = directory[i].file_name;
    std::string empty = "";
    if(nameInDir == name)
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
int FS::createEntry(const char* filepath, int entryIndex, uint16_t firstBlock, uint32_t size, uint8_t type, uint8_t access_rights)
{
  dir_entry newFile;
  strcpy(newFile.file_name, filepath);
  newFile.type = type;
  newFile.size = size;
  newFile.first_blk = firstBlock;
  newFile.access_rights = access_rights;
  currentDir[entryIndex] = newFile;
  return 0;
}
int FS::createEntry(dir_entry* directory, const char* filepath, int entryIndex, uint16_t firstBlock, uint32_t size, uint8_t type, uint8_t access_rights)
{
  dir_entry newFile;
  strcpy(newFile.file_name, filepath);
  newFile.type = type;
  newFile.size = size;
  newFile.first_blk = firstBlock;
  newFile.access_rights = access_rights;
  directory[entryIndex] = newFile;
  return 0;
}
int FS::getLastBlock(int startBlock)
{
  int16_t currentBlock = startBlock;
  if(fat[currentBlock] == FAT_FREE)
  {
    std::cout << "Fat is corrupted or entered wrong startblock" << std::endl;
    return -1;
  }
  while (fat[currentBlock] != FAT_EOF)
  {
    currentBlock = fat[currentBlock];
    if(fat[currentBlock] == FAT_FREE)
    {
      std::cout << "Fat is corrupted or entered wrong startblock" << std::endl;
      return -1;
    }
  }
  return int(currentBlock);
}
std::string FS::createString(uint8_t* buffer, int bufferSize)
{
  std::string temp;
  temp.assign(buffer, buffer + bufferSize);

  return std::string(temp.c_str());
}
int FS::loadDirectory(const std::string& filepath, dir_entry* theDirectory, int type, std::string& lastName)
{
  dir_entry loadedDir[ENTRY_COUNT];
  uint8_t buffer[BLOCK_SIZE];
  memset(buffer, 0 , BLOCK_SIZE);
  //Seperating filepath with the seperator '/' into a vector
  std::vector<std::string> sepVec;
  std::string str;
  std::istringstream path(filepath);
  int subDirBlock = CWD;//assumes CWD at start
  while (std::getline(path,str,'/'))
  {
    if(str !="")
    {
      sepVec.push_back(str);
    }
  }

  if(filepath[0] == '/')//Absolute path
  {
    subDirBlock = ROOT_BLOCK;//Start at ROOT_BLOCK
    disk.read(ROOT_BLOCK, buffer);
    memcpy(&loadedDir, buffer, sizeof(loadedDir));
  }
  else//Relative path
  {
    memcpy(&loadedDir, currentDir, sizeof(loadedDir));
  }
  int count = 0;//Is the number of subdirectories in the filepath seperated by '/'
  if(type == TYPE_DIR)
  {
    count = sepVec.size();//Path is just only directories
  }
  else if(type == TYPE_FILE)
  {
    count = sepVec.size() -1;//Path is ending with a filename
  }
  dir_entry subDirEntry;

  for (int i = 0; i < count; i++)
  {
    int dirIndex = locateEntry(loadedDir, sepVec[i], TYPE_DIR);
    if(dirIndex == -1)
    {
      return -1;
    }
    subDirEntry = loadedDir[dirIndex];//sub-directory
    subDirBlock = subDirEntry.first_blk;
    disk.read(subDirBlock, buffer);
    memcpy(&loadedDir, buffer, sizeof(loadedDir));
  }
  if(sepVec.size() > 0)
  {
    lastName = std::string(sepVec[sepVec.size()-1]);//either the last loaded directory or a name of a file
  }
  else
  {
    lastName = "Root";
  }


  memcpy(theDirectory, loadedDir, sizeof(loadedDir));

  return subDirBlock;//returning the block to the loaded directory
}
