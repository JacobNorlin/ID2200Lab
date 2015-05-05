#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>

void parseCmd(char* cmd, char** params);
int executeCmd(char** params);
int changeDirectory(char *path);
int backgr;	

#define MAX_NUMBER_OF_PARAMS 5
#define MAX_COMMAND_LENGTH 70

int main(int argc, char *argv[])
{
	char cmd[MAX_COMMAND_LENGTH + 1];
    char* params[MAX_NUMBER_OF_PARAMS + 1];

	while (1){
		fgets(cmd, sizeof(cmd),stdin);
	
		if(cmd[strlen(cmd)-1] == '\n') {
		        cmd[strlen(cmd)-1] = '\0';
		    }
		parseCmd(cmd,params);
		
		if (strcmp(params[0], "exit\0") == 0) 
		{
			break;
		}
	
		executeCmd(params);
	}
	
	return 0;
}

void parseCmd(char* cmd, char** params)
{   
	int i;
    for(i = 0; i < MAX_NUMBER_OF_PARAMS; i++) {
        params[i] = strsep(&cmd, " ");
	
        if(params[i] == NULL) break;
    }
	puts(params[i-1]);
	if(strcmp(params[i-1], "&") == 0){
			backgr = 1;
			params[i-1] = NULL;
	}
}

int executeCmd(char** params)
{
    // Fork process
    pid_t pid = fork();
    printf("Start process %d\n", pid);

    // Error
    if (pid == -1) {
        char* error = strerror(errno);
        printf("fork: %s\n", error);
        return 1;
    }

    // Child process
    else if (pid == 0) {
        // Execute command
        if(strcmp(params[0],"cd\0")==0)
        {
        	changeDirectory(params[1]);
        	}
        else
        {
        	execvp(params[0], params);
        	}  

        // Error occurred
        char* error = strerror(errno);
        printf("shell: %s: %s\n", params[0], error);
        return 0;
    }

    // Parent process
    else {
        // Wait for child process to finish
        int childStatus;
	if(backgr == 0)
        	waitpid(pid, &childStatus, 0);
        return 1;
    }
}

int changeDirectory(char *path)
{
	return chdir(path);
}

