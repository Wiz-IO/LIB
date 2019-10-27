import os, sys, struct, time, threading, subprocess, logging, random
from os.path import join
from binascii import hexlify

def HEX(s):
    return hexlify(s).decode("ascii").upper()

S_ISOCK     = 49152 # 0xC000
S_ISLNK     = 40960 # 0xA000
S_ISREG     = 32768 # 0x8000
S_ISBLK     = 24576 # 0x6000
S_ISDIR     = 16384 # 0x4000
S_ISCHR     =  8192 # 0x2000
S_IFIFO     =  4096 # 0x1000
S_IFMT      = S_IFIFO | S_ISCHR | S_ISDIR | S_ISREG # 0xF000
S_IRUSR     = 256 # 0x0100
S_IWUSR     = 128 # 0x0080
S_IXUSR     =  64 # 0x0040
S_IRWXU     = S_IXUSR | S_IWUSR | S_IRUSR # 0x01C0
S_IRGRP     = 32 # 0x0020
S_IWGRP     = 16 # 0x0010
S_IXGRP     = 8
S_IRWXG     = S_IXGRP | S_IWGRP | S_IRGRP # 0x0038
S_IROTH     = 4
S_IWOTH     = 2
S_IXOTH     = 1
S_IRWXO     = S_IXOTH | S_IWOTH | S_IROTH # 0x0007
S_ISUID     = 2048 # 0x0800
S_ISGID     = 1024 # 0x0400
S_ISVTX     =  512 # 0x0200
ONLY_PERM   = S_IRWXO | S_IRWXG | S_IRWXU # 0x01FF
CLEAR_PERM  = S_ISVTX | S_ISGID | S_ISUID | S_IFMT # 0xFE00
DEFAULT_DIR_PERM  = 493 # 0x01ED
DEFAULT_FILE_PERM = 420 # 0x01A4

PAGE_SIZE = 4096

def roundUp4(num): return num + 3 & -4;

def header(size = 0, count = 0):
    bin  = struct.pack("I", 0x28CD3D45)                         # offset[0]
    bin += struct.pack("I", size)                               # size of image
    bin += struct.pack("I", 3)                                  # 3
    bin += struct.pack("I", 0)                                  # 0
    bin += bytearray("Compressed ROMFS".encode('utf-8'))        # offset[16:32]
    bin += struct.pack("I", 0)                                  # crc ???
    bin += struct.pack("I", 0)                                  # 0
    bin += struct.pack("I", 0)                                  # 0
    bin += struct.pack("I", count)                              # root entry count
    bin += bytearray("Compressed\0\0\0\0\0\0".encode('utf-8'))  # offset[48:64]
    return bin

def inode(node):
    pass

class aNODE():
    def __init__(self, path, offset):
        print()
        #print(path)
        self.name = path[ len(path) - 1 ]
        self.name_size_up = roundUp4( len(self.name) )
        self.path = '/'.join(path)
        #print('PATH:', self.path)
        print('offst:', hex(offset))

        self.uid = 0 # ???
        self.gid = 0 # ???
        self.file_size = 0
        self.data_size = 0
        self.offset = 0
        self.image = [self.data_size] 
        self.mode = DEFAULT_DIR_PERM | S_ISDIR;

        if 1 == len(path):
            print('IS-ROOT:', self.name)
            self.offset = 0
            self.data_size = PAGE_SIZE
            self.image = header() 
            self.image += (PAGE_SIZE - 64) * b'\0'
            print('D-SIZE', hex(len(self.image)))

        elif os.path.isdir(self.path):    
            print('IS-DIR:', self.name)   
            self.offset = offset # ???
                
        elif os.path.isfile(self.path):
           print('IS-FILE:', self.name)
           self.file_size = os.path.getsize(self.path) 
           self.data_size = ( int( self.file_size / PAGE_SIZE ) + 1 ) * PAGE_SIZE
           #print('FSIZE:', hex(self.file_size))
           f = open(self.path,'rb') 
           self.image = f.read() 
           f.close()
           self.image += (self.data_size - self.file_size) * b'\0'
           print('DSIZE:', hex(len(self.image)))
           self.mode = DEFAULT_FILE_PERM | S_ISREG | S_ISVTX;
           self.offset = offset
           print('OFFST:', hex(self.offset))

        self.image = bytearray(self.image)
        

path = './approot' 
nodes = []
offset = 0
for root, dirs, files in os.walk(path):
    path = root.split(os.sep)
    node = aNODE(path, offset)
    offset += node.data_size
    nodes.append( node )
    for file in files:        
        path.append(file)
        node = aNODE( path, offset )
        offset += node.data_size
        nodes.append( node )

print()
img = bytearray()
total_size = 0
for n in nodes: total_size += n.data_size # calculate total
print('TOTAL SIZE', hex(total_size))

for n in nodes: img += n.image # pack images

for n in nodes: inode(n)

img[ 4: 8] = struct.pack("I", total_size)  
img[44:48] = struct.pack("I", len(nodes)) 
img[32:36] = struct.pack("I", 0xFFFFFFFF) # CRC(3988292384)   

print('TOTAL SIZE', hex(len(img)))

f = open('./image.bin','wb') 
f.write(img) 
f.close()