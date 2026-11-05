"""Dataset maintenance checks; run after installing the CPU training dependencies."""
import json,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/ai'))
from prepare_dataset import build,validate

class DatasetTests(unittest.TestCase):
    def setUp(self):
        self.groups={s:[{'text':text,'label':'HORA','family':s,'source':'test'}] for s,text in
                     [('train','relógio do ensaio alfa'),('calibration','horário do ensaio beta'),('evaluation','horas do ensaio gama')]}
    def test_valid(self):validate(self.groups,[])
    def test_normalized_overlap(self):
        self.groups['calibration'][0]['text']='RELOGIO DO ENSAIO ALFA!'
        with self.assertRaises(ValueError):validate(self.groups,[])
    def test_family_leak(self):
        self.groups['calibration'][0]['family']='train'
        with self.assertRaises(ValueError):validate(self.groups,[])
    def test_unknown_label(self):
        self.groups['train'][0]['label']='INVENTADO'
        with self.assertRaises(ValueError):validate(self.groups,[])
    def test_utf8_limit(self):
        self.groups['train'][0]['text']='á'*41
        with self.assertRaises(ValueError):validate(self.groups,[])
    def test_user_regression_excluded(self):
        with self.assertRaises(ValueError):validate(self.groups,[{'text':self.groups['train'][0]['text']}])
    def test_copy_preserves_frozen_bytes(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as tmp:
            output=Path(tmp)/'dataset';build(ROOT/'data/ai/dataset',output)
            for name in ['train.jsonl','calibration.jsonl','evaluation.jsonl']:
                self.assertEqual((output/name).read_bytes(),(ROOT/'data/ai/dataset'/name).read_bytes())
    def test_new_revision_required(self):
        with self.assertRaises(ValueError):build(ROOT/'data/ai/dataset',ROOT/'build/must-not-be-created',add_train=['not-read.jsonl'])

if __name__=='__main__':unittest.main()
