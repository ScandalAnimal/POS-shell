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
// #include <ctype.h>

// #include <sys/stat.h>
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

void sig_int_handler() {

}

void sig_child_handler() {
	
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    	printf("# bg process %d done (parent %d)\n",pid, getpid());
        //unregister_child(pid, status);
    }
}


void *start_input_routine(void *arg) {

	t_routine_arg *routine_arg = (t_routine_arg *)arg;

	while (routine_arg->active) {
	
		printf("$ ");
		fflush(stdout);
		memset(routine_arg->input, 0, INPUT_SIZE);

		ssize_t input_size = read(0, routine_arg->input, INPUT_SIZE);

		routine_arg->input[INPUT_SIZE-1] = '\0';
		
		if (input_size == INPUT_SIZE) {
			fprintf(stderr, "Input is too long, maximal length is: %d\n", INPUT_SIZE-1);
			while (getc(stdin) != '\n');
			continue;
		}

		if(strcmp(routine_arg->input, "exit\n") == 0) {
			routine_arg->active = false;
			send_signal(routine_arg);
			break;
		}
		// else if(data->buff[0] == '\n')
			// continue;

		send_signal(routine_arg);	
		wait_for_signal(routine_arg);
	}

	return NULL;
}

void parse_input(char *input, char *token_buffer, char **tokens) {

	int counter = 0;
	int position = 0;
	int start = 0;

	while (*input != '\0') {

		while (*input == ' ' || *input == '\n') {
			input++;
		}
		start = position;
		if (*input == '>' || *input == '<' || *input == '&') {
			token_buffer[position] = *input;
			position++;
			token_buffer[position] = '\0';
			position++;
			tokens[counter] = &token_buffer[start];
			counter++;
			input++;
		}
		else {
			while (*input != '\0' && *input != ' ' && *input != '>' && *input != '<' && *input != '&' && *input != '\n') {
				token_buffer[position] = *input;
				position++;
				input++;
			}
			token_buffer[position] = '\0';
			position++;
			tokens[counter] = &token_buffer[start];
			counter++;
		}
	}
}

void execute_program(t_program_arg program_arg) {
	
	if (program_arg.background){
		sa_int.sa_handler = SIG_IGN;
		sigaction(SIGINT, &sa_int, NULL);
	}
	else {
		sa_int.sa_handler = sig_int_handler;
		sigaction(SIGINT, &sa_int, NULL);
	}

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
			// printf("Opening file: %s\n", program_arg.input_file);
			int file = open(program_arg.input_file, O_RDONLY);
			if (file == -1) {
				printf("file open error\n");
				exit(-1);
			}
			if ((dup2(file, fileno(stdin))) < 0) {
				printf("dup error\n");
				exit(-1);
			}
			close(file);
		}
		if (program_arg.is_output) {
			// printf("Opening file: %s\n", program_arg.output_file);
			int file = open(program_arg.output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
			if (file == -1) {
				printf("file open error\n");
				exit(-1);
			}
			if ((dup2(file, fileno(stdout))) < 0) {
				printf("dup error\n");
				exit(-1);
			}
			close(file);
		}
		// for (unsigned int i = 0; i < INPUT_SIZE; i++) {
			// printf("|:%s", program_arg.input[i]);
		// }
		if (execvp(program_arg.input[0], program_arg.input) < 0) {
			printf("execvp error\n");
		}
		pthread_exit(0);
	}

}

void *start_execute_routine(void *arg) {

	t_routine_arg *routine_arg = (t_routine_arg *)arg;	
	
	char token_buffer[2*INPUT_SIZE];
	char *tokens[INPUT_SIZE] = {NULL};
	for (unsigned int i = 0; i < (2*INPUT_SIZE); i++) {
		token_buffer[i] = '\0';
	}
	for (unsigned int i = 0; i < INPUT_SIZE; i++) {
		tokens[i] = NULL;
	}

	while (routine_arg->active) {
	
		wait_for_signal(routine_arg);

		if (!routine_arg->active) {
			break;
		}

		parse_input(routine_arg->input, token_buffer, tokens);

		// for (unsigned int i = 0; i < INPUT_SIZE; i++) {
			// printf("|:%s", tokens[i]);
		// }

		t_program_arg program_arg;
		program_arg.background = false;
		program_arg.is_input = false;
		program_arg.is_output = false;
		program_arg.counter = 0;
		memset(program_arg.input, 0, INPUT_SIZE);
		
		for (unsigned int i = 0; i < INPUT_SIZE && tokens[i] != NULL; i++) {
			if (strcmp(tokens[i], "<") == 0) {
				i++;
				if (tokens[i] == NULL) {
					return NULL; 
				}
				program_arg.input_file = tokens[i];
				program_arg.is_input = true;
			}
			else if (strcmp(tokens[i], ">" ) == 0) {
				i++;
				if (tokens[i] == NULL) { 
					return NULL; 
				}
				program_arg.output_file = tokens[i];
				program_arg.is_output = true;
			}
			else if (strcmp(tokens[i], "&") == 0) {
				program_arg.background = true;
			}
			else if (strcmp(tokens[i], "\0") != 0){
				program_arg.input[program_arg.counter] = tokens[i];
				program_arg.counter++;
			}
		}

		printf("^Input: %s@\n", program_arg.input_file);
		printf("^Output: %s@\n", program_arg.output_file);
		printf("^Background: %s@\n", program_arg.background ? "true" : "false");
		// for (unsigned int i = 0; i < INPUT_SIZE; i++) {
			// printf("|:%s", program_arg.input[i]);
		// }
		
		if (program_arg.input[0]) {
			execute_program(program_arg);
		}

		send_signal(routine_arg);	
	}

	return NULL;
}

int main(void) {

	pthread_t tid[2];

	memset(&sa_child, 0, sizeof(sa_child));
	sa_child.sa_handler = sig_child_handler;
	sigaction(SIGCHLD, &sa_child, NULL);
	
	memset(&sa_int, 0, sizeof(sa_int));
	sa_int.sa_handler = sig_int_handler;
	sigaction(SIGINT, &sa_int, NULL);
	
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