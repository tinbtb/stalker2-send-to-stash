import hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools/python-deps'))
import pefile
RUNTIME_STATUS='Two-family beta.4: author confirmed the new ASI works in-game on Steam v2.0.6 on 2026-10-22. Both fixtures validated offline; development-fixture gameplay untested. Earlier detailed regression checks remain documented; CI does not run the game.'
asi=ROOT/'build/SendToStash.asi';pe=pefile.PE(str(asi))
imports={d.dll.decode():[i.name.decode() if i.name else str(i.ordinal) for i in d.imports] for d in pe.DIRECTORY_ENTRY_IMPORT}
exports=[s.name.decode() for s in pe.DIRECTORY_ENTRY_EXPORT.symbols if s.name]
assert pe.FILE_HEADER.Machine==0x8664 and pe.FILE_HEADER.Characteristics&0x2000
assert {'InitializeASI','SendToStash_GetStatus'} <= set(exports)
assert not any('ue4ss' in n.lower() for n in imports)
allowed={'kernel32.dll','msvcrt.dll','bcrypt.dll','user32.dll','ntdll.dll','advapi32.dll'}
assert all(n.lower() in allowed or (n.lower().startswith('api-ms-win-crt-') and n.lower().endswith('.dll')) for n in imports),imports.keys()
if '--artifact-only' in sys.argv:
    report={'asi_sha256':hashlib.sha256(asi.read_bytes()).hexdigest(),'machine':'AMD64','dll':True,'exports':exports,'imports':imports,'ue4ss_dependency':False,'target_sites_verified':False,'runtime_test':RUNTIME_STATUS}
    (ROOT/'build/static-verification.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))
    sys.exit(0)
profile=json.loads((ROOT/'research/binary/asi-profile.json').read_text())
exe=ROOT/'research/binary/Stalker2-Win64-Shipping.exe';raw=exe.read_bytes();target=pefile.PE(data=raw,fast_load=True)
assert hashlib.sha256(raw).hexdigest()==profile['sha256']
for name,entry in profile['functions'].items():
    expected=bytes.fromhex(entry['prefix']);assert target.get_data(int(entry['rva'],16),len(expected))==expected,name
report={'asi_sha256':hashlib.sha256(asi.read_bytes()).hexdigest(),'machine':'AMD64','dll':True,'exports':exports,'imports':imports,'target_sha256':profile['sha256'],'target_sites_verified':len(profile['functions']),'ue4ss_dependency':False,'runtime_test':RUNTIME_STATUS}
(ROOT/'build/static-verification.json').write_text(json.dumps(report,indent=2))
print(json.dumps({k:v for k,v in report.items() if k!='imports'},indent=2));print('Imported DLLs:',', '.join(imports))
