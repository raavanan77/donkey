#define _GNU_SOURCE
#include "donkey.h"
#include <dirent.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>

enum COMMAND commandresolver(char *arg);
int sendcommands(struct commands_s *data);

struct shared_state *state;

int main(int argc, char *argv[]){
    struct commands_s message;
    int fd = open("/var/lib/donkey/sharedmem.mmap", O_RDONLY);
    if(fd < 0){
        printf("Shared memory failed\n");
        exit(EXIT_FAILURE);
    }
    else{
        state = mmap(NULL, sizeof(struct shared_state), PROT_READ, MAP_SHARED, fd, 0);

        if(state == MAP_FAILED){
            printf("Shared memory failed\n");
            exit(EXIT_FAILURE);
        }
        close(fd);
    }
    if(argc < 1){
        fprintf(stderr, "Usage: %s <child-hostname> <rootfs_path> <oldroot_path>\n", argv[0]);
        exit(EXIT_SUCCESS);
    }
    else if(strcmp(argv[1], "rm") == 0){
        struct stat rmdir;
        char path[MAX_PATH_SIZE];
        for(int i = 2; i < argc - 1; i++){
            sprintf(path, "%s%s", CRATE_PATH, argv[i]);
            stat(path, &rmdir);
            if(S_ISDIR(rmdir.st_mode)){
                sprintf(path, "rm -rf %s%s", CRATE_PATH, argv[i]);
                if(system(path) == -1) printf("Unable to remove crate"); // Lazy work, I'll improve later
                else{
                    printf("crate %s removed\n", argv[i]);
                }
            }
        }
        exit(EXIT_SUCCESS);
    }
    else if(commandresolver(argv[1]) == PS){
        DIR *dir;
        struct dirent *entry;
        dir = opendir(CRATE_PATH);
        if(dir == NULL){
            printf("Error listing crates");
        }
        while((entry = readdir(dir)) != NULL){
            if(entry->d_type == DT_DIR && strlen(entry->d_name) > 2) printf("%s\n",entry->d_name);
        }
        if(closedir(dir) == -1) printf("ERROR closing Dir");
        exit(EXIT_SUCCESS);
    }
    else if (commandresolver(argv[1]) == STOPD){
        memset(&message, 0, sizeof(message));
        message.cmd = commandresolver(argv[1]);
        if(sendcommands(&message) != 0) printf("Command send failed\n");
    }
    else{
        int fds[5];
        int nss[5] = {CLONE_NEWUTS, CLONE_NEWNS, CLONE_NEWPID, CLONE_NEWIPC, CLONE_NEWCGROUP};
        char *ns[5] = {"uts", "mnt", "pid", "ipc", "cgroup"};
        char nspath[128];
        int rcrate = state->cratecount;
        if(rcrate > -1 && commandresolver(argv[1]) == EXEC){
            int yes = 1;
            int crateidx = 0;
            for(int i = 0; i < state->cratecount + 1; i++){
                if(strcmp(state->crates[i].cratename, argv[2]) == 0){
                    crateidx = i;
                    break;
                }
            }
            for(int i = 0; i < 5; i++){
                printf("/proc/%d/ns/%s\n", state->crates[crateidx].proc, ns[i]);
                sprintf(nspath, "/proc/%d/ns/%s", state->crates[crateidx].proc, ns[i]);
                fds[i] = open(nspath, O_RDONLY);
                if(fds[i] > -1){
                    setns(fds[i], nss[i]);
                }
            }
            pid_t child = fork();
            if( child == 0){
                printf("Forking\n");
                char cmd[16];
                strcpy(cmd, argv[3]);
                char *token;
                char *delimit = "/";
                char execarg[16];
                if(access(argv[3], X_OK) == 0){
                    token = (strtok(argv[3], delimit));
                    while(token != NULL){
                        strcpy(execarg, token);
                        printf("Arg :%s\n", execarg);
                        token = strtok(NULL, delimit);
                    }
                    execl(cmd, execarg, NULL);
                }
            }
            waitpid(child, 0, 0);
            for(int x = 0; x < 5; x++){
                close(fds[x]);
            }
            exit(EXIT_SUCCESS);
        }
        else{
            memset(&message, 0, sizeof(message));
            message.cmd = commandresolver(argv[1]);
            if(message.cmd == 1){
                strcpy(message.args.cratename,argv[3]);
                strcpy(message.args.imagename,argv[4]);
            }
            else{
                strcpy(message.args.cratename,argv[2]);
            }
            if(sendcommands(&message) != 0) printf("Command send failed\n");
        }
    }
    exit(EXIT_SUCCESS);
}

enum COMMAND commandresolver(char *arg){
    enum COMMAND cmd;
    if(strcmp(arg, "image") == 0) cmd = IMAGE;
    else if(strcmp(arg, "run") == 0) cmd = RUN;
    else if(strcmp(arg, "ps") == 0) cmd = PS;
    else if(strcmp(arg, "start") == 0) cmd = START;
    else if(strcmp(arg, "stop") == 0) cmd = STOP;
    else if(strcmp(arg, "stopd") == 0) cmd = STOPD;
    else if(strcmp(arg, "exec") == 0) cmd = EXEC;
    else if(strcmp(arg, "create") == 0) cmd = CREATE;
    else if(strcmp(arg, "rm") == 0) cmd = RM;
    else if(strcmp(arg, "rmi") == 0) cmd = RMI;
    else return ERR;
    return cmd;
}

int sendcommands(struct commands_s *data){
    int fd, size;
    struct sockaddr_un addr;
    unsigned char buffer[sizeof(data)];
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/var/lib/donkey/donkey.sock");

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) exit(EXIT_FAILURE);
    if(data->cmd == STOPD) bufferstream(data, buffer, STOPD);
    else {
        size = data->cmd == CREATE ? bufferstream(data, buffer, CREATE) : bufferstream(data, buffer, 0);
    }
    int rc = write(fd, buffer, size);
    if(rc == -1) {
        printf("write failed\n");
        close(fd);
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}