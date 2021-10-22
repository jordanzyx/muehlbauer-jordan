#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <time.h>

#define MAX 256
#define PORT 1234
#define EOT_SERVER "\r\r\r\r\n\n\r\r"

int n;

char ans[MAX];
char line[MAX];

int sfd, cfd;

void init();

int serverGet(char *fileName);
int serverPut(char *fileName);
int serverLs(char *path);
void displayDir(char *path);
int serverCd(char *path);
int serverMkDir(char *name);
int serverRmDir(char *path);
int serverRm(char *path);
int serverPWD();

void init(){
    chdir("./");
    chroot("./");
}

int serverLs(char *path){
    //Create a buffer to read the current working directory into
    char buf[MAX] = "";
    getcwd(buf, MAX);

    //When the path isnt defined use the working directory
    if (strlen(path) == 0){
        displayDir(buf);
        return 0;
    }

    //Show the asked for directory
    displayDir(path);
}

void displayDir(char *path){
    //Open the directory
    DIR *directory = opendir(path);

    //Create variable to use while crawling through the directory
    struct dirent *entry;

    //If we couldn't open the directory get out of function
    if (directory == NULL)return;

    //Attempt to read the first entry in the directory
    entry = readdir(directory);

    //This is where we store all of the entries to write out
    char output[MAX] = "";

    //Loop through all of the entries and display them
    while (entry != NULL){
        strcat(output,entry->d_name);
        strcat(output," ");
        entry = readdir(directory);
    }

    //Add new line to end
    strcat(output,"\n");

    //Write to client
    write(cfd,output,MAX);

    //Close the directory
    closedir(directory);
}

int serverGet(char *fileName) {
    //Define stat variables we use to lsat the files we are getting info on files
    struct stat fstat;
    struct stat *sp;

    sp = &fstat;


    //Define a buffer to store the size of the file into & send back to the client
    char sizeString[MAX];

    //Lstat into the file & store results into fstat.
    int result = lstat(fileName, &fstat);

    //After check results to make sure we could complete the operation
    if (result < 0) {
        printf("Cannot stat into %s\n", fileName);
        return -1;
    }

    //Store the file size we are getting so we know how to write on a loop
    int fSize = sp->st_size;

    //Write into the sizeString we created above and relay that info to the client so they know how long to read aswell
    sprintf(sizeString, "%d", fSize);

    //Write to client.
    write(cfd, sizeString, MAX);


    //Open the file the client wants to get so we can start writing
    int fp = open(fileName, O_RDONLY);

    //Confirm that we actually opened the file
    if (fp <= 0) {
        close(fp);
        return 0;
    }

    //Create a new buffer to read the bytes from the file into
    char fileBuf[MAX];

    //Read through the file 256 bytes at a time. This is the initial read
    result = read(fp, fileBuf, MAX);

    //Until we are out of information to read go through the file
    while (result > 0) {
        //Write the info
        write(cfd, fileBuf, MAX);

        //Read again
        result = read(fp, fileBuf, MAX);
    }

    //Close file & exit
    close(fp);
    return 0;
}

int serverPut(char *fileName) {
    // get the # of bytes that the file is
    char buf[MAX];

    //Read from the client how large this file is, parse using atoi
    read(cfd, buf, MAX);
    int fileSize = atoi(buf);

    // Create a file to write this data into
    int fd = open(fileName, O_WRONLY | O_CREAT, 0644);

    //If we could not create a file for some reason and the file descriptor is broken exit out of function
    printf("file size: %d\n",fileSize);
    printf("%d\n",fd);
    if (fd <= 0)return -1;

    //loop through until we are no longer reading data
    while (fileSize > 0) {
        //Read
        read(cfd, buf, MAX);

        //Figure out how much we need to write to the file
        int readAmount = fileSize >= MAX ? MAX : fileSize;

        //Write to the file
        write(fd,buf,readAmount);

        //Adjust how much is left to read
        fileSize -= readAmount;
    }

    //Close the file
    close(fd);
}

int serverMkDir(char *name) {
    return mkdir(name, 0755);
}

int serverRmDir(char *path) {
    return rmdir(path);
}

/**
 * Print the current working directory on the server to the client
 * @return nothing
 */
int serverPWD() {
    //Create a buffer to read the CWD into
    char buf[MAX] = "";

    //Read in cwd
    getcwd(buf, MAX);

    //Add new line to end
    strcat(buf, "\n");

    //Write to the client
    write(cfd, buf, MAX);

    //Print to our screen
    printf("%s", buf);
}

/**
 * Remove the file from our server
 * @param path of the file to remove
 * @return status of the operation
 */
int serverRm(char *path) {
    return unlink(path);
}

int serverCd(char *path) {
    return chdir(path);
}

int tokenizeLine(char *line,char *args[])
{
    //Where we are storing the argument
    int nArgs = 0;
    char *p = strtok(line, " ");

    //Loop through and store the arguments
    while (p != NULL)
    {
        args[nArgs++] = p;
        p = strtok(NULL, " ");
    }

    return nArgs;
}


int main() {
    int len;
    struct sockaddr_in saddr, caddr;
    int i, length;

    init();

    printf("1. create a socket\n");
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        printf("socket creation failed\n");
        exit(0);
    }

    printf("2. fill in server IP and port number\n");
    bzero(&saddr, sizeof(saddr));
    saddr.sin_family = AF_INET;
    //saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    saddr.sin_port = htons(PORT);

    printf("3. bind socket to server\n");
    if ((bind(sfd, (struct sockaddr *) &saddr, sizeof(saddr))) != 0) {
        printf("socket bind failed\n");
        exit(0);
    }

    // Now server is ready to listen and verification
    if ((listen(sfd, 5)) != 0) {
        printf("Listen failed\n");
        exit(0);
    }


    while (1) {
        // Try to accept a client connection as descriptor newsock
        length = sizeof(caddr);
        cfd = accept(sfd, (struct sockaddr *) &caddr, &length);
        if (cfd < 0) {
            printf("server: accept error\n");
            exit(1);
        }

        printf("server: accepted a client connection from\n");
        printf("-----------------------------------------------\n");
        printf("    IP=%s  port=%d\n", "127.0.0.1", ntohs(caddr.sin_port));
        printf("-----------------------------------------------\n");

        // Processing loop
        while (1) {
            printf("server ready for next request ....\n");
            n = read(cfd, line, MAX);
            if (n == 0) {
                printf("server: client died, server loops\n");
                close(cfd);
                break;
            }

            //Get our arguments
            char *args[64];
            int amount = tokenizeLine(line,args);
            char *dir = "";

            for (int j = 0; j < amount; ++j) {
                printf("ARG %d = %s\n",j,args[j]);
            }

            char *command = line;

            //Handle based on command
            if(strcmp(command,"get") == 0){
                if(args[1] != NULL){
                    serverGet(args[1]);
                }
            }

            if(strcmp(command,"put") == 0){
                if(args[1] != NULL){
                    serverPut(args[1]);
                }
            }
            if(strcmp(command,"ls") == 0){
                if (amount > 1)serverLs(args[1]);
                else serverLs(dir);
            }
            if(strcmp(command,"pwd") == 0){
                serverPWD();
            }
            if(strcmp(command,"mkdir") == 0){
                if(args[1] != NULL){
                    serverMkDir(args[1]);
                }
            }
            if(strcmp(command,"rmdir") == 0){
                if(args[1] != NULL){
                    serverRmDir(args[1]);
                }
            }
            if(strcmp(command,"rm") == 0){
                if(args[1] != NULL){
                    serverRm(args[1]);
                }
            }
            if(strcmp(command,"cd") == 0){
                if(args[1] != NULL){
                    serverCd(args[1]);
                }
            }

            // send the echo line to client
            n = write(cfd, EOT_SERVER, MAX);

//            printf("server: wrote n=%d bytes; ECHO=[%s]\n", n, line);
//           printf("server: ready for next request\n");
        }
    }
}
