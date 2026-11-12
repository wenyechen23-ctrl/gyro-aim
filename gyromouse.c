// gyromouse - virtual mouse driven by gyro data over local TCP
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define PORT 7788

static int g_fd = -1;

static int create_mouse(void)
{
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) { perror("open /dev/uinput"); return -1; }

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE);
    ioctl(fd, UI_SET_EVBIT, EV_REL);
    ioctl(fd, UI_SET_RELBIT, REL_X);
    ioctl(fd, UI_SET_RELBIT, REL_Y);
    ioctl(fd, UI_SET_RELBIT, REL_WHEEL);

    struct uinput_setup usetup;
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor  = 0x1234;
    usetup.id.product = 0x5678;
    strncpy(usetup.name, "GyroMouse", sizeof(usetup.name) - 1);

    if (ioctl(fd, UI_DEV_SETUP, &usetup) < 0) { perror("UI_DEV_SETUP"); close(fd); return -1; }
    if (ioctl(fd, UI_DEV_CREATE) < 0)          { perror("UI_DEV_CREATE"); close(fd); return -1; }
    return fd;
}

static void emit(int fd, int type, int code, int val)
{
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type; ev.code = code; ev.value = val;
    if (write(fd, &ev, sizeof(ev)) < 0) { /* ignore */ }
}

static void cleanup(int sig)
{
    (void)sig;
    if (g_fd >= 0) { ioctl(g_fd, UI_DEV_DESTROY); close(g_fd); }
    _exit(0);
}

int main(void)
{
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    g_fd = create_mouse();
    if (g_fd < 0) return 1;
    fprintf(stderr, "GyroMouse created\n");

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }
    int one = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(PORT);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(srv, 4) < 0) { perror("listen"); return 1; }
    fprintf(stderr, "listening on 127.0.0.1:%d\n", PORT);

    for (;;)
    {
        int c = accept(srv, NULL, NULL);
        if (c < 0) { if (errno == EINTR) continue; break; }
        setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        fprintf(stderr, "client connected\n");

        /* packet: int16 dx, int16 dy, int16 buttons, int16 wheel */
        short buf[4];
        size_t got = 0;
        for (;;)
        {
            ssize_t n = read(c, ((char *)buf) + got, sizeof(buf) - got);
            if (n <= 0) break;
            got += n;
            if (got < sizeof(buf)) continue;
            got = 0;

            if (buf[0]) emit(g_fd, EV_REL, REL_X, buf[0]);
            if (buf[1]) emit(g_fd, EV_REL, REL_Y, buf[1]);
            if (buf[3]) emit(g_fd, EV_REL, REL_WHEEL, buf[3]);
            emit(g_fd, EV_SYN, SYN_REPORT, 0);
        }
        fprintf(stderr, "client disconnected\n");
        close(c);
    }
    cleanup(0);
    return 0;
}
