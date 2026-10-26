"""Emit exact, relocation-normalized Steam 2.1.0 profiles after native review.
The mapped RVAs are review inputs, not runtime fallbacks. No game code is emitted.
"""
import sys,json,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'tools/python-deps'))
import pefile,capstone
from capstone.x86 import X86_OP_MEM,X86_OP_IMM,X86_REG_RIP
raw=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(raw).hexdigest()=='2bd3f115b959a8eee7e5c4b42b0142dd0025bd437e1014f92dfc60b9afee5479'
p=pefile.PE(data=raw,fast_load=True);p.parse_data_directories(directories=[3]);base=p.OPTIONAL_HEADER.ImageBase
ends={e.struct.BeginAddress:e.struct.EndAddress for e in p.DIRECTORY_ENTRY_EXCEPTION}
mapping=json.loads((R/'src/profile_210.json').read_text(encoding='utf-8-sig'));md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
arr=lambda b:'{'+','.join(map(str,b))+'}'
for mod,typ,prefix in [(mapping['mod'],'Hf' if mapping['mod']=='holdfire' else 'Sts','hf' if mapping['mod']=='holdfire' else 'sts')]:
 funcs=mapping[mod];names=list(funcs);addrs=list(funcs.values());edges=[];globals_=[];out=['/* Steam v2.1.0 exact normalized code/layout family; see PORT_210.md. */','#pragma once']
 leafs={funcs[k]:n for k,n in [('PAWN',20),('WEAK',84),('FOCUS_ACCESSOR',15)]} if mod=='holdfire' else {}
 glob={int(k):v for k,v in mapping['holdfire_globals'].items()} if mod=='holdfire' else {mapping['stash_manager']:0}
 def profile(name,start,index,helper=False):
  size=leafs.get(start,ends.get(start,0)-start);assert size>0
  data=bytearray(p.get_data(start,size));masks=[];consumed=0
  for i in md.disasm(bytes(data),base+start):
   consumed+=i.size;off=i.address-base-start
   for op in i.operands:
    target=None;disp=None
    if op.type==X86_OP_MEM and op.mem.base==X86_REG_RIP:
     disp=i.disp_offset;target=i.address+i.size+i.disp-base
     if not helper and target in glob:globals_.append((index,off+disp,off+i.size,glob[target]))
    elif op.type==X86_OP_IMM and (i.group(capstone.CS_GRP_CALL) or i.group(capstone.CS_GRP_JUMP)):
     target=op.imm-base
     if not start<=target<start+size:disp=i.imm_offset;assert i.imm_size==4
    if disp is not None:
     masks.append((off+disp,4))
     if not helper and target in addrs:edges.append((index,off+disp,off+i.size,addrs.index(target)))
  assert consumed==size,(name,consumed,size)
  norm=bytearray(data);anchor_mask=bytearray([255]*32)
  for off,n in masks:
   norm[off:off+n]=bytes(n)
   for x in range(off,min(off+n,32)):anchor_mask[x]=0
  anchor=bytes(x&y for x,y in zip(data[:32],anchor_mask)).ljust(32,b'\0')
  mn=f'v210_{prefix}_masks_{index}'
  out.append(f'static const {typ}Mask {mn}[] = '+'{'+','.join('{'+str(a)+','+str(n)+'}' for a,n in masks)+'};')
  return '{'+f'"{name}",{size},{arr(anchor)},{arr(anchor_mask)},{arr(hashlib.sha256(norm).digest())},{mn},{len(masks)}'+(f',{int(start in leafs)}' if mod=='holdfire' else '')+'}'
 rows=[profile(n,a,i) for i,(n,a) in enumerate(funcs.items())]
 if mod=='holdfire':
  helper=profile('OUTLINED_LAYOUT_HELPER',mapping['holdfire_helper'],99,True)
  out.append('static const HfProfile v210_helper_profile = '+helper+';')
  literal_target=funcs['CAPSULE_CLASS']+187+int.from_bytes(p.get_data(funcs['CAPSULE_CLASS']+183,4),'little',signed=True)
  assert p.get_data(literal_target,34)=='CapsuleComponent\0'.encode('utf-16le')
 out += [f'static const {typ}Profile v210_{prefix}_profiles[{prefix.upper()}_PROFILE_COUNT] = '+'{'+',\n'.join(rows)+'};',f'static const {typ}Reference v210_{prefix}_edges[] = '+'{'+','.join(arr(e) for e in edges)+'};',f'static const {typ}Reference v210_{prefix}_'+('global_refs' if mod=='holdfire' else 'manager_refs')+'[] = {'+','.join(arr(e) for e in globals_)+'};']
 if mod=='holdfire':out += ['static const HfReference v210_hf_capsule_name_ref = {11,183,187,0};']
 assert edges and globals_
 (R/'src/address_profile_210.h').write_text('\n'.join(out)+'\n')
 print(mod,len(rows),'profiles',len(edges),'native edges',len(globals_),'globals')
