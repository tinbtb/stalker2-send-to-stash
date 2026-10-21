import hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools/python-deps'))
import pefile
src=ROOT/'research/binary/Stalker2-Win64-Shipping.exe';raw=src.read_bytes();pe=pefile.PE(data=raw,fast_load=True)
functions={'MENU_SET_DATA':0x10977c4,'SELECT_ACTION':0x34f779e,'LOOKUP_ITEM':0x26d74b2,'GET_PLAYER':0x87ede,'TRANSFER_ITEM':0x3b3b30c,'TEXT_FROM_STRING':0x5818ccc,'GET_STORE':0x26e6726}
digest=hashlib.sha256(raw).digest()
profile={'size':len(raw),'sha256':digest.hex(),'image_size':pe.OPTIONAL_HEADER.SizeOfImage,'item_manager_global':'0xa2da4b0','functions':{n:{'rva':hex(r),'prefix':pe.get_data(r,24).hex()} for n,r in functions.items()}}
(ROOT/'research/binary/asi-profile.json').write_text(json.dumps(profile,indent=2))
lines=['/* Generated from the workspace executable copy; do not hand-edit. */','#pragma once',f'#define SUPPORTED_FILE_SIZE {len(raw)}ULL',f'#define SUPPORTED_IMAGE_SIZE 0x{pe.OPTIONAL_HEADER.SizeOfImage:x}u','static const unsigned char SUPPORTED_SHA256[32] = {'+','.join(f'0x{x:02x}' for x in digest)+'};']
for n,r in functions.items():
    lines += [f'#define RVA_{n} 0x{r:x}u',f'static const unsigned char BYTES_{n}[24] = '+'{'+','.join(f'0x{x:02x}' for x in pe.get_data(r,24))+'};']
(ROOT/'src').mkdir(exist_ok=True)
(ROOT/'src/build_profile.h').write_text('\n'.join(lines)+'\n')
print('Profile generated:',digest.hex())
