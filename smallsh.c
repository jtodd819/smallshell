//James Todd - 01/05/18

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

int main(){
	/*boolean to take input while the user has not entered 'exit' command, int to store result of various function 
	calls, ints to hold the last exit and termination statuses of a terminating foreground process, a boolean to determine
	if the last child process was terminated or exited, and an int to hold the last child's exitMethod with wait*/
 	int running = 1, result = 0, lastExitStatus = 0, lastTerminationStatus = 0, exitedLast = 1, childExitMethod;
	/*pid_t array to store the pids of running background processes, a pid for a child process, an int to count the current 
	 *number of them, and an int to determine if background commands are accepted*/
	pid_t runningBG[100000], actualPid;
	int bgCount = 0, bgStatus = 1;
	do{
		//set up a string inputLine to receieve user commands with getline()
		int charCnt = -1;
		size_t bufferSize = 0;
		char* inputLine = NULL;
		//function to change the background command status and print informative messages about it
		void changeBGStatus(int sigNum){
			//disable background
			if(bgStatus){
				bgStatus = 0;
				write(STDOUT_FILENO, "\nEntering foreground-only mode (& is now ignored)\n", 51);
				fflush(stdout);
			//reenable background
			}else{
				bgStatus = 1;
				write(STDOUT_FILENO, "\nExiting foreground-only mode\n", 31);
				fflush(stdout);
			}
		}
		//set up the signal handlers for SIGINT and SIGTSTP
		struct sigaction SIGINT_action = {{0}}, SIGTSTP_action = {{0}};
		//SIGINT ignored at the parent level
		SIGINT_action.sa_handler = SIG_IGN;
		SIGINT_action.sa_flags = 0;
		sigfillset(&SIGINT_action.sa_mask);
		//SIGSTP Will change the status of "&" being accepted
		SIGTSTP_action.sa_handler = changeBGStatus;
		SIGTSTP_action.sa_flags = 0;
		sigfillset(&SIGTSTP_action.sa_mask);
		//set the actions for each signal
		sigaction(SIGTSTP, &SIGTSTP_action, NULL);
		sigaction(SIGINT, &SIGINT_action, NULL);
		//take in the user input, clearing error and repeating if interrupted
		while(1){
			//display initial command prompt
			printf(": ");
			fflush(stdout);
			//clear errors if interrupted
			if((charCnt = getline(&inputLine, &bufferSize, stdin)) == - 1){
				clearerr(stdin);
			}else{
				//replace the new line with getline() with a null terminator and break if not interrupted
				inputLine[strcspn(inputLine, "\n")] = '\0';
				break;
			}
		}
		//check for "exit" or "exit other_commands" to exit
		if(!strcmp(inputLine, "exit") || !strncmp(inputLine, "exit ", (size_t)5)){
			running = 0;	
		//check for "cd" command as the first two letters of the input line
		}else if(!strncmp(inputLine, "cd", 2)){
			//string to hold input for "cd some_path"
			char* inputPath;
			//change to home if entering "cd"
			if(charCnt == 3){
				result = chdir(getenv("HOME"));
			}else if(charCnt > 4){
				//inputPath is the word after cd 
				strtok(inputLine, " ");	
				if((inputPath = strtok(NULL, " ")) != NULL){
					//handle pid expansion for changing directories
					char* inputSubstring;
					if((inputSubstring = strstr(inputPath, "$$")) != NULL){
						char* newInputPath = malloc(sizeof(char) * (strlen(inputPath) + 30));
						//handle string with $$ in the middle by splitting into beginning and end substrings
						if(strlen(inputSubstring) > 2){
							char* beginningSubstring = malloc(sizeof(char) * (strlen(inputPath) - strlen(inputSubstring) + 1));
							strncpy(beginningSubstring, inputPath, (size_t)(strlen(inputPath) - strlen(inputSubstring)));  
							char* endSubstring = malloc(sizeof(char) * (strlen(inputSubstring) - 1));
							strncpy(endSubstring, (inputSubstring + 2*sizeof(char)), (size_t)(strlen(inputSubstring) - 2));  
							sprintf(newInputPath, "%s%d%s", beginningSubstring, (int)getpid(), endSubstring);
							free(beginningSubstring);
							free(endSubstring);
						//if $$ at the end of an arg, just remove end and replace with pid
						}else if(strcmp(inputPath, "$$")){
							strcpy(inputSubstring, "");
							sprintf(newInputPath, "%s%d", inputPath, (int)getpid());
						//if the inputPath is just $$, replace it with pid completely
						}else{
							sprintf(newInputPath, "%d", (int)getpid());
						}
					//change to the new input path and free it
					result = chdir(newInputPath);
					free(newInputPath);
					newInputPath = NULL;
					//otherwise change to the old input path
					}else{
						result = chdir(inputPath);
					}
				}
			}
		//handle input "status" or "status &" command
		}else if(!strcmp(inputLine, "status") || !strcmp(inputLine, "status &")){
			//print the exit status of last foreground process if available
			if(exitedLast){
				printf("exit value %d\n", lastExitStatus);
				fflush(stdout);
			//if no exit status print the termination status of last foreground process
			}else{
				printf("terminated by signal %d\n", lastTerminationStatus);
				fflush(stdout);
			}
		//fork and execute other inputs that are not comments or blank lines
		}else if(inputLine[0] != '#' && strlen(inputLine)){
			//store the command up to 2048 characters in a copy to tokenize
			char inputCopy[2048];
			strncpy(inputCopy, inputLine, (size_t)2048);
			//boolean for running an argument in background, and indices of where to direct output and input
			int runInBackground = 0, inputIndex = -1, outputIndex = -1;
			//create a temporary arg holde to hold the input string tokens
			char* tempArg = strtok(inputCopy, " ");
			//an array of arguments and the number of them
			char* args[515];
			int argCount;
			args[0] = malloc(sizeof(char) * (strlen(tempArg) + 1));
			strcpy(args[0], tempArg);
			//put each argument separated by a space from the command into the array
			for(argCount = 1; argCount < 515; argCount++){
				//if we reach the end of arguments then stop looping
				if((tempArg = strtok(NULL, " ")) == NULL){
					break;
				}
				//convert instances of $$ to the shell's PID
				char* pidSubstring;
				if((pidSubstring = strstr(tempArg, "$$")) != NULL){
					args[argCount] = malloc(sizeof(char) * ((int)strlen(tempArg) + 15));
					//handle string with $$ in the middle by splitting into beginning and end substrings
					if(strlen(pidSubstring) > 2){
						char* beginningSubstring = malloc(sizeof(char) * (strlen(tempArg) - strlen(pidSubstring) + 1));
						strncpy(beginningSubstring, tempArg, (size_t)(strlen(tempArg) - strlen(pidSubstring)));  
						char* endSubstring = malloc(sizeof(char) * (strlen(pidSubstring) - 1));
						strncpy(endSubstring, (pidSubstring + 2*sizeof(char)), (size_t)(strlen(pidSubstring) - 2));  
						sprintf(args[argCount], "%s%d%s", beginningSubstring, (int)getpid(), endSubstring);
						free(beginningSubstring);
						free(endSubstring);
					//if $$ at the end of an arg, just remove end and replace with pid
					}else if(strcmp(tempArg, "$$")){
						strcpy(pidSubstring, "");
						sprintf(args[argCount], "%s%d", tempArg, (int)getpid());
					//if the arg is just $$, replace it with pid completely
					}else{
						sprintf(args[argCount], "%d", (int)getpid());
					}
				//otherwise copy the temp arg into args array
				}else{
					args[argCount] = malloc(sizeof(char) * (strlen(tempArg) + 1));
					strcpy(args[argCount], tempArg);
				}
			}
			/*determine array indices of input and output redirection 
			strings and background processing if necessary*/
			//last argument = "&"
			if(argCount > 1 && !strcmp(args[argCount - 1],"&")){
				runInBackground = 1;
				//third to last argument = "<"
				if(argCount > 3 && !strcmp(args[argCount - 3],"<")){
					inputIndex = argCount - 2;
					//fifth to last argument = ">"
					if(argCount > 5 && !strcmp(args[argCount - 5], ">")){
						outputIndex = argCount - 4;
					}
				//third to last argument = ">"
				}else if(argCount > 3 && !strcmp(args[argCount - 3],">")){
					outputIndex = argCount - 2;
					//fifth to last argument = "<"
					if(argCount > 5 && !strcmp(args[argCount - 5], "<")){
						inputIndex = argCount - 4;
					}
				}
			//second to last argument = ">"
			}else if(argCount > 2 && !strcmp(args[argCount - 2], ">")){
				outputIndex = argCount - 1;
				//fourth to last argument = "<"
				if(argCount > 4 && !strcmp(args[argCount - 4], "<")){
					inputIndex = argCount - 3;
				}
			//second to last argument = "<"
			}else if(argCount > 2 && !strcmp(args[argCount - 2], "<")){
				inputIndex = argCount - 1;
				//fourth to last argument = ">"
				if(argCount > 4 && !strcmp(args[argCount - 4], ">")){
					outputIndex = argCount - 3;
				}
			}
			//leave off the redirection and background commands from the argument count
			argCount -= runInBackground;
			if(inputIndex != -1){
				argCount -= 2;
			}
			if(outputIndex != -1){
				argCount -= 2;
			}
			/*create a copy of the arguments leaving off the redirection and background commands
			adding NULL for execvp*/
			char* argCopy[argCount + 1];
			argCopy[argCount + 1] = NULL;
			for(int i = 0; i < argCount; i++){
				argCopy[i] = malloc(sizeof(char) * (strlen(args[i]) + 1));
				strcpy(argCopy[i], args[i]);
			}
			//set up where to redirect input and output if necessary
			char* inputString, *outputString;
			//boolean to determine if input or output redirected to /dev/null
			int inputNull = 0, outputNull = 0;
			//redirect input if input index set
			if(inputIndex != -1){
				inputString = malloc(sizeof(char) * (strlen(args[inputIndex]) + 1));
				strcpy(inputString, args[inputIndex]);
			//if not, then redirect to /dev/null/ if running in the background
			}else if(runInBackground && bgStatus){
				inputString = "/dev/null";
				inputNull = 1;
			}
			//redirect output if output index set
			if(outputIndex != -1){
				outputString = malloc(sizeof(char) * (strlen(args[outputIndex]) + 1));
				strcpy(outputString, args[outputIndex]);
			//if not, then redirect to /dev/null/ if running in the background
			}else if(runInBackground && bgStatus){
				outputString = "/dev/null";
				outputNull = 1;
			}
			//fork off a child to execute the command
			pid_t childPid = fork();
			switch(childPid){
				//fork error
				case -1:
					printf("fork error\n");
					break;
			//child process
				case 0:
					//set the child to terminate itself if receiving a SIGINT signal
					SIGINT_action.sa_handler = SIG_DFL;
					SIGINT_action.sa_flags = 0;
					sigfillset(&SIGINT_action.sa_mask);
					sigaction(SIGINT, &SIGINT_action, NULL);
					//redirect output if the output index set or /dev/null
					if(outputIndex != -1 || outputNull){
						//open an output file printing an error and setting error status if failed
						int outputFD;
						if((outputFD = open(outputString, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1){
							printf("cannot open %s for output\n", outputString);
							fflush(stdout);
							lastTerminationStatus = 1;
							exit(1);
						}
						//try redirecting output printing error and exiting with error status if failed
						else if((result = dup2(outputFD, 1)) == -1){
							printf("ERROR: failed output redirection\n");
							fflush(stdout);
							exit(1);
						}
						//clean up outputString if malloc'd
						if(!outputNull){
							free(outputString);
							outputString = NULL;
						}
					}
					//redirect input if the input index set or /dev/null
					if(inputIndex != - 1 || inputNull){
						//open an input file printing an error and setting error status if failed
						int inputFD;
						if((inputFD = open(inputString, O_RDONLY)) == -1){
							printf("cannot open %s for input\n", inputString);
							fflush(stdout);
							lastTerminationStatus = 1;
							exit(1);
						}
						//try redirecting input printing error and exiting with error status if failed
						else if((result = dup2(inputFD, 0)) == -1){
							printf("ERROR: failed input redirection\n");
							fflush(stdout);
							exit(1);
						}
						//clean up inputString if malloc'd
						if(!inputNull){
							free(inputString);
							inputString = NULL;
						}
					}

					if((result = execvp(argCopy[0], argCopy)) == -1){
						printf("%s: no such file or directory\n", argCopy[0]);
						fflush(stdout);
						exit(1);
					}
					exit(0);
					break;
				//parent
				default:
					/*if the process is to be run in background, add it to the array to be waited on 
					 * up to 100000 processes, printing it's pid*/
					if((bgStatus && runInBackground) && (bgCount < 100000)){
							runningBG[bgCount] = childPid;
							bgCount++;
							printf("background pid is %d\n", (int)childPid);
							fflush(stdout);
					//foreground processes are waited for by shell
					}else{
						//set the value of last exit status or last termination status and last exited booleans
						actualPid = waitpid(childPid, &childExitMethod, 0);
						if(WIFEXITED(childExitMethod)){
							exitedLast = 1;
							lastExitStatus = WEXITSTATUS(childExitMethod);
						}else{
							exitedLast = 0;
							lastTerminationStatus = WTERMSIG(childExitMethod);
							printf("terminated by signal %d\n", lastTerminationStatus);
						}
					}
					break;
			}
			//clean up the allocated args and argCopy array
			for(int i = 0; i < argCount; i++){
				free(args[i]);
				free(argCopy[i]);
				argCopy[i] = NULL;
				args[i] = NULL;
			}
		}
		//function to wait for each running background process without blocking
		void checkBackgroundProcesses(){
			//keep track of the processes removed to not fill up the array
			int processesRemoved = 0;
			for(int i = 0; i < bgCount; i++){
				if((actualPid = waitpid(runningBG[i], &childExitMethod, WNOHANG)) != 0){
					processesRemoved++;
					// print out informative message if exited
					if(WIFEXITED(childExitMethod)){
						printf("background pid %d is done: exit value %d\n", (int)runningBG[i], WEXITSTATUS(childExitMethod));
						fflush(stdout);
					//print out informative message if terminated
					}else{
						printf("background pid %d is done: terminated by signal %d\n", (int)runningBG[i], WTERMSIG(childExitMethod));
						fflush(stdout);
					}
				};
			}
			bgCount -= processesRemoved;
		}
		//try waiting for each running background process
		checkBackgroundProcesses();
		//clean up allocated input
		free(inputLine);
		inputLine = NULL;
	}while(running);
	return(0);
}
