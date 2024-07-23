#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>

#define MAX_COMMAND_LENGTH 4096
#define MAX_ARGS 128
#define MAX_HISTORY_SIZE 256

// Keep track of variables
typedef struct {
    char *name;
    char *value;
} EnvVar;

EnvVar envvars[256];
int envcount = 0;

// Structure to store command and its timestamp
typedef struct {
    char *command;
    time_t timestamp;
    int num;
} Command;

// Function to parse input and break into command and arguments
void parseInput(char *input, char *command, char *args[]) {
    // Tokenize with a space
    char *token = strtok(input, " \n");
    int index = 0;

    // Extract command from the first token
    strcpy(command, token);

    // Extract arguments from remaining tokens
    while (token != NULL) {
        token = strtok(NULL, " \n");
        if (token != NULL) {
            args[index++] = strdup(token);
        }
    }
    args[index] = NULL;
}

// Move every element one index down in order to allow command in 0th index
// This is done for non-built in commands execvp()
void shift(char* arr[], int size) {
    for (int i = size-1; i > 0; i--) {
        arr[i] = arr[i-1];
    }
}

// If var exists then change it to its value before command
// Also done for non-built in command so ls $var works if $var=-al
void checkVar(char* arr[]) {
    int index = 0;
    while (arr[index] != NULL) {
        if (arr[index][0] == '$') {
            // if var exists then store its element in the array
            for (int i = 0; i < envcount; i++) {
                if (strcmp(envvars[i].name, arr[index]) == 0) {
                    arr[index] = envvars[i].value;
                }
            }
        }
        index++;
    }
}

// Function for the non-built commands
int executeCommand(char* command, char* args[]) {
    int a = -1;
    // Create a pipe so we can output to it
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("Pipe failed\n");
        return a;
    }

    // Create child process
    pid_t pid = fork();

    if (pid == -1) {
        // Fork error
        perror("Fork failed\n");
        return a;
    } 
    else if (pid == 0) {
        // Child process

        // Redirect stdout to the write end of the pipe
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("Dup2 failed");
            exit(EXIT_FAILURE);
        }

        // Close the unused ends of the pipe
        close(pipefd[0]);
        close(pipefd[1]);

        // Execute the command
        execvp(command, args);

        // Execvp error, this message will be displayed:
        printf("Missing keyword or command, or permission problem\n");
        exit(EXIT_FAILURE);
    } 
    else {
        // Parent process
        close(pipefd[1]);

        // Read and output the command results
        char buffer[1024];
        ssize_t bytesRead;
        while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            printf("%.*s", (int)bytesRead, buffer);
        }

        close(pipefd[0]);

        // Wait for the child process to finish
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            int exitStatus = WEXITSTATUS(status);
            if (exitStatus == EXIT_SUCCESS) {
                // The command is a success
                a = 0;
                return a;
            }
        } 
        else {
            // The command did not exist
            return -1;
        }
    }
    return a;
}

// Function to print 
int print(char *args[]) {
    int index = 0;
    char* arg;
    char *arr[128];
    int arrcount = 0;
    int checker = 0;

    // Print each argument
    while (args[index] != NULL) {
        arg = args[index];
        if (arg[0] == '$') {
            // If var then look for it and print its value
            for (int i = 0; i < envcount; i++) {
                if (strcmp(envvars[i].name, arg) == 0) {
                    // Store is temp array so we can output after
                    arr[arrcount++] = envvars[i].value;
                    checker = 1;
                }
            }
            if (checker == 0) {
                // The variable does not exist
                printf("Error: No Environment Variable %s found.\n", arg);
                return -1;
            }
            checker = 0;
        } 
        else {
            arr[arrcount++] = arg;
        }
        index++;
    }

    // Now we know every variable exists, we can print everything
    for (int i = 0; i < arrcount; i++) {
        printf("%s ", arr[i]);
    }

    printf("\n");
    return 0;
}

// Change colour of theme
int themeColour(char *args[]) {
    // Change to red, blue, or green
    if (strcmp(args[0], "red") == 0) {
        printf("\033[0;31m");
        return 1;
    }
    else if (strcmp(args[0], "blue") == 0) {
        printf("\033[0;34m");
        return 2;
    }
    else if (strcmp(args[0], "green") == 0) {
        printf("\033[0;32m");
        return 3;
    }
    // No other theme exists, let user know
    else {
        printf("unsupported theme\n");
        return -1;
    }
}

// Function to create new or update existing variables
int variableCommand(char *input) {
    int a = 0;
    int j;
    int count = strlen(input);

    // If there are any spaces around = or special characters in the variable name
    // then give an error and exit the function
    for (j = 1; j < count; j++) {
        if (input[j] == '=') {
            break;
        }
        if (isalpha(input[j]) == 0 && isdigit(input[j]) == 0 && input[j] != '_') {
            printf("Error: Please make sure the variable name only includes letters, numbers, or underscores.\n");
            a = -1;
            return a;
        }
    }
    if (input[j-1] == ' ' || input[j+1] == ' ') {
        printf("Error: Please get rid of the spaces around =.\n");
        a = -1;
        return a;
    }
    // Give error if variable has no name
    if (input[j-1] == '$') {
        printf("Error: Please have a variable name.\n");
        a = -1;
        return a;
    }

    // Split up the input based on = to get the variable and value
    char *token = strtok(input, "=");
    if (token != NULL) {
        char *name = strdup(token);
        token = strtok(NULL, "=");
        if (token != NULL) {
            char test[256];
            strcpy(test, token);
            token = strtok(test, " ");
            char *value = strdup(token);

            // Check if variable exists and if so update it
            for (int i = 0; i < envcount; i++) {
                if (strcmp(envvars[i].name, name) == 0) {
                    free(envvars[i].value);
                    envvars[i].value = value;
                    return a;
                }
            }

            // If variable does not exist, create it
            if (envcount < 256) {
                envvars[envcount].name = name;
                envvars[envcount].value = value;
                envcount++;
            }
            else {
                free(name);
                free(value);
                printf("No space remaining for new variables.\n");
                a = -1;
                return a;
            }
        } 
        else {
            printf("Please enter a value\n");
            a = -1;
            return a;
        }
    } 
    else {
        printf("Please enter a variable\n");
        a = -1;
        return a;
    }
    return a;
}

// Function to handle the log command
void logCommand(Command commandHistory[], int historySize) {
    // Iterate through the history log and output everything
    for (int i = 0; i < historySize; i++) {
        struct tm *localTime = localtime(&commandHistory[i].timestamp);
        printf("%s", asctime(localTime));
        printf(" %s %d\n", commandHistory[i].command, commandHistory[i].num);
    }
}

int main(int argc, char *argv[]) {
    // Variables to keep track of input, command and arguments
    char input[MAX_COMMAND_LENGTH];
    char command[MAX_COMMAND_LENGTH];
    char *args[MAX_ARGS];
    char temp[MAX_COMMAND_LENGTH];
    char temp2[MAX_COMMAND_LENGTH];
    // Keep track of what the theme colour currently is
    int theme_colour = 0;
    int temp_colour = 0;

    // Keep track of if the command was successful or not
    int success = 0;

    // 128 and 256 char limits for the variable name and value
    char name[128];
    char value[256];

    // Keep track of log history
    Command commandHistory[MAX_HISTORY_SIZE];
    int historySize = 0;

    // If there is a command line argument
    if (argc > 1) {
        int count = 0;
        int bufferlen = 255;
        char buffer[bufferlen];
        char c;

        // Open file
        FILE *fp2 = fopen(argv[1], "r");

        // Error and return 1 if the file cannot open
        if (fp2 == NULL) {
            printf("Unable to read script file: %s\n", argv[1]);
            return 1;
        }

        // Count how many lines are in the file
        // Assuming every line ends with a \n
        while (fgets(buffer, bufferlen, fp2) != NULL) {
            count++;
        }
        fclose(fp2);

        // Open new file in order to read
        FILE *fp = fopen(argv[1], "r");

        // Error if file cannot open
        // Should never get triggered because of the previous check
        if (fp == NULL) {
            printf("Unable to read script file: %s\n", argv[1]);
            return 1;
        }

        int linecount = 0;

        // Loop through and get each file line, and execute it
        while (fgets(input, MAX_COMMAND_LENGTH, fp) != NULL && linecount < count) {
            input[strcspn(input, "\n")] = '\0';

            // The file has to end on log
            if (linecount+1 == count) {
                strcpy(input, "log");
            }

            strcpy(temp, input);
            temp[strcspn(temp, "\n")] = '\0';
            strcpy(temp2, temp);

            if (strlen(temp) == 0) {
                printf("Error: Please enter a character.\n");
            }
            else {
                // Parse the input into a command and arguments
                parseInput(input, command, args);


                // Exit the loop, if the command is exit
                if (strcmp(command, "exit") == 0) {
                    break;
                }
                // Call the print function if the command is print 
                else if (strcmp(command, "print") == 0) {
                    success = print(args);
                } 
                // Call the log function if command is log
                else if (strcmp(command, "log") == 0) {
                    logCommand(commandHistory, historySize);
                } 
                // Call the theme function if the command is theme
                else if (strcmp(command, "theme") == 0) {
                    temp_colour = theme_colour;
                    theme_colour = themeColour(args);
                    if (theme_colour == -1) {
                        success = -1;
                        theme_colour = temp_colour;
                    }
                }
                // Call the variable function if 0th index is $ 
                else if (temp2[0] == '$') {
                    success = variableCommand(temp2);
                }
                // Else execute the non-built in commands
                else {
                    // Call shift in order to arguments one index over
                    shift(args, MAX_ARGS);
                    // Put command in the 0th index of the array
                    args[0] = command;
                    // Check if the var exists and put its value in the array
                    checkVar(args);
                    success = executeCommand(command, args);
                }

                // Store command in history, have space for 256 logs
                // 256 should be more than enough
                if (historySize < 256) {
                    time_t timestamp = time(NULL);
                    if (success == -1) {
                        commandHistory[historySize].command = strdup(temp);
                    } else {
                        commandHistory[historySize].command = strdup(command);
                    }
                    commandHistory[historySize].timestamp = timestamp;
                    commandHistory[historySize].num = success;
                    historySize++;
                }

                success = 0;
                linecount++;
            }
        }
        // Close file and print bye
        fclose(fp);
        printf("Bye!\n");
    } 
    // No command line argument
    else { 
        // Main loop
        while (1) {
            printf("cshell$ ");
            fflush(stdout);

            // Reset theme just for the input
            printf("\033[0m");
            // Read the user's input
            if (fgets(input, sizeof(input), stdin) == NULL) {
                printf("\n");
                break;
            } 
    

            // Change theme back to whatever colour user wanted it
            if (theme_colour == 1) {
                printf("\033[0;31m");
            }
            else if (theme_colour == 2) {
                printf("\033[0;34m");
            }
            else if ( theme_colour == 3) {
                printf("\033[0;32m");
            }
            
            strcpy(temp, input);
            temp[strcspn(temp, "\n")] = '\0';
            strcpy(temp2, temp);

            // Check if input is not empty
            if (strlen(temp) == 0) {
                printf("Error: Please enter a character.\n");
            }
            else {
                // Parse the input
                parseInput(input, command, args);

                // Print bye and exit loop
                if (strcmp(command, "exit") == 0) {
                    printf("Bye!\n");
                    break;
                } 
                // Call print function to print input
                else if (strcmp(command, "print") == 0) {
                    success = print(args);
                } 
                // Call log function to print the log
                else if (strcmp(command, "log") == 0) {
                    logCommand(commandHistory, historySize);
                } 
                // Call theme function to change theme colour
                else if (strcmp(command, "theme") == 0) {
                    temp_colour = theme_colour;
                    theme_colour = themeColour (args);
                    if (theme_colour == -1) {
                        success = -1;
                        theme_colour = temp_colour;
                    }
                } 
                // Call variable function to create/update variable
                else if (temp2[0] == '$') {
                    success = variableCommand(temp2);
                }
                // Else check if input is a non-built in command
                else {
                    // Put command in array and change variable name to value
                    shift(args, MAX_ARGS);
                    args[0] = command;
                    checkVar(args);
                    success = executeCommand(command, args);
                }

                // Store command in history
                if (historySize < 256) {
                    time_t timestamp = time(NULL);
                    if (success == -1) {
                        commandHistory[historySize].command = strdup(temp);
                    } 
                    else {
                        commandHistory[historySize].command = strdup(command);
                    }
                    commandHistory[historySize].timestamp = timestamp;
                    commandHistory[historySize].num = success;
                    historySize++;
                }
                success = 0;
            }

        }
    }

    // Reset theme back to default before exiting program
    printf("\033[0m");

    // Free dynamically allocated memory
    for (int i = 0; i < historySize; i++) {
        free(commandHistory[i].command);
    }

    for (int i = 0; i < envcount; i++) {
        free(envvars[i].name);
        free(envvars[i].value);
    }

    return 0;
}