"""Create a reviewed dataset revision without modifying the shipped snapshot."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'.pio-tools/ai'))
from text_features import LABELS,normalize

SPLITS=('train','calibration','evaluation')

def read_rows(path):
    return [json.loads(line) for line in Path(path).read_text(encoding='utf-8').splitlines() if line.strip()]

def validate(groups,regression):
    seen={};families={}
    for split in SPLITS:
        if not groups[split]:raise ValueError('Empty split: '+split)
        for row in groups[split]:
            if row.get('label') not in LABELS or not isinstance(row.get('text'),str) or not 0<len(row['text'].encode('utf-8'))<=80:
                raise ValueError('Invalid label or 1..80 UTF-8 byte boundary: '+str(row))
            family=row.get('family')
            if not isinstance(family,str) or not family.strip():raise ValueError('A reviewed family is required: '+row['text'])
            key=normalize(row['text'])
            if not key:raise ValueError('Empty normalized input')
            if key in seen:raise ValueError('Duplicate/overlapping normalized text: '+row['text']+' ('+seen[key]+', '+split+')')
            seen[key]=split
            if family in families and families[family]!=split:raise ValueError('Family crosses dataset splits: '+family)
            families[family]=split
    reserved={normalize(r['text']) for r in regression}
    if any(key in reserved and split!='evaluation' for key,split in seen.items()):
        raise ValueError('Known user regression entered train/calibration')

def build(base,output,add_train=(),add_calibration=(),evaluation=None,version=None):
    base=Path(base);output=Path(output)
    manifest=json.loads((base/'manifest.json').read_text(encoding='utf-8'))
    groups={split:read_rows(base/(split+'.jsonl')) for split in SPLITS}
    for split in SPLITS:
        path=base/(split+'.jsonl')
        if hashlib.sha256(path.read_bytes()).hexdigest()!=manifest['files'][path.name]['sha256']:
            raise ValueError('Base dataset changed: '+str(path))
    changed=bool(add_train or add_calibration or evaluation)
    revision=version if version is not None else manifest['version']
    if changed and revision<=manifest['version']:raise ValueError('Changes require --version greater than the base revision')
    for split,paths in [('train',add_train),('calibration',add_calibration)]:
        for path in paths:
            rows=read_rows(path)
            for row in rows:row.setdefault('source','reviewed-manual-extension')
            groups[split]+=rows
    if evaluation:groups['evaluation']=read_rows(evaluation)
    regression=json.loads((ROOT/'data/ai/user_regression.json').read_text(encoding='utf-8'))
    validate(groups,regression)
    result={'version':revision,'feature_profile':5,'derived_from':str(base),
            'provenance':'Frozen reviewed corpus; no device documents are read',
            'limitations':['Family independence requires human review; text checks cannot identify all paraphrases',
                           'Known evaluation sets are regression, not new evidence of generalization'], 'files':{}}
    output.mkdir(parents=True,exist_ok=False)
    for split,rows in groups.items():
        name=split+'.jsonl'
        untouched=not ((split=='train' and add_train) or (split=='calibration' and add_calibration) or (split=='evaluation' and evaluation))
        payload=(base/name).read_bytes() if untouched else ''.join(json.dumps(r,ensure_ascii=False,sort_keys=True)+'\n' for r in rows).encode('utf-8')
        (output/name).write_bytes(payload)
        result['files'][name]={'cases':len(rows),'sha256':hashlib.sha256(payload).hexdigest(),'per_label':dict(Counter(r['label'] for r in rows))}
    (output/'manifest.json').write_bytes((json.dumps(result,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base',type=Path,default=ROOT/'data/ai/dataset');p.add_argument('--output',type=Path,required=True)
    p.add_argument('--add-train',type=Path,action='append',default=[])
    p.add_argument('--add-calibration',type=Path,action='append',default=[])
    p.add_argument('--evaluation',type=Path,help='Fresh, pre-labeled JSONL for a genuinely new evaluation')
    p.add_argument('--version',type=int)
    a=p.parse_args()
    try:print(json.dumps(build(a.base,a.output,a.add_train,a.add_calibration,a.evaluation,a.version),ensure_ascii=False,indent=2))
    except (ValueError,FileExistsError) as error:p.error(str(error))
