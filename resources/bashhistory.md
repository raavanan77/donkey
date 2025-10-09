[netns: CHILD]# history 
1  ./build/client.o 
2  ls net
3  ls net/*  
4  ls ./net  
5  echo $$
6  ls -l net 
7  ls -l /proc/8631/ns/net 
8  ip link
9  ping 127.0.0.1
10  ip addr
11  ip addr add dev lo local 127.0.0.1/8  
12  ip link set lo up
13  ping 127.0.0.1
14  export PS1="[netns: CHILD]# "  
15  ls /var/run/ 
16  echo $$
17  mount -o bind /proc/8631/ns/net /var/run/netns/child
18  ip netns list
19  ip link
20  ip addr add dev veth1 local 10.11.12.2/24
21  ip link set veth1 up
22  ip addr
23  ping 10.11.12.1
24  history
[netns: HOST]# history
2  export PS1="[netns: HOST]# "
3  ip link
4  ip netns list
5  touch /var/run/netns/child
6  ip netns list
7  cd /var/run/
8  ls
9  mkdir -p /var/run/netns
10  touch /var/run/netns/child
11  ip netns list
12  ip link add veth0 type veth peer name veth1
13  ip link | grep veth
14  ip link set verth1 netns chlid
15  ip link set verth1 netns child
16  ip link set veth1 netns child
17  ip link | grep veth
18  ip addr add dev veth0 local 10.
19  ip addr add dev veth0 local 10.11.12.1/24
20  ip link set veth0 up
21  ip addr
22  ping 10.11.12.2
23  ls /sys/fs/cgroup/ -lh
43  mkdir /sys/fs/cgroup/chlid
44  cd chlid/
45  ls
46  cat ./memory.*
47  echo 200M | sudo tee /sys/fs/cgroup/demo/memory.max
48  echo 200M | sudo tee ./memory.max
49  echo $$
50  echo $$ | tee ./cgroup.procs 
51  cat ./memory.current 
52  unshare -u
53  unshare -u
54  history 
