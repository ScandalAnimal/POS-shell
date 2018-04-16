#define _POSIX_SOURCE

#define INPUT_SIZE 513
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// #include <sys/wait.h>
// #include <ctype.h>

// #include <sys/stat.h>
// #include <fcntl.h>

typedef struct t_routine_arg {
	bool active;
	char input[INPUT_SIZE];
 	pthread_mutex_t mutex;
 	pthread_cond_t mutex_condition;
} t_routine_arg;

typedef struct t_parsed_input {
	int argc;
	char *argv[];
} t_parsed_input;

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

		while (*input == ' ') {
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
			while (*input != '\0' && *input != ' ' && *input != '>' && *input != '<' && *input != '&') {
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

void *start_execute_routine(void *arg) {

	t_routine_arg *routine_arg = (t_routine_arg *)arg;	
	
	char token_buffer[1024];
	char *tokens[64] = {NULL};
	for (unsigned int i = 0; i < 1024; i++) {
		token_buffer[i] = '\0';
	}
	for (unsigned int i = 0; i < 64; i++) {
		tokens[i] = NULL;
	}

	while (routine_arg->active) {
	
		wait_for_signal(routine_arg);

		if (!routine_arg->active) {
			break;
		}

		parse_input(routine_arg->input, token_buffer, tokens);

		for (unsigned int i = 0; i < 64; i++) {
			printf("|:%s", tokens[i]);
		}
		// int pid = fork();

		// if (pid > 0) {
// 			if(!p.background)
// 			{
// 				pthread_mutex_lock(&(data->mutex));
// 				data->pid = fork_res; // save current pid, CTRL+c kill
// 				pthread_mutex_unlock(&(data->mutex));
// 				waitpid(fork_res, NULL, 0);
// 				pthread_mutex_lock(&(data->mutex));
// 				data->pid = 0;
// 				pthread_mutex_unlock(&(data->mutex));
// 			}
		// }
		// else if (pid == 0) {
			// execute_program(parsed_input);
		// }
		// else {
			// routine_arg->active = false;
			// break;
		// }

		send_signal(routine_arg);	
	}

	return NULL;
}


int main(void) {

	pthread_t tid[2];
	
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