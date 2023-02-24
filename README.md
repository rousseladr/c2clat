# c2clat

A tool to measure CPU core to core latency (inter-core latency).

Build:

```console
$ make

```

Example usage:

```console
$ ./c2clat
 CPU    0    1    2    3    4    5    6    7
   0    0   92   66   62   16   56   54   53
   1   92    0   50   54   53   14   49   50
   2   66   50    0   48   51   49   13   48
   3   62   54   48    0   53   50   48   13
   4   16   53   51   53    0   53   51   53
   5   56   14   49   50   53    0   49   50
   6   54   49   13   48   51   49    0   48
   7   53   50   48   13   53   50   48    0
```
or

```console
$ make run
 CPU    0    1    2    3    4    5    6    7
   0    0   92   66   62   16   56   54   53
   1   92    0   50   54   53   14   49   50
   2   66   50    0   48   51   49   13   48
   3   62   54   48    0   53   50   48   13
   4   16   53   51   53    0   53   51   53
   5   56   14   49   50   53    0   49   50
   6   54   49   13   48   51   49    0   48
   7   53   50   48   13   53   50   48    0

```

Create plot using [gnuplot](http://gnuplot.sourceforge.net/):

```console
$ make gnuplot

```

Generate a pdf file using python scrip:

```console
$ make pdf

```

