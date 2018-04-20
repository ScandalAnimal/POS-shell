#define _POSIX_SOURCE

#define INPUT_SIZE 513
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

typedef struct t_routine_arg {
	bool active;
	char input[INPUT_SIZE];
 	pthread_mutex_t mutex;
 	pthread_cond_t mutex_condition;
} t_routine_arg;

typedef struct t_program_arg {
	bool is_input;
	bool is_output;
	char *input_file;
	char *output_file;
	bool background;
	char *input[INPUT_SIZE];
	int counter;
} t_program_arg;

struct sigaction sa_int, sa_child;

void send_signal(t_routine_arg *arg) {
	pthread_mutex_lock(&(arg->mutex));
	pthread_cond_signal(&(arg->mutex_condition));
	pthread_mutex_unlock(&(arg->mutex));
}

void wait_for_signal(t_routine_arg *arg) {
	pthread_mutex_lock(&(arg->mutex));
	pthread_cond_wait(&(arg->mutex_condition), &(arg->mutex));
	pthread_mutex_unlock(&(arg->mutex));
}

void sig_child_handler() {
	
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    	printf("process %d finished in background.\n", pid);
    	printf("$ ");
		fflush(stdout);
		break;
    }
}


void *start_input_routine(void *arg) {

	t_routine_arg *routine_arg = (t_routine_arg *)arg;

	while (routine_arg->active) {
	
		printf("$ ");
		fflush(stdout);
		memset(routine_arg->input, 0, INPUT_SIZE);

		ssize_t size = read(0, routine_arg->input, INPUT_SIZE);

		routine_arg->input[INPUT_SIZE-1] = '\0';
		
		if (size == INPUT_SIZE) {
			fprintf(stderr, "Input is too long, maximal length is: %d\n", INPUT_SIZE - 1);
			while (getc(stdin) != '\n');
			continue;
		}

		if (strcmp(routine_arg->input, "exit\n") == 0) {
			routine_arg->active = false;
			send_signal(routine_arg);
			break;
		}

		send_signal(routine_arg);	
		wait_for_signal(routine_arg);
	}

	return NULL;
}

void parse_input(char *input, char *tmp_array, char **tokens) {

	int token_counter = 0;
	int position = 0;
	int tmp_index = 0;

	for (unsigned int i = 0; i < INPUT_SIZE; i++) {
		
		if (input[i] == '\0') {
			break;
		}
		if (input[i] == ' ' || input[i] == '\n') {
			continue;
		}

		tmp_index = position;
		if (input[i] == '>' || input[i] == '<' || input[i] == '&') {
			tmp_array[position] = input[i];
			tmp_array[position + 1] = '\0';
			position += 2;
			tokens[token_counter] = &tmp_array[tmp_index];
			token_counter++;
		}
		else {
			while (input[i] != '\0' && input[i] != ' ' && input[i] != '>' && input[i] != '<' && input[i] != '&' && input[i] != '\n') {
				tmp_array[position] = input[i];
				position++;
				i++;
			}
			tmp_array[position] = '\0';
			position++;
			tokens[token_counter] = &tmp_array[tmp_index];
			token_counter++;
		}
	}
}

void execute_program(t_program_arg program_arg) {

	pid_t pid = fork();
	if (pid < 0) {
		printf("fork error\n");
		exit(-1);
	}
	else if (pid > 0) {
		if (!program_arg.background) {
			int status;
			waitpid(pid, &status, 0);			
		}
	}
	else if (pid == 0) {
		if (program_arg.is_input) {
			int file = open(program_arg.input_file, O_RDONLY);
			if (file == -1) {
				printf("file open error\n");
				exit(-1);
			}
			if ((dup2(file, 0)) < 0) {
				printf("dup error\n");
				exit(-1);
			}
			close(file);
		}
		if (program_arg.is_output) {
			int file = open(program_arg.output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
			if (file == -1) {
				printf("file open error\n");
				exit(-1);
			}
			if ((dup2(file, 1)) < 0) {
				printf("dup error\n");
				exit(-1);
			}
			close(file);
		}
		if (execvp(program_arg.input[0], program_arg.input) < 0) {
			printf("execvp error\n");
		}
		pthread_exit(0);
	}

}

void set_signals(bool background) {

	if (background){
		sa_int.sa_handler = SIG_IGN;
		sigaction(SIGINT, &sa_int, NULL);
	}
	else {
		sa_int.sa_handler = SIG_DFL;
		sigaction(SIGINT, &sa_int, NULL);
	}
}

t_program_arg set_program_arg(char** tokens) {

	t_program_arg program_arg;

	program_arg.background = false;
	program_arg.is_input = false;
	program_arg.is_output = false;
	program_arg.counter = 0;
	memset(program_arg.input, 0, INPUT_SIZE);
	
	for (unsigned int i = 0; i < INPUT_SIZE; i++) {
		if (tokens[i] != NULL) {
			if (strcmp(tokens[i], "<") == 0) {
				i++;
				program_arg.input_file = tokens[i];
				program_arg.is_input = true;
			}
			else if (strcmp(tokens[i], ">" ) == 0) {
				i++;
				program_arg.output_file = tokens[i];
				program_arg.is_output = true;
			}
			else if (strcmp(tokens[i], "&") == 0) {
				program_arg.background = true;
			}
			else if (strcmp(tokens[i], "\0") != 0) {
				program_arg.input[program_arg.counter] = tokens[i];
				program_arg.counter++;
			}
		}
	}

	return program_arg;
}

void *start_execute_routine(void *arg) {

	t_routine_arg *routine_arg = (t_routine_arg *)arg;	
	
	char *tokens[INPUT_SIZE];
	for (unsigned int i = 0; i < INPUT_SIZE; i++) {
		tokens[i] = NULL;
	}
	char tmp[INPUT_SIZE];
	for (unsigned int i = 0; i < INPUT_SIZE; i++) {
		tmp[i] = '\0';
	}

	while (routine_arg->active) {
	
		wait_for_signal(routine_arg);

		if (!routine_arg->active) {
			break;
		}

		parse_input(routine_arg->input, tmp, tokens);

		t_program_arg program_arg = set_program_arg(tokens);
		if (program_arg.counter > 0) {
			set_signals(program_arg.background);
			execute_program(program_arg);
		}

		send_signal(routine_arg);	
	}

	return NULL;
}

int main(void) {

	pthread_t tid[2];

	memset(&sa_child, 0, sizeof(sa_child));
	memset(&sa_int, 0, sizeof(sa_int));

	sa_child.sa_handler = sig_child_handler;
	sigaction(SIGCHLD, &sa_child, NULL);
		
	t_routine_arg routine_arg;
	routine_arg.active = true;

	pthread_mutex_init(&(routine_arg.mutex), NULL);
	pthread_cond_init(&(routine_arg.mutex_condition), NULL);

	pthread_create(&(tid[0]), NULL, &start_input_routine, (void *) &routine_arg);
	pthread_create(&(tid[1]), NULL, &start_execute_routine, (void *) &routine_arg);

	pthread_join(tid[0], NULL);
	pthread_join(tid[1], NULL);

	pthread_mutex_destroy(&(routine_arg.mutex));
	pthread_cond_destroy(&(routine_arg.mutex_condition));

	return EXIT_SUCCESS;
}