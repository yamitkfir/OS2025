/* YAMIT KFIR */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h> // NEW
#include <fcntl.h> // NEW
#include <sys/wait.h> // NEW

#define OPEN_FILE 0600
#define SIZE_OF_BUFFER 1024
#define MAX_PIPES 10
#define FAIL "Execution failed - There's no such command"
#define FORK_FAIL "Failed to fork"
#define FAIL_FILE "Failed opening file"
#define FAIL_MEM "Failed allocating memory"
#define FAIL_PIPE "Piping failed"
#define FAIL_SIGINT "Failed to reset one of the signals"

// arglist - a list of char* arguments (words) provided by the user
// it contains count+1 items, where the last item (arglist[count]) and *only* the last is NULL
int process_arglist(int count, char** arglist);
int background_process(int count, char** arglist);
int foreground_proccess(int count, char** arglist);
int input_redirect(int count, char** arglist, int index);
int output_redirect(int count,char** arglist, int index);
int pipe_process(int count, char** arglist);
int prepare(void);
int finalize(void);
int readFromFile(char** buff, char* file_name);
char* SHELL_SIMBOLS[] = {"&", "|", "<", ">"};
void signal_resetters();
int wait_for_child();

// RETURNS - 1 if should continue, 0 otherwise
int process_arglist(int count, char** arglist){
	char* last_word = arglist[count-1];

	if (strcmp(last_word, SHELL_SIMBOLS[0]) == 0){
		return background_process(count, arglist);
	}
	int pipe = 1;
	for (int i=0; i<count; i++){ // looking for the other special chars
		if (strcmp(arglist[i], SHELL_SIMBOLS[2]) == 0){
			return input_redirect(count, arglist, i);
		}
		else if (strcmp(arglist[i], SHELL_SIMBOLS[3]) == 0){
			return output_redirect(count, arglist, i);
		}
		else if(strcmp(arglist[i], SHELL_SIMBOLS[1]) == 0){
			pipe += 1;
		}
	}
	if (pipe > 1){
		return pipe_process(count, arglist);
	}
	else{ // just a normal command
		return foreground_proccess(count, arglist);
	}
}

int foreground_proccess(int count, char** arglist){
	pid_t pid = fork();
	// int exit_code = -1;
	if (pid == -1) {
		fprintf(stderr, "%s\n", FORK_FAIL);
		return 0;
	}
	
	else if(pid > 0){ // FATHER
		return wait_for_child(pid);
	}

	else{ // SON
		signal_resetters();
		// Finally do the thing
		if (execvp(arglist[0], arglist) == -1) {
			fprintf(stderr, "%s\n", FAIL);
			exit(1);
		}
	}
}

int background_process(int count, char** arglist){
	pid_t pid = fork();
	if (pid == -1) {
		fprintf(stderr, "%s\n", FORK_FAIL);
		return 0;
	}
	else if(pid > 0){ // FATHER
		return 1;
	}
	else{ // SON
		// DONT Restore default SIGINT handling
		// Reset SIGCHLD handling for potential grandchildren
		if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
			fprintf(stderr, "%s\n", FAIL_SIGINT);
			exit(1);
		}
		arglist[count - 1] = NULL; // remove last word (&) from arglist
		if (execvp(arglist[0], arglist) == -1) {
			fprintf(stderr, "%s\n", FAIL);
			return 0;
		}
		exit(0); // NEW: NEEDED?
	}
}

int input_redirect(int count, char** arglist, int index){
	int pipefd[2];
	if (pipe(pipefd) == -1) {
        fprintf(stderr, "%s\n", FAIL_PIPE);
        return 0;
    }
	char* file_name = arglist[index+1];
	pid_t pid = fork();
	if (pid == -1) {
		close(pipefd[0]);
		close(pipefd[1]);
		fprintf(stderr, "%s\n", FORK_FAIL);
		return 0;
	}

	else if(pid > 0){ // FATHER - opens file and sends to son
		close(pipefd[0]);
		char* buff;
		int readed = readFromFile(&buff, file_name);
		if (readed == 0){ // cleanup
			close(pipefd[1]);
			kill(pid, SIGTERM);
			waitpid(pid, NULL, 0);
			return 0;
		} // now buff contains all file content
		write(pipefd[1], buff, strlen(buff));
		// free(buff);
		close(pipefd[1]);

		// now wait for son to finish
		return wait_for_child(pid);
	}

	else{ // SON - performs action
		signal_resetters();
		char buf[SIZE_OF_BUFFER];
  		// ssize_t n_bytes = -1;
		close(pipefd[1]);
		dup2(pipefd[0], STDIN_FILENO);
		close(pipefd[0]); // Close read side after duplicating
		arglist[index] = NULL;
		if (execvp(arglist[0], arglist) == -1) {
			fprintf(stderr, "%s\n", FAIL);
			return 0;
		}
	}
}

int output_redirect(int count,char** arglist, int index){
	int pipefd[2];
	if (pipe(pipefd) == -1) {
        fprintf(stderr, "%s\n", FAIL_PIPE);
        return 0;
    }
	char* file_name = arglist[index+1];
	pid_t pid = fork();
	if (pid == -1) {
		fprintf(stderr, "%s\n", FORK_FAIL);
		return 0;
	}

	else if(pid > 0){ // FATHER - opens file and puts there son's output
		close(pipefd[1]); // Close write end
		char buf[SIZE_OF_BUFFER];
		int fd = open(file_name, O_CREAT | O_TRUNC | O_RDWR, OPEN_FILE);
		ssize_t bytes_read; ssize_t bytes_written;
		if (fd == -1) {
			fprintf(stderr, "%s\n", FAIL_FILE);
			close(pipefd[0]); // Close read end before returning
			return 0;
		}
		// Read from pipe and write to file
		while ((bytes_read = read(pipefd[0], buf, sizeof(buf))) > 0) {
			// writing to file along the way!
			if (write(fd, buf, bytes_read) != bytes_read) {
				fprintf(stderr, "%s\n", FAIL_FILE);
				close(fd);
				return 0;
			}
		}
		close(fd);
		close(pipefd[0]);

		// Wait for child to finish
		return wait_for_child(pid);
	}
	
	else{ // SON - performs action and redirecrs output to father
		signal_resetters();
		arglist[count - 2] = NULL; // remove last word (>) from arglist
		close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
        close(pipefd[1]);
		if (execvp(arglist[0], arglist) == -1) {
			fprintf(stderr, "%s\n", FAIL);
			return 0;
		}
		// exit(0);
	}
}

int readFromFile(char** buff, char* file_name) {
    int fd = open(file_name, O_RDONLY, 0);
    if (fd == -1) {
        fprintf(stderr, "%s\n", FAIL_FILE);
        return 0;
    }
    off_t fileSize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    *buff = (char*)malloc(fileSize + 1);
    if (*buff == NULL) {
        fprintf(stderr, "%s\n", FAIL_MEM);
        close(fd);
        return 0;
    }
    ssize_t bytesRead = read(fd, *buff, fileSize);
    if (bytesRead == -1) {
        fprintf(stderr, "%s\n", FAIL_FILE);
        free(*buff);
        close(fd);
        return 0;
    }
    (*buff)[bytesRead] = '\0';
    close(fd);
    return 1;
}

int pipe_process(int count, char** arglist){
	int num_of_pipes = 1;
    // Create array to store the starting indices of each command
    int cmd_indices[MAX_PIPES + 1] = {0}; // First command starts at index 0
    int cmd_count = 1; // We already know where the first command starts

    // Find start of each command to send later to each child
    for (int i = 0; i < count; i++) {
        if (strcmp(arglist[i], SHELL_SIMBOLS[1]) == 0) {
            arglist[i] = NULL; // Replace pipe symbol with NULL to terminate command
            cmd_indices[cmd_count] = i + 1; // Next command starts after pipe
			cmd_count += 1;
			num_of_pipes += 1;
        }
    }
    
    // Create pipes for communication between processes. need (num_of_pipes - 1) pipes
    int pipes[num_of_pipes - 1][2];
    for (int i = 0; i < num_of_pipes - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            fprintf(stderr, "%s\n", FAIL_PIPE);
            // Close previos pipes - TODO needed? 
			// it's said we shouldn't bother exiting cleanly
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return 0;
        }
    }
    
    // Create processes for each command
    pid_t pids[num_of_pipes];
    for (int i = 0; i < num_of_pipes; i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            fprintf(stderr, "%s\n", FORK_FAIL);
            // Kill already created children - TODO needed? 
			// it's said we shouldn't bother exiting cleanly
            for (int j = 0; j < i; j++) {
                kill(pids[j], SIGTERM);
            }
            // Close all pipes
            for (int j = 0; j < num_of_pipes - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return 0;
		
        } else if (pids[i] == 0) { // CHILD
            signal_resetters();
            // Setup stdin from previous pipe (if its not first command)
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            // Setup stdout to next pipe (if itsnot last command)
            if (i < num_of_pipes - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            // Close all pipe file descriptors except the ones this child needs
            for (int j = 0; j < num_of_pipes - 1; j++) {
                if (i > 0 && j == i-1) { // Don't close the read end of previous pipe if not first command
                    close(pipes[j][1]);
                } else if (i < num_of_pipes - 1 && j == i) {  // Don't close the write end of next pipe if not last command
                    close(pipes[j][0]);
                } else { // Close both ends of all other pipes
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
            }
            // Finally do the thing
            if (execvp(arglist[cmd_indices[i]], &arglist[cmd_indices[i]]) == -1) {
                fprintf(stderr, "%s\n", FAIL);
                exit(1);
            }
        }
    }
    
    // PARENT - Close all pipes and wait for children
    for (int i = 0; i < num_of_pipes - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    // Wait for all children processes to complete
    for (int i = 0; i < num_of_pipes; i++) {
        if (!wait_for_child(pids[i])) {
            return 0; // If any child fails bye
        }
    }
    
    return 1;
}

int wait_for_child(pid_t pid){
	int status;
	if (waitpid(pid, &status, 0) == -1) {
		// ECHILD - "No child processes". can happen if the child was already waited for or if it was
		// automatically handled by the SIGCHLD handler.
		// EINTR - "Interrupted system call". common when signal handlers are installed.
		if (errno != ECHILD && errno != EINTR) { // errno tells you why a system call like waitpid() failed
			fprintf(stderr, "%s\n", FAIL);
			return 0;
		}
	} if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
		// Child exited with error
		return 0;
	}
	return 1;
}

void signal_resetters(){
	// We do this for the child processes that we wish would be terminated by SIGINT
	// Restore default SIGINT handling
	if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
		fprintf(stderr, "%s\n", FAIL_SIGINT);
		exit(1);
	}
	// Reset SIGCHLD handling for potential grandchildren
	if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
		fprintf(stderr, "%s\n", FAIL_SIGINT);
		exit(1);
	}
}

// This function returns 0 on success; any other return value indicates an error.
int prepare(void) {
	// SIGINT handler: wrote this func with help from Claude (AI)
	if (signal(SIGINT, SIG_IGN) == SIG_ERR) { // Ignore SIGINT to prevent shell termination on Ctrl+C
		fprintf(stderr, "%s\n", FAIL_SIGINT);
		return 1;
	}
	if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) { // Auto-cleanup zombies via SIGCHLD setting
		fprintf(stderr, "%s\n", FAIL_SIGINT);
		return 2;
	}
	return 0;
}

int finalize(void){
	// TODO
	return 0;
}