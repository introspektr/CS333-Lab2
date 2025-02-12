// Cody Jackson 
// CS333 
// 02/14/25
// Lab 2: UNIX File I/O - Arvik
// arvik.c 

#include "arvik.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

typedef struct stat stat_t;

#define BUF_SIZE 50000

#ifndef FALSE
# define FALSE 0
#endif // FALSE
#ifndef TRUE
# define TRUE 1
#endif // TRUE

#ifndef OFF 
# define OFF 0
#endif // OFF 
#ifndef ON 
# define ON 1
#endif // ON 

void showHelp(void);
void writeFileToArchive(int, int, int, const char*);

int main(int argc, char* argv[]){
    int opt = -1;
    var_action_t action = ACTION_NONE;
    char* fileName = NULL;   // -f filename
    int verboseFlag = OFF;   // -v 
    int determFlag = OFF;    // -D & -U

    int fd = -1; // file descriptor
    char buffer[BUF_SIZE]; // Buffer for reading ARMAG
    int isValid = 0; //Flag for ARMAG validation
    int member_fd = -1; // File descriptor for member files

    while ((opt = getopt(argc, argv, ARVIK_OPTIONS)) != -1){
        switch(opt){
            case 'x':
                action = ACTION_EXTRACT;
                break;
            case 'c':
                action = ACTION_CREATE; 
                break;
            case 't':
                action = ACTION_TOC;
                break;
            case 'f':
                fileName = optarg;
                break;
            case 'h':
                showHelp();
                exit(EXIT_SUCCESS);
                break;
            case 'v':
                verboseFlag = ON;
                break;
            case 'D':
                determFlag = ON; 
                break;
            case 'U':
                determFlag = OFF;
                break;
            default:
                fprintf(stderr, "Usage: %s %s\n", argv[0], ARVIK_OPTIONS);
                exit(INVALID_CMD_OPTION);
        }
    }

    // Ensure file name provided if an action is requested
    if (fileName == NULL){
        fprintf(stderr, "Error: must provide archive file name.\n");
        exit(NO_ARCHIVE_NAME);
    }
    
    // Ensure an action was provided
    if (action == ACTION_NONE){ // If not, exit
        fprintf(stderr, "*** %s No action specified\n", argv[0]);
        exit(NO_ACTION_GIVEN);
    }
    else{  // Otherwise open the archive file
        fd = open(fileName, O_RDWR | O_CREAT, 0664);
        if (fd == -1){
            perror("Error opening archive file");
            exit(CREATE_FAIL);
        }
        
        if (action == ACTION_CREATE){
            // Write the ARMAG macro to the file
            if (write(fd, ARMAG, SARMAG) != SARMAG){
                perror("Error writing ARMAG");
                close(fd);
                exit(CREATE_FAIL);
            }
            //Ensure correct permissions
            if (fchmod(fd, 0664) == -1){
                perror("Error setting file permissions");
                close(fd);
                exit(CREATE_FAIL);
            }
            //Process member file arguments
            if (optind < argc){
                for (int i = optind; i < argc; i++){
                    // Open the member file
                    member_fd = open(argv[i], O_RDONLY);
                    if (member_fd == -1){
                        perror("Error opening member file");
                        close(fd);
                        exit(CREATE_FAIL);
                    }
                    // Write the member file to the archive
                    if (verboseFlag){
                        printf("a - %s\n", argv[i]);
                    }
                    writeFileToArchive(fd, member_fd, determFlag, argv[i]);
                }
            }

        } else if (action == ACTION_EXTRACT || action == ACTION_TOC){
            // Validate the ARMAG macro at the start of the archive file
            if (read(fd, buffer, SARMAG) != SARMAG){
                fprintf(stderr, "Error: Failed to read ARMAG from archive file.\n");
                close(fd);
                exit(BAD_TAG);
            }

            // Manually compare the buffer with ARMAG
            isValid = 1;  // Assume the file is valid initially
            for (int i = 0; i < SARMAG; i++) {
                if (buffer[i] != ARMAG[i]) {
                    isValid = 0;  // File is invalid
                    break;
                }
            }

            if (!isValid) {
                fprintf(stderr, "Error: Invalid archive file format.\n");
                close(fd);
                exit(BAD_TAG);
            }
        }
    }
    close(fd);
    return EXIT_SUCCESS;
}

void showHelp(void){
    printf("Usage: arvik -[cxtvDUf:h] archive-file file...\n"
           "\t-c           create a new archive file\n"
           "\t-x           extract members from an existing archive file\n"
           "\t-t           show the table of contents of archive file\n"
           "\t-D           Determinisatic mode: all timestamps are 0, all owership is 0, "
           "permmissions are 0644.\n"
           "\t-U           Non-determinisatic mode: all timestamps are correct, all "
           "owership is saved, permmissions are as on source files.\n"
           "\t-f filename  name of archive file to use\n"
           "\t-v           verbose output\n"
           "\t-h           show help text\n");
}

void writeFileToArchive(int iarch, int member_fd, int determMode, const char* fileName){
    stat_t fileMetaData;
    ar_hdr_t header;
    ssize_t bytesRead;
    ssize_t bytesWritten;
    size_t nameLen;
    char dateBuf[13];
    char uidBuf[7];
    char gidBuf[7];
    char modeBuf[9];
    char sizeBuf[11];
    char buffer[BUF_SIZE]; // Buffer for reading incoming file data

    // Get file metadata
    if (fstat(member_fd, &fileMetaData) == -1){
        perror("Error getting file metadata");
        exit(CREATE_FAIL);
    }

    if (determMode == ON){
        fileMetaData.st_mtime = 0;
        fileMetaData.st_uid = 0;
        fileMetaData.st_gid = 0;
        fileMetaData.st_mode = 0644;
    }
    
    // Fill the header struct with file metadata
    
    // File name (16 bytes)
    strncpy(header.ar_name, fileName, sizeof(header.ar_name) - 1);
    header.ar_name[sizeof(header.ar_name) - 1] = '\0'; // Ensure null termination
    nameLen = strlen(header.ar_name);

    //Append '/' and pad with spaces
    if (nameLen < sizeof(header.ar_name) - 1){
        header.ar_name[nameLen] = '/'; // Append '/'
        memset(header.ar_name + nameLen + 1, ' ', sizeof(header.ar_name) - nameLen - 1);
    }
    else{
        header.ar_name[sizeof(header.ar_name) - 2] = '/';
        header.ar_name[sizeof(header.ar_name) - 1] = ' ';
    }

    // Time info (12 bytes)
    sprintf(dateBuf, "%-12ld", (long) fileMetaData.st_mtime);
    memcpy(header.ar_date, dateBuf, sizeof(header.ar_date));
           
    // User ID (6 bytes)
    sprintf(uidBuf, "%-6d", fileMetaData.st_uid);
    memcpy(header.ar_uid, uidBuf, sizeof(header.ar_uid));

    // Group ID (6 bytes)
    sprintf(gidBuf, "%-6d", fileMetaData.st_gid);
    memcpy(header.ar_gid, gidBuf, sizeof(header.ar_gid));

    //File mode (8 bytes)
    sprintf(modeBuf, "%-8o", fileMetaData.st_mode);
    memcpy(header.ar_mode, modeBuf, sizeof(header.ar_mode));

    //File size (10 bytes)
    sprintf(sizeBuf, "%-10ld", (long) fileMetaData.st_size);
    memcpy(header.ar_size, sizeBuf, sizeof(header.ar_size));

    //Magic number (2 bytes)
    memcpy(header.ar_fmag, ARFMAG, sizeof(header.ar_fmag));

    // Write the header to the archive
    bytesWritten = write(iarch, &header, sizeof(header));
    if (bytesWritten != sizeof(header)) {
        perror("Error writing file header to archive");
        exit(CREATE_FAIL);
    }

    // Write the file data to the archive
    while ((bytesRead = read(member_fd, buffer, sizeof(buffer))) > 0) {
        bytesWritten = write(iarch, buffer, bytesRead);
        if (bytesWritten != bytesRead) {
            perror("Error writing file data to archive");
            exit(CREATE_FAIL);
        }
    }

    // Pad the file data to ensure 2-byte alignment
    if (fileMetaData.st_size % 2 != 0) {
        char pad = '\n';
        if (write(iarch, &pad, 1) != 1) {
            perror("Error padding file data");
            exit(CREATE_FAIL);
        }
    }
}
