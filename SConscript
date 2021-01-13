# RT-Thread building script for component

from building import *

cwd = GetCurrentDir()
src = Glob('src/*.c')
CPPPATH = [cwd + '/inc']

group = DefineGroup('logmgr', src, depend = ['PKG_USING_LOGMGR'], CPPPATH = CPPPATH)

Return('group')
