#!/usr/bin/python3

# Part of this code source comes from https://github.com/nviennot/core-to-core-latency using MIT license

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import argparse
import os
import sys

def load_data(filename):
    m = np.array(pd.read_json(filename))
    return np.tril(m) + np.tril(m).transpose()

def show_heapmap(m, title=None, subtitle=None, vmin=None, vmax=None, yticks=True, figsize=None):
    vmin = np.nanmin(m) if vmin is None else vmin
    vmax = np.nanmax(m) if vmax is None else vmax
    black_at = (vmin+3*vmax)/4
    subtitle = "Inter-core one-way data latency between CPU cores" if subtitle is None else subtitle
    
    isnan = np.isnan(m)

    plt.rcParams['xtick.bottom'] = plt.rcParams['xtick.labelbottom'] = False
    plt.rcParams['xtick.top'] = plt.rcParams['xtick.labeltop'] = True

    figsize = np.array(m.shape)*0.3 + np.array([6,1]) if figsize is None else figsize
    fig, ax = plt.subplots(figsize=figsize, dpi=130)
    
    fig.patch.set_facecolor('w')
    
    plt.imshow(np.full_like(m, 0.7), vmin=0, vmax=1, cmap = 'gray') # for the alpha value
    plt.imshow(m, cmap = plt.cm.get_cmap('viridis'), vmin=vmin, vmax=vmax)
    
    fontsize = 9 if vmax >= 100 else 10

    for (i,j) in np.ndindex(m.shape):
        t = "" if isnan[i,j] else f"{m[i,j]:.1f}" if vmax < 10.0 else f"{m[i,j]:.0f}"
        c = "w" if m[i,j] < black_at else "k"
        plt.text(j, i, t, ha="center", va="center", color=c, fontsize=fontsize)
        
    plt.xticks(np.arange(m.shape[1]), labels=[f"{i+1}" for i in range(m.shape[1])], fontsize=9)
    if yticks:
        plt.yticks(np.arange(m.shape[0]), labels=[f"CPU {i+1}" for i in range(m.shape[0])], fontsize=9)
    else:
        plt.yticks([])

    plt.tight_layout()
    plt.title(f"{title}\n" +
              f"{subtitle}\n" +
              f"Min={vmin:0.1f}ns Median={np.nanmedian(m):0.1f}ns Max={vmax:0.1f}ns",
              fontsize=11, linespacing=1.5)

parser = argparse.ArgumentParser(description='c2clat plot program')

# Profile Support
parser.add_argument('-i', '--input', required=True, type=str, help="Input JSON file to plot")
parser.add_argument('-o', '--out', type=str, help="Output file")

args = parser.parse_args(sys.argv[1:])

with open(args.input, "r") as f:
    fd = load_data(f)
machine=os.popen("cat /proc/cpuinfo | awk '/model name/{print $0 ; exit}' | cut -d \":\" -f 2").readline()

if args.out:
    plt.savefig(args.out, format="pdf", bbox_inches="tight")
else:
    plt.savefig("c2clat.pdf", format="pdf", bbox_inches="tight")
#  plt.show()

