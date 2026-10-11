"""Project finite historical E1-P4 bytes; do not run or relabel qualification."""
import base64
import gzip
import hashlib
import json
from pathlib import Path
repo = Path(__file__).resolve().parents[3]
raw = (repo/'tools/e1ap-wire-qualification/accepted-profile.json').read_bytes()
envelope = json.loads(raw)
expanded = gzip.decompress(base64.b64decode(envelope['data'], validate=True))
assert hashlib.sha256(expanded).hexdigest() == envelope['profile_sha256']
profile = json.loads(expanded)
rows = []
for entry in profile['identities']:
    for case in entry['cases']:
        rows.append({key: case[key] for key in ('id','wire_hex','wire_sha256','criticality')} |
                    {key: entry[key] for key in ('message','role','code')})
assert len(rows) == 1341 and len({r['message'] for r in rows}) == 72
here = Path(__file__).resolve().parent
(here/'golden.json').write_text(json.dumps({'source_profile_sha256':hashlib.sha256(raw).hexdigest(),
    'claim':'Historical finite wire bytes reused for Python integration; no new wire qualification.',
    'outer_extension_policy':profile['outer_extension_policy'], 'cases':rows},indent=2)+'\n')
fixtures=json.loads((repo/'tools/e1ap-sdk-consumer/native-fixtures.json').read_text())
(here/'e1-setup-native.json').write_text(json.dumps(fixtures,indent=2)+'\n')
