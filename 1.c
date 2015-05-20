#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <pwd.h>

void parseCmd(char* cmd, char** params);
void registerSighandler(int signalCode, void (*handler)(int sig ));
void interruptHandler(int sig);
void printTermination(char* pType, int pid);
void blockSignal(int sig);
void unblockSignal(int sig);
void backgroundProcessHandler();
int checkEnv();
int executeCmd(int in, int out, char** params);
int changeDirectory(char *path);
int backgr;
int childStatus;	

#define MAX_NUMBER_OF_PARAMS 5
#define MAX_COMMAND_LENGTH 80
/*Taken from the internet*/
#define CHECK(x) if(!(x)) { perror(#x " failed"); }

/**
Will reap all terminated child processes currently in the process table.
This will also work with signals because all processes that have changed
status(i.e sent a signal) will be in the process table.
*/
void backgroundProcessHandler(int sig){
	pid_t childPid;
	while((childPid = waitpid(-1, &childStatus, WNOHANG)) > 0){
		printf("Terminated background process %d\n", childPid);
	}
	
}


void printTermination(char* pType, int pid){
	if(WIFEXITED(pid)) printf("%s process %i exited normally", pType, pid);
	if(WIFSIGNALED(pid)) printf("%s process was terminated by %i signal", pType, WTERMSIG(pid)); 
}


int main(int argc, char *argv[])
{
	char cmd[MAX_COMMAND_LENGTH + 1];
	char* params[MAX_NUMBER_OF_PARAMS + 1];
	char cwd[1024];
	char* error;
	int status;



	#if SIGDET==1
	registerSighandler(SIGCHLD, backgroundProcessHandler);
	#endif
	/*Ignore ctrl-c in the parent. Currently ctrl-c will kill 
	 *all children since the only way of preventing children from
	 *receiving the signal would be to change the process group.
	 *However, by changing the process group we will not be able to
	 *terminate them since no lists of processes are allowd*/
	registerSighandler(SIGINT, SIG_IGN);

	while (1){
		#if SIGDET==0
		backgroundProcessHandler(0);
		#endif

		/*Reset the background flag before a new command is read*/
		backgr = 0;

		/*Get the current directory and print it as the prompt. getcwd() = getcurrentowrkingdirectory*/
		CHECK(getcwd(cwd, sizeof(cwd)) != NULL);
		printf("%s$ ",cwd);

		/**
		Will block signals while waiting for input from stdin, becuase
		otherwise the signal handler will affect fgets.
		*/
		blockSignal(SIGCHLD);
		CHECK(fgets(cmd, sizeof(cmd),stdin) != NULL);
		unblockSignal(SIGCHLD);

		/*Check if there is any input, otherwise do nothing*/
		if(strlen(cmd) < 2){
			continue;
		}
		/*Replace last token with null character to make processing commands easier*/
		if(cmd[strlen(cmd)-1] == '\n') {
		        cmd[strlen(cmd)-1] = '\0';
		}
		
		parseCmd(cmd,params);


		/*Check if user writes exit*/
		if (strcmp(params[0], "exit\0") == 0) 
		{
			exit(0);
		}

		/*cd is its own commands, so here we check for that*/
		if(strcmp(params[0],"cd")==0)
		{
			if(params[1] == NULL) params[1] = "~"; /*writing just cd is equivalent to cd ~ */
			status = changeDirectory(params[1]);
			error = strerror(errno);
			if(status){
				printf("%s\n", error);
			}
		/*CheckEnv command*/
		}else if(strcmp(params[0], "checkEnv") == 0){
			checkEnv(params);

		}else{
			executeCmd(0, 1, params);
		}
	
	}
	
	return 0;
}

void parseCmd(char* cmd, char** params)
{   
	int i;
	/*Parse only the max number of commands. Currently only ignores if too many commands are given*/
	for(i = 0; i < MAX_NUMBER_OF_PARAMS; i++) {
		params[i] = strsep(&cmd, " ");
		if(params[i] == NULL) break;
	}
	/*Check if background process*/
	if(strcmp(params[i-1], "&") == 0){
		backgr = 1;
		params[i-1] = NULL;
	}
}

int checkEnv(char** params){
		/*File descriptors for pipes*/
		int in, fd[2];

		char* input[MAX_NUMBER_OF_PARAMS+1];
		char* pager;
		char* error;
		int execError;
		pid_t pid;
		/*Make sure input params contains null so execvp works properly*/
	    input[1] = NULL;
		
	    /*Create a new process to do the entire checkEnv pipeline within*/
		pid = fork();

		if(pid == 0){
			/*Set the in pipe to stdin
			 *Create a pipe for the file descriptor
			 *Set input to the command to be run
			 *Execute that command with in as input pipe
			 *and fd[1] as the output pipe.
			 *Close pipe
			 *Change in pipe to the write part of pipe*/
			in = 0;
			CHECK(pipe(fd) == 0);
			input[0] = "printenv";
			executeCmd(in, fd[1], input);
			CHECK(close(fd[1]) == 0);
			in = fd[0];

			/*In case user gives any params to checkEnv we run grep on those params*/
			if(params[1] != NULL){
				CHECK(pipe(fd) == 0);
				params[0] = "grep";
				executeCmd(in, fd[1], params);
				CHECK(close(fd[1]) == 0);
				in = fd[0];
			}

			CHECK(pipe(fd) == 0);
			input[0] = "sort";
			executeCmd(in, fd[1], input);
			CHECK(close(fd[1]) == 0);
			in = fd[0];
			/*Make sure output goes to stdout again*/
			if(in != 0)	CHECK(dup2(in, 0) != -1);
			
			/*Get the current PAGER environment variable, otherwise use less*/
			if((pager = getenv("PAGER"))==NULL){
				pager = "less";
			}
			
			input[0] = pager;
			/*Replace the new forked child process image with the process of the last command*/
			execError =	execvp(input[0], input);	

			/*In case there was an error with executing with less, execute with more instead.
			 *execvp stops execution of this process if it is sucessful, so this should only
			 *be run in case it fails.*/
			if(execError == -1){
				pager = "more";

				input[0] = pager;
				execvp(input[0], input);

			}


			error = strerror(errno);
			printf("shell: %s: %s errno: %i\n", params[0], error, errno);
		}else{
			/*Reap the final process*/
			wait(NULL);
		}


		
		return 0;

}


int executeCmd(int in, int out, char** params)
{
	char* error;
	struct timeval tvBefore;
	struct timeval tvAfter;
	time_t time;
	/*Fork new process*/
	pid_t pid = fork();


	/*Store time when process started*/
	
	CHECK(gettimeofday(&tvBefore, NULL) == 0); 
    	

	if (pid == -1) {
		error = strerror(errno);
		printf("fork: %s\n", error);
		return 1;
	}else if (pid == 0) {


		/*if(backgr) setpgid(0,0);*/

		/*In case we are not on 0,1(stdin,stdout). We have to replace the input and 
		 *output with the corresponding read and write parts of the given pipes*/
		if (in != 0)
		{
		  CHECK(dup2 (in, 0) != -1);
		  CHECK(close (in) != -1);
		}

		if (out != 1)
		{
		  CHECK(dup2 (out, 1) != -1);
		  CHECK(close (out) != -1);
		}
		
		execvp(params[0], params);	

		error = strerror(errno);
		printf("Command not found: %s: %s\n", params[0], error);
		return 0;
	}else {


		if(backgr == 0){
			
			printf("Start foreground process %d\n", pid);
			/*Wait for foreground process to change status
			 *We block SIGCHLD here to avoid having background
			 *processes killing the foreground process when exiting
			 *since the signal handler works on SIGCHLD*/
			blockSignal(SIGCHLD);
			pid = waitpid(pid, &childStatus, 0);
 			unblockSignal(SIGCHLD);
			
		
			/*Take timestamp after the process has finished, print out total execution time in ms*/
			CHECK(gettimeofday(&tvAfter, NULL) == 0);
			time = ((tvAfter.tv_sec - tvBefore.tv_sec)*1000000L
				   +(tvAfter.tv_usec - tvBefore.tv_usec))/1000;
			printf("Foreground process %d terminated in %ims\n", pid, (int)time);
			
		}else{
       		/*setpgid(pid, pid);*/
			printf("Start background process %d\n", pid);
		}
		
        return 1;
	}
}

/*Sets the current procmask to block the given signal*/
void blockSignal(int sig){
	sigset_t signalSet;
	CHECK(sigemptyset(&signalSet) == 0);

	CHECK(sigaddset(&signalSet, sig) == 0);

	CHECK(sigprocmask(SIG_BLOCK, &signalSet, NULL) == 0);

}
/*Sets the current procmask to unblock the given signal*/
void unblockSignal(int sig){

	sigset_t signalSet;
	CHECK(sigemptyset(&signalSet) == 0);

	CHECK(sigaddset(&signalSet, sig) == 0);

	CHECK(sigprocmask(SIG_UNBLOCK, &signalSet, NULL) == 0);

}


/*Registers a new signal handler for the given signal with the 
*given signal handler(function). Taken from the example code in
*"Användbara systemanrop" pdf.*/
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

/*Function for changing current directory to given path.
 *In case of ~ it will search for the current home folder
 *and switch to that one instad.*/
int changeDirectory(char *path)
{
	printf("Path %s", path);
	if(strcmp(path, "~") == 0){

		if ((path = getenv("HOME")) == NULL) {
		    path = getpwuid(getuid())->pw_dir;
		}	
	}
	return chdir(path);
}
