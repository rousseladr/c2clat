// © 2020 Erik Rigtorp <erik@rigtorp.se>
// SPDX-License-Identifier: MIT

// Measure inter-core one-way data latency
//
// Build:
// g++ -O3 -DNDEBUG c2clat.cpp -o c2clat -pthread
//
// Plot results using gnuplot:
// $ c2clat -p | gnuplot -p

#define _GNU_SOURCE

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/utsname.h>

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
  if (sched_setaffinity(0, sizeof(set), &set) == -1)
  {
    perror("sched_setaffinity");
    exit(1);
  }
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
  bool plot = false;

  int opt;
  while ((opt = getopt(argc, argv, "ps:")) != -1)
  {
   switch (opt)
   {
     case 'p':
       plot = true;
       break;
     case 's':
       nsamples = atoi(optarg);
       break;
     default:
       goto usage;
   }
  }

  if (optind != argc)
  {
  usage:
   fprintf(stderr, "c2clat 1.0.0 © 2020 Erik Rigtorp <erik@rigtorp.se>\n");
   fprintf(stderr, "usage: c2clat [-p] [-s number_of_samples]\n");
   fprintf(stderr, "\nPlot results using gnuplot:\n");
   fprintf(stderr, "c2clat -p | gnuplot -p\n");
   exit(1);
  }

  cpu_set_t set;
  CPU_ZERO(&set);
  if (sched_getaffinity(0, sizeof(set), &set) == -1)
  {
    perror("sched_getaffinity");
    exit(1);
  }

#define USE_HYPERTHREADING 1
#if USE_HYPERTHREADING
  int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
#else
  int num_cpus = sysconf(_SC_NPROCESSORS_ONLN)/2;
#endif

  // enumerate available CPUs
  int* cpus = (int*)malloc(sizeof(int) * num_cpus);
  for (int i = 0; i < CPU_SETSIZE; ++i)
  {
    if (CPU_ISSET(i, &set))
    {
      cpus[i] = i;
    }
  }

  double* data = (double*) malloc(sizeof(double) * num_cpus * num_cpus);

  for (int i = 0; i < num_cpus; ++i)
  {
    for (int j = i + 1; j < num_cpus; ++j)
    {

      uint64_t btest1, btest2;
      btest1 = btest2 = -1;

      thread_args *args = (thread_args*)malloc(sizeof(thread_args));
      args->a = &btest1;
      args->b = &btest2;
      args->nsamples = nsamples;
      args->cpu = cpus[i];

      pthread_t thread_i;
      if(pthread_create (& thread_i, NULL, thread_function, args) != 0)
      {
        fprintf(stderr, "pthread_create error\n");
        free(data);
        exit(1);
      }

      double rtt = 0.;

      pinThread(cpus[j]);
      for (int m = 0; m < nsamples; ++m)
      {
        btest1 = btest2 = -1;
        double ts1 = get_elapsedtime();
        for (uint64_t n = 0; n < 100; ++n)
        {
          __atomic_store_n(&btest1, n, __ATOMIC_RELEASE);
          while (__atomic_load_n(&btest2, __ATOMIC_ACQUIRE) != n)
            ;
        }
        double ts2 = get_elapsedtime();
        rtt += ts2 - ts1;
      }

      pthread_join(thread_i, NULL);

      data[i * num_cpus + j] = (rtt / 2 / 100) / nsamples;
      data[j * num_cpus + i] = (rtt / 2 / 100) / nsamples;
    }
  }

  if (plot)
  {
    fprintf(stdout, "set title \"Inter-core one-way data latency between CPU cores\"\n");
    fprintf(stdout, "set xlabel \"CPU\"\n");
    fprintf(stdout, "set ylabel \"CPU\"\n");
    fprintf(stdout, "set cblabel \"Latency (ns)\"\n");
    fprintf(stdout, "$data << EOD\n");
  }

  struct utsname buffer;

  errno = 0;
  if (uname(&buffer) < 0)
  {
    perror("uname");
    exit(EXIT_FAILURE);
  }

  char out_filename[100];
  sprintf(out_filename, "%s.csv", buffer.nodename);
  FILE* output;
  output = fopen(out_filename, "w+");

  fprintf(stdout, " %*s", 4, "CPU");
  fprintf(output, "%*s", 4, "CPU");
  for (int i = 0; i < num_cpus; ++i)
  {
    fprintf(stdout, " %*d", 4, cpus[i]);
    fprintf(output, "\t%*d", 4, cpus[i]);
  }

  fprintf(stdout, "\n");
  fprintf(output, "\n");
  for (int i = 0; i < num_cpus; ++i)
  {
    fprintf(stdout, " %*d", 4, cpus[i]);
    fprintf(output, "%*d", 4, cpus[i]);
    for (int j = 0; j < num_cpus; ++j)
    {
      fprintf(stdout, " %*.2lf", 4, 10E8*data[i * num_cpus + j]);
      fprintf(output, "\t%*.2lf", 4, 10E8*data[i * num_cpus + j]);
    }
    fprintf(stdout, "\n");
    fprintf(output, "\n");
  }

  if (plot)
  {
    fprintf(stdout, "EOD\n");
    fprintf(stdout, "plot '$data' matrix rowheaders columnheaders using 2:1:3 ");
    fprintf(stdout, "with image\n");
  }

  fclose(output);
  free(data);

  return 0;
}
