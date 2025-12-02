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

static volatile int keep_running = 1;

struct shared_state *state;

static void donkeyshoe();
void errorhandler(char *usercmd);
void initstated();
int commandresolver(int argc, struct commands_s *cmd, struct donkey_s *crate_data);
int createcrate(struct commands_s *path, struct donkey_s* crate_data);
int imageextractor(char *filename);
static int cratespawner(struct donkey_s* crate);
void stopdaemon(int sig);

int main(int argc, char *argv[]){
    openlog("donkeyd", LOG_PID, LOG_DAEMON);

    donkeyshoe();       // Init the deamon
    initstated();       // Creates shared memory

    syslog(LOG_NOTICE, "Daemon started, PID: %d", getpid());

    int no_of_connections = 1;
    struct pollfd fds[6];
    struct sockaddr_un unix_sock;
    struct commands_s usercmd;
    struct donkey_s carry;
    
    fds[0].fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fds[0].fd == -1) errorhandler("Failed to create a socket");
    
    unix_sock.sun_family = AF_UNIX;
    strcpy(unix_sock.sun_path, "/var/lib/donkey/donkey.sock");
    
    unlink("/var/lib/donkey/donkey.sock");
    
    if(bind(fds[0].fd, (struct sockaddr*)&unix_sock, sizeof(unix_sock)) < 0) errorhandler("socket failed to bind");
    
    if(listen(fds[0].fd, 5) < 0) errorhandler("Socket failed to listen");
    fds[0].events = POLLIN;
    
    while(keep_running){
        int polled_fd = poll(fds, no_of_connections, -1);
        if(polled_fd < 1) continue;
        for(int i = 0; i < no_of_connections; i++){
            if(fds[i].revents & POLLIN){
                if(i != 0){
                    int tokencount = 0;
                    memset(&usercmd, 0, sizeof(usercmd));
                    int recvbytes = read(fds[i].fd, &usercmd.cmd, sizeof(enum COMMAND));
                    syslog(LOG_NOTICE, "CMD :%d", usercmd.cmd);
                    if(recvbytes < 1){ // Close client connection since no bytes read
                        if(close(fds[i].fd) == 0) syslog(LOG_NOTICE, "Client closed");
                        for(int j = i; j < no_of_connections - 1; j++){
                            fds[j] = fds[j+1];
                        }
                        no_of_connections--;
                        i--;
                        continue; // Done, skipping with continue to avoid listen for new client
                    }
                    syslog(LOG_NOTICE, "Message :%d", usercmd.cmd);
                    if(usercmd.cmd == CREATE){
                        recvbytes = read(fds[i].fd, &usercmd.args.cratename, 16);
                        usercmd.args.cratename[recvbytes] = '\0';
                        recvbytes = read(fds[i].fd, &usercmd.args.imagename, 16);
                        usercmd.args.imagename[recvbytes] = '\0';
                    }
                    else if(usercmd.cmd == STOPD){
                        syslog(LOG_NOTICE, "Shutting down daemon");
                    }
                    else{
                        recvbytes = read(fds[i].fd, usercmd.args.cratename, 16);
                        usercmd.args.cratename[recvbytes] = '\0';
                    }
                    syslog(LOG_NOTICE, "Cname :%s Iname :%s", usercmd.args.cratename, usercmd.args.imagename);
                    if(commandresolver(0, &usercmd, &carry) != 0){
                        if(usercmd.cmd == CREATE) syslog(LOG_NOTICE, "Commands failed");
                        //exit(EXIT_FAILURE);
                    }
                    continue;
                }
                polled_fd = accept(fds[i].fd, NULL, NULL);
                if(polled_fd > 0){
                    fds[no_of_connections].fd = polled_fd;
                    fds[no_of_connections].events = POLLIN;
                    no_of_connections++;
                    syslog(LOG_NOTICE, "Connected to client");
                }
            }
        }
    }
    close(fds[0].fd);
    exit(EXIT_SUCCESS);
}

void errorhandler(char *usercmd){
    perror("ERROR Message:");
    syslog(LOG_ERR, usercmd);
    exit(EXIT_FAILURE);
}

static void donkeyshoe(){
    __pid_t daemon = fork();

    if(daemon < 0) exit(EXIT_FAILURE);
    if(daemon > 0) exit(EXIT_SUCCESS);

    if(setsid() < 0) exit(EXIT_FAILURE);

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    daemon = fork();
    if(daemon == 0) syslog(LOG_NOTICE,"Daemon Process :%d\n", getpid());
    if(daemon < 0) exit(EXIT_FAILURE);
    if(daemon > 0) exit(EXIT_SUCCESS);

    umask(0);

    chdir("/");

    int x;
    for(x=sysconf(_SC_OPEN_MAX); x>=0;x--){
        close(x);
    }

}

void initstated(){
    int s_mem_fd = open("/var/lib/donkey/sharedmem.mmap", O_CREAT | O_RDWR, 0666);
    ftruncate(s_mem_fd, sizeof(struct shared_state));

    state = mmap(NULL, sizeof(struct shared_state), PROT_READ| PROT_WRITE, MAP_SHARED, s_mem_fd, 0);

    if(state == MAP_FAILED){
        syslog(LOG_ERR, "Failed to create shared memory");
        close(s_mem_fd);
        exit(EXIT_FAILURE);
    }

    close(s_mem_fd);
    state->cratecount = 0;
    syslog(LOG_NOTICE, "Shared memory initialized");
}

int commandresolver(int argc, struct commands_s *cmd, struct donkey_s *crate_data){
    syslog(LOG_NOTICE, "Solving a command :%d", cmd->cmd);
    switch (cmd->cmd){
        case CREATE:
            syslog(LOG_NOTICE, "Calling create crate function");
            if(createcrate(cmd, crate_data) == 0) return 0;
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
            struct stat crate_dir;
            char create_path[MAX_PATH_SIZE];
            char create_prop_path[MAX_PATH_SIZE];
            sprintf(create_path, "%s%s", CRATE_PATH, cmd->args.cratename);
        
            stat(create_path, &crate_dir);
            if(S_ISDIR(crate_dir.st_mode)){
                struct donkey_s* rdata = (struct donkey_s*)crate_data;
                sprintf(create_prop_path, "%s%s", create_path, "/props.txt");
                syslog(LOG_NOTICE, "Crate path @ %s, Properties file @ %s\n", create_path, create_prop_path);
                FILE* crate_prop_file = fopen(create_prop_path, "r");
                int polled_fd = fread(rdata, sizeof(struct donkey_s), 1, crate_prop_file);
                syslog(LOG_NOTICE, "Hostname : %s", rdata->hostname);
                if(polled_fd > 0){
                    syslog(LOG_NOTICE, "crate_prop_path :%s\n overlay :%s", rdata->fs.target, rdata->fs.overlaycat);
                    crate_data->pid = fork();
                    if(crate_data->pid == 0){
                        syslog(LOG_NOTICE, "Crate spawning called");
                        if(cratespawner(crate_data) != 0) errorhandler("Spawning crate failed");
                        syslog(LOG_NOTICE, "Crate Spawned");
                        exit(EXIT_SUCCESS);
                    }
                    signal(SIGCHLD, SIG_IGN);
                    //return 0;
                }
                fclose(crate_prop_file);
            }
            break;
        case RMI:
            break;
        case START:
            break;
        case STOP:
            int crateidx = 0;
            for(int i = 0; i < state->cratecount + 1; i++){
                if(strcmp(state->crates[i].cratename, cmd->args.cratename) == 0){
                    crateidx = i;
                    break;
                }
            }
            syslog(LOG_NOTICE, "Stopping Crate :%s", state->crates[crateidx].cratename);
            syslog(LOG_NOTICE, "Crate PID : %d", state->crates[crateidx].proc);
            if(kill(state->crates[crateidx].proc, SIGKILL) == 0) {  // This is not suppose to happen fix this
                char nsd[256];
                sprintf(nsd, "%sns/%s/pid", NSDIR, state->crates[crateidx].cratename);
                if(umount(nsd) < 0) syslog(LOG_ERR, "umount %s failed", nsd);
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

int createcrate(struct commands_s *path, struct donkey_s* crate_data){
    syslog(LOG_NOTICE, "Creating crate with given image");

    char upperdir[MAX_PATH_SIZE], crate_prop_path[MAX_PATH_SIZE];
    const char* upperdirs[3] = {"/diff", "/work", "/merged"};
    
    syslog(LOG_NOTICE, "Creating paths for crate");
    if(strlen(path->args.cratename) < 4) syslog(LOG_ERR, "Crate name should be more than 3 letters");
    sprintf(crate_data->fs.target, "%s%s", CRATE_PATH, path->args.cratename);
    syslog(LOG_NOTICE, "Creating path %s", crate_data->fs.target);
    sprintf(crate_prop_path, "%s%s/props.txt", CRATE_PATH, path->args.cratename);
    syslog(LOG_NOTICE, "Creating path %s", crate_prop_path);
    if(mkdir(crate_data->fs.target, 0777) != 0) syslog(LOG_ERR, "Creating crate failed");
    for(int i = 0; i < 3; i++){
        sprintf(upperdir, "%s%s", crate_data->fs.target, upperdirs[i]);
        syslog(LOG_NOTICE, "Creating path %s", upperdir);
        if(mkdir(upperdir, 0755) != 0) syslog(LOG_ERR, "Creating %s dir failed", upperdir);
    }
    if(path->args.imagename != NULL){
        sprintf(crate_data->fs.source, "%s%s", IMAGE_PATH, path->args.imagename);
        syslog(LOG_NOTICE, "Creating path %s", crate_data->fs.source);
        sprintf(crate_data->hostname, "%s", path->args.cratename);
        syslog(LOG_NOTICE, "Folder name %s", crate_data->hostname);
        sprintf(crate_data->fs.overlaycat, "lowerdir=%s,upperdir=%s/diff,workdir=%s/work", crate_data->fs.source, crate_data->fs.target, crate_data->fs.target);
        syslog(LOG_NOTICE, "source img path : %s crate path : %s", crate_data->fs.source, crate_data->fs.target);
        syslog(LOG_NOTICE, "overlay param :%s", crate_data->fs.overlaycat);
    }   
    strcpy(crate_data->fs.target, upperdir);
    FILE* crate_prop_file = fopen(crate_prop_path, "w+");   
    int polled_fd = fwrite(crate_data, sizeof(struct  donkey_s), 1, crate_prop_file);
    fclose(crate_prop_file);
    if(polled_fd == 0) return 0;
    syslog(LOG_NOTICE, "Crate created successfully");
    return 1;
}

static int cratespawner(struct donkey_s* crate){
    char nsdir[MAX_PATH_SIZE], nspath[MAX_PATH_SIZE], crate_prop_path[MAX_PATH_SIZE], cgroupcrate[MAX_PATH_SIZE];
    struct stat dir;
    int oldrootexists = 0;

    if(unshare(CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWCGROUP | CLONE_NEWNET) == 0){
        syslog(LOG_NOTICE, "Unshare success");
        sprintf(nsdir, "%sns/%s", NSDIR, crate->hostname);
        if(mkdir(nsdir, 0755) == -1) syslog(LOG_ERR, "ns mkdir failed");
        sprintf(nspath, "%s/pid", nsdir);
        int fd = open(nspath, O_CREAT | O_RDONLY, 0644);
        if(fd < 0){
            syslog(LOG_ERR, "create ns file failed: %s", strerror(errno));
        }
        close(fd);
        sprintf(crate_prop_path, "/proc/%d/ns/pid", getpid());
        if(mount(crate_prop_path, nspath, NULL, MS_BIND, NULL) == -1) syslog(LOG_ERR, "mounted failed");
        syslog(LOG_NOTICE, "mounted %s", crate_prop_path);
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
        umount(nspath);
        exit(0);
    }
    
    state->crates[idx].proc = init;
    return 0;
}

void stopdaemon(int sig){
    keep_running = 0;
}
