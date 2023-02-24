CC=gcc
CFLAGS=-O3 -Wall -DNDEBUG -DUSE_SMT
LDFLAGS=-lnuma -pthread

PGR=c2clat

compile: $(PGR)

usage: $(PGR)
	./$< -h

pdf: runcsv
	./plot_heapmap_c2c.py

gnuplot: $(PGR)
	./$< -p | gnuplot -p

runcsv: $(PGR)
	./$< -c -s 10000

run: $(PGR)
	./$< -s 10000

$(PGR): c2clat.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f c2clat c2clat.csv c2clat.pdf

proper:
	rm -f c2clat.o
