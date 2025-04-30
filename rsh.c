#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

#define N 13

extern char **environ;
char uName[20];

char *allowed[N] = {
    "cp", "touch", "mkdir", "ls", "pwd", "cat", "grep",
    "chmod", "diff", "cd", "exit", "help", "sendmsg"
};

struct message {
    char source[50];
    char target[50];
    char msg[200];
};

void terminate(int sig) {
    exit(0);  // Do not print here — only the listener prints "Exiting..."
}

void sendmsg(char *user, char *target, char *msg) {
    struct message m;
    strcpy(m.source, user);
    strcpy(m.target, target);
    strcpy(m.msg, msg);

    int fd = open("serverFIFO", O_WRONLY);
    if (fd < 0) {
        perror("Failed to open server FIFO");
        return;
    }

    write(fd, &m, sizeof(m));
    close(fd);
}

void *messageListener(void *arg) {
    while (1) {
        int fd = open(uName, O_RDONLY);
        if (fd < 0) {
            perror("Failed to open user FIFO");
            sleep(1);
            continue;
        }

        struct message m;
        int bytes = read(fd, &m, sizeof(m));
        if (bytes == 0) {
            // FIFO has been closed
            printf("Exiting....\n");
            break;
        }
        if (bytes > 0) {
            printf("Incoming message from [%s]: %s\n", m.source, m.msg);
        }

        close(fd);
    }
    pthread_exit(NULL);
}

int isAllowed(const char *cmd) {
    for (int i = 0; i < N; i++) {
        if (strcmp(cmd, allowed[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    pid_t pid;
    char line[256];
    int status;
    posix_spawnattr_t attr;
    pthread_t tid;

    if (argc != 2) {
        printf("Usage: ./rsh <username>\n");
        exit(1);
    }

    signal(SIGINT, terminate);
    strcpy(uName, argv[1]);

    // Start message listener thread
    if (pthread_create(&tid, NULL, messageListener, NULL) != 0) {
        perror("Failed to create listener thread");
        exit(1);
    }

    while (1) {
        fprintf(stderr, "rsh> ");
        if (fgets(line, 256, stdin) == NULL) {
            // End of input — let the listener continue
            break;
        }

        if (strcmp(line, "\n") == 0) continue;
        line[strcspn(line, "\n")] = 0;  // Strip newline

        char cmd[256], line2[256];
        strcpy(line2, line);
        strcpy(cmd, strtok(line, " "));

        if (!isAllowed(cmd)) {
            printf("NOT ALLOWED!\n");
            continue;
        }

        if (strcmp(cmd, "sendmsg") == 0) {
            char *target = strtok(NULL, " ");
            if (!target) {
                printf("sendmsg: you have to specify target user\n");
                continue;
            }

            char *messageStart = strstr(line2, target);
            if (messageStart) {
                messageStart += strlen(target);
                while (*messageStart == ' ') messageStart++;
            }

            if (!messageStart || strlen(messageStart) == 0) {
                printf("sendmsg: you have to enter a message\n");
                continue;
            }

            sendmsg(uName, target, messageStart);
            continue;
        }

        if (strcmp(cmd, "exit") == 0) break;

        if (strcmp(cmd, "cd") == 0) {
            char *targetDir = strtok(NULL, " ");
            if (strtok(NULL, " ") != NULL) {
                printf("-rsh: cd: too many arguments\n");
            } else {
                chdir(targetDir);
            }
            continue;
        }

        if (strcmp(cmd, "help") == 0) {
            printf("The allowed commands are:\n");
            for (int i = 0; i < N; i++) {
                printf("%d: %s\n", i + 1, allowed[i]);
            }
            continue;
        }

        // Setup command args
        char **cargv = malloc(sizeof(char *));
        cargv[0] = strdup(cmd);

        char *token = strtok(line2, " ");
        token = strtok(NULL, " ");
        int n = 1;
        while (token != NULL) {
            cargv = realloc(cargv, sizeof(char *) * (n + 1));
            cargv[n] = strdup(token);
            token = strtok(NULL, " ");
            n++;
        }
        cargv = realloc(cargv, sizeof(char *) * (n + 1));
        cargv[n] = NULL;

        posix_spawnattr_init(&attr);

        if (posix_spawnp(&pid, cmd, NULL, &attr, cargv, environ) != 0) {
            perror("spawn failed");
            exit(EXIT_FAILURE);
        }

        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            exit(EXIT_FAILURE);
        }

        posix_spawnattr_destroy(&attr);
        for (int i = 0; i < n; i++) free(cargv[i]);
        free(cargv);
    }

    // Wait for message listener thread to finish (on FIFO close)
    pthread_join(tid, NULL);

    return 0;
}
