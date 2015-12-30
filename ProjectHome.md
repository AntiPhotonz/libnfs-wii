A custom written NFS library, build from scratch.

It supports NFS both secure and insecure. Authentication systems are limited to None and Unix.

This code was created especially for the Wii.

Since the Wii doesn't have (and doesn't need) a full version of RPC, most libraries out there are not compatible with the Wii. Therefore, this version was created. It implements only the part of the RPC protocol which is required to make NFS work. Also, it implements most NFS methods out there, except EXPORT, READDIR and SYMLINK. READDIRPLUS is supported, so methods like opendir, readdir and closedir will work as expected.

It has full devoptab support, the only methods you need to use are nfsMount and nfsUnmount.