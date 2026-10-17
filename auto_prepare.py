#!/usr/bin/env python3
"""Manage known lighting adapters across the user's registered Linux launchers."""
from pathlib import Path
import argparse
import json
import os

from automation.discovery import discover
from automation.hooks import install_hooks
from automation.prepare import ROOT, Store, serialized, prepare_prefix, sdk_files, pe_info, atomic_bytes
from automation.runtime import enabled, configuration, save_status


def main():
    parser=argparse.ArgumentParser()
    mode=parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--plan',action='store_true')
    mode.add_argument('--enable',action='store_true')
    mode.add_argument('--cycle',action='store_true')
    mode.add_argument('--prelaunch',action='store_true')
    parser.add_argument('--home',type=Path,default=Path.home())
    args=parser.parse_args(); home=args.home.resolve()
    inventory=discover(home)
    if args.plan:
        libraries=[]
        for target in inventory.targets.values():
            for game in target.games:
                for file in sdk_files(game):
                    try: architecture,family=pe_info(file)
                    except Exception: architecture,family=None,None
                    libraries.append({'file':str(file),'bits':architecture,'family':family})
        report={'prefixes':[{'path':str(t.prefix),'platforms':sorted(t.platforms),'exists':(t.prefix/'system.reg').exists()} for t in inventory.targets.values()],
                'proton':[str(p) for p in sorted(inventory.tools)],'libraries':libraries}
        print(json.dumps(report,indent=2)); return 0
    if not args.enable and not enabled(home): return 0
    with serialized(home):
        store=Store(home)
        if args.enable:
            try: config=json.loads(configuration(home).read_text())
            except (OSError,ValueError): config={}
            config.update({'enabled':True,'script':str(ROOT/'auto_prepare.py'),'status':str(home/'.local/share/openrgb-game-sync/automation/status.json'),'rest':True})
            atomic_bytes(configuration(home),(json.dumps(config)+'\n').encode())
            (ROOT/'auto_launch.sh').chmod(0o755)
        hooks=install_hooks(inventory,store)
        targets=list(inventory.targets.values())
        launch_prefix=os.environ.get('WINEPREFIX') if args.prelaunch else None
        if launch_prefix:
            games=[os.environ.get('STEAM_COMPAT_INSTALL_PATH') or os.environ.get('GAME_DIRECTORY')]
            target=inventory.add(launch_prefix,'Launch', [p for p in games if p and Path(p).is_dir()])
            targets=[target]
        reports=[prepare_prefix(t.prefix,t.games,store,force=args.prelaunch) for t in targets]
        report={'hooks':hooks,'prefixes':reports,'changed':sum(r['changed'] for r in reports),
                'ready':sum(not r['deferred'] and not r['errors'] for r in reports),
                'deferred':sum(bool(r['deferred']) for r in reports),'errors':hooks['errors']+[e for r in reports for e in r['errors']]}
        save_status(report,home)
        print(json.dumps(report,indent=2))
    return 0


if __name__=='__main__':
    try: raise SystemExit(main())
    except Exception as error:
        # A launcher pre-hook should fail open; the status file keeps the cause.
        if '--prelaunch' in os.sys.argv:
            save_status({'errors':[str(error)]}); raise SystemExit(0)
        raise
