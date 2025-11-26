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
void errorhandler(char *msg);

struct shared_state *state;

int main(int argc, char *argv[]){
    struct commands_s user_cmd;
    int s_mem_fd = open("/var/lib/donkey/sharedmem.mmap", O_RDONLY);
    if(s_mem_fd < 0){
        errorhandler("Shared memory failed\n");
        exit(EXIT_FAILURE);
    }
    else{
        state = mmap(NULL, sizeof(struct shared_state), PROT_READ, MAP_SHARED, s_mem_fd, 0);

        if(state == MAP_FAILED){
            errorhandler("Shared memory failed\n");
        }
        close(s_mem_fd);
    }
    if(argc < 1){
        fprintf(stderr, "Usage: %s <child-hostname> <rootfs_path> <oldroot_path>\n", argv[0]);
        exit(EXIT_SUCCESS);
    }
    else if(strcmp(argv[1], "rm") == 0){
        if(argc < 3) {
            fprintf(stderr, "Usage: %s <child-hostname> <rootfs_path> <oldroot_path>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
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
            errorhandler("Error listing crates");
        }
        while((entry = readdir(dir)) != NULL){
            if(entry->d_type == DT_DIR && strlen(entry->d_name) > 2) printf("%s\n",entry->d_name);
        }
        if(closedir(dir) == -1) printf("ERROR closing Dir");
        exit(EXIT_SUCCESS);
    }
    else if (commandresolver(argv[1]) == STOPD){
        memset(&user_cmd, 0, sizeof(user_cmd));
        user_cmd.cmd = commandresolver(argv[1]);
        if(sendcommands(&user_cmd) != 0) errorhandler("Failed to execute/send the command to daemon, Make sure the request is valid");
    }
    else{
        int namespace_fds[6];
        int namespaces[6] = {CLONE_NEWUTS, CLONE_NEWNS, CLONE_NEWPID, CLONE_NEWIPC, CLONE_NEWCGROUP, CLONE_NEWNET};
        char *namespaces_file[6] = {"uts", "mnt", "pid", "ipc", "cgroup", "net"};
        char namespace_path[128];
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
            for(int i = 0; i < 6; i++){
                sprintf(namespace_path, "/proc/%d/ns/%s", state->crates[crateidx].proc, namespaces_file[i]);
                namespace_fds[i] = open(namespace_path, O_RDONLY);
                if(namespace_fds[i] > -1){
                    setns(namespace_fds[i], namespaces[i]);
                }
            }
            pid_t child = fork();
            if( child == 0){
                char cmd[16];
                strcpy(cmd, argv[3]);
                char *token;
                char *delimit = "/";
                char execarg[16];
                if(access(argv[3], X_OK) == 0){
                    token = (strtok(argv[3], delimit));
                    while(token != NULL){
                        strcpy(execarg, token);
                        token = strtok(NULL, delimit);
                    }
                    execl(cmd, execarg, NULL);
                }
            }
            waitpid(child, 0, 0);
            for(int x = 0; x < 6; x++){
                close(namespace_fds[x]);
            }
            exit(EXIT_SUCCESS);
        }
        else{
            memset(&user_cmd, 0, sizeof(user_cmd));
            user_cmd.cmd = commandresolver(argv[1]);
            if(user_cmd.cmd == 1){
                strcpy(user_cmd.args.cratename,argv[3]);
                strcpy(user_cmd.args.imagename,argv[4]);
            }
            else{
                strcpy(user_cmd.args.cratename,argv[2]);
            }
            if(sendcommands(&user_cmd) != 0) errorhandler("Failed to execute the command, Make sure the request is valid");
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
    struct sockaddr_un unix_sock;
    unsigned char buffer[sizeof(data)];

    unix_sock.sun_family = AF_UNIX;
    strcpy(unix_sock.sun_path, "/var/lib/donkey/donkey.sock");

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd < 0) errorhandler("Failed to create socket");
    if(connect(fd, (struct sockaddr*)&unix_sock, sizeof(unix_sock)) < 0) errorhandler("Failed to connect to socket");
    if(data->cmd == STOPD) bufferstream(data, buffer, STOPD);
    else {
        size = data->cmd == CREATE ? bufferstream(data, buffer, CREATE) : bufferstream(data, buffer, 0);
    }
    int rc = write(fd, buffer, size);
    if(rc == -1) {
        close(fd);
        errorhandler("Failed to execute the command, Make sure the request is valid");
    }
    exit(EXIT_SUCCESS);
}

void errorhandler(char *msg){
    perror("Error Message:");
    exit(EXIT_FAILURE);
}