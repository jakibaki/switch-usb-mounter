#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#include <switch.h>
#include <sys/stat.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

//#define DEBUG
#ifdef DEBUG
#define write_log(format, args...) \
    do                             \
    {                              \
        printf(format, ##args);    \
        gfxFlushBuffers();         \
        gfxSwapBuffers();          \
    } while (0)
#else
#define write_log(format, args...) ;
#endif

size_t transport_safe_read(void *buffer, size_t size)
{
    u8 *bufptr = buffer;
    size_t cursize = size;
    size_t tmpsize = 0;

    while (cursize)
    {
        tmpsize = usbCommsRead(bufptr, cursize);
        bufptr += tmpsize;
        cursize -= tmpsize;
    }

    return size;
}

size_t transport_safe_write(const void *buffer, size_t size)
{
    const u8 *bufptr = (const u8 *)buffer;
    size_t cursize = size;
    size_t tmpsize = 0;

    while (cursize)
    {
        tmpsize = usbCommsWrite(bufptr, cursize);
        bufptr += tmpsize;
        cursize -= tmpsize;
    }

    return size;
}

#define RW_BUF_SIZE 2048
#define PATH_LEN 1024

unsigned char pkg_buf[RW_BUF_SIZE];

void __getattr_handler()
{
    struct __attribute__((packed))
    {
        int fd;
        char path[PATH_LEN];
    } args;
    transport_safe_read(&args, sizeof(args));

    struct stat stbuf = {0};

    struct __attribute__((packed))
    {
        int res;
        s64 size;
        u16 mode;
    } reply;

    reply.res = stat(args.path, &stbuf);
    write_log("NORMAL GETATTR: FILE: %s FD: %d Res: %d Size: %ld Mode: %d\n", args.path, args.fd, reply.res, stbuf.st_size, stbuf.st_mode);

    if(reply.res != 0 && args.fd != 0) {
        reply.res = fstat(args.fd, &stbuf);
        stbuf.st_size = lseek(args.fd, 0, SEEK_END); // **Of course** the fstat has to fuck that up...
        write_log("FD GETATTR: FILE: %s FD: %d Res: %d Size: %ld Mode: %d\n", args.path, args.fd, reply.res, stbuf.st_size, stbuf.st_mode);
    }

    reply.size = stbuf.st_size;
    reply.mode = stbuf.st_mode;

    transport_safe_write(&reply, sizeof(reply));
}

void __readdir_handler()
{
    struct __attribute__((packed))
    {
        char path[PATH_LEN];
    } args;

    transport_safe_read(&args, sizeof(args));

    struct __attribute__((packed))
    {
        int success;         // 0 on success, anything else otherwise
        char path[PATH_LEN]; // 0-len string gets sent last
    } reply = {0};

    DIR *dp;
    struct dirent *de;
    dp = opendir(args.path);
    if (dp == NULL)
    {
        reply.success = -1;
        transport_safe_write(&reply, sizeof(reply));
        return;
    }

    while ((de = readdir(dp)) != NULL)
    {
        strcpy(reply.path, de->d_name);
        transport_safe_write(&reply, sizeof(reply));
    }
    strcpy(reply.path, "");
    transport_safe_write(&reply, sizeof(reply));

    closedir(dp);
}

void __rmdir_handler()
{
    struct __attribute__((packed))
    {
        char path[PATH_LEN];
    } args;
    transport_safe_read(&args, sizeof(args));

    int res = rmdir(args.path);
    transport_safe_write(&res, sizeof(res));
}

void __mkdir_handler()
{
    struct __attribute__((packed))
    {
        char path[PATH_LEN];
        u64 mode;
    } args;
    transport_safe_read(&args, sizeof(args));

    int res = mkdir(args.path, args.mode);
    transport_safe_write(&res, sizeof(res));
}

void __unlink_handler()
{
    struct __attribute__((packed))
    {
        char path[PATH_LEN];
    } args;
    transport_safe_read(&args, sizeof(args));

    int res = unlink(args.path);
    transport_safe_write(&res, sizeof(res));
}

void __rename_handler()
{
    struct __attribute__((packed))
    {
        char path_orig[PATH_LEN];
        char path_new[PATH_LEN];
    } args;
    transport_safe_read(&args, sizeof(args));

    int res = rename(args.path_orig, args.path_new);
    transport_safe_write(&res, sizeof(res));
}

void __open_handler()
{
    struct __attribute__((packed))
    {
        char path[PATH_LEN];
        int flags;
    } args;
    transport_safe_read(&args, sizeof(args));

    int res = open(args.path, args.flags);
    transport_safe_write(&res, sizeof(res));
}

void __create_handler()
{
    struct __attribute__((packed))
    {
        u64 flags;
        u64 mode;
        char path[PATH_LEN];
    } args;
    transport_safe_read(&args, sizeof(args));

    int res = open(args.path, args.flags, args.mode);
    transport_safe_write(&res, sizeof(res));
}

void __read_handler()
{
    struct __attribute__((packed))
    {
        int fd;
        size_t size;
        size_t offset;
    } args;

    transport_safe_read(&args, sizeof(args));

    ssize_t reply;

    int fd = args.fd;

    size_t file_size = lseek(fd, 0, SEEK_END);

    if (args.offset > file_size)
    {
        reply = -1;
        transport_safe_write(&reply, sizeof(reply));
        return;
    }

    reply = MIN(file_size - args.offset, args.size);

    transport_safe_write(&reply, sizeof(reply));

    if (reply == 0)
        return;

    lseek(fd, args.offset, SEEK_SET);

    while (reply > 0)
    {
        size_t read_size = MIN(reply, RW_BUF_SIZE);
        read(fd, pkg_buf, read_size);
        transport_safe_write(pkg_buf, read_size);
        reply -= read_size;
    }

    if (args.fd == 0)
        close(fd);
}

void __write_handler()
{
    struct __attribute__((packed))
    {
        int fd;
        size_t size;
        size_t offset;
    } args;
    transport_safe_read(&args, sizeof(args));

    ssize_t reply;

    int fd;

    fd = args.fd;

    reply = args.size;

    off_t seekres = lseek(fd, 0, SEEK_CUR);
    if(seekres != args.offset)
        seekres = lseek(fd, args.offset, SEEK_SET);

    write_log("Writing at offset %ld res: %ld\n", args.offset, seekres);
    if(seekres < 0)
        reply = seekres;

    while (args.size > 0)
    {
        size_t write_size = MIN(args.size, RW_BUF_SIZE);
        transport_safe_read(pkg_buf, write_size);

        if (reply >= 0)
            write(fd, pkg_buf, write_size);

        args.size -= write_size;
    }

    transport_safe_write(&reply, sizeof(reply));
}

void __release_handler()
{
    struct __attribute__((packed))
    {
        int fd;
    } args;
    transport_safe_read(&args, sizeof(args));
    close(args.fd);
}

enum
{
    GETATTR,
    READDIR,
    RMDIR,
    MKDIR,
    UNLINK,
    RENAME,
    OPEN,
    CREATE,
    READ,
    WRITE,
    RELEASE
};
typedef u32 action;

void (*action_handlers[])(void) = {
    &__getattr_handler,
    &__readdir_handler,
    &__rmdir_handler,
    &__mkdir_handler,
    &__unlink_handler,
    &__rename_handler,
    &__open_handler,
    &__create_handler,
    &__read_handler,
    &__write_handler,
    &__release_handler};

Result usb_loop(void)
{

    while (appletMainLoop())
    {
        action act;
        transport_safe_read(&act, sizeof(act));
        if (act > RELEASE)
        {
            printf("Invalid command! %d\n", act);
            printf("Why would you do such a thing?\n");
            gfxFlushBuffers();
            gfxSwapBuffers();
            continue;
        }
        action_handlers[act]();
    }

    return 0;
}

int main(int argc, char **argv)
{
    socketInitializeDefault();

    gfxInitDefault();
    consoleInit(NULL);

    Result ret;

    printf("Calling usbCommsInitialize()...\n");
    ret = usbCommsInitialize();

    if (R_SUCCEEDED(ret))
    {
        printf("usbCommsInitialize succeeded!\n");
        gfxFlushBuffers();
        gfxSwapBuffers();

        ret = usb_loop();

        printf("Exiting...\n");

        gfxFlushBuffers();
        gfxSwapBuffers();
        usbCommsExit();
    }

    if (R_FAILED(ret))
        fatalSimple(ret);

    return 0;
}
