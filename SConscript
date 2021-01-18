# RT-Thread building script for component

from building import *

cwd = GetCurrentDir()
src = Glob('src/*.c')
CPPPATH = [cwd + '/inc']

if GetDepend(['LOGMGR_USING_SAMPLES']):
    src += Glob('samples/*.c')

group = DefineGroup('logmgr', src, depend = ['PKG_USING_LOGMGR'], CPPPATH = CPPPATH)

Return('group')
