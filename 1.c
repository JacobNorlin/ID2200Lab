#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>

void parseCmd(char* cmd, char** params);
void handleBackgroundProcesses();
int executeCmd(char** params);
int changeDirectory(char *path);
int backgr;
int childStatus;	

#define MAX_NUMBER_OF_PARAMS 5
#define MAX_COMMAND_LENGTH 70

static void foo(int sig, siginfo_t *siginfo, void *context){
	waitpid(-1, NULL, 0);
	printf ("Reaped PID: %ld", (long)siginfo->si_pid);
}

int main(int argc, char *argv[])
{
	char cmd[MAX_COMMAND_LENGTH + 1];
	char* params[MAX_NUMBER_OF_PARAMS + 1];

	void handleBackgroundProcesses(){
		//Poll for changes in background process
		#if !defined(SIGDET)
		pid_t childPid;
		while((childPid = waitpid(-1, &childStatus, WNOHANG)) > 0){
			printf("Terminated background process %d\n", childPid);
		}
		#else
		
		#endif
		
		
	}

	struct sigaction action;
	memset (&action, '\0', sizeof(action));
	action.sa_sigaction = &foo;
	action.sa_flags = SA_SIGINFO;
	sigaction(SIGCHLD, &action, NULL);

	while (1){
		backgr = 0;
		//handleBackgroundProcesses();
		printf(">");
		fgets(cmd, sizeof(cmd),stdin);
		//Check if there is any input, otherwise do nothing
		if(strlen(cmd) < 2){
			continue;
		}
		if(cmd[strlen(cmd)-1] == '\n') {
		        cmd[strlen(cmd)-1] = '\0';
		    }
		parseCmd(cmd,params);
		
		//Check if user writes exit
		if (strcmp(params[0], "exit\0") == 0) 
		{
			//handleBackgroundProcesses();
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
	struct timeval tvBefore;
	gettimeofday(&tvBefore, NULL); 

    pid_t pid = fork();
    

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
       	
	if(backgr == 0){
		sigset_t mask;
		sigset_t orig_mask;

		sigemptyset (&mask);
		sigaddset (&mask, SIGCHLD);


		sigprocmask(SIG_BLOCK, &mask, &orig_mask);
		printf("Start foreground process %d\n", pid);
		struct timeval tvAfter;
        	pid = waitpid(pid, &childStatus, 0);
		gettimeofday(&tvAfter, NULL); 
		time_t time = ((tvAfter.tv_sec - tvBefore.tv_sec)*1000000L
			   +(tvAfter.tv_usec - tvBefore.tv_usec))/1000;
		printf("Parent process %d terminated in %ims\n", pid, (int)time);
		sigprocmask(SIG_SETMASK, &orig_mask, NULL);
		
		//Skriv ut tid
	}else{
		printf("Start background process %d\n", pid);
	}
        return 1;
    }
}

int changeDirectory(char *path)
{
	return chdir(path);
}

