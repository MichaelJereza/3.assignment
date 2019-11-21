#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

struct status{
    int mode; //0: exit | 1: signal
    int result;     // exit status or signal
};
struct bps{
    int amt;
    int* pid;
};
char* SHELL_LOCATION;
struct status lastTerm;
struct bps BACKGROUND_Ps;
bool BLOCKED = false; // Variable blocking bg process
int msg = 0; // Variable for blocking message 1 for block 2 for unblock

void catchSigTstp(int signo){
    BLOCKED = !BLOCKED;
    if(BLOCKED){
        msg = 1;
    }
    else{
        msg = 2;
    }
}
void catchSigInt(int signo){
    int test = getpid();
    kill(test, 15);
    lastTerm.mode = 1;
    lastTerm.result = 15;
    exit(15);
}

// Fork and execute command in arg[]
// Set bg false for fg process
int executeCommand(char** arg, bool bg, int in, int out){
    // if input and output arent -1
    // set them
    int oldIn = dup(0);
    int oldOut = dup(1);
    if(in!=-1){
        dup2(in, 0);
    }
    if(out!=-1){
        dup2(out, 1);
    }

    // Init sigaction
    struct sigaction SIGINT_action = {0};
    // Block all signals
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;

    // Fork
    int commandPid = fork();
    int cExit = -5;
    int result = -1;
    switch(commandPid){
        case -1:
            printf("ERROR");
            exit(1);
            break;
        case 0:
        
            // HERE
            SIGINT_action.sa_handler = catchSigInt;
            sigaction(SIGINT, &SIGINT_action, NULL);

            result = execvp(arg[0], arg);
            if(result == -1){
                perror("Error");
                fflush(stderr);                
                exit(1);
                break;
            }
            
            dup2(oldOut, 1);
            printf("Background PID: %d is done; exit value: %d\n", commandPid, 0);            
            exit(0);
            break;

        default: 
            SIGINT_action.sa_handler = SIG_IGN;
            sigaction(SIGINT, &SIGINT_action, NULL);

            //FIXME signals
            if(bg){
                
                BACKGROUND_Ps.pid = realloc(BACKGROUND_Ps.pid, (BACKGROUND_Ps.amt+1)*sizeof(int));
                BACKGROUND_Ps.pid[BACKGROUND_Ps.amt] = commandPid;
                BACKGROUND_Ps.amt++;
                // Return stdout
                dup2(oldOut, 1);
                printf("Background PID: %d\n", commandPid);

                //waitpid(commandPid, &cExit, WNOHANG);
                //printf("Hi\n");
            }
            else{
                waitpid(commandPid, &cExit, 0);
                lastTerm.mode = 0;
                lastTerm.result = WEXITSTATUS(cExit);
            }
            
            // Return stdout and stdin
            dup2(oldIn, 0);
            dup2(oldOut, 1);
            break;
    }
    return commandPid;
}

// Parses command line into argument array
// Calls fork creation
bool parseCommand(char** commandLine){
    //512 args
    // 2048 input
    char* cmd = strtok(*commandLine, " ");
    char** args = malloc(sizeof(char*[512]));
    args[0] = malloc(2048*sizeof(char));
    strcpy(args[0], cmd);
    
    int size = 1;
    int returnStdout = dup(1);
    int returnStin = dup(0);
    int fOut = -1;
    int fIn = -1;
    char* pid;
    // Parse commandLine into args array of parameters
    char* param = strtok(NULL, " ");
    while(param!=NULL){    

        // Handle redirection
        if(param[0] == '>'|| param[0] == '<'){
            if(strcmp(param, ">") == 0){
                // Get file name
                // Open file stream
                // Redirect stdout to fstream
                param = strtok(NULL, " ");
                
                if(strcmp(param, "$$") == 0){
                    char id[10];
                    sprintf(id, "%d", getpid());
                    param = id;
                }
                
                fOut = open(param, O_CREAT | O_WRONLY | O_TRUNC, 0664 );
                //dup2(fOut, 1);
                if(fOut==-1){
                    // If can't be opened reprompt
                    printf("%s can't be accessed!\n", param);
                    lastTerm.mode = 0;
                    lastTerm.result = 1;
                    return true;
                }
            }
            else if(strcmp(param, "<") == 0){
                // Get file name
                // Open file stream
                // Redirect stdin to fstream
                param = strtok(NULL, " ");
                
                if(strcmp(param, "$$") == 0){
                    char id[10];
                    sprintf(id, "%d", getpid());
                    param = id;
                }

                fIn = open(param, O_RDONLY, 0664 );
                if(fIn==-1){
                    // If can't be opened reprompt
                    printf("%s can't be accessed!\n", param);
                    lastTerm.mode = 0;
                    lastTerm.result = 1;
                    return true;
                }


            }

        }
        else{
            if(strcmp(param, "$$") == 0){
                char id[10];
                sprintf(id, "%d", getpid());
                param = id;
            }
            args[size] = malloc(2048*sizeof(char));//realloc(args, sizeof(args)+temp_size);
            strcpy(args[size], param);
            size++;
        }
        param = strtok(NULL, " ");
    }

    bool backgrounding = false;

    // If last entry is &, remove from args
    if(strcmp(args[size-1], "&") == 0){
        free(args[size-1]);
        args[size-1]=NULL;
        size--;
        if(!BLOCKED){
            backgrounding = true;
            if(fOut==-1){
                fOut = open("/dev/null", O_WRONLY, 0664 );
            }
            if(fIn==-1){
                fIn = open("/dev/null", O_RDONLY, 0664 );
            }
        }

    }

    args[size] = NULL;
    executeCommand(args, backgrounding, fIn, fOut);

  
    // Clean up
    // If redirecting : then close
    if(fOut!=-1){
        close(fOut);
    }
    if(fIn!=-1){
        close(fIn);
    }
    // Free args
    //                  !----- problem here -----!
    int i;
    for(i = 0; i < size+1; i++){
        args[i] = NULL;
        free(args[i]);
    }
    free(args);

    return true;
}

bool getStatus(){
    if(lastTerm.mode==1){
        printf("terminated by signal: %d\n", lastTerm.result);
    }
    else{
        printf("exit value: %d\n", lastTerm.result);        
    }
    fflush(stdout);
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

    // reprompt for no input
    if(strlen(input)<=0){
        return true;
    }
    else if(*input == '#'){
        return true;
    }
    else if(strcmp(input, "exit")==0){
        int i;
        for(i = 0; i < BACKGROUND_Ps.amt; i++){
            kill(BACKGROUND_Ps.pid[i], 15);
            printf("Killed %d", BACKGROUND_Ps.pid[i]);
        }
        if(BACKGROUND_Ps.amt > 0){
            free(BACKGROUND_Ps.pid);
        }
        return false;
    }
    else if(strncmp(input, "cd", 2)==0){
        return changeDirectory(&input);
    }
    else if(strncmp(input, "status", 6)==0){
        return getStatus();
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
        // Handle Background Processes
        bool removed = false;
        do{
        int i;
        for(i = 0; i < BACKGROUND_Ps.amt; i++){
            int cExit = -5;
            waitpid(BACKGROUND_Ps.pid[i], &cExit, WNOHANG);
            if(cExit!=-5){
                if(cExit==15){
                    printf("Background PID: %d is done; terminated by signal: %d\n", BACKGROUND_Ps.pid[i], cExit);                    
                }
                else{
                    printf("Background PID: %d is done; exit value: %d\n", BACKGROUND_Ps.pid[i], cExit);
                }
                
                // Pointer to pids
                int* temp = BACKGROUND_Ps.pid;
                // If there is more than finished job in background: reduce mem for finished job
                if(BACKGROUND_Ps.amt-1!=0){
                    BACKGROUND_Ps.pid = malloc(sizeof(int)*(BACKGROUND_Ps.amt-1));
                }
                // Copy new array
                int p = 0;    
                int j;
                for(j = 0; j < BACKGROUND_Ps.amt-1; j++){
                    if(p==i){
                        p++;
                    }
                    BACKGROUND_Ps.pid[j] = temp[p];
                    p++;
                }
                
                BACKGROUND_Ps.amt--;
                //free(temp);
            }
        }}while(removed == true);
        // Free buffer
        if(buffer != NULL){
          free(buffer);
          buffer = NULL;
        }
        bufsize = 0;
        buflen = 0;

        if(msg!=0){
            if(msg==1){
                printf("Entering foreground-only mode (& is now ignored)\n");
                msg=0;
            }
            if(msg==2){
                printf("Exiting foreground-only mode\n");
                msg=0;
            }
        }
        struct sigaction prompt_signals = {0};
        sigfillset(&prompt_signals.sa_mask);
        prompt_signals.sa_flags = 0;

        // Prompt
        prompt_signals.sa_handler = SIG_IGN;
        //sigaction(SIGTSTP, &prompt_signals, NULL);
        sigaction(SIGINT, &prompt_signals, NULL);
        prompt_signals.sa_handler = catchSigTstp;
        sigaction(SIGTSTP, &prompt_signals, NULL);


        printf(":");
        fflush(stdout);
        fflush(stdin);

        do{
            buflen = getline(&buffer, &bufsize, stdin);
            fflush(stdin);
        }while(buflen==-1);

        if(buflen < 0){
           // buffer = NULL;
        }
        // Remove \n
        if(buflen > 0){
            if(buffer[buflen-1] == '\n'){
                // Deallocate unnecessary memory for newline
                // Replace newline with terminator
               // buffer = realloc(buffer, sizeof(char[buflen-1]));
                buffer[buflen-1] = '\0';
            }
        }
    }while(doInput(buffer) == true);
    if(buffer!=NULL){
        free(buffer);
    }
}

void main(){

    sigset_t handledSigs;
    sigemptyset(&handledSigs);
    sigaddset(&handledSigs, SIGINT); // kill fg only print signal
    sigaddset(&handledSigs, SIGTSTP);

    struct sigaction handled;
    handled.sa_handler;
    handled.sa_flags = SA_SIGINFO;

    lastTerm.mode = 0;
    lastTerm.result = 0;
    BACKGROUND_Ps.amt = 0;
    shellPrompt();
}