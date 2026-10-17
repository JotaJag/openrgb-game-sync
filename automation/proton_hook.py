"""Runs after Proton has initialized its prefix, immediately before the game."""
from pathlib import Path
import os
import sys

from automation.prepare import Store, serialized, prepare_prefix


def install_hook(main):
    session = getattr(main,'g_session',None)
    proton = getattr(main,'g_proton',None)
    compat = getattr(main,'g_compatdata',None)
    if not session or not proton or not compat or not callable(getattr(session,'run',None)): return False
    if getattr(session,'_orgbgs_prepared_hook',False): return True
    original = session.run
    def before_run(*args,**kwargs):
        try:
            from automation.runtime import enabled, save_status
            if enabled():
                prefix = Path(session.env.get('WINEPREFIX',compat.prefix_dir))
                env = dict(os.environ,**session.env)
                game = env.get('STEAM_COMPAT_INSTALL_PATH') or env.get('GAME_DIRECTORY')
                games = [Path(game)] if game and Path(game).is_dir() else []
                from automation.process import bounded_run
                worker=Path(__file__).with_name('launch_worker.py')
                code=bounded_run([sys.executable,str(worker),str(prefix),str(proton.wine_bin)] +
                                 [str(p) for p in games],timeout=6,env=env)
                if code: save_status({'errors':['Preparación RGB omitida; el juego sigue arrancando']})
        except Exception as exc:
            # RGB integration must never prevent a game from launching.
            try: save_status({'errors':['Preparación RGB omitida: '+str(exc)]})
            except Exception: pass
            sys.stderr.write('OpenRGB Game Sync: preparation skipped: '+str(exc)+'\n')
        return original(*args,**kwargs)
    session.run = before_run
    session._orgbgs_prepared_hook = True
    return True
