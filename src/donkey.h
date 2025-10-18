#ifndef DONKEY_H
#define DONKEY_H

#include <sys/utsname.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define STACK_SIZE (8*1024*1024)
#define MAX_PATH_SIZE 128
#define IMAGE_PATH "/var/lib/donkey/image/"
#define CRATE_PATH "/var/lib/donkey/crate/"
#define CGROUP_PATH "/sys/fs/cgroup/"
#define NSDIR "/var/lib/donkey/namespaces/"

enum COMMAND {
    ERR = -1, IMAGE, CREATE, START, STOP, RUN, EXEC, RM, RMI, PS, STOPD,
};

enum STATUS {
    STATUS_ERR = -1, RUNNING, EXITED
};

struct rootfs_t{
    char source[MAX_PATH_SIZE];
    char target[MAX_PATH_SIZE];
    char overlaycat[1024];
};

struct donkey_s{
    enum COMMAND cmd;
    struct utsname uts;
    struct rootfs_t fs;
    char hostname[16];
    __pid_t pid;
};

struct commands_s {     // Sends over socket with user input
    enum COMMAND cmd;
    struct cmd_s {
        char cratename[16];
        char imagename[16];
    } args;
};

struct crate_state{        // list of currently running crates
    pid_t proc;
    char cratename[16];
    char imagename[16];
    enum STATUS st;
    time_t started_at;
};

struct shared_state{
    int cratecount;
    struct crate_state crates[100];
    struct commands_s cmds;
};

int bufferstream(struct commands_s *msg, unsigned char *buffer, int iflag);

#endif // DONKEY_H