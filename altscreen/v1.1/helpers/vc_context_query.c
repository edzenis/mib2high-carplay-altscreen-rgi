/* AltScreen: read-only K2161 DisplayManager context snapshot through a PTY.
 * AltScreen captured no stdout from dmdt gs in a non-interactive shell even when rc=0.
 * This helper only runs `dmdt gs`, never `sc`, `dc`, or any mutating command.
 * The PTY supplies a terminal to programs that suppress output on pipes.
 * Bounded runtime, no daemon, no device writes other than terminal I/O.
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_CAPTURE (64*1024)
#define QUERY_TIMEOUT_MS 2200

static long long now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return -1;
    return (long long) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
static void diagnostic(const char *s, int e) {
    fprintf(stderr, "VC_CONTEXT_QUERY_ERROR=%s errno=%d\n", s, e);
}
int main(int argc, char **argv) {
    const char *binary = argc >= 2 ? argv[1] : "/eso/bin/apps/dmdt";
    int master=-1, slave=-1, child_status=0, have_status=0, bytes=0;
    pid_t pid=-1;
    char *slave_name;
    long long start;
    if (argc == 2 && strcmp(binary, "--timestamp") == 0) {
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return 3;
        printf("%ld.%03ld\n", (long)ts.tv_sec, ts.tv_nsec/1000000L);
        return 0;
    }
    if (argc > 2) { diagnostic("USAGE", 0); return 2; }
    master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0) { diagnostic("PTY_OPEN", errno); return 20; }
    if (grantpt(master) < 0 || unlockpt(master) < 0) {
        diagnostic("PTY_SETUP", errno); close(master); return 21;
    }
    slave_name = ptsname(master);
    if (!slave_name) { diagnostic("PTY_NAME", errno); close(master); return 22; }
    slave=open(slave_name,O_RDWR | O_NOCTTY);
    if (slave < 0) { diagnostic("PTY_SLAVE", errno); close(master); return 23; }
    pid=fork();
    if (pid < 0) { diagnostic("FORK", errno); close(slave); close(master); return 24; }
    if (pid == 0) {
        char *const args[] = {(char *)binary, "gs", NULL};
        close(master);
        (void) setsid();
        (void) ioctl(slave, TIOCSCTTY, 0);
        if (dup2(slave,STDIN_FILENO)<0 || dup2(slave,STDOUT_FILENO)<0 || dup2(slave,STDERR_FILENO)<0) _exit(125);
        if (slave>STDERR_FILENO) close(slave);
        execv(binary,args);
        _exit(126);
    }
    close(slave);
    if (fcntl(master,F_SETFL,fcntl(master,F_GETFL,0)|O_NONBLOCK)<0) diagnostic("NONBLOCK",errno);
    start=now_ms();
    for (;;) {
        fd_set set;
        struct timeval timeout;
        int ready, n, cap;
        char buffer[4096];
        FD_ZERO(&set); FD_SET(master,&set);
        timeout.tv_sec=0; timeout.tv_usec=100000;
        ready=select(master+1,&set,NULL,NULL,&timeout);
        if (ready>0 && FD_ISSET(master,&set)) {
            cap=MAX_CAPTURE-bytes;
            if (cap<=0) { diagnostic("OUTPUT_LIMIT",0); break; }
            n=read(master,buffer,(size_t)(cap<4096?cap:4096));
            if (n>0) { (void)fwrite(buffer,1,(size_t)n,stdout); bytes+=n; }
            else if (n==0 || (n<0 && errno==EIO) || (n<0 && errno!=EAGAIN && errno!=EINTR)) break;
        }
        if (!have_status && waitpid(pid,&child_status,WNOHANG)==pid) have_status=1;
        if (have_status && ready>0) continue; /* drain trailing tty buffer */
        if (have_status && ready==0) break;
        if (now_ms()-start>=QUERY_TIMEOUT_MS) { diagnostic("TIMEOUT",0); break; }
    }
    close(master);
    /* PTY hangup can precede waitpid's observable child exit; allow a
     * short bounded reaping grace period instead of misreporting success
     * with captured stdout as a failed query. */
    if (!have_status) {
        int i;
        for (i=0;i<12;i++) {
            if (waitpid(pid,&child_status,WNOHANG)==pid) { have_status=1; break; }
            usleep(10000);
        }
    }
    if (!have_status) {
        (void) kill(pid,SIGKILL);
        (void) waitpid(pid,&child_status,0);
        diagnostic("TERMINATED",0);
        return 25;
    }
    fflush(stdout);
    if (!WIFEXITED(child_status) || WEXITSTATUS(child_status)!=0) {
        diagnostic("DMDT_FAILED",WIFEXITED(child_status)?WEXITSTATUS(child_status):-1); return 26;
    }
    if (!bytes) { diagnostic("OUTPUT_EMPTY",0); return 27; }
    return 0;
}
