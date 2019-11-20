#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

char* SHELL_LOCATION;

// Fork and execute command in arg[]
int executeCommand(char** arg){
    int commandPid = fork();
    int cExit = -5;
    int result;
    switch(commandPid){
        case -1:
            printf("ERROR");
            exit(1);
            break;
        case 0:
            result = execvp(arg[0], arg);
            if(result == -1){
                perror("Error");
                exit(1);
                break;
            }
            fflush(stdout);
            exit(0);
            break;
        default: 
            //FIXME signals
            waitpid(commandPid, &cExit, 0);
            break;
    }
    return commandPid;
}

// Opens file fOut
// returns file descriptor
int outputIO(char* fOut){
    open(fOut, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
}

// Parses command line into argument array
// Calls fork creation
bool parseCommand(char** commandLine){
    char* cmd = strtok(*commandLine, " ");
    char** args = malloc(sizeof(cmd));
    args[0] = cmd;
    int size = 0;
    int returnStdout = dup(1);
    int fOut;
    // Parse commandLine into args array of parameters
    char* param = strtok(NULL, " ");
    while(param!=NULL){    

        // Handle output
        if(*param == '>'){
            
            // Get file name
            // Open file stream
            // Redirect stdout to fstream
            param = strtok(NULL, " ");
            fOut = open(param, O_CREAT | O_WRONLY | O_TRUNC, 0664 );
            dup2(fOut, 1);
        }
        else{
            size++;
            args = realloc(args, sizeof(args)+sizeof(param));
            args[size] = param;
        }
        param = strtok(NULL, " ");
    }
    

    executeCommand(args);

    // Clean up
    // Return stdout to terminal
    dup2(returnStdout, 1);
    close(fOut);
    free(args);
  //  return false;

    return true;
}

// Changes the directory
// Returns true if directory changes
// Returns false if directory dne
bool changeDirectory(char** mvDir){
    int commandLen = strlen(*mvDir);
    char* param = strtok(*mvDir, " ");

    if((strcmp(param, "cd")==0) && (commandLen <= 3)){
        SHELL_LOCATION = getenv("HOME");
        if(chdir(SHELL_LOCATION) == 0){
            return true;
        }
    }
    // Do nothing if bad command and reprompt
    else if (strcmp(param, "cd")!=0){
        return true;
    }
    else{
    
        param = strtok(NULL, " ");
        SHELL_LOCATION = param;
       
        if(chdir(param) == 0){
            return true;
        }

    }
    printf("Error changing to directory: %s\n", param);
    fflush(stdout);
    return false;
}

// Calls command function
// Returns true if successful
// Returns false if input is "exit" or error
bool doInput(char* input){

    if(*input == '#'){
        return true;
    }
    else if(strcmp(input, "exit")==0){
        // FIXME Kill children processes
        return false;
    }
    else if(strncmp(input, "cd", 2)==0){
        return changeDirectory(&input);
    }
    else if(strcmp(input, "status")==0){
        return true;
    }
    else{
        return parseCommand(&input);
    }
}

void shellPrompt(){
    char* buffer = NULL;
    size_t bufsize;
    ssize_t buflen;

    SHELL_LOCATION = getenv("HOME");

    do{
        if(buffer != NULL){
          free(buffer);
        }
        bufsize = 0;
        buflen = 0;
        printf(":");
        fflush(stdout);
        fflush(stdin);
        buflen = getline(&buffer, &bufsize, stdin);
        if(buflen < 0){
            printf("Error!\nBUFFER:%s \nBUFSIZE:%d \nBUFLEN:%d", buffer, bufsize, buflen);
            break;
        }
        // Remove \n
        if(buflen > 0){
            if(buffer[buflen-1] = '\n'){
                // Deallocate unnecessary memory for newline
                // Replace newline with terminator
                buffer = realloc(buffer, sizeof(buffer)-sizeof(char));
                buffer[buflen-1] = '\0';
            }
        }
    }while(doInput(buffer) == true);

    free(buffer);
}

void main(){
    shellPrompt();
}