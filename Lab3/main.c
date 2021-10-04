#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

//https://github.com/Eastonco/CS360/tree/master/lab3
//lots of code I took from here and appended to the starter file. I was not completely following how pipes work so I worked through most of this code and used it to help me better understand how this works.
//This is my personal break down of how this program works
/**
 *
 * When the program starts we look at the enviornment variables which allows us to store the starting directories to execute commands from.
 * When we use execve () we need to have a path + / + command, (arguments with arg0 equal to command)
 *
 *
 *  Initially I thought the following was true.
 * We store that information in the dpath & *dir[64] array. This allows us to access it easier when executing. We call getcwd(cmd,128) for commands starting with ./
 *
 * char dpath[128];    // hold dir strings in PATH
 * char *dir[64];      // dir string pointers
 * int  ndir;          // number of dirs
 *
 * These are the variables we use to store the directory which we use at execution. I do not fully understand why when we execute @ the actual level we need to loop throug the directories and we send a call at each level.
 * I will definitely ask silas about this. I understand now we are going through the directories that we have because it will only execute the runnable once we find what directory it is from So we loop through all of our directories
 * attempting to execute the command and when we find the directory that actually has this command the execve actually works.
 *
 *  Thats how I view the most basic of execution here. Piping is where things get more complicated. I spent sometime looking over the mandocs for the following methods which I used
 *  https://man7.org/linux/man-pages/man2/pipe.2.html
 *  https://man7.org/linux/man-pages/man2/close.2.html
 *  https://man7.org/linux/man-pages/man2/dup.2.html
 *
 *  Reading the pipe page helped me a good bit because it made the lpd make more sense. I was somewhat confused as to what we actually do with these file descriptors. Why do we need to manually close them? When we actually call
 *  out execCommand method there is no reference to these file descriptors so why do we need them? I understand that it is creating a data channel. Does this just mean that when we do execute these commands they are in a diff
 *  channel?
 *
 *
 *
 */

char gpath[128];    // hold token strings
char *arg[64];      // token string pointers
int  n;             // number of token strings

char dpath[128];    // hold dir strings in PATH
char *dir[64];      // dir string pointers
int  ndir;          // number of dirs

char *head, *tail; //we needs these for when we pipe
int lpd[2]; //we need this for when we call pipe. these are the file descriptors returned

void tokenize(char *pathname) // YOU have done this in LAB2
{                             // YOU better know how to apply it from now on
    char *s;
    strcpy(gpath, pathname);   // copy into global gpath[]
    s = strtok(gpath, " ");
    n = 0;
    while(s){
        arg[n++] = s;           // token string pointers
        s = strtok(0, " ");
    }
    arg[n] = 0;                // arg[n] = NULL pointer
}

int scan(char *cmdLine){
    //Go through the line sent from the user and check for pipes
    for(int i = strlen(cmdLine)-1; i > 0 ; i--){

        //Prepare for piping when we find a valid pipe symbol
        if(cmdLine[i] == '|' ){

            //End string
            cmdLine[i] = 0;

            //Set up location for tail & head of pipe
            tail = cmdLine + i + 1;
            head = cmdLine;
            return 1;
        }
    }

    //No pipe was found return as normal
    head = cmdLine;
    tail = NULL;
    return 0;
}

void ioRedir()
{
    for (int i = 0; i < n; i++)
    {
        //Handle writing directly to file
        if (strcmp(arg[i], ">") == 0)
        {
            arg[i] = 0;
            close(1);
            int fd = open(arg[i + 1], O_WRONLY | O_CREAT);
            dup2(fd, 1); //use dup2 to get the new-file description
        }

        //Handle appending to the file we got as input
        else if (strcmp(arg[i], ">>") == 0)
        {
            arg[i] = 0;
            close(1);
            int fd = open(arg[i + 1], O_WRONLY | O_CREAT | O_APPEND);
            dup2(fd, 1);
        }

        //Handle reading in the file from input
        else if (strcmp(arg[i], "<") == 0)
        {
            arg[i] = 0; //wipe the argument we are looking at so its split proprly
            close(0);
            int fd = open(arg[i + 1], O_RDONLY);
            dup2(fd, 0);
        }
    }
}


int execCommand(char *cmdLine)
{
    memset(arg, 0, sizeof(arg)); //set the end of the string to 0

    tokenize(cmdLine); //split up the exact command we got. we have to do this again because when we pipe we need new arguments for the head/tail
    ioRedir(); //io - redirect

    char cmd[128]; //this is the command we are looking at specifically

    //Handle when we start our command with ./ and execute our of the current working directory
    if(arg[0][0] == '.' && arg[0][1] == '/'){
        char temp[128];

        //Find the working directory for then add ontop of that the command we are using.
        getcwd(temp, 128);
        strcat(temp, "/");
        strcat(temp, arg[0]);

        //Execute at the system level the command we built based on the working directory, command and send its arguments in with it
        execve(temp, arg, __environ);
    }

    //Loop through our directories and for each one execute the command.
    for (int i = 0; i < ndir; i++)
    {
        // printf("Executing from dir# %d %s\n",i,dir[i]);
        //Get the start of the command we specifically send to the system. Why do we have to repeat this? ask silas
        strcpy(cmd, dir[i]);
        strcat(cmd, "/");
        strcat(cmd, arg[0]);

        //Execute at the system level the command we built based on the working directory, command and send its arguments in with it
        execve(cmd, arg, __environ);
    }

    //Inform that the command could not be found
    printf("cmd %s not found, child sh exit\n", arg[0]);
    exit(1); //exit the forked process
}

int execPipe(char *cmdLine, int *pd)
{
    //What are we looking at here why are we using the array of file descriptors in one instance and 0 in another?
    //https://man7.org/linux/man-pages/man2/dup.2.html
    //https://man7.org/linux/man-pages/man2/close.2.html
    if (pd)
    {
        //Close the first fd so we cannot reuse
        close(pd[0]);

        //Reset the second fd to a new file descriptor
        dup2(pd[1], 1);

        //Close down the 2nd fd
        close(pd[1]);
    }

    //First we are going to check if we have any pipes in our command sent in
    int hasPipe = scan(cmdLine);

    //Handle when we have a pipe
    if(hasPipe){
        //First we are going to attempt to pipe and it is going to return to us two file descriptors into the lpd array
        pipe(lpd);

        //We get a process id returned to us from calling fork
        int pid = fork();

        //When PID is positive we close down the file pipe and execute the backend just the tail
        if(pid){
            close(lpd[1]); //close down the writing end
            dup2(lpd[0], 0); //find the new file descriptor to read from
            close(lpd[0]); //close the reading end
            execCommand(tail); //execute tail which will create new lpd if it needs to or it will execute normally
        }

        //When the PID is not positive we execute the head of the piped command
        else{
            execPipe(head, lpd); //this is our recursive call to pipe
        }
    }

    //When there is no pipe we simply just execute our command
    else{
        execCommand(cmdLine);
    }
}


int main(int argc, char *argv[], char *env[])
{
    char line[128];
    int i = 0;
    int pid, status;

    //Search through enviornment variables to figure out what path we are using
    while (env[i])
    {
        //PRINT OUT ENV VARIABLES FOR DEBUG
        printf("env[%d] = %s\n", i, env[i]);

        // Identify when one of the enviornment variables is PATH
        if (strncmp(env[i], "PATH=", 5) == 0)
        {
            //Break down path into dpath
            strcpy(dpath, &env[i][5]);

            //Show us the directory path
            printf("%s", dpath);
            ndir = 0;

            //Split our path
            char *p = strtok(dpath, ":");

            //Loop through the tokenized directory path and put their specific names into an array
            while (p != NULL)
            {
                dir[ndir++] = p;
                p = strtok(NULL, ":");
            }

            //Go through & print our exact directory strings
            for (int i = 0; i < ndir; i++)
            {
                printf("%d. %s\n", i, dir[i]);
            }

            printf("DIR 0 %s\n",dir[1]);

            break;
        }

        //Increment I so that we go to the next enviornment variable looking for the PATH= incase we have not found it yet
        i++;
    }

    while (1)
    {
        //Print basic info and ask for a command
        printf("sh %d running\n", getpid());
        printf("$ ");

        fgets(line, 128, stdin);
        line[strlen(line) - 1] = 0; //remove the \n at the end of the fgets

        //Validate that our input was obtained
        if (line[0] == 0)
            continue;

        printf("line = %s\n", line); // print line to see what you got

        //Copy the line that we got as input into the gpath and use it when we execPipe later which is starting the process of breaking down our command in the scenario that there is pipes
        strcpy(gpath, line);
        tokenize(&line);

        // show token strings
        for (i=0; i<n; i++){
            printf("arg[%d] = %s\n", i, arg[i]);
        }

        //Handle cd
        if (strcmp(arg[0], "cd")==0){
            chdir(arg[1]);
            continue;
        }

        //Handle exit
        if (strcmp(arg[0], "exit")==0)
            exit(1);



        //Fork process
        pid = fork();

        if (pid)
        {
            printf("sh %d forked a child sh %d\n", getpid(), pid);
            printf("sh %d wait for child sh %d to terminate\n", getpid(), pid);
            pid = wait(&status);
            printf("ZOMBIE child=%d exitStatus=%x\n", pid, status);
            printf("main sh %d repeat loop\n", getpid());
        }
        else
        {
            execPipe(gpath, 0);
        }
    }
}