#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>


static char cmd[1000];
char *args[100];
char *args_temp[100];

bool chain = false;
bool chain_killed = false;

int fg_pid = -1;
int fg_grpid = -1;


void trim_newline() {				
	for(int i = 0; i < 1000; i++) {
		if (cmd[i] == '\n') {
			cmd[i] = '\0';
			break;
		}
	}			
}


int getin(char *filepath, int pos) {

	FILE *fp;
	char *data = (char*)malloc(100);
	int i;
	
	fp = fopen(filepath, "r");

	if (fp) {
		fgets(data, 50, fp);
		data[strlen(data)] = '\0';
	}
	else
		printf("\nError in reading the file.");

	fclose(fp);

	args[pos] = strtok(data, " ");
	for (i = pos+1; args[i-1] != NULL; i++) {		
		args[i] = strtok(NULL, " ");
	}

	return(i-1);
}


void getout(char *filepath, char out[4096]) {

	FILE *fp;	
	int i, pipe_cnt = 0;
	
	fp = fopen(filepath, "w+");

	if (fp)
		fputs(out, fp);
	else
		printf("\nError in writing file.");

	fclose(fp);
}


void sigintHandler(int sig_num) 
{ 
	if(kill(fg_pid, SIGTERM) < 0)
		printf("Error in Cntrl+C!\n");

	if(chain) 
		chain_killed = true;

	fflush(stdout); 
} 


int main() { 

	printf("\nStarting mini-shell..Enter \"exit\" to end.\n");

	int i = 1, background = 0, output = 0, link[2];
	char out_file[100];

	signal(SIGINT, sigintHandler); 
		 
	while(1) {

		int cmd_cnt = 0, cmd_exec = 0;

		chain = false;
		chain_killed = false;

		printf("\nminish> ");
		fflush(NULL);
		fgets(cmd, 1000, stdin);		

		
		if (strncmp(cmd, "exit", 4) == 0)
			if(killpg(getpgrp(), SIGTERM) == -1)
				printf("Error in exiting!\n");
					
		
		trim_newline();	
			
		args_temp[0] = strtok(cmd, "|");				
		for (i = 1; args_temp[i-1] != NULL; i++) {
			args_temp[i] = strtok(NULL, "|");
			cmd_cnt++;
		}
		args_temp[i] = NULL;		

		int num_pipes = cmd_cnt - 1;
		int fd[2], old_fd = 0;
		 		
		// parsing and executing each command
		for(int k = 0; args_temp[k] != NULL; k++)	{
		
			args[0] = strtok(args_temp[k], " ");	
			for (i = 1; args[i-1] != NULL; i++) {		
				args[i] = strtok(NULL, " ");					
	
				if (strcmp(args[i-1], "<") == 0)
					i = getin(args[i], i-1);
			
				if (strcmp(args[i-1], ">") == 0) {
					strcpy(out_file, args[i]);			
					i = i-2;	 
					output = 1;	
				}				
				
				if (args[i] != NULL) 
					if (strcmp(args[i], "&") == 0) {
						background = 1;
						i--;
					}			
			}	
			args[i] = NULL;	

			for(i = 0; args[i] != NULL; i++) {			
				for(int temp = 0; temp < 100; temp++) {
					if (args[i][temp] == '\n') {
						args[i][temp] = '\0';
						break;
					}
				}
			}


			if(output)
				if(pipe(link) == -1) {
					printf("\nError while creating pipe for file output.");
					exit(0);
				}


			if(pipe(fd) == -1) {
				printf("\nError while creating pipe for pipelining.");
				exit(0);
			}


			if(strcmp(args[0], "kill") == 0) {
				signal(SIGCHLD, SIG_IGN);
				if(kill(atoi(args[1]), SIGTERM) == -1)
					printf("Error in exiting!\n");

				continue;
			}

			
			if(cmd_cnt > 1) {
				chain = true;
			}
			else {
				chain = false;
				fg_grpid = -1;
			}

			int cid = -1;
			if(!chain_killed)
				cid = fork();			
			else {
				chain_killed = false;
				break;
			}

			if (cid < 0) {
				printf("\nError while forking..\n");		
				exit(1);
			}

			if (cid == 0) {	
		
				if(output) {
					if(dup2(link[1], STDOUT_FILENO) < 0) {
						printf("\nError in dup2 for file output.");
						exit(0);
					}
					close(link[0]);		
				}

				if(dup2(old_fd, 0) < 0) {
					printf("\nError in dup2.");
					exit(0);
				}

				if(args_temp[k+1] != NULL) {
					if(dup2(fd[1], 1) < 0) {
						printf("\nError in dup2.");
						exit(0);
					}
				}
				
				close(fd[0]);

				if (execvp(args[0], args) < 0) {
					printf("\nError while executing the command.");
					exit(0);
				}
			}	
			
			if (cid > 0)  {

				fg_pid = cid;
				signal(SIGCHLD, SIG_DFL);

				if(output) {
					char out[4096];
					close(link[1]);
					read(link[0], out, sizeof(out));
					getout(out_file, out);
					output = 0;
				}
		
				if(!background) {				

					int status;
					if(waitpid(cid, &status, 0) < 0)
						printf("\nError in applying wait.");

				}
				else {	
					signal(SIGCHLD,SIG_IGN);									
					printf("\nProcess %d in background mode", cid);
					background = 0;					
				}

				close(fd[1]);
				old_fd = fd[0];
			}
		}
	}
	
	return(0);
}
