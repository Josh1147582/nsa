#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>

/// Prints usage message
void usage(char* filename) {
    printf("\tNSA - yet another CIA network installer for FBI 2.0 or"
           "greater.\n"
           "\tUsage: %s ip-address 1st.cia [2nd.cia 3rd.cia ...]\n", filename);
}

int main(int argc, char* argv[]) {

    // Print usage message if too few args are given.
    if(argc < 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Check the files first.
    // Keep each file name with its size, which we'll need when sending the
    // file.
    struct file_w_size {
        char * file;
        unsigned long int size;
    };

    struct file_w_size filelist[argc-2];

    for( int i=2; i<argc; i++) {
        // Check that the file is readable
        if(access(argv[i], R_OK) == -1) {
            fprintf(stderr, "Cannot read file %s\n", argv[i]);
            return EXIT_FAILURE;
        }

        // Get the size and store it
        struct stat st;
        stat(argv[i], &st);

        filelist[i-2].size = st.st_size;
        filelist[i-2].file = argv[i];

    }

    // Create a TCP/IP socket
    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(sock == -1) {
        fprintf(stderr, "Failed to create socket\n");
        return EXIT_FAILURE;
    }

    // Parse the 3DS's IP address as an unsigned long
    struct sockaddr_in serv_addr;

    if( inet_pton(AF_INET, argv[1], &serv_addr.sin_addr.s_addr) != 1) {
        fprintf(stderr, "Invalid IP\n");
        return EXIT_FAILURE;
    }

    // Set the rest of sockaddr_in
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(5000); // Port 5000
    memset(serv_addr.sin_zero, 0, sizeof(serv_addr.sin_zero));

    // Connect to the 3DS
    if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) {
        fprintf(stderr, "Failed to connect to socket\n");
        return EXIT_FAILURE;
    }


    // FBI first accepts the # of files being sent as a big endian uint

    unsigned int numfiles = argc-2;

    printf("Telling FBI we have %u files to send...\n", numfiles);

    // Swap to big endian
    numfiles = htonl(numfiles);

    send(sock, &numfiles, sizeof(unsigned int), 0);

    // Send each file

    // Allocate a 16MB buffer to hold what's read of the file
    char * buf = malloc(16000000 * sizeof(char));

    for(int i=0; i< argc-2; i++) {

        // Recieve 1 byte ACK from FBI
        recv(sock, NULL, 1, 0);

        // Send the file size as a big endian ulong int
        filelist[i].size = (filelist[i].size & 0x00000000FFFFFFFF) << 32 |
            (filelist[i].size & 0xFFFFFFFF00000000) >> 32;
        filelist[i].size = (filelist[i].size & 0x0000FFFF0000FFFF) << 16 |
            (filelist[i].size & 0xFFFF0000FFFF0000) >> 16;
        filelist[i].size = (filelist[i].size & 0x00FF00FF00FF00FF) << 8  |
            (filelist[i].size & 0xFF00FF00FF00FF00) >> 8;

        printf("Sending size of %s...\n", filelist[i].file);
        send(sock, &filelist[i].size, sizeof(unsigned long int), 0);

        // Open and send the file
        printf("Sending %s...\n", filelist[i].file);
        FILE *fp = fopen(filelist[i].file, "r");

        size_t bytes_read;
        do {
            bytes_read = fread(buf, 1, 16000000, fp);
            send(sock, buf, bytes_read, 0);
        }while(bytes_read);

        printf("Sent %s!\n", filelist[i].file);
        fclose(fp);
    }

    free(buf);

    close(sock);
    return EXIT_SUCCESS;
}

