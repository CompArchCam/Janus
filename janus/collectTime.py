#!/usr/bin/env python

import sys
import glob, os
import os.path
from os import walk

if len(sys.argv) == 2:
	with open(sys.argv[1]+".loop") as loopFile:
		for line in loopFile:
			loop = line.split()
			id = loop[0]
			name = loop[1]
			cycleFile = "Loop_"+id+"_cycles.txt"
			if os.path.exists(cycleFile):
				with open(cycleFile) as cycleFile:
					for line2 in cycleFile:
						ss = line2.split()
						frac = ss[1]
						invo = ss[2]
						if float(frac) > 0.01:
							print id + " " + name + " " + frac + " " + invo;
