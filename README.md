Caroline Kistler
Thomas Sallurday
CPSC 3220

Project #4: Recovering the Lost Bits
Due: November 3rd, 2020

Academic Integrity DISCLAIMER: Any use of this code as a guide/copy for Dr. Sorber's intro to Operating Systems course is prohibited. He uses a plagarism checker he programmed himself so you will be caught. Dr. Sorber if you are reading this and would like me to make this private, please email me at tsallurd@gmail.com.

DESCRIPTION:
A tool that will parse disk images using FAT12 and will print out information about the files that the disk images contain. The program will print out a single line for each file that it finds and also write the recovered files contents into a file in the specified output directory.

DESIGN:
We created 2 different arrays. The first array held the data of 1.4 megabytes of all of the data in the FAT12 file system. The second array held the FAT1 (or FAT2 if corrupted). Futhermore, to hold information about each file, deleted or normal, we created a struct to track the files specification. Some of these include the name of the file, file size, if it a directory or not, and a lot more. We intialized these variables in the init function. Then we decode the FAT array. Lastly, we parse through the root directory. It says information about each file or directory that it finds and will write to the output directory specified in the command line arguments. If it finds a directory it will recursively search through the directory until it finds a file or does not. Lastly, it will find deleted files and directories and recover them. The last part of the program is that it will print the file, whether it is deleted or normal, the path to the file, and the file size.
