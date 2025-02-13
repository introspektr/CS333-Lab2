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
#include <time.h>

// Alias for struct
typedef struct stat stat_t;

// Buffer and mode definitions
#define BUF_SIZE 8192           // Buffer size for reading/writing files
#define DATE_BUF_SIZE 20        // Buffer size for date string formatting
#define DEFAULT_MODE 0664       // Default permissions (rw-rw-r--)
#define DETERMINISTIC_MODE 0644 // Deterministic mode permissions (rw-r--r--)

// Define ON/OFF constants
#ifndef OFF 
# define OFF 0
#endif // OFF 
#ifndef ON 
# define ON 1
#endif // ON 

// Function prototypes
void show_help(void);
void print_toc(int, int);
void get_perm_string(mode_t, char*);
void write_to_archive(int, int, int, const char*);
void extract_archive(int, int);            

int main(int argc, char* argv[]){
    int opt = -1;             // Option for getopt()
    var_action_t action = ACTION_NONE;  // Selected archive action
    char* file_name = NULL;   // -f filename
    int verbose_mode = OFF;   // -v verbose output flag 
    int determ_mode = OFF;    // -D deterministic mode flag
    int archive_fd = -1;      // Archive file descriptor
    int member_fd = -1;       // Member file descriptor
    char buffer[SARMAG];      // Buffer for reading archive tag (ARMAG)

    // Process command line options using getopt
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
                file_name = optarg;
                break;
            case 'h':
                show_help();
                exit(EXIT_SUCCESS);
                break;
            case 'v':
                verbose_mode = ON;
                break;
            case 'D':
                determ_mode = ON; 
                break;
            case 'U':
                determ_mode = OFF;
                break;
            default:
                fprintf(stderr, "Usage: %s %s\n", argv[0], ARVIK_OPTIONS);
                exit(INVALID_CMD_OPTION);
        }
    }
    
    // Validate command arguments
    if (action == ACTION_NONE){ // If not, exit
        fprintf(stderr, "*** %s No action specified\n", argv[0]);
        exit(NO_ACTION_GIVEN);
    }
    
    if (file_name == NULL){
        fprintf(stderr, "Error: must provide archive file name.\n");
        exit(NO_ARCHIVE_NAME);
    }

    // Open or create archive file with read/write permissions
    archive_fd = open(file_name, O_RDWR | O_CREAT, DEFAULT_MODE);
    if (archive_fd == -1){
        perror("Error opening archive file");
        exit(CREATE_FAIL);
    }
    
    if (action == ACTION_CREATE){
        // Write the ARMAG macro to the file
        if (write(archive_fd, ARMAG, SARMAG) != SARMAG){
            perror("Error writing ARMAG");
            close(archive_fd);
            exit(CREATE_FAIL);
        }
        // Ensure correct permissions
        if (fchmod(archive_fd, DEFAULT_MODE) == -1){
            perror("Error setting file permissions");
            close(archive_fd);
            exit(CREATE_FAIL);
        }
        // Process each file argument
        if (optind < argc){
            for (int i = optind; i < argc; i++){
                // Open each member file and write to archive
                member_fd = open(argv[i], O_RDONLY);
                if (member_fd == -1){
                    perror("Error opening member file");
                    close(archive_fd);
                    exit(CREATE_FAIL);
                }
                if (verbose_mode){
                    printf("a - %s\n", argv[i]);
                }
                write_to_archive(archive_fd, member_fd, determ_mode, argv[i]);
            }
        }
    } 
    else{
        // Verify archive format by checking ARMAG signature
        if (read(archive_fd, buffer, SARMAG) != SARMAG){
            fprintf(stderr, "Error: Failed to read ARMAG from archive file.\n");
            close(archive_fd);
            exit(BAD_TAG);
        }

        if (memcmp(buffer, ARMAG, SARMAG) != 0){
            fprintf(stderr, "Error: Invalid archive file format.\n");
            close(archive_fd);
            exit(BAD_TAG);
        }

        // Execute requested action
        if (action == ACTION_TOC){
            print_toc(archive_fd, verbose_mode);
        }

        if (action == ACTION_EXTRACT){
            extract_archive(archive_fd, verbose_mode);            
        }
    }
    close(archive_fd);
    return EXIT_SUCCESS;
}

// Display usage information and available options
void show_help(void){
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

// Display archive contents with optional verbose information
void print_toc(int archive_fd, int verbose_mode){
    ar_hdr_t archive_md;    // Archive header structure
    char buffer[BUF_SIZE];  // General purpose buffer
    char date_buffer[20];   // Buffer for formatted date string
    char permissions[10];   // Buffer for permission string
    char* back_pos = NULL;  // Position of '/' in filename
    mode_t mode;            // File permissions
    time_t mtime;           // File modification time
    struct tm* time_info;   // Structured time information
    off_t file_size;        // Size of current file

    // Process each archive entry
    while (read(archive_fd, &archive_md, sizeof(ar_hdr_t)) == sizeof(ar_hdr_t)){
        // Clean up filename by removing trailing '/'
        strncpy(buffer, archive_md.ar_name, sizeof(archive_md.ar_name) - 1);
        buffer[sizeof(archive_md.ar_name) - 1] = '\0';

        if ((back_pos = strchr(buffer, '/')) != NULL){
            *back_pos = '\0';
        }

        if (verbose_mode){ 
            // Format and display detailed metadata 
            mode = (mode_t) strtol(archive_md.ar_mode, NULL, 8);
            memset(permissions, 0, sizeof(permissions));
            get_perm_string(mode, permissions);

            mtime = strtol(archive_md.ar_date, NULL, 10);
            time_info = localtime(&mtime);
            memset(date_buffer, 0, sizeof(date_buffer));
            strftime(date_buffer, sizeof(date_buffer), "%b %e %R %Y", time_info);

            printf("%s %ld/%ld %ld %s %s\n", permissions,
                   strtol(archive_md.ar_uid, NULL, 10),
                   strtol(archive_md.ar_gid, NULL, 10),
                   strtol(archive_md.ar_size, NULL, 10),
                   date_buffer, buffer);
        }
        else{
            printf("%s\n", buffer);
        }

        // Skip to next entry, accounting for padding
        file_size = strtol(archive_md.ar_size, NULL, 10);
        if (lseek(archive_fd, file_size, SEEK_CUR) == -1){
            perror("Error seeking in archive");
            exit(TOC_FAIL);
        }

        if (file_size % 2 != 0){
            if (lseek(archive_fd, 1, SEEK_CUR) == -1){
                perror("Error seeking in archive");
                exit(TOC_FAIL);
            }
        }
    }
}

// Convert file mode bits to string representation
void get_perm_string(mode_t mode, char* permissions){
    // Set user permissions
    permissions[0] = (mode & S_IRUSR) ? 'r' : '-';
    permissions[1] = (mode & S_IWUSR) ? 'w' : '-';
    permissions[2] = (mode & S_IXUSR) ? 'x' : '-';

    // Set group permissions
    permissions[3] = (mode & S_IRGRP) ? 'r' : '-';
    permissions[4] = (mode & S_IWGRP) ? 'w' : '-';
    permissions[5] = (mode & S_IXGRP) ? 'x' : '-';

    // Set other permissions
    permissions[6] = (mode & S_IROTH) ? 'r' : '-';
    permissions[7] = (mode & S_IWOTH) ? 'w' : '-';
    permissions[8] = (mode & S_IXOTH) ? 'x' : '-';

    // Null terminate the string
    permissions[9] = '\0';
}

// Write a file to the archive with metadata
void write_to_archive(int archive_fd, int member_fd, int determ_mode, const char* file_name){
    stat_t file_md;         // File metadata
    ar_hdr_t archive_md;    // Archive header
    ssize_t bytes_read;     // Bytes read from member file
    ssize_t bytes_written;  // Bytes written to archive
    size_t name_len;        // Length of filename
    char date_buffer[13];   // Buffer for formatted date
    char uid_buffer[7];     // Buffer for user ID
    char gid_buffer[7];     // Buffer for group ID
    char mode_buffer[9];    // Buffer for file mode
    char size_buffer[11];   // Buffer for file size
    char buffer[BUF_SIZE];  // General purpose buffer

    // Get file metadata
    if (fstat(member_fd, &file_md) == -1){
        perror("Error getting file archive_md");
        exit(CREATE_FAIL);
    }

    // Apply deterministic mode settings if enabled
    if (determ_mode == ON){
        file_md.st_mtime = 0;
        file_md.st_uid = 0;
        file_md.st_gid = 0;
        file_md.st_mode = DETERMINISTIC_MODE;
    }
    

    // Format file name with trailing '/' and padding
    strncpy(archive_md.ar_name, file_name, sizeof(archive_md.ar_name) - 1);
    archive_md.ar_name[sizeof(archive_md.ar_name) - 1] = '\0'; // Ensure null termination
    name_len = strlen(archive_md.ar_name);

    if (name_len < sizeof(archive_md.ar_name) - 1){
        archive_md.ar_name[name_len] = '/'; // Append '/'
        memset(archive_md.ar_name + name_len + 1, ' ', sizeof(archive_md.ar_name) - name_len - 1);
    }
    else{
        archive_md.ar_name[sizeof(archive_md.ar_name) - 2] = '/';
        archive_md.ar_name[sizeof(archive_md.ar_name) - 1] = ' ';
    }

    // Format metadata fields for header
    sprintf(date_buffer, "%-12ld", (long) file_md.st_mtime);
    memcpy(archive_md.ar_date, date_buffer, sizeof(archive_md.ar_date));
           
    sprintf(uid_buffer, "%-6d", file_md.st_uid);
    memcpy(archive_md.ar_uid, uid_buffer, sizeof(archive_md.ar_uid));

    sprintf(gid_buffer, "%-6d", file_md.st_gid);
    memcpy(archive_md.ar_gid, gid_buffer, sizeof(archive_md.ar_gid));

    sprintf(mode_buffer, "%-8o", file_md.st_mode);
    memcpy(archive_md.ar_mode, mode_buffer, sizeof(archive_md.ar_mode));

    sprintf(size_buffer, "%-10ld", (long) file_md.st_size);
    memcpy(archive_md.ar_size, size_buffer, sizeof(archive_md.ar_size));

    memcpy(archive_md.ar_fmag, ARFMAG, sizeof(archive_md.ar_fmag));

    // Write header to archive
    bytes_written = write(archive_fd, &archive_md, sizeof(ar_hdr_t));
    if (bytes_written != sizeof(archive_md)){
        perror("Error writing file header to archive");
        exit(CREATE_FAIL);
    }

    // Copy file contents to archive
    while ((bytes_read = read(member_fd, buffer, sizeof(buffer))) > 0){
        bytes_written = write(archive_fd, buffer, bytes_read);
        if (bytes_written != bytes_read){
            perror("Error writing file data to archive");
            exit(CREATE_FAIL);
        }
    }

    // Add padding byte if needed 
    if (file_md.st_size % 2 != 0){
        char pad = '\n';
        if (write(archive_fd, &pad, 1) != 1){
            perror("Error padding file data");
            exit(CREATE_FAIL);
        }
    }
}

// Extract files from archive while preserving metadata
void extract_archive(int archive_fd, int verbose_mode){
    ar_hdr_t archive_md;        // Archive header
    char* back_pos;             // Position of '/' in filename
    int output_fd;              // Output file descriptor
    char buffer[BUF_SIZE];      // General purpose buffer
    ssize_t bytes_read;         // Bytes read from archive
    ssize_t bytes_written;      // Bytes written to output
    off_t file_size;            // Size of current file
    mode_t fileMode;            // File permissions
    struct timespec times[2];   // File timestamps

    // Process each file in archive
    while (read(archive_fd, &archive_md, sizeof(ar_hdr_t)) > 0){
        // Extract and clean up filename (remove trailing '/')
        char file_name[sizeof(archive_md.ar_name) + 1];
        strncpy(file_name, archive_md.ar_name, sizeof(file_name) - 1);
        back_pos = strchr(file_name, '/');
        if (back_pos != NULL){
            *back_pos = '\0';
        }

        // Create new file for output with default permissions
        output_fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, DEFAULT_MODE);
        if (output_fd == -1){
            perror("Error opening output file");
            close(output_fd);
            close(archive_fd);
            exit(EXTRACT_FAIL);
        }

        // Copy file contents to output file
        file_size = strtol(archive_md.ar_size, NULL, 10);
        while (file_size > 0){
            bytes_read = read(archive_fd, buffer, MIN(sizeof(buffer), (size_t)file_size));
            if (bytes_read <= 0){
                perror("Error reading archive data");
                close(output_fd);
                close(archive_fd);
                exit(EXTRACT_FAIL);
            }
            bytes_written = write(output_fd, buffer, bytes_read);
            if (bytes_written != bytes_read){
                perror("Error writing output file");
                close(output_fd);
                close(archive_fd);
                exit(EXTRACT_FAIL);
            }
            file_size -= bytes_read;
        }

        // Restore original file permissions
        fileMode = strtol(archive_md.ar_mode, NULL, 8);
        if (fchmod(output_fd, fileMode) == 1){
            perror("Error setting file permissions");
            close(output_fd);
            close(archive_fd);
            exit(EXTRACT_FAIL);
        }

        // Restore original timestamps
        times[0].tv_sec = strtol(archive_md.ar_date, NULL, 10);
        times[1].tv_sec = strtol(archive_md.ar_date, NULL, 10);
        if (futimens(output_fd, times) == -1){
            perror("Error setting file timestamps");
            close(output_fd);
            close(archive_fd);
            exit(EXTRACT_FAIL);
        }

        // Print extraction status if verbose mode is on
        if (verbose_mode){
            printf("x - %s\n", file_name);
        }

        close(output_fd);

        // Skip padding byte if file size is odd
        if (strtol(archive_md.ar_size, NULL, 10) % 2 != 0){
            lseek(archive_fd, 1, SEEK_CUR);
        }
    }
}
