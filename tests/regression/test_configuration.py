"""Regression test: portable backups never write outside their chosen mod keys."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import uuid
import winreg

ROOT = Path(__file__).resolve().parents[2]

class ConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.backup = Path(self.tmp.name) / 'backup.json'
        self.key = 'Software\\SplitTrayTests\\' + uuid.uuid4().hex
        self.base = 'HKCU:\\' + self.key
        self.settings = self.key + '\\Mods\\local@split-tray\\Settings'
        self.state = self.key + '\\ModsWritable\\local@split-tray\\LocalStorage'
        self.put(self.settings, 'defaultTray', 'secondary')
        self.put(self.settings, 'maxVisibleIcons', 8)
        self.put(self.state, 'iconPlacement', 'app.exe#1=s')
        self.put(self.state, 'embeddedIconOrder', 'app.exe#1')

    def put(self, path, name, value):
        with winreg.CreateKey(winreg.HKEY_CURRENT_USER, path) as key:
            winreg.SetValueEx(key, name, 0, winreg.REG_DWORD if isinstance(value,int) else winreg.REG_SZ, value)

    def read(self, path, name):
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, path) as key:
            return winreg.QueryValueEx(key, name)[0]

    def run_tool(self, action, ok=True):
        result = subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',
            str(ROOT/'tools'/'configuration.ps1'),'-Action',action,'-Path',str(self.backup),
            '-RegistryBase',self.base],capture_output=True,text=True,timeout=30)
        self.assertEqual(result.returncode == 0, ok, result.stdout + result.stderr)
        return result

    def tearDown(self):
        def remove(path):
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER,path,0,winreg.KEY_ALL_ACCESS) as key:
                while True:
                    try: child=winreg.EnumKey(key,0)
                    except OSError: break
                    remove(path+'\\'+child)
            winreg.DeleteKey(winreg.HKEY_CURRENT_USER,path)
        remove(self.key)
        self.tmp.cleanup()

    def test_round_trip_replaces_only_selected_mod_and_keeps_recovery(self):
        self.run_tool('Export')
        data=json.loads(self.backup.read_text(encoding='utf-8-sig'))
        self.assertEqual(data['format'],'SplitTray')
        self.assertEqual(len(data['settings']),2)
        self.put(self.settings,'defaultTray','primary')
        self.put(self.settings,'maxVisibleIcons',20)
        self.put(self.state,'iconPlacement','other=s')
        self.run_tool('Import')
        self.assertEqual(self.read(self.settings,'defaultTray'),'secondary')
        self.assertEqual(self.read(self.settings,'maxVisibleIcons'),8)
        self.assertEqual(self.read(self.state,'iconPlacement'),'app.exe#1=s')
        self.assertEqual(len(list(self.backup.parent.glob('*.before-restore-*.json'))),1)

    def test_retired_diagnostic_setting_round_trips(self):
        self.put(self.settings,'dumpXamlTree',0)
        self.run_tool('Export')
        self.put(self.settings,'dumpXamlTree',1)
        self.run_tool('Import')
        self.assertEqual(self.read(self.settings,'dumpXamlTree'),0)

    def test_invalid_entry_rejects_entire_import_before_writing(self):
        self.run_tool('Export')
        data=json.loads(self.backup.read_text(encoding='utf-8-sig'))
        data['settings'][0]['value']='changed'
        data['state'].append({'name':'..\\foreign','kind':'String','value':'bad'})
        self.backup.write_text(json.dumps(data),encoding='utf-8')
        self.run_tool('Import',ok=False)
        self.assertEqual(self.read(self.settings,'defaultTray'),'secondary')
        self.assertEqual(len(list(self.backup.parent.glob('*.before-restore-*.json'))),0)

    def test_duplicate_entries_and_enabled_mod_are_rejected(self):
        self.run_tool('Export')
        original=json.loads(self.backup.read_text(encoding='utf-8-sig'))
        duplicate=json.loads(json.dumps(original))
        duplicate['state'].append(duplicate['state'][0])
        self.backup.write_text(json.dumps(duplicate),encoding='utf-8')
        self.run_tool('Import',ok=False)
        self.backup.write_text(json.dumps(original),encoding='utf-8')
        self.put(self.key+'\\Mods\\local@split-tray','Disabled',0)
        self.run_tool('Import',ok=False)
        self.assertEqual(self.read(self.state,'iconPlacement'),'app.exe#1=s')

if __name__=='__main__': unittest.main()
