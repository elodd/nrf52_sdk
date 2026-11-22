/*
 * mic-receiver: prints the microphone lines from the nRF52840.
 * Usage: mic-receiver <serial-device>   e.g. mic-receiver /dev/ttyUSB0
 * Only lines containing "mic:" are printed.
 */
#define _DEFAULT_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define LINE_MAX_LEN 512

static int open_serial(const char *path)
{
    int fd = open(path, O_RDONLY | O_NOCTTY);
    if (fd < 0) {
        perror(path);
        return -1;
    }

    struct termios tio;
    if (tcgetattr(fd, &tio) < 0) {
        perror("tcgetattr");
        close(fd);
        return -1;
    }

    cfmakeraw(&tio);
    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);
    tio.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);
    tio.c_cflag |= CS8 | CLOCAL | CREAD;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tio) < 0) {
        perror("tcsetattr");
        close(fd);
        return -1;
    }
    return fd;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <serial-device>\n", argv[0]);
        return 2;
    }

    int fd = open_serial(argv[1]);
    if (fd < 0) {
        return 1;
    }

    char line[LINE_MAX_LEN];
    size_t len = 0;
    int too_long = 0;
    char buf[256];

    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            close(fd);
            return 1;
        }

        for (ssize_t i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n') {
                if (!too_long) {
                    line[len] = '\0';
                    if (strstr(line, "mic:") != NULL) {
                        puts(line);
                        fflush(stdout);
                    }
                }
                len = 0;
                too_long = 0;
            } else if (c != '\r') {
                if (len < sizeof(line) - 1) {
                    line[len++] = c;
                } else {
                    too_long = 1;
                }
            }
        }
    }
}
