//
// Created by fred1 on 2020/8/11.
//


#include <csignal>
#include <cerrno>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
// https://linux.die.net/man/3/inet_ntoa
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>

#define LISTENQ 1024  // second argument to  listen().
#define MAXLINE 1024  // max length of a line.
#define RIO_BUF_SIZE 1024

typedef struct {
    int rio_fd;  // descriptor for this buf.
    int rio_cnt;  // unread byte in this buf.
    char *rio_bufptr;  // next unread byte in this buf.
    char rio_buf[RIO_BUF_SIZE];  // internal buffer
} rio_t;

/* Simplifies calls to bind(), connect(), and accept() */
typedef struct sockaddr SA;

typedef struct {
    char filename[512];
    /*
     * https://stackoverflow.com/a/10634759/4270398
     * off_t is Posix, signed, and refers to the size of a file.
     * */
    off_t offset;  // for support Range.
    size_t end;
} http_request;

typedef struct {
    const char *extension;
    const char *mime_type;
} mime_map;

mime_map mime_types [] = {
        {".css", "text/css"},
        {".gif", "image/gif"},
        {".htm", "text/html"},
        {".html", "text/html"},
        {".jpeg", "image/jpeg"},
        {".jpg", "image/jpeg"},
        {".ico", "image/x-icon"},
        {".js", "application/javascript"},
        {".pdf", "application/pdf"},
        {".mp4", "video/mp4"},
        {".png", "image/png"},
        {".svg", "image/svg+xml"},
        {".xml", "text/xml"},
        {nullptr, nullptr},
};

const char *default_mime_type = "text/plain";

void rio_readinit(rio_t *rp, int fd) {
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}

/*
 * https://stackoverflow.com/a/15739705/4270398
 * you should use size_t whenever you mean to return a size in bytes,
 * and ssize_t whenever you would return either a size in bytes or a (negative) error value.
 * https://www.quora.com/What-are-the-use-cases-of-ssize_t-in-C++
 * According to posix standards <sys/types.h>, ssize_t is used for count of bytes or an error indicatio
 * The type ssize_t is capable of storing values at least in the range [-1, SSIZE_MAX]
 * */
ssize_t writen(int fd, void *usrbuf, size_t n) {
    size_t nleft = n;
    ssize_tt nwritten;
    char *bufp =  usrbuf;

    while (nleft > 0) {
        /*
         * #include <unistd.h>
         * ssize_t write(int fd, const void *buf, size_t count);
         * write() writes up to count bytes from the buffer pointed buf to the file referred to by the file descriptor fd.
         * https://linux.die.net/man/2/write
         * */
        if (nwritten = write(fd, bufp, nleft) <= 0) {
            /*
             * Many system calls will report the EINTR error code if a signal occurred while the system call was in progress.
             * No error actually occurred, it's just reported that way because the system isn't able to resume the system call automatically.
             * This coding pattern simply retries the system call when this happens, to ignore the interrupt.
             * For instance, this might happen if the program makes use of alarm() to run some code asynchronously when a timer runs out.
             * If the timeout occurs while the program is calling write(), we just want to retry the system call (aka read/write, etc).
             * https://stackoverflow.com/a/41474692/4270398
             * */
            if (errno == EINTR) {  // interrupted by sig handler return
                nwritten = 0;
            } else {
                return -1;
            }
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    return n;
}

/*
 * rio_read - This is a wrapper for the Unix read() function that
 *    transfers min(n, rio_cnt) bytes from an internal buffer to a user
 *    buffer, where n is the number of bytes requested by the user and
 *    rio_cnt is the number of unread bytes in the internal buffer. On
 *    entry, rio_read() refills the internal buffer via a call to
 *    read() if the internal buffer is empty.
 */
/* $begin rio_read */
static ssize_t rio_read(rio_t *rp, char* usrbuf, size_t n) {
    int cnt;
    while (rp->rio_cnt <= 0) { // refill if buf is empty.
        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));
        if (rp->rio_cnt < 0) {
            if (errno != EINTR) {  /// interrupted by sig handler return
                return -1;
            }
        } else if (rp->rio_cnt == 0)  {  // EOF
            return 0;
        } else {
                rp->rio_bufptr = rp->rio_buf;  // reset buffer ptr.
        }
    }
    /// Copy min(n, rp->rio_cnt) bytes from internal buf to user buf
    cnt = n;
    if (rp->rio_cnt < n) {
        cnt = rp->rio_cnt;
    }
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}

/*
 * rio_readlineb - robustly read a text line (buffered)
 */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) {}

void format_size(char *buf, struct stat *stat) {}

void handle_directory_request(int out_fd, int dir_fd, char *filename) {}

// todo: use pair or map.
static const char* get_mime_type(char *filename) {
    // Returns a pointer to the last occurrence of character in the C string str.
    // http://www.cplusplus.com/reference/cstring/strrchr/
    char *dot = strrchr(filename, reinterpret_cast<const int>("."));
    if (dot) {  // strrchar Locate last occurrence of character in string
        mime_map *map = mime_types;
        if (strcmp(map->extension, dot) == 0) {
            return map->mime_type;
        }
        map++;
    }
    return default_mime_type;
}

int open_listenfd(int port) {}

void url_decode(char *src, char *dest, int max) {}

void parse_request(int fd, http_request *req) {}

/*
 *   struct sockaddr_in {
 *   sa_family_t    sin_family; // address family: AF_INET
 *   in_port_t      sin_port;   // port in network byte order
 *   struct in_addr sin_addr;   // internet address
 *   };
 *
 *   These are the basic structures for all syscalls and functions
 *   that deal with internet addresses. In memory,
 *   the struct sockaddr_in is the same size as struct sockaddr,
 *   and you can freely cast the pointer of one type to the other
 *   without any harm, except the possible end of the universe.
 *   https://man7.org/linux/man-pages/man7/ip.7.htm
 *   https://www.gta.ufrj.br/ensino/eel878/sockets/sockaddr_inman.html
 * */
void log_access(int status, struct sockaddr_in *c_addr, http_request *req) {
    printf("%s:%d %d - %s\n", inet_ntoa(c_addr->sin_addr), ntohs(c_addr->sin_port), status, req->filename);
}

void client_error(int fd, int status, char *msg, char *longmsg) {}

void serve_static(int out_fd, int in_fd, http_request *req, size_t total_size) {}

void process(int fd, struct sockaddr_in *clientaddr) {}

int main(int argc, char** argv) {
    return 0;
}
