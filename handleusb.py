import usb.core
import usb.util
from struct import pack, unpack, calcsize

# find our device
dev = usb.core.find(idVendor=0x057e, idProduct=0x3000)

# was it found?
if dev is None:
    raise ValueError('Device not found')

# set the active configuration. With no arguments, the first
# configuration will be the active one
dev.set_configuration()

# get an endpoint instance
cfg = dev.get_active_configuration()
intf = cfg[(0, 0)]

epout = usb.util.find_descriptor(
    intf,
    # match the first OUT endpoint
    custom_match=lambda e: \
    usb.util.endpoint_direction(e.bEndpointAddress) == \
    usb.util.ENDPOINT_OUT)

epin = usb.util.find_descriptor(
    intf,
    # match the first OUT endpoint
    custom_match=lambda e: \
    usb.util.endpoint_direction(e.bEndpointAddress) == \
    usb.util.ENDPOINT_IN)


timeout = 10000

def recv(numBytes):
    return epin.read(numBytes, timeout).tobytes()

def send(buffer):
    epout.write(buffer, timeout)

GETATTR = 0
READDIR = 1
RMDIR = 2
MKDIR = 3
UNLINK = 4
RENAME = 5
OPEN = 6
CREATE = 7
READ = 8
WRITE = 9
RELEASE = 10

PATH_LEN = 1024

def bytesToNullStr(buffer):
    buffer = buffer[:buffer.find(b"\x00")]
    return buffer.decode()

def getattr(fd, path):
    send(pack("=I", GETATTR))
    send(pack("=i1024s", fd, path.encode()))
    return unpack("=iqH", recv(calcsize("=iqH")))
    # res, size, mode

def readdir(path):
    send(pack("=I", READDIR))
    send(pack("=1024s", path.encode()))
    dirents = ['.', '..']
    while True:
        success, hit = unpack("=i1024s", recv(calcsize("=i1024s")))
        hit = bytesToNullStr(hit)
        if(success != 0):
            print("WARNING: READDIR {} FAILED!".format(path))
            return []
        if(len(hit) == 0):
            return dirents
        dirents.append(hit)

def rmdir(path):
    send(pack("=I", RMDIR))
    send(pack("=1024s", path.encode()))
    return unpack("=i", recv(calcsize("=i")))[0]

def mkdir(path, mode):
    send(pack("=I", MKDIR))
    send(pack("=1024sq", path.encode(), mode))
    return unpack("=i", recv(calcsize("=i")))[0]

def unlink(path):
    send(pack("=I", UNLINK))
    send(pack("=1024s", path.encode()))
    return unpack("=i", recv(calcsize("=i")))[0]

def rename(old, new):
    send(pack("=I", RENAME))
    send(pack("=1024s1024s", old.encode(), new.encode()))
    return unpack("=i", recv(calcsize("=i")))[0]

def open(path, flags):
    send(pack("=I", OPEN))
    send(pack("=1024si", path.encode(), flags))
    return unpack("=i", recv(calcsize("=i")))[0]

def create(path, flags, mode):
    send(pack("=I", CREATE))
    send(pack("=qq1024s", flags, mode, path.encode()))
    return unpack("=i", recv(calcsize("=i")))[0]

def read(size, offset, fd):
    send(pack("=I", READ))
    send(pack("=iQQ", fd, size, offset))
    res = unpack("=q", recv(calcsize("=q")))[0]

    if(res == -1):
        print("A read failed! fd: {}".format(fd))
        return -1, b''
    if(res == 0):
        return 0, b''

    return res, recv(res)

def write(buf, offset, fd):
    send(pack("=I", WRITE))
    send(pack("=iQQ", fd, len(buf), offset))
    if(len(buf) > 0):
        send(buf)
    return unpack("=Q", recv(calcsize("=Q")))[0]

def release(fd):
    send(pack("=I", RELEASE))
    send(pack("=i", fd))