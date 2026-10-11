#!/usr/bin/env python3
"""Losslessly promote an independently reviewed, exact matched E1-P4 candidate."""
import argparse
import base64
import gzip
import hashlib
import json
import re
from pathlib import Path

def sha(raw):return hashlib.sha256(raw).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--candidate',type=Path,required=True)
    ap.add_argument('--review',type=Path,required=True)
    ap.add_argument('--output-dir',type=Path,required=True)
    args=ap.parse_args();raw=args.candidate.read_bytes();r=json.loads(raw)
    review=args.review.read_bytes()
    if b'**ACCEPT**' not in review:raise ValueError('independent review acceptance absent')
    reviewed_hashes=re.findall(rb'^Candidate-SHA256: ([0-9a-f]{64})$',review,re.MULTILINE)
    if reviewed_hashes!=[sha(raw).encode()]:
        raise ValueError('independent review is not bound to the exact candidate SHA-256')
    if r['status']!='MATCHED_E1AP_CANDIDATE_REQUIRES_INDEPENDENT_REVIEW' or r['unresolved_disagreements']!=0:
        raise ValueError('candidate is not matched')
    if r['counts']['messages']!=72 or r['counts']['procedures']!=40 or len(r['identities'])!=72:
        raise ValueError('identity closure absent')
    if any(i['status']!='PASS' or any(c['status']!='PASS' for c in i['cases']) for i in r['identities']):
        raise ValueError('candidate contains failure')
    envelope={'format':'nrforge-e1ap-lossless-profile-envelope-v1','profile_bytes':len(raw),'profile_sha256':sha(raw),
              'data':base64.b64encode(gzip.compress(raw,mtime=0)).decode()}
    expanded=gzip.decompress(base64.b64decode(envelope['data'],validate=True))
    if expanded!=raw:raise ValueError('lossless transport changed payload')
    stored=(json.dumps(envelope,indent=2)+'\n').encode()
    summary={'status':'ACCEPTED_FINITE_E1AP_WIRE_PROFILE','phase':'E1-P4','baseline_commit':r['baseline_commit'],
             'independent_review_sha256':sha(review),'profile_file':'accepted-profile.json',
             'profile_storage_format':envelope['format']+'; expand and verify before --accepted-profile',
             'profile_storage_sha256':sha(stored),'profile_bytes':len(raw),'profile_sha256':sha(raw),
             'executable_sha256':r['executable_sha256'],'counts':r['counts'],
             'coverage':{k:r['coverage'][k] for k in ['declared_row_occurrences','covered_row_occurrences','missing_row_occurrences',
                       'unsigned64_boundary_values','unsigned64_path_values']},
             'distinct_choice_selections':len(r['coverage']['actual_choice_selections']),
             'repeated_nested_optional_omission_entries':len(r['coverage']['nested_optional_omissions']),
             'unsigned64_boundary_cases':sum('unsigned64-boundary-' in c['id'] for i in r['identities'] for c in i['cases']),
             'wire_octet_range':[min(c['octet_count'] for i in r['identities'] for c in i['cases']),
                                 max(c['octet_count'] for i in r['identities'] for c in i['cases'])],
             'unresolved_disagreements':0,'production_changes':False,'limitations':r['limitations']}
    args.output_dir.mkdir(parents=True,exist_ok=True)
    (args.output_dir/'accepted-profile.json').write_bytes(stored)
    (args.output_dir/'qualification-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({'profile_sha256':sha(raw),'storage_sha256':sha(stored),'counts':r['counts']}))
if __name__=='__main__':main()
