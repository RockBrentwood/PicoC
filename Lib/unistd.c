// unistd.h library for large systems:
// Small embedded systems use Lib.c instead.
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include "../Extern.h"

#ifndef BUILTIN_MINI_STDLIB

static int ZeroValue = 0;

void UnistdAccess(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = access(Param[0]->Val->Pointer, Param[1]->Val->Integer);
}

void UnistdAlarm(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = alarm(Param[0]->Val->Integer);
}

void UnistdChdir(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = chdir(Param[0]->Val->Pointer);
}

void UnistdChroot(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = chroot(Param[0]->Val->Pointer);
}

void UnistdChown(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = chown(Param[0]->Val->Pointer, Param[1]->Val->Integer, Param[2]->Val->Integer);
}

void UnistdClose(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = close(Param[0]->Val->Integer);
}

void UnistdConfstr(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = confstr(Param[0]->Val->Integer, Param[1]->Val->Pointer, Param[2]->Val->Integer);
}

void UnistdCtermid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = ctermid(Param[0]->Val->Pointer);
}

#if 0
void UnistdCuserid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = cuserid(Param[0]->Val->Pointer);
}
#endif

void UnistdDup(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = dup(Param[0]->Val->Integer);
}

void UnistdDup2(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = dup2(Param[0]->Val->Integer, Param[1]->Val->Integer);
}

void Unistd_Exit(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   _exit(Param[0]->Val->Integer);
}

void UnistdFchown(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = fchown(Param[0]->Val->Integer, Param[1]->Val->Integer, Param[2]->Val->Integer);
}

void UnistdFchdir(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = fchdir(Param[0]->Val->Integer);
}

void UnistdFdatasync(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
#ifndef F_FULLSYNC
   ReturnValue->Val->Integer = fdatasync(Param[0]->Val->Integer);
#else // Mac OS X equivalent.
   ReturnValue->Val->Integer = fcntl(Param[0]->Val->Integer, F_FULLFSYNC);
#endif
}

void UnistdFork(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = fork();
}

void UnistdFpathconf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = fpathconf(Param[0]->Val->Integer, Param[1]->Val->Integer);
}

void UnistdFsync(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = fsync(Param[0]->Val->Integer);
}

void UnistdFtruncate(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = ftruncate(Param[0]->Val->Integer, Param[1]->Val->Integer);
}

void UnistdGetcwd(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = getcwd(Param[0]->Val->Pointer, Param[1]->Val->Integer);
}

void UnistdGetdtablesize(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = getdtablesize();
}

void UnistdGetegid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = getegid();
}

void UnistdGeteuid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = geteuid();
}

void UnistdGetgid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = getgid();
}

void UnistdGethostid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = gethostid();
}

void UnistdGetlogin(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = getlogin();
}

void UnistdGetlogin_r(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = getlogin_r(Param[0]->Val->Pointer, Param[1]->Val->Integer);
}

void UnistdGetpagesize(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = getpagesize();
}

void UnistdGetpass(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = getpass(Param[0]->Val->Pointer);
}

#if 0
void UnistdGetpgid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = getpgid(Param[0]->Val->Integer);
}
#endif

void UnistdGetpgrp(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = getpgrp();
}

void UnistdGetpid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = getpid();
}

void UnistdGetppid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = getppid();
}

#if 0
void UnistdGetsid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = getsid(Param[0]->Val->Integer);
}
#endif

void UnistdGetuid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = getuid();
}

void UnistdGetwd(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = getcwd(Param[0]->Val->Pointer, PATH_MAX);
}

void UnistdIsatty(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = isatty(Param[0]->Val->Integer);
}

void UnistdLchown(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = lchown(Param[0]->Val->Pointer, Param[1]->Val->Integer, Param[2]->Val->Integer);
}

void UnistdLink(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = link(Param[0]->Val->Pointer, Param[1]->Val->Pointer);
}

void UnistdLockf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = lockf(Param[0]->Val->Integer, Param[1]->Val->Integer, Param[2]->Val->Integer);
}

void UnistdLseek(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = lseek(Param[0]->Val->Integer, Param[1]->Val->Integer, Param[2]->Val->Integer);
}

void UnistdNice(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = nice(Param[0]->Val->Integer);
}

void UnistdPathconf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = pathconf(Param[0]->Val->Pointer, Param[1]->Val->Integer);
}

void UnistdPause(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = pause();
}

#if 0
void UnistdPread(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = pread(Param[0]->Val->Integer, Param[1]->Val->Pointer, Param[2]->Val->Integer, Param[3]->Val->Integer);
}

void UnistdPwrite(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = pwrite(Param[0]->Val->Integer, Param[1]->Val->Pointer, Param[2]->Val->Integer, Param[3]->Val->Integer);
}
#endif

void UnistdRead(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = read(Param[0]->Val->Integer, Param[1]->Val->Pointer, Param[2]->Val->Integer);
}

void UnistdReadlink(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = readlink(Param[0]->Val->Pointer, Param[1]->Val->Pointer, Param[2]->Val->Integer);
}

void UnistdRmdir(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = rmdir(Param[0]->Val->Pointer);
}

void UnistdSbrk(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = sbrk(Param[0]->Val->Integer);
}

void UnistdSetgid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = setgid(Param[0]->Val->Integer);
}

void UnistdSetpgid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = setpgid(Param[0]->Val->Integer, Param[1]->Val->Integer);
}

void UnistdSetpgrp(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = setpgrp();
}

void UnistdSetregid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = setregid(Param[0]->Val->Integer, Param[1]->Val->Integer);
}

void UnistdSetreuid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = setreuid(Param[0]->Val->Integer, Param[1]->Val->Integer);
}

void UnistdSetsid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = setsid();
}

void UnistdSetuid(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = setuid(Param[0]->Val->Integer);
}

void UnistdSleep(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = sleep(Param[0]->Val->Integer);
}

#if 0
void UnistdSwab(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = swab(Param[0]->Val->Pointer, Param[1]->Val->Pointer, Param[2]->Val->Integer);
}
#endif

void UnistdSymlink(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = symlink(Param[0]->Val->Pointer, Param[1]->Val->Pointer);
}

void UnistdSync(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   sync();
}

void UnistdSysconf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = sysconf(Param[0]->Val->Integer);
}

void UnistdTcgetpgrp(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = tcgetpgrp(Param[0]->Val->Integer);
}

void UnistdTcsetpgrp(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = tcsetpgrp(Param[0]->Val->Integer, Param[1]->Val->Integer);
}

void UnistdTruncate(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = truncate(Param[0]->Val->Pointer, Param[1]->Val->Integer);
}

void UnistdTtyname(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = ttyname(Param[0]->Val->Integer);
}

void UnistdTtyname_r(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = ttyname_r(Param[0]->Val->Integer, Param[1]->Val->Pointer, Param[2]->Val->Integer);
}

void UnistdUalarm(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = ualarm(Param[0]->Val->Integer, Param[1]->Val->Integer);
}

void UnistdUnlink(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = unlink(Param[0]->Val->Pointer);
}

void UnistdUsleep(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = usleep(Param[0]->Val->Integer);
}

void UnistdVfork(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = vfork();
}

void UnistdWrite(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = write(Param[0]->Val->Integer, Param[1]->Val->Pointer, Param[2]->Val->Integer);
}

// Handy structure definitions.
const char UnistdDefs[] =
   "typedef int uid_t; "
   "typedef int gid_t; "
   "typedef int pid_t; "
   "typedef int off_t; "
   "typedef int size_t; "
   "typedef int ssize_t; "
   "typedef int useconds_t;"
   "typedef int intptr_t;";

// All unistd.h functions.
struct LibraryFunction UnistdFunctions[] = {
   { UnistdAccess, "int access(char *, int);" },
   { UnistdAlarm, "unsigned int alarm(unsigned int);" },
#if 0
   { UnistdBrk, "int brk(void *);" },
#endif
   { UnistdChdir, "int chdir(char *);" },
   { UnistdChroot, "int chroot(char *);" },
   { UnistdChown, "int chown(char *, uid_t, gid_t);" },
   { UnistdClose, "int close(int);" },
   { UnistdConfstr, "size_t confstr(int, char *, size_t);" },
   { UnistdCtermid, "char *ctermid(char *);" },
#if 0
   { UnistdCuserid, "char *cuserid(char *);" },
#endif
   { UnistdDup, "int dup(int);" },
   { UnistdDup2, "int dup2(int, int);" },
#if 0
   { UnistdEncrypt, "void encrypt(char[64], int);" },
   { UnistdExecl, "int execl(char *, char *, ...);" },
   { UnistdExecle, "int execle(char *, char *, ...);" },
   { UnistdExeclp, "int execlp(char *, char *, ...);" },
   { UnistdExecv, "int execv(char *, char *[]);" },
   { UnistdExecve, "int execve(char *, char *[], char *[]);" },
   { UnistdExecvp, "int execvp(char *, char *[]);" },
#endif
   { Unistd_Exit, "void _exit(int);" },
   { UnistdFchown, "int fchown(int, uid_t, gid_t);" },
   { UnistdFchdir, "int fchdir(int);" },
   { UnistdFdatasync, "int fdatasync(int);" },
   { UnistdFork, "pid_t fork(void);" },
   { UnistdFpathconf, "long fpathconf(int, int);" },
   { UnistdFsync, "int fsync(int);" },
   { UnistdFtruncate, "int ftruncate(int, off_t);" },
   { UnistdGetcwd, "char *getcwd(char *, size_t);" },
   { UnistdGetdtablesize, "int getdtablesize(void);" },
   { UnistdGetegid, "gid_t getegid(void);" },
   { UnistdGeteuid, "uid_t geteuid(void);" },
   { UnistdGetgid, "gid_t getgid(void);" },
#if 0
   { UnistdGetgroups, "int getgroups(int, gid_t []);" },
#endif
   { UnistdGethostid, "long gethostid(void);" },
   { UnistdGetlogin, "char *getlogin(void);" },
   { UnistdGetlogin_r, "int getlogin_r(char *, size_t);" },
#if 0
   { UnistdGetopt, "int getopt(int, char * [], char *);" },
#endif
   { UnistdGetpagesize, "int getpagesize(void);" },
   { UnistdGetpass, "char *getpass(char *);" },
#if 0
   { UnistdGetpgid, "pid_t getpgid(pid_t);" },
#endif
   { UnistdGetpgrp, "pid_t getpgrp(void);" },
   { UnistdGetpid, "pid_t getpid(void);" },
   { UnistdGetppid, "pid_t getppid(void);" },
#if 0
   { UnistdGetsid, "pid_t getsid(pid_t);" },
#endif
   { UnistdGetuid, "uid_t getuid(void);" },
   { UnistdGetwd, "char *getwd(char *);" },
   { UnistdIsatty, "int isatty(int);" },
   { UnistdLchown, "int lchown(char *, uid_t, gid_t);" },
   { UnistdLink, "int link(char *, char *);" },
   { UnistdLockf, "int lockf(int, int, off_t);" },
   { UnistdLseek, "off_t lseek(int, off_t, int);" },
   { UnistdNice, "int nice(int);" },
   { UnistdPathconf, "long pathconf(char *, int);" },
   { UnistdPause, "int pause(void);" },
#if 0
   { UnistdPipe, "int pipe(int [2]);" },
   { UnistdPread, "ssize_t pread(int, void *, size_t, off_t);" },
   { UnistdPthread_atfork, "int pthread_atfork(void (*)(void), void (*)(void), void(*)(void));" },
   { UnistdPwrite, "ssize_t pwrite(int, void *, size_t, off_t);" },
#endif
   { UnistdRead, "ssize_t read(int, void *, size_t);" },
   { UnistdReadlink, "int readlink(char *, char *, size_t);" },
   { UnistdRmdir, "int rmdir(char *);" },
   { UnistdSbrk, "void *sbrk(intptr_t);" },
   { UnistdSetgid, "int setgid(gid_t);" },
   { UnistdSetpgid, "int setpgid(pid_t, pid_t);" },
   { UnistdSetpgrp, "pid_t setpgrp(void);" },
   { UnistdSetregid, "int setregid(gid_t, gid_t);" },
   { UnistdSetreuid, "int setreuid(uid_t, uid_t);" },
   { UnistdSetsid, "pid_t setsid(void);" },
   { UnistdSetuid, "int setuid(uid_t);" },
   { UnistdSleep, "unsigned int sleep(unsigned int);" },
#if 0
   { UnistdSwab, "void swab(void *, void *, ssize_t);" },
#endif
   { UnistdSymlink, "int symlink(char *, char *);" },
   { UnistdSync, "void sync(void);" },
   { UnistdSysconf, "long sysconf(int);" },
   { UnistdTcgetpgrp, "pid_t tcgetpgrp(int);" },
   { UnistdTcsetpgrp, "int tcsetpgrp(int, pid_t);" },
   { UnistdTruncate, "int truncate(char *, off_t);" },
   { UnistdTtyname, "char *ttyname(int);" },
   { UnistdTtyname_r, "int ttyname_r(int, char *, size_t);" },
   { UnistdUalarm, "useconds_t ualarm(useconds_t, useconds_t);" },
   { UnistdUnlink, "int unlink(char *);" },
   { UnistdUsleep, "int usleep(useconds_t);" },
   { UnistdVfork, "pid_t vfork(void);" },
   { UnistdWrite, "ssize_t write(int, void *, size_t);" },
   { NULL, NULL }
};

// Creates various system-dependent definitions.
void UnistdSetupFunc(State pc) {
// Define NULL.
   if (!VariableDefined(pc, TableStrRegister(pc, "NULL")))
      VariableDefinePlatformVar(pc, NULL, "NULL", &pc->IntType, (AnyValue)&ZeroValue, false);
// Define optarg and friends.
   VariableDefinePlatformVar(pc, NULL, "optarg", pc->CharPtrType, (AnyValue)&optarg, true);
   VariableDefinePlatformVar(pc, NULL, "optind", &pc->IntType, (AnyValue)&optind, true);
   VariableDefinePlatformVar(pc, NULL, "opterr", &pc->IntType, (AnyValue)&opterr, true);
   VariableDefinePlatformVar(pc, NULL, "optopt", &pc->IntType, (AnyValue)&optopt, true);
}

#endif // !BUILTIN_MINI_STDLIB.
