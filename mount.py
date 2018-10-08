#!/usr/bin/env python

from __future__ import with_statement

import os
import sys
import errno
import handleusb
from stat import *

from fuse import FUSE, FuseOSError, Operations


debug = False
def debug_log(string):
    if debug:
        print(string)


class SwitchUSB(Operations):

    # Filesystem methods
    # ==================
    fdOfPath = {}
    countOfFd = {}
    # Horizon doesn't like to open a file multiple times.
    # So as a workaround we just reuse the same fd multiple times

    # Getattr is somehow wrong in a way that makes vs-code hate us (confirmed with the passthrough function)

    def getattr(self, path, fh=None):
        fd = 0
        if path in self.fdOfPath:
            fd = self.fdOfPath[path]

        res, size, mode = handleusb.getattr(fd, path)

        ret = dict((key, 0) for key in ('st_atime', 'st_ctime',
                                        'st_gid', 'st_mode', 'st_mtime', 'st_nlink', 'st_size', 'st_uid'))

        try:
            ret["st_uid"] = os.getuid()
            ret["st_gid"] = os.getgid()
        except:
            pass  # probably windows
        ret["st_size"] = size
        ret["st_blksize"] = size
        ret["st_mode"] = mode

        if(res < 0):
            debug_log("getattr failed with {} on path {}".format(res, path))
            raise FileNotFoundError(
                errno.ENOENT, os.strerror(errno.ENOENT), path)

        return ret

    def readdir(self, path, fh):
        dirents = handleusb.readdir(path)
        for r in dirents:
            yield r

    def rmdir(self, path):
        return handleusb.rmdir(path)

    def mkdir(self, path, mode):
        return handleusb.mkdir(path, mode)

    def unlink(self, path):
        return handleusb.unlink(path)

    def rename(self, old, new):
        return handleusb.rename(old, new)

    def statfs(self, path):
        return {'f_bavail': 28116584, 'f_bfree': 28810875,
                'f_blocks': 62463564, 'f_bsize': 12312321,
                'f_favail': 4293618664, 'f_ffree': 4293618664,
                'f_files': 4294967295, 'f_flag': 0,
                'f_frsize': 4096, 'f_namemax': 128}

    # File methods
    # ============

    def open(self, path, flags):
        # flags = flags | os.O_RDWR
        # We need to reuse filehandles

        if path in self.fdOfPath:
            debug_log("OPEN: File already open!!!")
            #self.release("", self.fdOfPath[path])
            # debug_log("A"*1000)
            ret = self.fdOfPath[path]
            self.countOfFd[ret] += 1
            return ret

        ret = handleusb.open(path, flags)
        debug_log("OPEN: Path {} Flags {} Ret {}".format(path, flags, ret))
        if(ret < 0):
            debug_log("Open failed!!")
            raise FileNotFoundError(
                errno.ENOENT, os.strerror(errno.ENOENT), path)

        self.fdOfPath[path] = ret
        self.countOfFd[ret] = 1

        return ret

    def create(self, path, mode, fi=None):

        if path in self.fdOfPath:
            debug_log("CREATE: File already open!!!")
            ret = self.fdOfPath[path]
            self.countOfFd[ret] += 1
            return ret

        handleusb.unlink(path)
        ret = handleusb.create(path, os.O_RDWR | os.O_CREAT, mode)
        debug_log("CREATE: Path {} Mode {} Ret {}".format(path, mode, ret))
        if(ret < 0):
            debug_log("Create Failed!!")
            raise FileNotFoundError(
                errno.ENOENT, os.strerror(errno.ENOENT), path)

        self.fdOfPath[path] = ret
        self.countOfFd[ret] = 1

        return ret

    def read(self, path, length, offset, fh):
        res, buffer = handleusb.read(length, offset, fh)
        return buffer

    def write(self, path, buf, offset, fh):
        ret = handleusb.write(buf, offset, fh)
        # debug_log("Writing! Want {} Got {} OFF {}".format(len(buf), ret, offset))
        if(ret < 0):
            raise FileNotFoundError(
                errno.ENOENT, os.strerror(errno.ENOENT), path)

        return ret

    def release(self, path, fh):
        debug_log("Release {}".format(fh))

        self.countOfFd[fh] -= 1

        if self.countOfFd[fh] <= 0:
            for path, fd in self.fdOfPath.items():
                if fd == fh:
                    debug_log("Actually releasing!")
                    self.fdOfPath.pop(path)
                    handleusb.release(fh)
                    break

        return 0


def main(mountpoint):
    FUSE(SwitchUSB(), mountpoint, nothreads=True, foreground=True)


if __name__ == '__main__':
    main(sys.argv[1])
