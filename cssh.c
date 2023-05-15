#define _POSIX_C_SOURCE 200809L // required for strdup() on cslab
#define _DEFAULT_SOURCE // required for strsep() on cslab
#define _BSD_SOURCE // required for strsep() on cslab

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define MAX_ARGS 32

char **get_next_command(size_t *num_args)
{
    // print the prompt
    printf("cssh$ ");

    // get the next line of input
    char *line = NULL;
    size_t len = 0;
    getline(&line, &len, stdin);
    if (ferror(stdin))
    {
        perror("getline");
        exit(1);
    }
    if (feof(stdin))
    {
        return NULL;
    }

    // turn the line into an array of words
    char **words = (char **)malloc(MAX_ARGS*sizeof(char *));
    int i=0;

    char *parse = line;
    while (parse != NULL)
    {
        char *word = strsep(&parse, " \t\r\f\n");
        if (strlen(word) != 0)
        {
            words[i++] = strdup(word);
        }
    }
    *num_args = i;
    for (; i<MAX_ARGS; ++i)
    {
        words[i] = NULL;
    }

    // all the words are in the array now, so free the original line
    free(line);

    return words;
}

void free_command(char **words)
{
    for (int i=0; i<MAX_ARGS; ++i)
    {
        if (words[i] == NULL)
        {
            break;
        }
        free(words[i]);
    }
    free(words);
}

int main()
{
    size_t num_args;

    // get the next command
    char **command_line_words = get_next_command(&num_args);
    while (command_line_words != NULL)
    {
        // run the command here
        // don't forget to skip blank commands
        // and add something to stop the loop if the user 
        // runs "exit"
		if (num_args == 0 || strcmp(command_line_words[0], "exit") == 0)
		{
			break;
		}
	
		if (num_args > 0)
		{
			int num_input_redirects = 0;
			int num_output_redirects = 0;
			for (int i = 1; i < num_args; i++)
			{
				if (strcmp(command_line_words[i], "<") == 0)
				{
					num_input_redirects++;
					if (num_input_redirects > 1)
					{
						fprintf(stderr, "Error! Can't have two <'s!\n");
						free_command(command_line_words);
						command_line_words = get_next_command(&num_args);
						continue;
					}
				}
				else if (strcmp(command_line_words[i], ">") == 0 || strcmp(command_line_words[i], ">>") == 0)
				{
					num_output_redirects++;
					if (num_output_redirects > 1)
					{
						fprintf(stderr, "Error! Can't have two >'s or >>'s!\n");
						free_command(command_line_words);
						command_line_words = get_next_command(&num_args);
						continue;
					}
				}
			}

			int input_fd = STDIN_FILENO;
			int output_fd = STDOUT_FILENO;
			for (int i = 1; i < num_args; i++)
			{
				if (strcmp(command_line_words[i], "<") == 0)
				{
					input_fd = open(command_line_words[i+1], O_RDONLY);
					if (input_fd == -1)
					{
						perror(command_line_words[i+1]);
						free_command(command_line_words);
						command_line_words = get_next_command(&num_args);
						continue;
					}
					free(command_line_words[i]);
					command_line_words[i] = NULL;
					free(command_line_words[i+1]);
					command_line_words[i+1] = NULL;
					i++;
				}
				else if (strcmp(command_line_words[i], ">") == 0)
				{
					output_fd = open(command_line_words[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
					if (output_fd == -1)
					{
						perror(command_line_words[i+1]);
						free_command(command_line_words);
						command_line_words = get_next_command(&num_args);
						continue;
					}
					free(command_line_words[i]);
					command_line_words[i] = NULL;
					free(command_line_words[i+1]);
					command_line_words[i+1] = NULL;
					i++;
				}
				else if (strcmp(command_line_words[i], ">>") == 0)
				{
					output_fd = open(command_line_words[i+1], O_WRONLY | O_CREAT | O_APPEND, 0666);
					if (output_fd == -1)
					{
						perror(command_line_words[i+1]);
						free_command(command_line_words);
						command_line_words = get_next_command(&num_args);
						continue;
					}
					free(command_line_words[i]);
					command_line_words[i] = NULL;
					free(command_line_words[i+1]);
					command_line_words[i+1] = NULL;
					i++;
				}
			}
			pid_t pid = fork();
			if (pid == -1)
			{	
				perror("fork");
			}
			else if (pid == 0)
			{
				if (input_fd != -1)
				{
					if (dup2(input_fd, STDIN_FILENO) == -1)
					{
						perror("dup2");
						exit(1);
					}
					close(input_fd);
				}
				if (output_fd != -1)
				{
					if (dup2(output_fd, STDOUT_FILENO) == -1)
					{
						perror("dup2");
						exit(1);
					}
				}
				
				execvp(command_line_words[0], command_line_words);
				perror(command_line_words[0]);
				exit(1);
			}
			else
			{
				int status;
				if (waitpid(pid, &status, 0) == -1)
				{
					perror("waitpid");
				}
			}
		}	        

        // free the memory for this command
        free_command(command_line_words);

        // get the next command
        command_line_words = get_next_command(&num_args);
    }

    // free the memory for the last command
    free_command(command_line_words);

    return 0;
}
