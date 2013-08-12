#!/usr/bin/python
import struct
import sys

def num2mpi(n):
        """convert number to MPI string"""
        if n == 0:
                return struct.pack(">I", 0)
        r = ""
        neg_flag = bool(n < 0)
        n = abs(n)
        while n:
                r = chr(n & 0xFF) + r
                n >>= 8
        if ord(r[0]) & 0x80:
                r = chr(0) + r
        if neg_flag:
                r = chr(ord(r[0]) | 0x80) + r[1:]
        datasize = len(r)
        return struct.pack(">I", datasize) + r

def GetCompact(n):
        """convert number to bc compact uint"""
        mpi = num2mpi(n)
        nSize = len(mpi) - 4
        nCompact = (nSize & 0xFF) << 24
        if nSize >= 1:
                nCompact |= (ord(mpi[4]) << 16)
        if nSize >= 2:
                nCompact |= (ord(mpi[5]) << 8)
        if nSize >= 3:
                nCompact |= (ord(mpi[6]) << 0)
        return nCompact

print GetCompact(eval(sys.argv[1]))
