#include <stdio.h>             // for I/O
#include <stdlib.h>            // for I/O
#include <libgen.h>            // for dirname()/basename()
#include <stdbool.h>
#include <string.h>

typedef struct node{
    char  name[64];       // node's name string
    char  type;           // 'D' for DIR; 'F' for file
    struct node *child, *sibling, *parent;
}NODE;


NODE *root, *cwd, *start;
char line[128];
char command[16], pathname[64];

//               0       1      2       3     4     5       6       7       8       9      10
char *cmd[] = {"mkdir", "ls", "quit", "cd", "pwd", "rmdir","creat","rm","save","reload","menu", 0};

int findCmd(char *command)
{
    int i = 0;
    while(cmd[i]){
        if (strcmp(command, cmd[i])==0)
            return i;
        i++;
    }
    return -1;
}

bool hasChild(NODE *parent,char *name){
    NODE *viewing = parent->child;

    while (viewing != NULL){
        if(strcmp(viewing->name,name) == 0)return true;
        viewing = viewing->sibling;
    }

    return false;
}


NODE *search_child(NODE *parent, char *name)
{
    NODE *p;
  //  printf("search for %s in parent DIR\n", name);
    p = parent->child;
    if (p==0)
        return 0;
    while(p){
        if (strcmp(p->name, name)==0)
            return p;
        p = p->sibling;
    }
    return 0;
}

/**
 * This is for when we are searching into multiple directories
 * @return
 */

NODE *deepSearch(NODE *parent,char *path){

    //Define starting node
    NODE* current = parent;

    //Adjust for starting with /
    if('/' == path[0]){
       current = root;
    }

    //Split on / to get the items
    const char delim[2] = "/";
    char *token;

    //Get the first token
    token = strtok(path,delim);

    //Loop & find potential directories / files
    while (token != NULL){
        current = search_child(current,token);
        token = strtok(NULL,delim);
    }

    return current;
}

int insert_child(NODE *parent, NODE *q)
{
    NODE *p;
 //   printf("insert NODE %s into END of parent child list\n", q->name);
    p = parent->child;
    if (p==0)
        parent->child = q;
    else{
        while(p->sibling)
            p = p->sibling;
        p->sibling = q;
    }
    q->parent = parent;
    q->child = 0;
    q->sibling = 0;
}

/***************************************************************
 This mkdir(char *name) makes a DIR in the current directory
 You MUST improve it to mkdir(char *pathname) for ANY pathname
****************************************************************/

int insertSafe(char *name,char type){
    NODE *p, *q;
  //  printf("mkdir: name=%s\n", name);

    //Error out when improper name is supplied for file creation
    if(type == 'F'){
        int length = strlen(name);
        if(name[length-1] == '/')return 0;
    }

    //Dont allow invalid names for directories
    if (!strcmp(name, "/") || !strcmp(name, ".") || !strcmp(name, "..")){
        printf("can't mkdir with %s\n", name);
        return -1;
    }

    //Define starting node
    NODE* current = cwd;

    //Adjust for starting with /
    if('/' == name[0]){
        current = root;
    }

    //Split on / to get the items
    const char delim[2] = "/";
    char *token;

    //Get the first token
    token = strtok(name,delim);

    //Loop & find potential directories / files
    while (token != NULL){
        char *working = token;
        token = strtok(NULL,delim);

        //Check to see if it is the last item we are looking at
        if(token == NULL){
            if(current != NULL){
                //Check if this is already a directory
                if(hasChild(current,working)){
                    printf("Already a file/directory named %s\n",working);
                    return -1;
                }

      //          printf("Attempting insert %s %s\n",working,current->name);
                q = (NODE *)malloc(sizeof(NODE));
                q->type = type;
                strcpy(q->name, working);
                insert_child(current, q);
                return 0;
            }
        }

        current = search_child(current,working);
    }
    return 0;
}

int mkdir(char *name)
{
    return insertSafe(name,'D');
}

// This ls() list CWD. You MUST improve it to ls(char *pathname)
int ls(char *pathName)
{

    NODE *viewing = deepSearch(cwd,pathName);

    //Handle when the directory doesnt exist
    if(viewing == NULL){
        return 0;
    }

    NODE *p = viewing->child;
    printf("cwd contents = ");
    while(p){
        printf("[%c %s] ", p->type, p->name);
        p = p->sibling;
    }
    printf("\n");
}


int initialize()
{
    root = (NODE *)malloc(sizeof(NODE));
    strcpy(root->name, "/");
    root->parent = root;
    root->sibling = 0;
    root->child = 0;
    root->type = 'D';
    cwd = root;
 //   printf("Root initialized OK\n");
}

int cd(char *pathName){
    //Handle cases where we aren't just looking down. / to start @ root, .. to go up a directory to parent
    if(strcmp("..",pathName) == 0){
        if(cwd->parent != NULL){
            cwd = cwd->parent;
            return 0;
        }
    }

    if('/' == pathName[0]){
        NODE* found = deepSearch(root,pathName);
        cwd = found;
        return 0;
    }

    NODE* found = deepSearch(cwd,pathName);

    //Handle when the search was unsucessful
    if(found == NULL)return 0;
    if(found->type == 'F')return 0;
    else cwd = found;
}

void pwdHelp(NODE *current){
    //Handle root
    if(current == root){
        printf("/");
        return;
    }

    //Call node above
    pwdHelp(current->parent);

    //Handle formatting differences
    if(current->parent == root)printf("%s",current->name);
    else printf("/%s",current->name);
}

int pwd(){
    //This is the pointer we use to traverse up
    pwdHelp(cwd);
    printf("\n");
    return 0;
}

int rmdir(char *pathName){
    //Find the directory to remove
    NODE *found = deepSearch(cwd,pathName);

    //Make sure this is a directory
    if(found == NULL)return 0;
    if(found->type == 'F'){
        printf("rmdir is for directories not files\n");
        return 0;
    }
    if(found->child != NULL){
        printf("directory is not empty\n");
        return 0;
    }

    //Go up to parent
    NODE *current = found->parent->child;

    //Initial check
    if(current == found){
        //Unpoint
        found->parent->child = NULL;
        free(found);
        return 0;
    }

    //Find node to unpoint
    while (current->sibling != found){
        current = current->sibling;
    }

    //Unpoint
    current->sibling = NULL;
    free(found);
    return 0;
}

int creat(char *pathName){
    return insertSafe(pathName,'F');
}

int rm(char *pathName){
    //Find the file to remove
    NODE *found = deepSearch(cwd,pathName);

    //Error out when improper name is supplied for file creation
    int length = strlen(pathName);
    if(pathName[length-1] == '/')return 0;


    //Make sure this is a file
    if(found == NULL)return 0;
    if(found->type == 'D'){
        printf("rm is for files not directories\n");
        return 0;
    }

    //Go up to parent
    NODE *current = found->parent->child;

    //Initial check
    if(current == found){
        //Unpoint
        found->parent->child = found->sibling;
        free(found);
        return 0;
    }

    //Find node to unpoint
    while (1){
        if(current->sibling == found){
            current->sibling = current->sibling->sibling;
            break;
        }
        current = current->sibling;
    }

    //Unpoint
    free(found);
    return 0;
}

void savePrint(NODE *print, FILE *fp){
    //Handle root
    if(print == root){
        fprintf(fp,"/");
        return;
    }

    //Call node above
    savePrint(print->parent,fp);

    //Handle formatting differences
    if(print->parent == root)fprintf(fp,"%s",print->name);
    else fprintf(fp,"/%s",print->name);
}

void saveHelp(NODE *save,FILE *fp){
    if(save == NULL)return;
    fprintf(fp,"%c \t\t",save->type);
    savePrint(save,fp);
    fputs("\n",fp);
    saveHelp(save->child,fp);
    saveHelp(save->sibling,fp);
}

int save(char* fileName){
    //Create the file
    FILE *fp;
    fp = fopen(fileName,"w");

    //Write the whole damn file system in there
    fputs("Type \t PathName\n",fp);
    fputs("---- \t ----\n",fp);

    //You are my high!
    saveHelp(root,fp);

    //Close the stream
    fclose(fp);
}


int reload(char *fileName){
    //Find the file
    FILE *fp;
    fp = fopen(fileName,"r");

    if(fp == NULL){
        printf("No such file\n");
        return -1;
    }

    //Create buffer to read with
    char buff[256];

    //Get rid of the formatting part of the file
    fscanf(fp,"%s",buff);
    fscanf(fp,"%s",buff);
    fscanf(fp,"%s",buff);
    fscanf(fp,"%s",buff);
    fscanf(fp,"%s",buff);
    fscanf(fp,"%s",buff);

    //Re-initialize
    initialize();

    //Read in lines of the file
    while (!feof(fp)){

        //Get the type of item
        fscanf(fp,"%s",buff);
        if(feof(fp)){
            return 0;
        }
        char type = buff[0];

        //Get the name
        fscanf(fp,"%s",buff);
        if(feof(fp)){
            return 0;
        }

        //Insert into file system
        insertSafe(buff,type);
    }


    return 0;
}

int quit()
{
    save("filesys.txt");
    printf("Program exit\n");
    exit(0);
}

int menu(){
    printf("mkdir pathname :make a new directory for a given pathname\n");
    printf("rmdir pathname :remove the directory, if it is empty.\n");
    printf("cd [pathname] :change CWD to pathname, or to / if no pathname.\n");
    printf("ls [pathname] :list the directory contents of pathname or CWD\n");
    printf("pwd \t :print the (absolute) pathname of CWD\n");
    printf("creat pathname :create a FILE node.\n");
    printf("rm pathname\t :remove the FILE node.");
    printf("save filename :save the current file system tree as a file\n");
    printf("reload filename :construct a file system tree from a file\n");
    printf("menu\t : show a menu of valid commands\n");
    printf("quit\t : save the file system tree, then terminate the program.\n");
    return 0;
}

int main()
{
    int index;

    initialize();

    while(1){
        printf("$ ");
        fgets(line, 128, stdin);
        line[strlen(line)-1] = 0;

        command[0] = pathname[0] = 0;
        sscanf(line, "%s %s", command, pathname);
 //       printf("command=%s pathname=%s\n", command, pathname);

        if (command[0]==0)
            continue;

        index = findCmd(command);
        switch (index){
            case 0: mkdir(pathname); break;
            case 1: ls(pathname);            break;
            case 2: quit();          break;
            case 3: cd(pathname);  break;
            case 4: pwd(); break;
            case 5: rmdir(pathname); break;
            case 6: creat(pathname);break;
            case 7: rm(pathname); break;
            case 8: save(pathname); break;
            case 9: reload(pathname); break;
            case 10: menu(); break;
        }
    }
}