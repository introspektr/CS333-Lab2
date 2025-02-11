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

int main(int argc, char* argv[]){
    int opt = -1;
    var_action_t action = ACTION_NONE;
    char* fileName = NULL;   // -f filename
    int verboseFlag = OFF;   // -v 
    int determFlag = OFF;    // -D
    int nonDetermFlag = OFF; // -U

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
                nonDetermFlag = ON;
                break;
            default:
                fprintf(stderr, "Invalid option: -%c\n", opt);
                exit(INVALID_CMD_OPTION);
        }
    }

    // Silence unused variable warnings
    (void)determFlag;
    (void)nonDetermFlag;


    // Ensure file name provided if an action is requested
    if (fileName == NULL && action != ACTION_NONE){
        fprintf(stderr, "Error: must provide archive file name.\n");
        exit(NO_ARCHIVE_NAME);
    }
    
    // Ensure an action was provided
    if (action == ACTION_NONE){ // If not, exit
        fprintf(stderr, "Error: no action specified.\n");
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
                perror("Erroe writing ARMAG");
                close(fd);
                exit(CREATE_FAIL);
            }
            //Ensure correct permissions
            if (fchmod(fd, 0664) == -1){
                perror("Error setting file permissions");
                close(fd);
                exit(CREATE_FAIL);
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


    // Handle non-option arguments (file members for -c)
    if (optind < argc && action == ACTION_CREATE){
        for (int i = optind; i < argc; i++){
            if (verboseFlag){
                printf("Adding file: %s\n", argv[i]);
            }
            member_fd = open(argv[i], O_RDONLY);
            if (member_fd == -1){
                perror("Error opening member file");
                exit(CREATE_FAIL);
            }

            //Process the file (to be implemented later)
            close(member_fd);
        }
    }

    close(fd);
    return EXIT_SUCCESS;
}

void showHelp(void){
    printf("Usage: arvik -[cxtvDUf:h] archive-file file...\n"
           "\t-c\t\tcreate a new archive file\n"
           "\t-x\t\textract members from an existing archive file\n"
           "\t-t\t\tshow the table of contents of archive file\n"
           "\t-D\t\tDeterminisatic mode: all timestamps are 0, all owership is 0, "
           "permmissions are 0644.\n"
           "\t-U\t\tNon-determinisatic mode: all timestamps are correct, all "
           "owership is saved, permmissions are as on source files.\n"
           "\t-f filename\tname of archive file to use\n"
           "\t-v\t\tverbose output\n"
           "\t-h\t\tshow help text\n");
}
