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
#include <libgen.h>     // for dirname()/basename()
#include <time.h>

#define MAX 256
#define PORT 1234
#define EOT_CLIENT "\r\r\r\r\n\n\r\r"

char line[MAX], ans[MAX];
int n;

struct sockaddr_in saddr;
int sfd;

int localLs(char * pathName);
int localCat(char *fileName);
void displayDirectory(char *pathName);
int isEOT(char *s);
int localCd(char *pathName);
int localPut(char *fileName);
int localGet(char *fileName);
int localPWD();
int localRm(char *pathName);
int localMkDir(char *pathName);
int localRmDir(char *pathName);
int menu();

int localPut(char *fileName){
    //Define stat variables we use to lsat the files we are getting info on files
    struct stat fstat;
    struct stat *sp;

    sp = &fstat;


    //Define a buffer to store the size of the file into & send back to the client
    char sizeString[MAX] = "";

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
    write(sfd, sizeString, MAX);


    //Open the file the client wants to get so we can start writing
    int fp = open(fileName, O_RDONLY);

    printf("1\n");

    //Confirm that we actually opened the file
    if (fp <= 0) {
        close(fp);
        return 0;
    }

    printf("2\n");

    //Create a new buffer to read the bytes from the file into
    char fileBuf[MAX] = "";



    //Read through the file 256 bytes at a time. This is the initial read
    result = read(fp, fileBuf, MAX);

    printf("3\n");

    //Until we are out of information to read go through the file
    while (result > 0) {
        //Write the info
        write(sfd, fileBuf, MAX);

        //Read again
        result = read(fp, fileBuf, MAX);
        printf("4\n");
    }

    //Close file & exit
    close(fp);
    return 0;
}

int localGet(char *fileName){
    // get the # of bytes that the file is
    char buf[MAX];

    //Read from the client how large this file is, parse using atoi
    read(sfd, buf, MAX);
    int fileSize = atoi(buf);

    // Create a file to write this data into
    int fd = open(fileName, O_WRONLY | O_CREAT, 0644);

    //If we could not create a file for some reason and the file descriptor is broken exit out of function
    if (fd <= 0)return -1;

    printf("File size: %d\n",fileSize);

    //loop through until we are no longer reading data
    while (fileSize > 0) {

        //Read
        read(sfd, buf, MAX);

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

int localLs(char * pathname){
    //Create a buffer to read the current working directory into
    char buf[MAX] = "";
    getcwd(buf, MAX);

    //When the path isnt defined use the working directory
    if (strlen(pathname) == 0){
        displayDirectory(buf);
        return 0;
    }

    //Show the asked for directory
    displayDirectory(pathname);
}

int localCat(char *filename){
    //Create a buffer to use while reading the contents of the file
    char buf[512] = "";

    //Open up the file we are going to be catting into
    FILE *fd = fopen(filename, "r");

    //Exit if we cannot get into the file
    if(fd == NULL){
        printf("Issue while using fopen, exiting\n");
        return 0;
    }

    //Loop through the file 512 bytes at a time and print
    while (fgets(buf, 512, fd) != NULL)puts(buf);

    return 0;
}

void displayDirectory(char *pathname){
    //Open the directory
    DIR *directory = opendir(pathname);

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


    //Write to local client
    printf("%s\n",output);

    //Close the directory
    closedir(directory);
}

int isEOT(char *s){
    return strcmp(s,EOT_CLIENT);
}

int localCd(char *pathname){
    return chdir(pathname);
}

int localPWD(){
    char buf[MAX] = "";
    getcwd(buf,MAX);
    printf("%s\n",buf);
    return 1;
}

int localRm(char *pathname){
    return unlink(pathname);
}
int localMkDir(char *pathname){
    return mkdir(pathname,0755);
}
int localRmDir(char *pathname){
    return rmdir(pathname);
}
int menu()
{
    printf("*********************** MENU **********************\n");
    printf("* get  put  ls   cd   pwd   mkdir   rmdir   rm  *\n"); // executed by server
    printf("* lcat     lls  lcd  lpwd  lmkdir  lrmdir  lrm  *\n"); // executed locally
    printf("****************************************************\n");
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

int main(int argc, char *argv[], char *env[])
{
    int n; char how[64];
    int i;

    printf("1. create a socket\n");
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        printf("socket creation failed\n");
        exit(0);
    }

    printf("2. fill in server IP and port number\n");
    bzero(&saddr, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    saddr.sin_port = htons(PORT);

    printf("3. connect to server\n");
    if (connect(sfd, (struct sockaddr *)&saddr, sizeof(saddr)) != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    //Display commands
    menu();

    printf("********  processing loop  *********\n");
    while (1){
        printf("input a line : ");
        bzero(line, MAX);                // zero out line[ ]
        fgets(line, MAX, stdin);         // get a line (end with \n) from stdin

        line[strlen(line)-1] = 0;        // kill \n at end
        if (line[0]==0)                  // exit if NULL line
            exit(0);


        char *args[64];
        char raw[64] = "";
        strcat(raw,line);
        int argcount = tokenizeLine(line,args);
        char *command = line;
        char *dir = "";

        if(strcmp(command,"lcat") == 0){
            if(args[1] != NULL){
                localCat(args[1]);
                continue;
            }
        }

        if(strcmp(command,"lls") == 0){
            if(argcount > 1)localLs(args[1]);
            else localLs(dir);
            continue;
        }

        if(strcmp(command,"lcd") == 0){
            if(args[1] != NULL){
                localCd(args[1]);
                continue;
            }
        }

        if(strcmp(command,"lpwd") == 0){
            localPWD();
            continue;
        }

        if(strcmp(command,"lmkdir") == 0){
            if(args[1] != NULL){
                localMkDir(args[1]);
                continue;
            }
        }

        if(strcmp(command,"lrmdir") == 0){
            if(args[1] != NULL){
                localRmDir(args[1]);
                continue;
            }
        }

        if(strcmp(command,"lrm") == 0){
            if(args[1] != NULL){
                localRm(args[1]);
                continue;
            }
        }


        // Send ENTIRE line to server
        printf("sending %s to server\n",raw);
        n = write(sfd, raw, MAX);

        //Handle Put & get
        if(strcmp(line,"put") == 0){
            if(args[1] != NULL){
                localPut(args[1]);
            }
        }
        if(strcmp(line,"get") == 0){
            if(args[1] != NULL){
                localGet(args[1]);
            }
        }

        //     printf("client: wrote n=%d bytes; line=(%s)\n", n, line);


        // Read a line from sock and show it
        while (1){
            char buffer[MAX];
            read(sfd,buffer,MAX);
            if(strcmp(buffer,EOT_CLIENT) == 0){
                break;
            }
            printf("%s",buffer);
        }

 //       printf("client: read  n=%d bytes; echo=(%s)\n",n, ans);
    }
}
