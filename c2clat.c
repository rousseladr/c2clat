/*
 Copyright 2020 Erik Rigtorp <erik@rigtorp.se>
 SPDX-License-Identifier: MIT

 Copyright 2023 Adrien Roussel <adrien.roussel@protonmail.com>
 SPDX-License-Identifier: MIT

 Copyright 2023 Hugo Taboada <hmt23@pm.me>
 SPDX-License-Identifier: MIT

 Measure inter-core one-way data latency

 Plot results using gnuplot:
 $ c2clat -p | gnuplot -p

*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include <time.h>

#include <sys/utsname.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>


#include <pthread.h>
#include <errno.h>

#include <numa.h>
#include <numaif.h>

typedef struct timespec struct_time;

#define gettime(t) clock_gettime(CLOCK_MONOTONIC_RAW, t)
#define get_sub_seconde(t) (1e-9*(double)t.tv_nsec)

/** return time in second
*/
double get_elapsedtime(void)
{
  struct_time st;
  int err = gettime(&st);
  if (err !=0) return 0;
  return (double)st.tv_sec + get_sub_seconde(st);
}

void pinThread(int cpu) {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  pthread_t current_thread = pthread_self();
  if(pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &set) != 0)
  {
    perror("pthread_setaffinity_np");
    exit(EXIT_FAILURE);
  }
}

long pinMemory(void* addr, size_t size, int cpu)
{
  nodemask_t target_nodemask;
  struct bitmask * bindmask = numa_bitmask_alloc(numa_num_possible_nodes());
  numa_bitmask_clearall(bindmask);
  numa_bitmask_setbit(bindmask, numa_node_of_cpu(cpu));
  copy_bitmask_to_nodemask(bindmask, &target_nodemask);
  numa_bitmask_free(bindmask);

  return mbind(addr, size, MPOL_BIND, target_nodemask.n, 128, MPOL_MF_MOVE);
}

typedef struct {
  uint64_t *a;
  uint64_t *b;
  int nsamples;
  int cpu;
} thread_args;

void* thread_function(void* args)
{
  thread_args *cur_args = (thread_args*)args;
  int cpu = cur_args->cpu;
  int nsamples = cur_args->nsamples;
  uint64_t* a = cur_args->a;
  uint64_t* b = cur_args->b;

  pinThread(cpu);
  for (int m = 0; m < nsamples; ++m)
  {
    for (uint64_t n = 0; n < 100; ++n)
    {
      while (__atomic_load_n(a, __ATOMIC_ACQUIRE) != n)
        ;
      __atomic_store_n(b, n, __ATOMIC_RELEASE);
    }
  }

  return 0;
}

int main(int argc, char *argv[])
{

  int nsamples = 1000;
  bool gnuplot = false;
  bool csvplot = false;

  int opt;
  while ((opt = getopt(argc, argv, "chps:")) != -1)
  {
    switch (opt)
    {
      case 'p':
        gnuplot = true;
        break;
      case 'c':
        gnuplot = false;
        csvplot = true;
        break;
      case 's':
        nsamples = atoi(optarg);
        break;
      case 'h':
        goto usage;
        break;
      default:
        goto usage;
    }
  }

  if (optind != argc)
  {
usage:
    fprintf(stdout, "c2clat 2.0.0\n");
    fprintf(stdout, "usage: c2clat\n\t[-c generate csv output]\n\t[-h print this help]\n\t[-p plot with gnuplot]\n\t[-s number_of_samples]\n");
    fprintf(stdout, "\nPlot results using gnuplot:\n");
    fprintf(stdout, "c2clat -p | gnuplot -p\n");
    fprintf(stdout, "\nPlot results using csv:\n");
    fprintf(stdout, "c2clat -c && ./plot_heapmap_c2c.py\n");
    exit(EXIT_SUCCESS);
  }

  cpu_set_t set;
  CPU_ZERO(&set);
  if (sched_getaffinity(0, sizeof(set), &set) != 0)
  {
    perror("sched_getaffinity");
    exit(EXIT_FAILURE);
  }

  int num_cpus = get_nprocs();

  bool* smt = (bool*)malloc(sizeof(bool) * CPU_SETSIZE);
  // enumerate available CPUs
  int* cpus = malloc(sizeof(int) * num_cpus);
  int li=0;
  for (int i = 0; i < CPU_SETSIZE; ++i)
  {
    smt[i] = false;
    if (CPU_ISSET(i, &set))
    {
      cpus[li] = i;
      li++;
    }
  }
  num_cpus = li;

  FILE* input;
  int nb_phys_cpus = 0;
  // This loop eliminates the hyper-threads to only conserve one PU per processor
  for(int i = 0; i < num_cpus; ++i)
  {
    char input_file[1024];
    sprintf(input_file, "/sys/devices/system/cpu/cpu%d/topology/thread_siblings_list", cpus[i]);
    input = fopen(input_file, "r");
    if(input == NULL)
    {
      perror("fopen");
      exit(EXIT_FAILURE);
    }
    char* line = NULL;
    size_t len;
    // Get the list of SMT on the cpu cpus[i]
    // Format gives list of PU separated by commas
    if(getline(&line, &len, input) == -1)
    {
      perror("getline");
      exit(EXIT_FAILURE);
    }

    // We are only interested by the first PU in the list,
    // so find the 1st occurence of the comma
    char* delim = strpbrk(line, ",");
    if(delim != NULL)
    {
      // Ends the line by filling it with '\0'
      // and read the value before ',' symbol
      *delim = '\0';
    }

    // Convert into integer
    int cur_cpu = atoi(line);

    // if cur_cpu has already been set (false in smt array), then continue
    // Else, set the pu to true
    if(!smt[cur_cpu] && cur_cpu >= 0 && cur_cpu < CPU_SETSIZE)
    {
      nb_phys_cpus++;
      smt[cur_cpu] = true;
    }
    fclose(input);
    if(line != NULL)
    {
      free(line);
    }
  }

  int *phys_cpus = (int*) malloc(sizeof(int) * nb_phys_cpus);
  li = 0;
  for(int i = 0; i < CPU_SETSIZE; ++i)
  {
    if(smt[i])
    {
      phys_cpus[li] = i;
      li++;
    }
  }
  free(smt);
  free(cpus);

  num_cpus = nb_phys_cpus;
  cpus = phys_cpus;

  double* data = (double*)malloc(num_cpus * num_cpus * sizeof(double));
  memset(data, 0, num_cpus * num_cpus * sizeof(double));

  for (int i = 0; i < num_cpus; ++i)
  {
    for (int j = 0; j < num_cpus; ++j)
    {

      if(i == j) continue;

      uint64_t *btest1 = mmap ( NULL, sizeof(uint64_t), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0 );
      if(btest1 == MAP_FAILED)
      {
        fprintf(stderr, "Error: unable to allocate %ld size of memory at line %d\n", sizeof(uint64_t), __LINE__);
        return EXIT_FAILURE;
      }

      uint64_t *btest2 = mmap ( NULL, sizeof(uint64_t), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0 );
      if(btest2 == MAP_FAILED)
      {
        fprintf(stderr, "Error: unable to allocate %ld size of memory at line %d\n", sizeof(uint64_t), __LINE__);
        return EXIT_FAILURE;
      }

      if(pinMemory((void*)btest2, sizeof(uint64_t), cpus[i]) != 0)
      {
        fprintf(stderr, "Error: unable to bind ptr1 on numa node %d\n", numa_node_of_cpu(cpus[i]));
        perror("mbind");
        return EXIT_FAILURE;
      }

      if(pinMemory((void*)btest1, sizeof(uint64_t), cpus[j]) != 0)
      {
        fprintf(stderr, "Error: unable to bind ptr2 on numa node %d\n", numa_node_of_cpu(cpus[j]));
        perror("mbind");
        return EXIT_FAILURE;
      }

      *btest1 = *btest2 = -1;

      thread_args *args = malloc(sizeof(thread_args));
      args->a = btest1;
      args->b = btest2;
      args->nsamples = nsamples;
      args->cpu = cpus[i];

      pthread_t thread_i;
      if(pthread_create (& thread_i, NULL, thread_function, args) != 0)
      {
        fprintf(stderr, "pthread_create error\n");
        free(data);
        exit(EXIT_FAILURE);
      }

      double rtt = 0.;

      pinThread(cpus[j]);
      for (int m = 0; m < nsamples; ++m)
      {
        *btest1 = *btest2 = -1;
        double ts1 = get_elapsedtime();
        for (uint64_t n = 0; n < 100; ++n)
        {
          __atomic_store_n(btest1, n, __ATOMIC_RELEASE);
          while (__atomic_load_n(btest2, __ATOMIC_ACQUIRE) != n)
            ;
        }
        double ts2 = get_elapsedtime();
        rtt = ts2 - ts1;
      }

      pthread_join(thread_i, NULL);

      data[i * num_cpus + j] = (rtt / 2 / 100);

      munmap(btest1, sizeof(uint64_t));
      munmap(btest2, sizeof(uint64_t));
      free(args);
    }
  }

  if (!gnuplot && !csvplot)
  {
    fprintf(stdout, " %*s", 4, "CPU");
    for (int i = 0; i < num_cpus; ++i)
    {
      fprintf(stdout, " %*d", 4, cpus[i]);
    }

    fprintf(stdout, "\n");
    for (int i = 0; i < num_cpus; ++i)
    {
      fprintf(stdout, " %*d", 4, cpus[i]);
      for (int j = 0; j < num_cpus; ++j)
      {
        fprintf(stdout, " %*.2lf", 4, 1E9*data[i * num_cpus + j]);
      }
      fprintf(stdout, "\n");
    }
  }
  if (gnuplot)
  {
    fprintf(stdout, "set title \"Inter-core one-way data latency between CPU cores\"\n");
    fprintf(stdout, "set xlabel \"CPU\"\n");
    fprintf(stdout, "set ylabel \"CPU\"\n");
    fprintf(stdout, "set cblabel \"Latency (ns)\"\n");
    fprintf(stdout, "$data << EOD\n");

    fprintf(stdout, " %*s", 4, "CPU");
    for (int i = 0; i < num_cpus; ++i)
    {
      fprintf(stdout, " %*d", 4, cpus[i]);
    }

    fprintf(stdout, "\n");
    for (int i = 0; i < num_cpus; ++i)
    {
      fprintf(stdout, " %*d", 4, cpus[i]);
      for (int j = 0; j < num_cpus; ++j)
      {
        fprintf(stdout, " %*.2lf", 4, 1E9*data[i * num_cpus + j]);
      }
      fprintf(stdout, "\n");
    }


    fprintf(stdout, "EOD\n");
    fprintf(stdout, "plot '$data' matrix rowheaders columnheaders using 2:1:3 ");
    fprintf(stdout, "with image\n");
  }

  if(csvplot)
  {
    FILE* output;
    output = fopen("c2clat.csv", "w");

    for (int i = 0; i < num_cpus; ++i)
    {
      for (int j = 0; j < num_cpus; ++j)
      {
        if (j<i) {
          fprintf(output, "%*.2lf,", 4, 1E9*data[i * num_cpus + j]);
        }
        else if( j != num_cpus-1) {
          fprintf(output, ",");
        }
      }
      fprintf(output, "\n");
    }
    fclose(output);
  }


  free(cpus);
  free(data);

  return 0;
}
