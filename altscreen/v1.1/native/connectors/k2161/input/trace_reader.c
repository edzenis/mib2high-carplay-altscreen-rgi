/* K2161-only diagnostic transport. Normalized events are produced by the parser;
 * the core never handles QNX TCP sockets, DSITracer ports or factory services. */
#define _GNU_SOURCE
#include "trace_reader.h"
#include "framework/logging.h"
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#define ALTSCREEN_SESSION_MODULE "AltScreenSession"
static int g_started;
static k2161_trace_event_callback g_callback;
static k2161_trace_metrics_callback g_metrics;
static uint64_t k2161_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return (uint64_t)ts.tv_sec*1000ULL+(uint64_t)ts.tv_nsec/1000000ULL;
}
static void *k2161_trace_reader(void *arg)
{
    int buffered = (arg != NULL);
    unsigned short port = buffered ? 20991 : 1798;
    char buf[1024], line[512]; size_t ll = 0;
    unsigned int attempts = 0;
    for (;;) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;
        ssize_t n = 0, j;
        uint64_t lines_seen = 0;
        if (fd < 0) {
            if (++attempts == 1 || attempts % 15 == 0)
                LOG_WARN(ALTSCREEN_SESSION_MODULE,
                         "DIAG_TRACER_SOCKET_FAIL port=%u errno=%d attempts=%u",
                         port, errno, attempts);
            sleep(2); continue;
        }
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            int err = errno;
            if (++attempts == 1 || attempts % 15 == 0)
                LOG_WARN(ALTSCREEN_SESSION_MODULE,
                         "DIAG_TRACER_CONNECT_FAIL port=%u errno=%d attempts=%u",
                         port, err, attempts);
            close(fd); sleep(buffered ? 3 : 2); continue;
        }
        attempts = 0;
        LOG_WARN(ALTSCREEN_SESSION_MODULE, "ALTSESSION_TRACER_CONNECT t_ms=%llu port=%u source=%s status=CONNECTED",
                 (unsigned long long)k2161_now_ms(), port, buffered ? "BUFFERED" : "LIVE");
        LOG_INFO(ALTSCREEN_SESSION_MODULE, "ALTSESSION_TRACER_CONNECTED port=%d", port);
        ll = 0;
        for (;;) {
            fd_set rd;
            struct timeval tv;
            int ready;
            FD_ZERO(&rd);
            FD_SET(fd, &rd);
            tv.tv_sec = 20;
            tv.tv_usec = 0;
            ready = select(fd + 1, &rd, NULL, NULL, &tv);
            if (ready == 0) {
                uint64_t total, accepted, rejected, buffered_events;
                total = accepted = rejected = buffered_events = 0;
                if (g_metrics) g_metrics(&total, &accepted, &rejected, &buffered_events);
                LOG_WARN(ALTSCREEN_SESSION_MODULE,
                         "ALTSESSION_TRACE_HEARTBEAT t_ms=%llu port=%u connected=1 received_lines=%llu keypanel_total=%llu accepted=%llu rejected=%llu buffered=%llu",
                         (unsigned long long)k2161_now_ms(), port, (unsigned long long)lines_seen,
                         (unsigned long long)total, (unsigned long long)accepted,
                         (unsigned long long)rejected, (unsigned long long)buffered_events);
                continue;
            }
            if (ready < 0) {
                if (errno == EINTR) continue;
                n = -1;
                break;
            }
            n = read(fd, buf, sizeof(buf));
            if (n <= 0) break;
            for (j = 0; j < n; ++j) {
                if (buf[j] == '\n' || buf[j] == '\r') {
                    if (ll != 0) {
                        char *q;
                        line[ll] = 0;
                        ++lines_seen;
                        /* Joey buffered record prefix: <seq>?1#... */
                        q = buffered ? strchr(line, '?') : NULL;
                        if (g_callback) g_callback(q != NULL ? q + 1 : line, buffered);
                        ll = 0;
                    }
                } else if (ll < sizeof(line) - 1) line[ll++] = buf[j];
            }
        }
        LOG_WARN(ALTSCREEN_SESSION_MODULE,
                 "DIAG_TRACER_DISCONNECT port=%u result=%d errno=%d received_lines=%llu reconnecting=1",
                 port, (int)n, n < 0 ? errno : 0,
                 (unsigned long long)lines_seen);
        close(fd); sleep(buffered ? 3 : 2);
    }
    return NULL;
}

void k2161_start_trace_readers(k2161_trace_event_callback callback, k2161_trace_metrics_callback metrics)
{
    pthread_t worker;
    if (g_started) return;
    g_started = 1;
    g_callback = callback;
    g_metrics = metrics;
    if (pthread_create(&worker, NULL, k2161_trace_reader, NULL) == 0)
        pthread_detach(worker);
    if (pthread_create(&worker, NULL, k2161_trace_reader, (void *)1) == 0)
        pthread_detach(worker);
}

