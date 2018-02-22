# xv6 File System

## Disk Layout

<pre>
  +------------+--------+--------+------+
  | superblock | inodes | bitmap | data |
  +------------+--------+--------+------+
</pre>

The superblock (12 bytes), inode (64 bytes), directory entry (16 bytes) are defined in [include/fs.h](../include/fs.h)

    struct superblock {
      uint size;         // Size of file system image (blocks)
      uint nblocks;      // Number of data blocks
      uint ninodes;      // Number of inodes.
    };

    struct dinode {
      short type;           // File type
      short major;          // Major device number (T_DEV only)
      short minor;          // Minor device number (T_DEV only)
      short nlink;          // Number of links to inode in file system
      uint size;            // Size of file (bytes)
      uint addrs[NDIRECT+1];   // Data block addresses
    };

    struct dirent {
      ushort inum;
      char name[DIRSIZ];
    };



## Make FS
[tools/mkfs.c](../tools/mkfs.c) is about how to creating a file system image.

    fsfd = open(argv[1], O_RDWR|O_CREAT|O_TRUNC, 0666);

Takes the second argument as the output of the fs (``fs.img``).

    mkfs(995, 200, 1024);

call ``int mkfs(int nblocks, int ninodes, int size)`` function with arguments ``nblocks=995, ninodes=200, size=1024``. The size means total number of blocks in the FS and nblocks is number of data blocks. The number of inode blocks can be computed by 200 / (512 B / 64 B) + 1 = 200 / 8 + 1 = 26. Thus the number of blocks needed is 1 (bitmap) + 26 + 995 + 1 (superblock) = 1023.

The superblock is located in block 1. Block 0 is reserved.

    memset(buf, 0, sizeof(buf));
    memmove(buf, &sb, sizeof(sb));
    wsect(1, buf);

After ``mkfs(995, 200, 1024)``, we have an ``fs.img`` looks like

<pre>
            superblock
    +------+-------------+------+------+------+------+------+
    | 0    |size:1024    | 0    | 0    | 0    | 0    | 0    |
    |      |nblocks:995  |      |      |      |      |      |
    |      |ninodes:200  |      |      |      |      |      |
    +------+-------------+------+------+------+------+------+
sec 0      1             2       ...   29       ...  1023
                                       freeblock

freeinode = 1, freeblock=29
</pre>

Then ``mkfs`` sets the root directory. The third argument is ``fs`` which contains the user programs (like cat.c, ls.c, sh.c) under ``user/``. Notice ``root_dir`` is not the real root director in the xv6. ``mkfs`` only copies programs from ``fs`` to the root directory of xv6.

    root_dir = opendir(argv[2]);
    root_inode = ialloc(T_DIR);
    assert(root_inode == ROOTINO);

After these instructions, the ``fs.img`` will be
<pre>
                                  +-- root inode
            superblock            v
    +------+-------------+------+------+-----------+
    | 0    |size:1024    |      |inode1|  0  |  0  |
    |      |nblocks:995  |   0  | 0    |  0  |  0  |
    |      |ninodes:200  |   0  | 0    |  0  |  0  |
    |      |             |   0  | 0    |  0  |  0  |
    +------+-------------+------+------+-----------+
sec 0      1             2             3            ...
</pre>

Next operation is ``r = add_dir(root_dir, root_inode, root_inode);``. It will copy the ``fs`` directory to ``fs.img``'s root directory recursively.
<pre>
                                  +-- root inode
            superblock            v                             inode1.addr[0]    
    +------+-------------+------+------+-----------+-----------+----------+-----------+
    | 0    |size:1024    |      |inode1|  0  |  0  |  ...      | (ls,...) |  ...      |
    |      |nblocks:995  |inode2|inode3|  0  |  0  |           | (sh,...) |           |
    |      |ninodes:200  |inode4|inode5|  0  |  0  |           | ...      |           |
    |      |             |inode6|inode7|  0  |  0  |           |          |           |
    +------+-------------+------+------+-----------+-----------+----------+-----------+
sec 0      1             2             3            ...        29         
</pre>


In the [Makefile](../Makefile), ``fs.img`` is forged, including all executable programs under the ``fs`` directory.

    USER_BINS := $(notdir $(USER_PROGS))
    fs.img: tools/mkfs fs/README $(addprefix fs/,$(USER_BINS))
      ./tools/mkfs fs.img fs

## File I/O system calls
We can find these system calls related to files [user/user.h](../user/user.h),

    int write(int, void*, int);
    int read(int, void*, int);
    int close(int);
    int kill(int);
    int exec(char*, char**);
    int open(char*, int);
    int mknod(char*, short, short);
    int unlink(char*);
    int fstat(int fd, struct stat*);
    int link(char*, char*);
    int mkdir(char*);
    int chdir(char*);
    int dup(int);

Operation on a file starts with ``open(path, option)``. The ``option`` can be any of the control options defined in [include/fcntl.h](../include/fcntl.h). The ``sys_open`` [kernel/sysfile.c](../kernel/sysfile.c) will either create a new inode or load the existing inode. It also allocate a ``struct file*`` and a file descriptor.

The first argument of write/read is a file descriptor. How does xv6 link the file descriptor with the fs? In ``sys_read``, 

    int
    sys_read(void)
    {
      struct file *f;
      int n;
      char *p;

      if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
        return -1;
      return fileread(f, p, n);
    }

The file descriptor is transferred to ``struct file *f`` [kernel/file.h](../kernel/file.h).

    struct file {
      enum { FD_NONE, FD_PIPE, FD_INODE } type;
      int ref; // reference count
      char readable;
      char writable;
      struct pipe *pipe;
      struct inode *ip;
      uint off;
    };

The file descriptor is, in fact, a index of an array in each process,
    struct proc {
      ...
      struct file *ofile[NOFILE];  // Open files
      ...
    }

The ``argfd(0, 0, &f)`` set ``f = ofile[fd]``. There is one field ``*ip`` inside the struct file ([kernel/file.h](../kernel/file.h)), 

    struct inode {
      uint dev;           // Device number
      uint inum;          // Inode number
      int ref;            // Reference count
      int flags;          // I_BUSY, I_VALID

      short type;         // copy of disk inode
      short major;
      short minor;
      short nlink;
      uint size;
      uint addrs[NDIRECT+1];
    };

Thus we have can track information about the file on ``fs.img``. Then ``int fileread(struct file *f, char *addr, int n)`` ([kernel/file.c](../kernel/file.c)) is called, which calls ``int readi(struct inode *ip, char *dst, uint off, uint n)`` ([kernel/fs.c](../kernel/fs.c)) to get the desired content from the fs image.
