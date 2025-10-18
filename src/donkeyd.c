/*-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/
    Made with hope and hunger by Raavanan  /
    Drink water, stay hydrated             /
-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/*/

#define _GNU_SOURCE
#include "donkey.h"
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

static volatile int stop = 1;

struct shared_state *state;

static void donkeyshoe();
void initstated();
int commandresolver(int argc, struct commands_s *cmd, struct donkey_s *data);
int createcrate(struct commands_s *path, struct donkey_s* data);
int imageextractor(char *filename);
static int cratespawner(void *arg);
void stopdaemon(int sig);

int main(int argc, char *argv[]){
    donkeyshoe();
    initstated();

    openlog("donkeyd", LOG_PID, LOG_DAEMON);
    syslog(LOG_ERR, "Daemon started, PID: %d", getpid());
    
    char *commands[6];
    int ccount = 1;
    int fd;
    struct pollfd fds[6];
    struct sockaddr_un addr;
    struct commands_s msg;
    struct donkey_s carry;
    
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd == -1) syslog(LOG_ERR, "Socket failed");
    
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/var/lib/donkey/donkey.sock");
    
    unlink("/var/lib/donkey/donkey.sock");
    
    if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) syslog(LOG_ERR, "Bind failed");
    
    listen(fd, 5);
    
    fds[0].fd = fd;
    fds[0].events = POLLIN;
    
    while(stop){
        int rc = poll(fds, ccount, -1);
        if(rc < 1) continue;
        for(int i = 0; i < ccount; i++){
            if(fds[i].revents & POLLIN){
                if(i != 0){
                    int tokencount = 0;
                    memset(&msg, 0, sizeof(msg));
                    rc = read(fds[i].fd, &msg.cmd, sizeof(enum COMMAND));
                    syslog(LOG_NOTICE, "CMD :%d", msg.cmd);
                    if(rc < 1){ // Close client connection since no bytes read
                        if(close(fds[i].fd) == 0) syslog(LOG_NOTICE, "Client closed");
                        for(int j = i; j < ccount - 1; j++){
                            fds[j] = fds[j+1];
                        }
                        ccount--;
                        i--;
                        continue; // Done, skipping with continue to avoid listen for new client
                    }
                    syslog(LOG_NOTICE, "Message :%d", msg.cmd);
                    if(msg.cmd == CREATE){
                        rc = read(fds[i].fd, &msg.args.cratename, 16);
                        msg.args.cratename[rc] = '\0';
                        rc = read(fds[i].fd, &msg.args.imagename, 16);
                        msg.args.imagename[rc] = '\0';
                    }
                    else if(msg.cmd == STOPD){
                        syslog(LOG_NOTICE, "Shutting down daemon");
                    }
                    else{
                        rc = read(fds[i].fd, msg.args.cratename, 16);
                        msg.args.cratename[rc] = '\0';
                    }
                    syslog(LOG_NOTICE, "Cname :%s Iname :%s", msg.args.cratename, msg.args.imagename);
                    if(commandresolver(0, &msg, &carry) != 0){
                        if(msg.cmd == CREATE) syslog(LOG_NOTICE, "Commands failed");
                        //exit(EXIT_FAILURE);
                    }
                    else if(stop == 1 && msg.cmd == RUN){
                        carry.pid = fork();
                        if(carry.pid == 0){
                            syslog(LOG_NOTICE, "Crate spawning called");
                            cratespawner(&carry);
                            syslog(LOG_NOTICE, "Crate Spawned");
                            exit(EXIT_SUCCESS);
                        }
                        signal(SIGCHLD, SIG_IGN);
                    }
                    continue;
                }
                rc = accept(fds[i].fd, NULL, NULL);
                if(rc > 0){
                    fds[ccount].fd = rc;
                    fds[ccount].events = POLLIN;
                    ccount++;
                    syslog(LOG_NOTICE, "Connected to client");
                }
            }
        }
    }
    close(fds[0].fd);
    exit(EXIT_SUCCESS);
}

static void donkeyshoe(){
    __pid_t child = fork();

    if(child < 0) exit(EXIT_FAILURE);
    if(child > 0) exit(EXIT_SUCCESS);

    if(setsid() < 0) exit(EXIT_FAILURE);

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    child = fork();
    if(child == 0) syslog(LOG_NOTICE,"Daemon Process :%d\n", getpid());
    if(child < 0) exit(EXIT_FAILURE);
    if(child > 0) exit(EXIT_SUCCESS);

    umask(0);

    chdir("/");

    int x;
    for(x=sysconf(_SC_OPEN_MAX); x>=0;x--){
        close(x);
    }

}

void initstated(){
    int fd = open("/var/lib/donkey/sharedmem.mmap", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(struct shared_state));

    state = mmap(NULL, sizeof(struct shared_state), PROT_READ| PROT_WRITE, MAP_SHARED, fd, 0);

    if(state == MAP_FAILED){
        syslog(LOG_ERR, "State map failed");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
    state->cratecount = 0;
    syslog(LOG_NOTICE, "Shared mmap initialized");
}

int commandresolver(int argc, struct commands_s *cmd, struct donkey_s *data){
    syslog(LOG_NOTICE, "Solving a command :%d", cmd->cmd);
    switch (cmd->cmd){
        case CREATE:
            syslog(LOG_NOTICE, "Calling create crate function");
            if(createcrate(cmd, data) == 0) return 0;
            return 1;
        case IMAGE:
            if(imageextractor(cmd->args.imagename) == 0){
                return 0;
            }
            break;
        case PS:
            DIR *dir;
            struct dirent *entry;
            dir = opendir(CRATE_PATH);
            if(dir == NULL){
                syslog(LOG_ERR, "Error listing crates");
            }
            while((entry = readdir(dir)) != NULL){
                if(entry->d_type == DT_DIR) syslog(LOG_NOTICE,"%s\n",entry->d_name);
            }
            if(closedir(dir) == -1) syslog(LOG_ERR, "ERROR closing Dir");
            exit(EXIT_SUCCESS);
            break;
        case RUN:
            struct stat rdir;
            char pspath[MAX_PATH_SIZE];
            char props[MAX_PATH_SIZE];
            sprintf(pspath, "%s%s", CRATE_PATH, cmd->args.cratename);
        
            stat(pspath, &rdir);
            if(S_ISDIR(rdir.st_mode)){
                struct donkey_s* rdata = (struct donkey_s*)data;
                sprintf(props, "%s%s", pspath, "/props.txt");
                syslog(LOG_NOTICE, "Path %s props %s\n", pspath, props);
                FILE* fs = fopen(props, "r");
                int rc = fread(rdata, sizeof(struct donkey_s), 1, fs);
                syslog(LOG_NOTICE, "Hostname : %s", rdata->hostname);
                if(rc > 0){
                    syslog(LOG_NOTICE, "propspath :%s\n overlay :%s", rdata->fs.target, rdata->fs.overlaycat);
                    return 0;
                }
                fclose(fs);
            }
            break;
        case RMI:
            break;
        case START:
            break;
        case STOP:
            int crateidx = 0;
            for(int i = 0; i < state->cratecount + 1; i++){
                syslog(LOG_NOTICE, "Crate :%s", state->crates[i].cratename);
                if(strcmp(state->crates[i].cratename, cmd->args.cratename) == 0){
                    crateidx = i;
                    break;
                }
            }
            if(kill(state->crates[crateidx].proc, SIGKILL) == 0) {
                syslog(LOG_NOTICE, "Crate %s is shutdown", state->crates[crateidx].cratename);
                if(crateidx == 0 && state->cratecount == 0) memset(state, 0, sizeof(struct shared_state));
                else{
                    for(int j = crateidx; j < state->cratecount - 1; j++){
                        state->crates[j] = state->crates[j+1];
                    }
                    state->cratecount--;
                }
            }
            return 0;
            break;
        case STOPD:
            syslog(LOG_NOTICE, "Stopping daemon");
            stopdaemon(1);
            return 0;
        default:
            break;
    }
    return 1;
}

int imageextractor(char *filename){
    char path[256];
    char *file;
    strcpy(file, filename);
    const char *delimiter = "/";
    char *token;
    char *imagename;

    token = strtok(filename, delimiter);
    while(token != NULL){
        if(token[strlen(token) - 1] == 'z' && token[strlen(token) - 2] == 'g'){
            const char *deli = "-";
            imagename = strtok(token, deli);
            if(imagename != NULL){
                char imagepath[256];
                sprintf(imagepath, "%s%s", IMAGE_PATH, imagename);
                if(mkdir(imagepath, 777) != 0) syslog(LOG_ERR, "Unable to create image directory");
                break;
            }
        }
        token = strtok(NULL, delimiter);
    }
    sprintf(path, "tar -xzf %s -C %s%s", file, IMAGE_PATH, imagename);
    if(system(path) == 0){
        syslog(LOG_NOTICE, "Image %s extractored and added\n", imagename);
        return 0;
    }
    return 1;
}

int createcrate(struct commands_s *path, struct donkey_s* data){
    syslog(LOG_NOTICE, "Creating crate with given image");
    char upperdir[MAX_PATH_SIZE];
    char propspath[MAX_PATH_SIZE];
    const char* upperdirs[3] = {"/diff", "/work", "/merged"};
    syslog(LOG_NOTICE, "Creating paths for crate");
    if(strlen(path->args.cratename) < 4) syslog(LOG_ERR, "Crate name should be more than 3 letters");
    sprintf(data->fs.target, "%s%s", CRATE_PATH, path->args.cratename);
    syslog(LOG_NOTICE, "Creating path %s", data->fs.target);
    sprintf(propspath, "%s%s/props.txt", CRATE_PATH, path->args.cratename);
    syslog(LOG_NOTICE, "Creating path %s", propspath);
    if(mkdir(data->fs.target, 0777) != 0) syslog(LOG_ERR, "Creating crate failed");
    for(int i = 0; i < 3; i++){
        sprintf(upperdir, "%s%s", data->fs.target, upperdirs[i]);
        syslog(LOG_NOTICE, "Creating path %s", upperdir);
        if(mkdir(upperdir, 0755) != 0) syslog(LOG_ERR, "Creating %s dir failed", upperdir);
    }
    if(path->args.imagename != NULL){
        sprintf(data->fs.source, "%s%s", IMAGE_PATH, path->args.imagename);
        syslog(LOG_NOTICE, "Creating path %s", data->fs.source);
        sprintf(data->hostname, "%s", path->args.cratename);
        syslog(LOG_NOTICE, "Folder name %s", data->hostname);
        sprintf(data->fs.overlaycat, "lowerdir=%s,upperdir=%s/diff,workdir=%s/work", data->fs.source, data->fs.target, data->fs.target);
        syslog(LOG_NOTICE, "source img path : %s crate path : %s", data->fs.source, data->fs.target);
        syslog(LOG_NOTICE, "overlay param :%s", data->fs.overlaycat);
    }   
    strcpy(data->fs.target, upperdir);
    FILE *fs;
    fs = fopen(propspath, "w+");   
    int rc = fwrite(data, sizeof(struct  donkey_s), 1, fs);
    fclose(fs);
    if(rc == 0) return 0;
    syslog(LOG_NOTICE, "Crate created successfully");
    return 1;
}

static int cratespawner(void *arg){
    char nsdir[MAX_PATH_SIZE];
    char nspath[MAX_PATH_SIZE];
    char procpath[MAX_PATH_SIZE];
    struct donkey_s* crate = (struct donkey_s*)arg;
    struct stat dir;
    char cgroupcrate[MAX_PATH_SIZE];
    int oldrootexists = 0;

    if(unshare(CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWCGROUP) == 0){
        syslog(LOG_NOTICE, "Unshare success");
        sprintf(nsdir, "%sns/%s", NSDIR, crate->hostname);
        if(mkdir(nsdir, 0755) == -1) syslog(LOG_ERR, "ns mkdir failed");
        sprintf(nspath, "%s/pid", nsdir);
        int fd = open(nspath, O_CREAT | O_RDONLY, 0644);
        if(fd < 0){
            syslog(LOG_ERR, "create ns file failed: %s", strerror(errno));
        }
        close(fd);
        sprintf(procpath, "/proc/%d/ns/pid", getpid());
        if(mount(procpath, nspath, NULL, MS_BIND, NULL) == -1) syslog(LOG_ERR, "mounted failed");
        syslog(LOG_NOTICE, "mounted %s", procpath);
    }
    
    // Setting hostname from arg
    if(sethostname(crate->hostname, strlen(crate->hostname)) == -1){
        syslog(LOG_ERR, "sethostname");
    }
    
    // get hostname
    if(uname(&crate->uts) == -1){
        syslog(LOG_ERR, "uname");
    }
    
    // Mount Private the root '/'
    if(mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) syslog(LOG_ERR, "MS_PRIVATE failed");
    
    // Mount overlay
    if(mount("overlay", crate->fs.target, "overlay", 0, crate->fs.overlaycat) == -1) syslog(LOG_ERR, "Mount overlay on target %s with arg %s failed", crate->fs.target, crate->fs.overlaycat);
    // Mount --bind
    if(mount(crate->fs.target, crate->fs.target, NULL, MS_BIND , NULL) == -1) syslog(LOG_ERR, "Mount bind failed");
    
    // change dir to newroot
    if(chdir(crate->fs.target) == -1) syslog(LOG_ERR, "Can't chdir");
    
    stat("oldroot", &dir);
    if(S_ISDIR(dir.st_mode)) oldrootexists = 1;
    syslog(LOG_NOTICE, "OLD :%d\n", oldrootexists);
    
    // make dir for oldroot
    if(oldrootexists == 0){
        if(mkdir("oldroot", 777) == -1){
            syslog(LOG_ERR, "mkdir oldroot failed");
            exit(EXIT_FAILURE);
        }
    }
    
    // pivot_root Change new file system as root '/'
    if(syscall(SYS_pivot_root, ".", "oldroot") != 0) syslog(LOG_ERR, "Pivot syscall failed");
    
    // umount old root file system
    if(umount2("oldroot", MNT_DETACH) == -1) syslog(LOG_ERR, "Unmount of oldroot failed");
    
    // mount proc and dev
    if(mount("sysfs", "/sys", "sysfs", 0, NULL) == -1) syslog(LOG_ERR, "Mount sys");
    if(mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) == -1) syslog(LOG_ERR, "Mount dev");
    int idx = state->cratecount;
    strcpy(state->crates[idx].cratename, crate->hostname);
    strcpy(state->crates[idx].imagename, crate->fs.source);
    state->crates[idx].st = RUNNING;
    state->cratecount++;
    pid_t init = fork();
    if(init == 0){
        if(mount("proc", "/proc", "proc", 0, NULL) == -1) syslog(LOG_ERR, "Mount proc");
        syslog(LOG_NOTICE, "UTS.nodename inside crate : %s\n", crate->uts.nodename);
        syslog(LOG_NOTICE, "PID inside crate :%d", getpid());
        signal(SIGTERM, SIG_DFL);  // Allow termination
        signal(SIGINT, SIG_DFL);
        while(1) pause();
        exit(0);
    }
    
    state->crates[idx].proc = init;
    return 0;
}

void stopdaemon(int sig){
    stop = 0;
}
