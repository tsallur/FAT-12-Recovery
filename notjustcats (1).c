/* Thomas Sallurday and Caroline Kistler
   Professor Sorber
   CPSC 3220
   12-4-2020
*/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#define SECTOR_SIZE 512
#define FAT_SIZE 12
#define BYTES_PER_DIRECTORY 32

#define DELETED_BYTE 0xE5
#define SPACE 0x20
#define DIRECTORY_ATTRIBUTE 0x10
#define EMPTY_CLUSTER 0x000

typedef struct file_tag {
  unsigned char* filename; //changing filename to char* from unsigned char*
  unsigned char* extension;
  unsigned char* attributes;
  unsigned char* reserved;
  unsigned char* creationTime;
  unsigned char* creationDate;
  unsigned char* lastAccessDate;
  unsigned char* lastWriteTime;
  unsigned char* lastWriteDate;
  unsigned char* firstLogicalCluster;
  unsigned char* file_size;
  unsigned int actualFileSize;
  unsigned int decodedFLC;
  unsigned char* path;
  bool isDeleted;
  bool isDirectory;
}file;

int fileNumberCounter;
//array for FAT
uint16_t FAT1[3072]; // got 3072 by ((512 * 9) * 2) / 3
//read in file with data
unsigned char allFileData[1474560];
//keep track of file numbers
int fileWriteCounter = 0;
int strArrayCounter;

void recursiveExploreDirectory(file* OgInfo, char* output_directory);
// function will open the disk image file and copy its data
// into the allFileData array for ease of parsing through data
void init(char* input){
  FILE* inputFile = fopen(input,"rb"); // open input file of disc image
  if(inputFile == NULL){
    printf("invalid file");
    exit(1);
  }
  fread(allFileData,sizeof(allFileData),1, inputFile);
  fclose (inputFile);
  strArrayCounter = 0;
  fileNumberCounter = 0;
}


//create file with correct directory name and file path
FILE* createFile(char* directoryName, char* fileName){
  const int buffer = 255;
  char currentDir[buffer]; //make array of size 255
  getcwd(currentDir,buffer); //get string of current working directory
  strcat(currentDir,"/"); // adds / to string
  strcat(currentDir,directoryName); // adds dir destinatiocn to string
  strcat(currentDir,"/");
  strcat(currentDir,fileName); // creating filepath for new file
  FILE* filePtr = fopen(currentDir,"w");
  return filePtr;

}

int compute_sector_number(int cluster){
  int sector_number;
  sector_number = 33 + cluster - 2;
  return sector_number;
}


//write file data into new files in specified directory
void writeToFile(file *fileToBeCopied, char* directoryName){
  unsigned char copiedFile[fileToBeCopied->actualFileSize];
  //find the first cluster
  if(fileToBeCopied->actualFileSize == 0){ //added since we were outputting files that didn't exist
    return;
  }
  
  char string[15];
  sprintf(string, "file%d.", fileNumberCounter);
  fileNumberCounter++;
  strcat(string,(char *)fileToBeCopied->extension);

  FILE* fileToWriteTo = createFile(directoryName,string);
  strArrayCounter++;
  int pos = 0;
  int counter = 0,end = SECTOR_SIZE ;
  bool endCondition = true;
  unsigned int LC = fileToBeCopied->decodedFLC,temp;
  temp = LC;

  while(endCondition){
    pos = SECTOR_SIZE * (compute_sector_number(temp));
    temp = FAT1[LC];
    LC = temp; // LFG!!! it works -Thomas
    while(counter < end){
      copiedFile[counter] = allFileData[pos];
      counter++;
      pos++;
      if(counter > fileToBeCopied->actualFileSize){
        endCondition=false;
        break;
      }
    }
    end += SECTOR_SIZE;
  }

  //copy into
  fwrite(copiedFile,sizeof(unsigned char),sizeof(copiedFile),fileToWriteTo);
}


/* The procedure in writing to deleted files vs normal files is different.
   When writing to deleted files we don't know where the next cluster is
   going to be, so we will just go sequentially until we reach our
   limit to writing to the file (bytes written == fileSize)
*/
void writeToDeletedFile(file *fileToBeCopied,char* directoryName){
  unsigned char copiedFile[fileToBeCopied->actualFileSize];
  //find the first cluster
  char string[15];
  sprintf(string, "file%d.", fileNumberCounter);
  fileNumberCounter++;
   if(fileToBeCopied->actualFileSize == 0){ //addded since we were outputting files that didn't exist
    return;
  }
  strcat(string,(char *)fileToBeCopied->extension);
  FILE* fileToWriteTo = createFile(directoryName,string);
  strArrayCounter++;
  int pos = 0;
  int counter = 0,end = SECTOR_SIZE ;
  bool endCondition = true;
  unsigned int LC = fileToBeCopied->decodedFLC,temp;
  unsigned int zero = 0;
  if(FAT1[LC] != zero){
    return; // we return from this since chances are, deleted file is no longer recoverable
  }
  temp = LC;
  pos = SECTOR_SIZE * (compute_sector_number(temp));
  while(endCondition){
    while(counter < end){
      copiedFile[counter] = allFileData[pos];
      counter++;
      pos++; //position just keeps on getting incremented by 1
      if(counter > fileToBeCopied->actualFileSize){
        endCondition=false;
        break;
      }
    }
    end += SECTOR_SIZE;
  }

  //copy into
  fwrite(copiedFile,sizeof(unsigned char),sizeof(copiedFile),fileToWriteTo);
}

//decodes the FAT entries and adds to FAT1 array
void decodeFat(){
  int current = SECTOR_SIZE;
  int end = SECTOR_SIZE * 10;
  unsigned char first, second, third;
  int entryIter = 0;

  while((current + 3) < end){
    //get correct entry values
    first = allFileData[current];
    second = allFileData[current + 1];
    third = allFileData[current + 2];

    uint16_t first_nib = (second >> 4); // goes to the front of the first niblet
    uint16_t second_nib = ((second & 0x0f) << 8); // goes to the end of the second niblet

    //put nibs in correct entries
    uint16_t firstEntry = (first | second_nib);
    uint16_t secondEntry = (first_nib | (third << 4));

    //add entries into FAT 1 array
    FAT1[entryIter] = firstEntry;
    FAT1[entryIter+1] = secondEntry;
    //iterate for while loop
    entryIter += 2;
    current += 3;
  }
}


//offset will be the new file info starts, function will check if all entries are 0
// since that means the root directory file has no more info to share
bool checkEnd(int offset){ 
  int counter = 0;         
  int zeroCounter = 0;
  while(counter < BYTES_PER_DIRECTORY){
      if(allFileData[offset + counter] == EMPTY_CLUSTER){
        zeroCounter++;
      }
    counter++;
  }
  if(zeroCounter == BYTES_PER_DIRECTORY){
    return true;
  }
  else{
    return false;
  }
}


//prints the output to the terminal
file* printOutput(file *printedFiles,unsigned char* filepath){
  int i = 0;
  for(i = 0; i < 8; i++){
    if(printedFiles->filename[i] == SPACE){
      printedFiles->filename[i] = '\0'; //changed from null
    }
  }

   if(printedFiles->actualFileSize == 0){ //added since we were printing out files that didn't exist
    return printedFiles;
  }
  if(printedFiles->filename[0] == '_'){
    printedFiles->isDeleted = true;
  } else {
    printedFiles->isDeleted = false;
  }
  
  printf("FILE\t");
  
  if (printedFiles->isDeleted == false){
    printf("NORMAL\t");
  } else {
    printf("DELETED\t");
  }

  int size = strlen((char*)filepath);
 
  for(i = 0; i < size; i++){
    if(filepath[i] == SPACE){
      filepath[i] = '\0';
    }
  }

  
  printf("%s",(char*)filepath);

  printf("%s",(char*)printedFiles->filename);
  *(printedFiles->extension + 3) = '\0'; //stops weird characters from printing out after
  printf(".%s\t",printedFiles->extension);
  printf("%u",printedFiles->actualFileSize);
  printf("\n");

  return printedFiles;
}


void recursiveExploreDELETEDDirectory(file* OgInfo, char* output_directory){
//and gives out direct output when files are found
  int start = compute_sector_number(OgInfo->decodedFLC) * SECTOR_SIZE;
  start += 64; // skip . and ..
  int end = start + SECTOR_SIZE; // 512 is sector size, so add that to start 
  int counter = 0;
  bool endCondition = false;
  //go until root directory is empty
  while(endCondition == false){
    if(start == end){
      return;
    }

    file *fileInfo = malloc(sizeof(file));
    fileInfo->path = malloc(sizeof(unsigned char) * 100);
    strcpy((char*) fileInfo->path,(char* )OgInfo->path);
    
    //filename
    fileInfo->filename = (unsigned char*)malloc(sizeof(8 * sizeof(char)));
    // code does nothing but too scared to take it ouut AKA USELESS
    if( (strcmp((char*)fileInfo->filename,".") == 0) || (strcmp((char *)fileInfo->filename,"..") == 0)){
      //skip section
      start += BYTES_PER_DIRECTORY;
    }
    else{
      for (int i = 0; i < 8; i++) {
        fileInfo->filename[i] = allFileData[start + i];
        if(fileInfo->filename[0] == DELETED_BYTE ){
          fileInfo->isDeleted = true;
          fileInfo->filename[0] = '_'; //replace 0xe5 with an underscore since 0xe5 is not printable
        } 
        else {
          fileInfo->isDeleted = false;
        }
      }
    
      start += 8;

      //extension
      fileInfo->extension = (unsigned char*)malloc(sizeof(3 * sizeof(char)));
      for(int i = 0; i < 3; i++){
        fileInfo->extension[i] = allFileData[start + i];
      }
      start += 3;

      //attributes
      fileInfo->attributes = (unsigned char*)malloc(sizeof( unsigned char));
      fileInfo->attributes[0] = allFileData[start];
      start += 1;
    
      uint8_t attribute = (uint8_t)fileInfo->attributes[0];

    
      if ((attribute & DIRECTORY_ATTRIBUTE) != DIRECTORY_ATTRIBUTE){
        //file
        fileInfo->isDirectory = false; //magic
      } else {
        //directory
        fileInfo->isDirectory = true;
      
      }
      //reserved
      fileInfo->reserved = (unsigned char*)malloc(sizeof(char) * 2);
      for(int i = 0; i < 2; i++){
        fileInfo->reserved[i] = allFileData[start + i];
      }
      start += 2;

      //creation time
      fileInfo->creationTime = (unsigned char*)malloc(sizeof(char) * 2);
      for(int i = 0; i < 2; i++){
        fileInfo->creationTime[i] = allFileData[start + i];
      }
      start += 2;

      //creation date
      fileInfo->creationDate = (unsigned char*)malloc(sizeof(char) * 2);
      for(int i = 0; i < 2; i++){
        fileInfo->creationDate[i] = allFileData[start + i];
      }
      start += 2;

      //last Access date
      fileInfo->lastAccessDate = (unsigned char*)malloc(sizeof(char) * 4);
      for(int i = 0; i< 2;i++){
        fileInfo->lastAccessDate[i] = allFileData[start + i];
      }
      start += 4;

      //last write time
      fileInfo->lastWriteTime = (unsigned char*)malloc(sizeof(char) * 2);
      for (int i = 0; i < 2; i++){
        fileInfo->lastWriteTime[i] = allFileData[start + i];
      }
      start += 2;

      //last write date
      fileInfo->lastWriteDate = (unsigned char*)malloc(sizeof(char) * 2);
      for (int i = 0; i < 2; i++){
        fileInfo->lastWriteDate[i] = allFileData[start + i];
      }
      start += 2;

      //first logical cluster
      fileInfo->firstLogicalCluster = (unsigned char*)malloc(sizeof(char) * 2);
      for (int i = 0; i < 2; i++){
        fileInfo->firstLogicalCluster[i] = allFileData[start + i];
      }
      fileInfo->decodedFLC = (fileInfo->firstLogicalCluster[0] + (fileInfo->firstLogicalCluster[1] << 8));

      start += 2;

      //file size
      fileInfo->file_size =(unsigned char*)malloc(sizeof(char) * 4);
      int bitShift = 0;
      for (int i = 0; i < 4; i++){
        //convert to integers
        fileInfo->file_size[i] = (allFileData[start + i] >> bitShift) & 0xFF;
      }
      fileInfo->actualFileSize = (fileInfo->file_size[0] << 24) | (fileInfo->file_size[1] << 16) | (fileInfo->file_size[2] << 8) | fileInfo->file_size[3];

      fileInfo->actualFileSize = *(int *)fileInfo->file_size;

      start += 4;
      counter++;
      endCondition = checkEnd(start); 
      // This is a normal file
      if(fileInfo->isDirectory != true && fileInfo->isDeleted == false){
        writeToFile(fileInfo,output_directory);
        printOutput(fileInfo,fileInfo->path);
      }
      //deleted file
      else if(fileInfo->isDirectory != true && fileInfo->isDeleted == true){
        writeToDeletedFile(fileInfo,output_directory);
        printOutput(fileInfo,fileInfo->path);
      }
      //is a directory
      else if (fileInfo->isDirectory == true && fileInfo->isDeleted == false){
       
        //printf("filepath after not adding a /: %s\n",(char*)fileInfo->path);
        strcat((char*)fileInfo->path,(char*)fileInfo->filename);
        for(int i = 0; i < strlen((char*) fileInfo->path); i++){
          if(fileInfo->path[i] == SPACE){
            fileInfo->path[i] = '\0';
          }
        }
        strcat((char*) fileInfo->path,"/");
        recursiveExploreDirectory(fileInfo,output_directory);
      }
      else{
          //printf("filepath after not adding a /: %s\n",(char*)fileInfo->path);
        strcat((char*)fileInfo->path,(char*)fileInfo->filename);
        for(int i = 0; i < strlen((char*) fileInfo->path); i++){
          if(fileInfo->path[i] == SPACE){
            fileInfo->path[i] = '\0';
          }
        }
        strcat((char*) fileInfo->path,"/");
        recursiveExploreDELETEDDirectory(fileInfo,output_directory);
      }
    } //this is where else bracket ends for . and ..
  }
  return;

}








// and so the nightmare begins...
// recursive function for navigating through subdirectories found in the root directory
void recursiveExploreDirectory(file* OgInfo, char* output_directory){
//and gives out direct output when files are found
  int start = compute_sector_number(OgInfo->decodedFLC) * SECTOR_SIZE;
  start += 64; // skip . and ..
  int end = start + SECTOR_SIZE; // 512 is sector size, so add that to start 
  int counter = 0;
  bool endCondition = false;
  unsigned int LC = OgInfo->decodedFLC,temp;
  temp = LC;
  //go until root directory is empty
  while(endCondition == false){
    if(start == end){
      start  = SECTOR_SIZE * (compute_sector_number(temp));
      temp = FAT1[LC];
      LC = temp;
      end = start + SECTOR_SIZE;
    }

    file *fileInfo = malloc(sizeof(file));
    fileInfo->path = malloc(sizeof(unsigned char) * 100);
    strcpy((char*) fileInfo->path,(char* )OgInfo->path);
    
    //filename
    fileInfo->filename = (unsigned char*)malloc(sizeof(8 * sizeof(char)));

    if( (strcmp((char*)fileInfo->filename,".") == 0) || (strcmp((char *)fileInfo->filename,"..") == 0)){
      //skip section
      start += BYTES_PER_DIRECTORY;
    }
    else{
      for (int i = 0; i < 8; i++) {
        fileInfo->filename[i] = allFileData[start + i];
      }
      if(fileInfo->filename[0] == DELETED_BYTE ){
        fileInfo->isDeleted = true;
        fileInfo->filename[0] = '_'; //replace 0xe5 with an underscore since 0xe5 is not printable
      } else {
        fileInfo->isDeleted = false;
      }
    
      start += 8;

      //extension
      fileInfo->extension = (unsigned char*)malloc(sizeof(3 * sizeof(char)));
      for(int i = 0; i < 3; i++){
        fileInfo->extension[i] = allFileData[start + i];
      }
      start += 3;

      //attributes
      fileInfo->attributes = (unsigned char*)malloc(sizeof( unsigned char));
      fileInfo->attributes[0] = allFileData[start];
      start += 1;
    
      uint8_t attribute = (uint8_t)fileInfo->attributes[0];

    
      if ((attribute & DIRECTORY_ATTRIBUTE) != DIRECTORY_ATTRIBUTE){
        //file
        fileInfo->isDirectory = false; //magic
      } else {
        //directory
        fileInfo->isDirectory = true;
      
      }
      //reservedd
      fileInfo->reserved = (unsigned char*)malloc(sizeof(char) * 2);
      for(int i = 0; i < 2; i++){
        fileInfo->reserved[i] = allFileData[start + i];
      }
      start += 2;

      //creation time
      fileInfo->creationTime = (unsigned char*)malloc(sizeof(char) * 2);
      for(int i = 0; i < 2; i++){
        fileInfo->creationTime[i] = allFileData[start + i];
      }
      start += 2;

        //creation date
        fileInfo->creationDate = (unsigned char*)malloc(sizeof(char) * 2);
        for(int i = 0; i < 2; i++){
          fileInfo->creationDate[i] = allFileData[start + i];
        }
      start += 2;

      //last Access date
      fileInfo->lastAccessDate = (unsigned char*)malloc(sizeof(char) * 4);
      for(int i = 0; i< 2;i++){
        fileInfo->lastAccessDate[i] = allFileData[start + i];
      }
      start += 4;

      //last write time
      fileInfo->lastWriteTime = (unsigned char*)malloc(sizeof(char) * 2);
      for (int i = 0; i < 2; i++){
        fileInfo->lastWriteTime[i] = allFileData[start + i];
      }
      start += 2;

      //last write date
      fileInfo->lastWriteDate = (unsigned char*)malloc(sizeof(char) * 2);
      for (int i = 0; i < 2; i++){
        fileInfo->lastWriteDate[i] = allFileData[start + i];
      }
      start += 2;

      //first logical cluster
      fileInfo->firstLogicalCluster = (unsigned char*)malloc(sizeof(char) * 2);
      for (int i = 0; i < 2; i++){
        fileInfo->firstLogicalCluster[i] = allFileData[start + i];
      }
      fileInfo->decodedFLC = (fileInfo->firstLogicalCluster[0] + (fileInfo->firstLogicalCluster[1] << 8));

      start += 2;

      //file size
      fileInfo->file_size =(unsigned char*)malloc(sizeof(char) * 4);
      int bitShift = 0;
      for (int i = 0; i < 4; i++){
        //convert to integers
        fileInfo->file_size[i] = (allFileData[start + i] >> bitShift) & 0xFF;
      }
      fileInfo->actualFileSize = (fileInfo->file_size[0] << 24) | (fileInfo->file_size[1] << 16) | (fileInfo->file_size[2] << 8) | fileInfo->file_size[3];

      fileInfo->actualFileSize = *(int *)fileInfo->file_size;

      start += 4;
      counter++;
      endCondition = checkEnd(start); 
      // This is a normal file
      if(fileInfo->isDirectory != true && fileInfo->isDeleted == false){
        writeToFile(fileInfo,output_directory);
        printOutput(fileInfo,fileInfo->path);
      }
      //deleted file
      else if(fileInfo->isDirectory != true && fileInfo->isDeleted == true){
        writeToDeletedFile(fileInfo,output_directory);
        printOutput(fileInfo,fileInfo->path);
      }
      //is a directory
      else if (fileInfo->isDirectory == true && fileInfo->isDeleted == false){
        //printf("filepath after not adding a /: %s\n",(char*)fileInfo->path);
        strcat((char*)fileInfo->path,(char*)fileInfo->filename);
        for(int i = 0; i < strlen((char*) fileInfo->path); i++){
          if(fileInfo->path[i] == SPACE){
            fileInfo->path[i] = '\0';
          }
        }
        strcat((char*) fileInfo->path,"/");
        recursiveExploreDirectory(fileInfo,output_directory);
      }
      else{
        //printf("filepath after not adding a /: %s\n",(char*)fileInfo->path);
        strcat((char*)fileInfo->path,(char*)fileInfo->filename);
        for(int i = 0; i < strlen((char*) fileInfo->path); i++){
          if(fileInfo->path[i] == SPACE){
            fileInfo->path[i] = '\0';
          }
        }
        strcat((char*) fileInfo->path,"/");
        recursiveExploreDELETEDDirectory(fileInfo,output_directory);
      }
    } // this is where the else bracket goes for . and .. directories
  }
  return;

}

void parseRootDirectoryForFiles(char* output_directory){ //function parses through the root directory 
//and gives out direct output when files are found
  int start = SECTOR_SIZE * 19; // 19 is the sector where the root directory starts
  int counter = 0;
  bool endCondition = false;
  //go until root directory is empty
  while((counter < (14 * SECTOR_SIZE) / BYTES_PER_DIRECTORY) && endCondition == false){ 
    file *fileInfo = malloc(sizeof(file));
    
    //filename
    fileInfo->filename = (unsigned char*)malloc(sizeof(8 * sizeof(char)));
    for (int i = 0; i < 8; i++) {
      fileInfo->filename[i] = allFileData[start + i];
    }
    start += 8;
    if(fileInfo->filename[0] == DELETED_BYTE){
      fileInfo->isDeleted = true;
      fileInfo->filename[0] = '_'; //replace 0xe5 with an underscore since 0xe5 is not printable
    } else {
      fileInfo->isDeleted = false;
    }

    fileInfo->path = malloc(sizeof(unsigned char) * 100);
    strcat((char*)fileInfo->path,"/");

    //extension
    fileInfo->extension = (unsigned char*)malloc(sizeof(3 * sizeof(char)));
    for(int i = 0; i < 3; i++){
      fileInfo->extension[i] = allFileData[start + i];
    }
    start += 3;

    //attributes
    fileInfo->attributes = (unsigned char*)malloc(sizeof( unsigned char));
    fileInfo->attributes[0] = allFileData[start];
    start += 1;
    
    uint8_t attribute = (uint8_t)fileInfo->attributes[0];

    
    if ((attribute & DIRECTORY_ATTRIBUTE) != DIRECTORY_ATTRIBUTE){
      //file
      fileInfo->isDirectory = false; 
    } else {
      //directory
      fileInfo->isDirectory = true;
    }
    //reserved
    fileInfo->reserved = (unsigned char*)malloc(sizeof(char) * 2);
    for(int i = 0; i < 2; i++){
      fileInfo->reserved[i] = allFileData[start + i];
    }
    start += 2;

    //creation time
    fileInfo->creationTime = (unsigned char*)malloc(sizeof(char) * 2);
    for(int i = 0; i < 2; i++){
      fileInfo->creationTime[i] = allFileData[start + i];
    }
    start += 2;

    //creation date
    fileInfo->creationDate = (unsigned char*)malloc(sizeof(char) * 2);
    for(int i = 0; i < 2; i++){
      fileInfo->creationDate[i] = allFileData[start + i];
    }
    start += 2;

    //last Access date
    fileInfo->lastAccessDate = (unsigned char*)malloc(sizeof(char) * 4);
    for(int i = 0; i< 2;i++){
      fileInfo->lastAccessDate[i] = allFileData[start + i];
    }
    start += 4;

    //last write time
    fileInfo->lastWriteTime = (unsigned char*)malloc(sizeof(char) * 2);
    for (int i = 0; i < 2; i++){
      fileInfo->lastWriteTime[i] = allFileData[start + i];
    }
    start += 2;

    //last write date
    fileInfo->lastWriteDate = (unsigned char*)malloc(sizeof(char) * 2);
    for (int i = 0; i < 2; i++){
      fileInfo->lastWriteDate[i] = allFileData[start + i];
    }
    start += 2;

    //first logical cluster
    fileInfo->firstLogicalCluster = (unsigned char*)malloc(sizeof(char) * 2);
    for (int i = 0; i < 2; i++){
      fileInfo->firstLogicalCluster[i] = allFileData[start + i];
    }
    fileInfo->decodedFLC = (fileInfo->firstLogicalCluster[0] + (fileInfo->firstLogicalCluster[1] << 8));

    start += 2;

    //file size
    fileInfo->file_size =(unsigned char*)malloc(sizeof(char) * 4);
    int bitShift = 0;
    for (int i = 0; i < 4; i++){
      //convert to integers
      fileInfo->file_size[i] = (allFileData[start + i] >> bitShift) & 0xFF;
    }
    fileInfo->actualFileSize = (fileInfo->file_size[0] << 24) | (fileInfo->file_size[1] << 16) | (fileInfo->file_size[2] << 8) | fileInfo->file_size[3];

    fileInfo->actualFileSize = *(int *)fileInfo->file_size;

    start += 4;
    counter++;
    endCondition = checkEnd(start);
    // This is a normal file
    if(fileInfo->isDirectory != true && fileInfo->isDeleted == false){
      writeToFile(fileInfo,output_directory);
      printOutput(fileInfo,fileInfo->path);
    }
    //deleted file
    else if(fileInfo->isDirectory != true && fileInfo->isDeleted == true){
      writeToDeletedFile(fileInfo,output_directory);
      printOutput(fileInfo,fileInfo->path);
    }
    //is a directory
    else if(fileInfo->isDirectory == true && fileInfo->isDeleted == false){
      strcat((char*)fileInfo->path,(char*)fileInfo->filename);
      strcat((char*) fileInfo->path, "/");
      
      int size = sizeof(fileInfo->path);
      for(int i = 0; i < size; i++){
        if(fileInfo->path[i] == SPACE){
          fileInfo->path[i] = '\0';
        }
      }
      strcat((char*) fileInfo->path,"/");
      recursiveExploreDirectory(fileInfo, output_directory);
    }
    else{
      strcat((char*)fileInfo->path,(char*)fileInfo->filename);
      for(int i = 0; i < strlen((char*) fileInfo->path); i++){
        if(fileInfo->path[i] == SPACE){
          fileInfo->path[i] = '\0';
        }
      }
      strcat((char*) fileInfo->path,"/");
      recursiveExploreDELETEDDirectory(fileInfo,output_directory);
    }

  }
  return;

}



int main(int arg, char** argv){
  init(argv[1]);
  decodeFat();
  parseRootDirectoryForFiles(argv[2]);
  return 0;
}
