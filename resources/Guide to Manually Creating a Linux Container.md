## Step 1
Guide to Manually Creating a Linux ContainerThis guide will walk you through the container.sh script, explaining each step in the process of creating a container manually. It builds upon the concepts you've explored in your procedure.md file.1. Isolating the Environment with 
`unshare --fork --pid --mount-proc --uts --ipc /bin/bash`

unshare:
- This is the core command we use to create a new process with its own set of namespaces, effectively isolating it from the host system.
- --fork: This tells unshare to create a new, detached process.
- --pid: Creates a new PID namespace. The first process inside this namespace will have PID 1.
- --mount-proc: This is crucial when creating a new PID namespace. It automatically mounts the /proc filesystem, which is necessary for many tools that read process information.
- --uts: Creates a new UTS namespace, which allows us to set a unique hostname for our container.
- --ipc: Creates a new IPC namespace for isolated inter-process communication./bin/bash: This is a "here document" that allows us to execute a series of commands within the new, isolated shell.

## Step 2
Setting a Hostname

`hostname container`

Inside the new UTS namespace, we can now set a new hostname without affecting the host's hostname.

## Step 3. 
Limiting Resources with cgroups (v2 Compatible)You're absolutely right to point out the difference with cgroup v2. The hierarchy rules are stricter. Here is the updated, v2-compatible approach:mkdir -p /sys/fs/cgroup/mycontainer

```bash
mkdir -p /sys/fs/cgroup/mycontainer
echo $$ > /sys/fs/cgroup/mycontainer/cgroup.procs
echo 100M > /sys/fs/cgroup/mycontainer/memory.max
```

- This command remains the same; we create a directory to represent our cgroup.
- `echo $$ > /sys/fs/cgroup/mycontainer/cgroup.procs`: We add the current process to the new cgroup.
- In v2, a cgroup that contains processes (like this one) cannot also have resource controllers enabled to distribute to children (the "no internal processes" rule).
- Since this is our final container cgroup, this is the correct approach.
- `echo 100M > /sys/fs/cgroup/mycontainer/memory.max`: In cgroup v2, the file to control memory limits is memory.max, not memory.limit_in_bytes.
- Important Prerequisite for cgroup v2: For the memory.max file to be available, the memory controller must be enabled for the cgroup's parent.
- You may need to run this command on your host before running the script:
    - This enables the memory controller for the root cgroup's children
    - `sudo echo "+memory" > /sys/fs/cgroup/cgroup`.subtree_control. This tells the kernel that child cgroups (like the mycontainer one we're about to create) are allowed to have memory controls.
    
## Step 4. 
Switching the Root Filesystem with pivot_rootThis is the most complex part of containerization. We want our container to have its own, separate root filesystem.

well I used [openwrt-general-rootfs-18.06.4-x86-64](https://downloads.openwrt.org/releases/18.06.4/targets/x86/64/openwrt-18.06.4-x86-64-generic-rootfs.tar.gz) image from OpenWRT.

```bash
tar -xzf ~/Downloads/openwrt-18.06.4-x86-64-generic-rootfs.tar.gz -C /opt/openwrt
```
then do following commands.
```bash
mkdir -p /opt/openwrt/old_root
mount --bind /opt/openwrt /opt/openwrt
pivot_root /opt/openwrt /opt/openwrt/old_root
mkdir -p /opt/openwrt/old_root
```

- pivot_root needs a place to put the old root filesystem. We create a directory inside our new root for this purpose.
- `mount --bind /opt/openwrt /opt/openwrt`: This is a prerequisite for pivot_root.
- It's a bit of a technical detail, but pivot_root requires the new root and the old root to not be on the same filesystem. 
- A bind mount creates a separate mount point that satisfies this requirement.`pivot_root /opt/openwrt /opt/openwrt/old_root`: This is the magic command. It swaps the root filesystem.
- The current root filesystem is moved to `/opt/openwrt/old_root` (which is now inside our new root), and `/opt/openwrt` becomes the new 
## Step 5. 
Mounting Essential Filesystems

```bash
mount -t proc proc /proc
mount -t devtmpfs devtmpfs /dev
```

After changing the root, we need to mount some essential filesystems that many Linux programs expect to be present./proc: The proc filesystem contains information about processes and is essential for tools like ps and top./dev: The devtmpfs filesystem provides device files like /dev/null, /dev/zero, and /dev/random.

## Step 6. 
Starting a Shell in the Container
`exec /bin/ash`
- The OpenWRT rootfs provides the ash shell. The exec command is important here. 
- It replaces the current process (/bin/bash from the unshare command) with /bin/ash.
- This means that when you exit from the ash shell, the container's main process terminates, and the container stops.How to Run the ScriptMake sure you have the OpenWRT root filesystem at /opt/openwrt.Enable memory controller (if needed): `sudo echo "+memory" > /sys/fs/cgroup/cgroup`.
- subtree_controlSave the script as `container.sh` .Make it executable: `chmod +x container.sh` Run it with sudo: `sudo ./container.sh` You will then be dropped into a shell inside your new container!

