#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
checks = []

def require(cond, msg):
    checks.append((bool(cond), msg))

for rel in [
    'docs/WHY_OPENASTROLINK.md','docs/MANIFESTO.md','docs/USER_GUIDE.md',
    'docs/DEVELOPER_GUIDE.md','docs/INTEGRATION_GUIDE.md',
    'docs/uk/WHY_OPENASTROLINK.md','docs/uk/MANIFESTO.md','docs/uk/USER_GUIDE.md',
    'docs/uk/DEVELOPER_GUIDE.md','docs/uk/INTEGRATION_GUIDE.md',
    'site/about/index.html','site/manifesto/index.html','site/docs/index.html',
    'site/docs/users/index.html','site/docs/developers/index.html','site/docs/integrators/index.html','site/docs/drivers/index.html',
    'site/uk/about/index.html','site/uk/manifesto/index.html','site/uk/docs/index.html',
    'site/uk/docs/users/index.html','site/uk/docs/developers/index.html','site/uk/docs/integrators/index.html','site/uk/docs/drivers/index.html',
    'site/sitemap.xml','site/robots.txt',
]:
    require((root/rel).is_file(), f'{rel} exists')

home = (root/'site/index.html').read_text(encoding='utf-8')
ua = (root/'site/uk/index.html').read_text(encoding='utf-8')
require('id="why"' in home and '/manifesto/' in home and '/docs/' in home, 'EN home exposes rationale/manifesto/docs')
require('id="why"' in ua and '/uk/manifesto/' in ua and '/uk/docs/' in ua, 'UA home exposes rationale/manifesto/docs')
why = (root/'docs/WHY_OPENASTROLINK.md').read_text(encoding='utf-8')
require('Existing ecosystems' in why or 'existing astronomy' in why, 'rationale explicitly addresses existing solutions')
manifest = (root/'docs/MANIFESTO.md').read_text(encoding='utf-8')
require('Hardware belongs to the node' in manifest and 'Recording is more important than preview' in manifest, 'manifesto contains core principles')
index = (root/'docs/DOCUMENTATION_INDEX.md').read_text(encoding='utf-8')
require('INTEGRATION_GUIDE.md' in index and 'USER_GUIDE.md' in index and 'DEVELOPER_GUIDE.md' in index, 'documentation index exposes audience guides')

failed = [m for ok,m in checks if not ok]
for ok,msg in checks:
    print(('PASS' if ok else 'FAIL') + ': ' + msg)
print(f'PUBLIC_SITE_DOCS_CHECK: {len(checks)-len(failed)}/{len(checks)} PASS')
sys.exit(1 if failed else 0)
