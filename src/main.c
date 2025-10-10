/*-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/
    Made with hope and hunger by Raavanan  /
    Drink water, stay hydrated             /
-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/*/

#define _GNU_SOURCE
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <unistd.h>


#define STACK_SIZE (8*1024*1024)
#define MAX_PATH_SIZE 128
#define IMAGE_PATH "/var/lib/donkey/image/"
#define CRATE_PATH "/var/lib/donkey/crate/"
#define CGROUP_PATH "/sys/fs/cgroup/"

typedef char* string;

enum COMMAND {
    CREATE = 1, START, STOP, RUN, EXEC, RM, RMI,
};

typedef struct rootfs{
    char source[MAX_PATH_SIZE];
    char target[MAX_PATH_SIZE];
    char overlaycat[1024];
} rootfs;

typedef struct donkey_t{
    enum COMMAND cmd;
    struct utsname uts;
    struct rootfs fs;
    char hostname[16];
    string stack;
    string stacktop;
    __pid_t pid;
} donkey_t;

int commandresolver(int arg, string args[], void* data);
int createcrate(string args[], struct donkey_t* data);
int imageextractor(string filename);
static int childfunc(void *arg);

int main(int argc, string argv[]){
    struct donkey_t carry;
    if(argc < 1){
        fprintf(stderr, "Usage: %s <child-hostname> <rootfs_path> <oldroot_path>\n", argv[0]);
        exit(EXIT_SUCCESS);
    }
    else{
        if(commandresolver(argc, argv, &carry) != 0) exit(EXIT_FAILURE);
    }

    // Fork a child
    pid_t newspace = fork();
    
    if(newspace == 0){
        if(unshare(CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWCGROUP) != 0) err(EXIT_FAILURE, "unshare failed");
        
        carry.pid = fork();
        if(carry.pid == 0){
            childfunc(&carry);
            exit(EXIT_SUCCESS);
        }
        waitpid(carry.pid, NULL, 0);
        exit(EXIT_SUCCESS);
    }
    else if(carry.pid > 0){
        printf("clone returned %jd\n",(intmax_t)carry.pid);
        sleep(1);
        if(uname(&carry.uts) == -1) err(EXIT_FAILURE, "unmae");
        if(waitpid(newspace, NULL, 0) == -1) err(EXIT_FAILURE, "waitpid");
    }
    printf("child is terminated\n");
    exit(EXIT_SUCCESS);
}

int commandresolver(int argc, string args[], void* data){
    if(strcmp(args[1], "image") == 0){
        if(imageextractor(args[2]) == 0){
            return 0;
        }
    }
    else if(strcmp(args[1], "run") == 0){
        struct stat dir;
        char path[MAX_PATH_SIZE];
        char props[MAX_PATH_SIZE];
        sprintf(path, "%s%s", CRATE_PATH, args[2]);

        stat(path, &dir);
        if(S_ISDIR(dir.st_mode)){
            struct donkey_t* rdata = (struct donkey_t*)data;
            sprintf(props, "%s%s", path, "/props.txt");
            printf("Path %s props %s\n", path, props);
            FILE* fs = fopen(props, "r");
            int rc = fread(rdata, sizeof(struct donkey_t), 1, fs);
            printf("RC : %d\n", rc);
            if(rc > 0){
                printf("propspath :%s\n", rdata->fs.target);
                return 0;
            }
        }
        // call image extractor
    }
    else if(strcmp(args[1], "ps") == 0){
        DIR *dir;
        struct dirent *entry;
        dir = opendir(CRATE_PATH);
        if(dir == NULL){
            err(EXIT_FAILURE, "Error listing crates");
        }
        while((entry = readdir(dir)) != NULL){
            if(entry->d_type == DT_DIR) printf("%s\n",entry->d_name);
        }
        if(closedir(dir) == -1) err(EXIT_FAILURE, "ERROR closing Dir");
        exit(EXIT_SUCCESS);
    }
    else if(strcmp(args[1], "start") == 0){
        // call image extractor
    }
    else if(strcmp(args[1], "stop") == 0){
        // call image extractor
    }
    else if(strcmp(args[1], "create") == 0){
        if(createcrate(args, (struct donkey_t*)data) == 0) exit(EXIT_SUCCESS);
    }
    else if(strcmp(args[1], "rm") == 0){
        struct stat dir;
        char path[MAX_PATH_SIZE];
        for(int i = 2; i < argc; i++){
            sprintf(path, "%s%s", CRATE_PATH, args[i]);
            stat(path, &dir);
            if(S_ISDIR(dir.st_mode)){
                sprintf(path, "rm -rf %s%s", CRATE_PATH, args[i]);
                if(system(path) == -1) err(EXIT_FAILURE, "Unable to remove crate"); // Lazy work, I'll improve later
                else{
                    printf("crate %s removed\n", args[i]);
                }
            }
        }
        exit(EXIT_SUCCESS);
    }
    // else if(strcmp(args[1], "rmi") == 0){
    //     // call image extractor
    // }
    else{
        return 1;
    }
    return 1;
}

int imageextractor(string filename){
    char path[256];
    string file;
    strcpy(file, filename);
    const string delimiter = "/";
    string token;
    string imagename;

    token = strtok(filename, delimiter);
    while(token != NULL){
        if(token[strlen(token) - 1] == 'z' && token[strlen(token) - 2] == 'g'){
            const string deli = "-";
            imagename = strtok(token, deli);
            if(imagename != NULL){
                char imagepath[256];
                sprintf(imagepath, "%s%s", IMAGE_PATH, imagename);
                if(mkdir(imagepath, 777) != 0) err(EXIT_FAILURE, "Unable to create image directory");
                break;
            }
        }
        token = strtok(NULL, delimiter);
    }
    sprintf(path, "tar -xzf %s -C %s%s", file, IMAGE_PATH, imagename);
    if(system(path) != 0){
        printf("Image %s extractored and added\n", imagename);
        return 1;
    }
    return 0;
}

int createcrate(string args[], struct donkey_t* data){
    char upperdir[MAX_PATH_SIZE];
    char propspath[MAX_PATH_SIZE];
    const char* upperdirs[3] = {"/diff", "/work", "/merged"};
    if(strcmp(args[2], "-name") == 0){
        if(strlen(args[3]) < 4) err(EXIT_FAILURE, "Crate name should be more than 3 letters");
        sprintf(data->fs.target, "%s%s", CRATE_PATH, args[3]);
        sprintf(propspath, "%s%s/props.txt", CRATE_PATH, args[3]);
        if(mkdir(data->fs.target, 0777) != 0) err(EXIT_FAILURE, "Creating crate failed");
        for(int i = 0; i < 3; i++){
            sprintf(upperdir, "%s%s", data->fs.target, upperdirs[i]);
            if(mkdir(upperdir, 0755) != 0) err(EXIT_FAILURE, "Creating %s dir failed", upperdir);
        }
        if(args[4] != NULL){
            sprintf(data->fs.source, "%s%s", IMAGE_PATH, args[4]);
            sprintf(data->hostname, "%s", args[4]);
            sprintf(data->fs.overlaycat, "lowerdir=%s,upperdir=%s/diff,workdir=%s/work", data->fs.source, data->fs.target, data->fs.target);
            printf("source img path : %s crate path : %s\n", data->fs.source, data->fs.target);
        }   
        strcpy(data->fs.target, upperdir);
        FILE *fs;
        fs = fopen(propspath, "w+");   
        int rc = fwrite(data, sizeof(struct  donkey_t), 1, fs);
        fclose(fs);
        if(rc == 0) return 1;
    }
    return 0;
}

static int childfunc(void *arg){
    struct donkey_t* crate = (struct donkey_t*)arg;
    struct stat dir;
    char cgroupcrate[MAX_PATH_SIZE];
    int oldrootexists = 0;
    
    // Setting hostname from arg
    if(sethostname(crate->hostname, strlen(crate->hostname)) == -1){
        err(EXIT_FAILURE, "sethostname");
    }
    
    // get hostname
    if(uname(&crate->uts) == -1){
        err(EXIT_FAILURE, "uname");
    }
    
    // Mount Private the root '/'
    if(mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) err(EXIT_FAILURE, "MS_PRIVATE failed");
    
    // Mount overlay
    if(mount("overlay", crate->fs.target, "overlay", 0, crate->fs.overlaycat) == -1) err(EXIT_FAILURE, "Mount overlay on target %s with arg %s failed", crate->fs.target, crate->fs.overlaycat);
    // Mount --bind
    if(mount(crate->fs.target, crate->fs.target, NULL, MS_BIND , NULL) == -1) err(EXIT_FAILURE, "Mount bind failed");
    
    // change dir to newroot
    if(chdir(crate->fs.target) == -1) err(EXIT_FAILURE, "Can't chdir");
    
    stat("oldroot", &dir);
    if(S_ISDIR(dir.st_mode)) oldrootexists = 1;
    printf("OLD :%d\n", oldrootexists);
    
    // make dir for oldroot
    if(oldrootexists == 0){
        if(mkdir("oldroot", 777) == -1) err(EXIT_FAILURE, "mkdir oldroot failed");
    }
    
    // pivot_root Change new file system as root '/'
    if(syscall(SYS_pivot_root, ".", "oldroot") != 0) err(EXIT_FAILURE, "Pivot syscall failed");
    
    // umount old root file system
    if(umount2("oldroot", MNT_DETACH) == -1) err(EXIT_FAILURE, "Unmount of oldroot failed");
    
    // mount proc and dev
    if(mount("proc", "/proc", "proc", 0, NULL) == -1) err(EXIT_FAILURE, "Mount proc");
    if(mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) == -1) err(EXIT_FAILURE, "Mount dev");
    if(mount("sysfs", "/sys", "sysfs", 0, NULL) == -1) err(EXIT_FAILURE, "Mount sys");
    
    
    printf("UTS.nodename in chlid : %s\n", crate->uts.nodename);

    // get into shell
    if(execl("/bin/bash", "bash", NULL) > 0) printf("Bash not found trying other shells");
    else if(execl("/bin/ash", "ash", NULL) > 0) printf("Ash not found trying other shells");

    return 0;
}
