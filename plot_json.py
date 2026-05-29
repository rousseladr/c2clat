#!/bin/env python3

import matplotlib.pyplot as plt
import json
import sys
import argparse
import os

import subprocess

def get_cpu_info():
    try:
        result = subprocess.run(['lscpu'], capture_output=True, text=True, check=True)
        lines = result.stdout.splitlines()

        sockets = None
        model_name = None

        for line in lines:
            if line.startswith('Socket(s):'):
                sockets = line.split(':')[1].strip()
            elif line.startswith('Model name:') or line.startswith('CPU(s):'):
                # Certains systèmes utilisent "Model name" ou "CPU(s)" pour le nom du processeur
                model_name = line.split(':', 1)[1].strip()

        if sockets and model_name:
            #return f"{sockets} x {model_name}"
            return f"1 x {model_name}"
        else:
            return "Information non trouvée"
    except subprocess.CalledProcessError as e:
        return f"Erreur lors de l'exécution de lscpu : {e}"


parser = argparse.ArgumentParser(description='memapper plot program')

# Profile Support
parser.add_argument('-i', '--input', required=True, type=str, help="Input JSON file to plot")
parser.add_argument('-o', '--out', type=str, help="Output file")


args = parser.parse_args(sys.argv[1:])

with open(args.input, "r") as f:
    data = json.load(f)

axes = range(0, len(data.keys()))

z = []
for k in data.keys():
    loc = []
    for kk in data.keys():
        v = data[k][kk]
        loc.append(v)
    z.append(loc)

fig, ax = plt.subplots()
   
c = ax.pcolormesh(axes, axes, z, shading='auto', vmax=400)

cbar = fig.colorbar(c, ax = ax)
cbar.set_label('Time (ns)')  # Texte à côté de la légende

#machine=os.popen("cat /proc/cpuinfo | awk '/model name/{print $0 ; exit}' | cut -d \":\" -f 2").readline()
machine = get_cpu_info()
plt.title(machine)

if args.out:
    plt.savefig(args.out)
else:
    plt.show()

