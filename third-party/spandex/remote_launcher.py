# Author Zeran Zhu
# Copyright University of Illinois 2020

import os, sys

target = sys.argv[1][:-7] # trim off -remote



HOSTNAME = 'sarangi'
HOST = HOSTNAME + '.cs.illinois.edu'
workspace = os.path.dirname(os.path.realpath(__file__)) + '/../../'

os.system('cp ' + workspace + '/third-party/spandex/remote_hls.sh ~')


print('Launching remote worker ' + HOST + ' ...')
os.system('ssh -X $(whoami)@sarangi.cs.illinois.edu -t ". sim; . hls; export UP_STREAM_DIR=' + workspace + '; export MAKE_TARGET=' + target + '; sh remote_hls.sh;"')
