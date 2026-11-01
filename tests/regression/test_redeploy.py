"""Redeploy settings must remain data at the elevation boundary."""
from pathlib import Path
import json
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class RedeployTests(unittest.TestCase):
    def test_quoted_settings_are_never_executed(self):
        self.exercise_setting("split-tray-")

    def test_checkout_paths_with_quotes_and_spaces_stay_data(self):
        self.exercise_setting("split-tray-'quoted path-")

    def exercise_setting(self, prefix):
        with tempfile.TemporaryDirectory(prefix=prefix) as folder:
            root = Path(folder)
            for name in ('tools','src','build'): (root/name).mkdir()
            source = (ROOT/'tools/redeploy.ps1').read_text(encoding='utf-8-sig')
            prefix = source.split('# Run the install inline',1)[0]
            self.assertNotEqual(prefix,source)
            (root/'tools/redeploy.ps1').write_text(prefix,encoding='utf-8-sig')
            shutil.copyfile(ROOT/'src/split-tray.wh.cpp',root/'src/split-tray.wh.cpp')
            marker = root/'executed.txt'
            literal_path = str(marker).replace("'","''")
            value = "prefix'+$([IO.File]::WriteAllText('"+literal_path+"','executed'))+'suffix"
            def run(*args):
                result=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',*map(str,args)],capture_output=True,text=True,timeout=30)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            run(root/'tools/redeploy.ps1','-NoBuild','-Setting','defaultTray='+value)
            (root/'tools/install.ps1').write_text('param([switch]$Install,[switch]$Enable)',encoding='utf-8')
            probe=root/'probe.ps1'
            probe.write_text("""param([string]$Root)
$ErrorActionPreference='Stop'
function Set-ItemProperty { param($Path,$Name,$Value,$Type)
    [pscustomobject]@{name=$Name;value=$Value} | ConvertTo-Json -Compress | Add-Content -LiteralPath (Join-Path $Root 'observed.jsonl')
}
& (Join-Path $Root 'build/redeploy-elevated.ps1') -RepoRoot $Root -Transcript (Join-Path $Root 'build/mock.log') -OverridesPath (Join-Path $Root 'build/redeploy-settings.json')
""",encoding='utf-8-sig')
            run(probe,root)
            self.assertFalse(marker.exists(),'setting value executed as PowerShell code')
            entries=[json.loads(line) for line in (root/'observed.jsonl').read_text(encoding='utf-8-sig').splitlines()]
            self.assertEqual(entries,[{'name':'defaultTray','value':value}])

if __name__=='__main__': unittest.main()
