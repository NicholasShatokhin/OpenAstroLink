#!/usr/bin/env python3
"""Executable protocol model for OALW v1 fragmentation/latest-frame semantics."""
import struct, random
MAGIC=b'OALW'; VERSION=1; HEADER=24; PAYLOAD=48*1024

def fragment(role,seq,packet):
    count=(len(packet)+PAYLOAD-1)//PAYLOAD
    out=[]
    for i in range(count):
        chunk=packet[i*PAYLOAD:(i+1)*PAYLOAD]
        flags=(1 if i==0 else 0)|(2 if i+1==count else 0)
        out.append(MAGIC+bytes([VERSION,1 if role=='guide' else 0,flags,0])+struct.pack('>QIHH',seq,len(packet),i,count)+chunk)
    return out

class LatestAssembler:
    def __init__(self): self.state={}
    def push(self,msg):
        assert len(msg)>=HEADER and msg[:4]==MAGIC and msg[4]==VERSION
        role='guide' if msg[5] else 'main'
        seq,total,index,count=struct.unpack('>QIHH',msg[8:24])
        if total<=0 or total>32*1024*1024 or count<=0 or index>=count: return None
        s=self.state.get(role)
        if s and seq<s['seq']: return None
        if not s or seq!=s['seq'] or total!=s['total'] or count!=s['count']:
            s={'seq':seq,'total':total,'count':count,'parts':[None]*count}; self.state[role]=s
        if s['parts'][index] is None: s['parts'][index]=msg[HEADER:]
        if any(p is None for p in s['parts']): return None
        packet=b''.join(s['parts']); self.state.pop(role,None)
        return (role,seq,packet) if len(packet)==total else None

payload=bytes((i*17+31)&255 for i in range(PAYLOAD*3+777))
parts=fragment('main',42,payload)
assert len(parts)==4 and all(len(x)<=HEADER+PAYLOAD for x in parts)
random.Random(7).shuffle(parts)
a=LatestAssembler(); result=None
for p in parts: result=a.push(p) or result
assert result==('main',42,payload)
# Incomplete older frame is discarded when a newer sequence arrives.
a=LatestAssembler(); old=fragment('guide',100,b'a'*(PAYLOAD+9)); new=fragment('guide',101,b'b'*(PAYLOAD+11))
assert a.push(old[0]) is None
assert a.push(new[0]) is None
assert a.push(old[1]) is None
assert a.push(new[1])==('guide',101,b'b'*(PAYLOAD+11))
# Main and Guide state are independent.
a=LatestAssembler(); m=fragment('main',5,b'm'*100); g=fragment('guide',8,b'g'*200)
assert a.push(g[0])==('guide',8,b'g'*200)
assert a.push(m[0])==('main',5,b'm'*100)
print('PASS OALW v1 fragmentation/reassembly/latest-frame model')
