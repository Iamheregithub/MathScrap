import importlib.util
from pathlib import Path
import unittest
spec = importlib.util.spec_from_file_location('analysis', Path(__file__).with_name('analyze-memory.py'))
a = importlib.util.module_from_spec(spec)
spec.loader.exec_module(a)

class MemoryAnalysis(unittest.TestCase):
    def samples(self):
        return [dict(cycle=i, pid=123, privateBytes=100*1024**2, handles=100, gdiObjects=20, userObjects=20) for i in range(40)]
    def test_stable_and_warmup(self):
        s = self.samples()
        for i in range(5): s[i]['privateBytes'] = i*20*1024**2
        self.assertEqual(a.analyze(s)['status'], 'no-growth-flag')
    def test_continuous_growth(self):
        s = self.samples()
        for i in range(40): s[i]['privateBytes'] += i*2*1024**2
        self.assertEqual(a.analyze(s)['status'], 'suspicious-growth')
    def test_handle_growth(self):
        s = self.samples()
        for i in range(40): s[i]['handles'] += i*5
        self.assertTrue(a.analyze(s)['counters']['handles']['suspicious'])
    def test_repeated_workload_units(self):
        s = self.samples()
        for i, sample in enumerate(s):
            sample['workloadRepeatsPerSample'] = 4
            sample['privateBytes'] += i*4*1024**2
        result = a.analyze(s)
        self.assertEqual(result['workloadCycles'], 160)
        self.assertAlmostEqual(result['counters']['privateBytes']['slopePerWorkloadCycle'], 1024**2)
        s[-1]['workloadRepeatsPerSample'] = 1
        with self.assertRaises(ValueError): a.analyze(s)
    def test_incomplete_and_restarted(self):
        with self.assertRaises(ValueError): a.analyze(self.samples()[:-1])
        s = self.samples(); s[-1]['pid'] = 456
        with self.assertRaises(ValueError): a.analyze(s)

if __name__ == '__main__': unittest.main()
