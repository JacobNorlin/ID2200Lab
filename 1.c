#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>

void parseCmd(char* cmd, char** params);
void registerSighandler(int signalCode, void (*handler)(int sig ));
void interruptHandler(int sig);
void printTermination(char* pType, int pid);
void blockSignal(int sig);
void unblockSignal(int sig);
void backgroundProcessHandler();
int executeCmd(char** params);
int changeDirectory(char *path);
int backgr;
int childStatus;	

#define MAX_NUMBER_OF_PARAMS 5
#define MAX_COMMAND_LENGTH 80


/**
Will reap all terminated child processes currently in the process table.
This will also work with signals because all processes that have changed
status(i.e sent a signal) will be in the process table.
*/
void backgroundProcessHandler(int sig){
	pid_t childPid;
	while((childPid = waitpid(-1, &childStatus, WNOHANG)) > 0){
		printf("Terminated background process %d\n", childPid);
		//printTermination("Background", pid);
	}
	
}

void printTermination(char* pType, int pid){
	if(WIFEXITED(pid)) printf("%s process %i exited normally", pType, pid);
	if(WIFSIGNALED(pid)) printf("%s process was terminated by %i signal", pType, WTERMSIG(pid)); 
}

void interruptHandler(int sig){
	pid_t pid = getpid();
	printf("pid %i", pid);
	kill(SIGKILL, -1*pid);
}

int main(int argc, char *argv[])
{
	char cmd[MAX_COMMAND_LENGTH + 1];
	char* params[MAX_NUMBER_OF_PARAMS + 1];


	registerSighandler(SIGCHLD, backgroundProcessHandler);
	registerSighandler(SIGINT, SIG_IGN);

	while (1){
		
		backgr = 0;
		printf(">");
		/**
		Will block signals while waiting for input from stdin, becuase
		otherwise the signal handler will affect fgets.
		*/
		blockSignal(SIGCHLD);
		fgets(cmd, sizeof(cmd),stdin);
		unblockSignal(SIGCHLD);

		//Check if there is any input, otherwise do nothing
		if(strlen(cmd) < 2){
			continue;
		}
		//Replace last token with null character to make processing commands easier
		if(cmd[strlen(cmd)-1] == '\n') {
		        cmd[strlen(cmd)-1] = '\0';
		}
		
		parseCmd(cmd,params);
		
		//Check if user writes exit
		if (strcmp(params[0], "exit\0") == 0) 
		{
			exit(0);
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
	if(strcmp(params[i-1], "&") == 0){
		backgr = 1;
		params[i-1] = NULL;
	}
}

int executeCmd(char** params)
{
	gid_t shellGid = getgid();
	pid_t pid = fork();
	setpgid(pid, shellGid);

	struct timeval tvBefore;
	gettimeofday(&tvBefore, NULL); 
    	

	if (pid == -1) {
		char* error = strerror(errno);
		printf("fork: %s\n", error);
		return 1;
	}else if (pid == 0) {
		
		if(strcmp(params[0],"cd\0")==0)
		{
			changeDirectory(params[1]);
		}
		else
		{
			execvp(params[0], params);
		}  

		char* error = strerror(errno);
		printf("shell: %s: %s\n", params[0], error);
		return 0;
	}else {
       	
		if(backgr == 0){
			registerSighandler(SIGINT, interruptHandler);
			blockSignal(SIGCHLD);
			printf("Start foreground process %d\n", pid);
			struct timeval tvAfter;
			//Wait for foreground process to change status
			pid = waitpid(pid, &childStatus, 0);
 			unblockSignal(SIGCHLD);
			registerSighandler(SIGINT, SIG_IGN);
		
			//Skriv ut tid
			gettimeofday(&tvAfter, NULL); 
			time_t time = ((tvAfter.tv_sec - tvBefore.tv_sec)*1000000L
				   +(tvAfter.tv_usec - tvBefore.tv_usec))/1000;
			printf("Parent process %d terminated in %ims\n", pid, (int)time);
			
			
		}else{
			
			printf("Start background process %d\n", pid);
		}
        return 1;
	}
}

void blockSignal(int sig){
	sigset_t signalSet;
	sigemptyset(&signalSet);

	sigaddset(&signalSet, sig);

	sigprocmask(SIG_BLOCK, &signalSet, NULL);

}

void unblockSignal(int sig){

	sigset_t signalSet;
	sigemptyset(&signalSet);

	sigaddset(&signalSet, sig);

	sigprocmask(SIG_UNBLOCK, &signalSet, NULL);

}



void registerSighandler(int signalCode, void (*handler)(int sig)){
	int ret;
	struct sigaction signalParameters;
	  /*
	   * ange parametrar för anrop till sigaction
	   * sa_handler = den rutin som ska köras om signal ges
	   * sa_mask    = mängd av övriga signaler som ska spärras
	   *              medan handlern körs (här: inga alls)
	   * sa_flags   = extravillkor (här: inga alls)
	   */
	signalParameters.sa_handler = handler;
	sigemptyset( &signalParameters.sa_mask );
	signalParameters.sa_flags = 0;
	ret = sigaction( signalCode, &signalParameters, (void *) 0 );
	if( -1 == ret )
	{ perror( "sigaction() failed" ); exit( 1 );}
}

int changeDirectory(char *path)
{
	return chdir(path);
}

