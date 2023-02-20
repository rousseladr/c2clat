CC=gcc
CFLAGS=-O3 -Wall -DNDEBUG -DUSE_SMT
LDFLAGS=-lnuma -pthread

PGR=c2clat

usage: $(PGR)
	./$< -h

display: runcsv
	./plot_heapmap_c2c.py

gnuplot: $(PGR)
	./$< -p | gnuplot -p

runcsv: $(PGR)
	./$< -c -s 10000 > $(PGR).csv

$(PGR): c2clat.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f c2clat c2clat.csv
