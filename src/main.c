/*
  Name: Aniv Surana
  ID: 1001912967
*/
// MIT License
// 
// Copyright (c) 2023 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <stdint.h>
#include "utility.h"
#include "star.h"
#include "float.h"

#include <sys/time.h>
#include <pthread.h>

#define NUM_STARS 30000 
#define MAX_LINE 1024
#define DELIMITER " \t\n"

struct Star star_array[ NUM_STARS ];
uint8_t   (*distance_calculated)[NUM_STARS];

// struct containing start and end positions
// to help store total distance of a chunk of the array
struct ThreadResults
{
  int start;
  int end;
  double distance_sum;
  int count;
};

double  min  = FLT_MAX;
double  max  = FLT_MIN;

// declare and initialize a mutex variable for mutex lock
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// display help menu
void showHelp()
{
  printf("Use: findAngular [options]\n");
  printf("Where options are:\n");
  printf("-t          Number of threads to use\n");
  printf("-h          Show this help\n");
}

// 
// Embarassingly inefficient, intentionally bad method
// to calculate all entries one another to determine the
// average angular separation between any two stars 


// calculate the total angular distance between stars in a set;
// total is being calculated instead of average
// and is divided by the number of threads in teh end
void determineAverageAngularDistance( void* set )
{
  
  // double mean = 0;
  uint32_t i, j;
  uint64_t count = 0;
  // input set is a ThreadResults struct
  struct ThreadResults *data = (struct ThreadResults *)set;

  // Loop through smaller set of stars
  for (i = data->start; i < data->end; i++)
  {
    for (j = 0; j < NUM_STARS; j++)
    {
      if( i!=j && distance_calculated[i][j] == 0 )
      {
        double distance = calculateAngularDistance( star_array[i].RightAscension, star_array[i].Declination,
                                                    star_array[j].RightAscension, star_array[j].Declination ) ;
        distance_calculated[i][j] = 1;
        distance_calculated[j][i] = 1;
        data->distance_sum += distance;
        data->count++;
        if( min > distance )
        {
          min = distance;
        }

        if( max < distance )
        {
          max = distance;
          }
        // mean = mean + (distance-mean)/count;
      }
        
    }
  }
  pthread_exit(NULL);
}


int main( int argc, char * argv[] )
{

  FILE *fp;
  uint32_t star_count = 0;

  uint32_t n;

  distance_calculated = malloc(sizeof(uint8_t[NUM_STARS][NUM_STARS]));

  if( distance_calculated == NULL )
  {
    uint64_t num_stars = NUM_STARS;
    uint64_t size = num_stars * num_stars * sizeof(uint8_t);
    printf("Could not allocate %ld bytes\n", size);
    exit( EXIT_FAILURE );
  }
  int num_threads;
  int i, j;
  // default every thing to 0 so we calculated the distance.
  // This is really inefficient and should be replace by a memset
  // for (i = 0; i < NUM_STARS; i++)
  // {
  //   for (j = 0; j < NUM_STARS; j++)
  //   {
  //     distance_calculated[i][j] = 0;
  //   }
  // }

  memset(distance_calculated, 0, sizeof(uint8_t)*NUM_STARS*NUM_STARS);
  for( n = 1; n < argc; n++ )          
  {
    if( strcmp(argv[n], "-help" ) == 0 || strcmp(argv[n], "-h") == 0)
    {
      showHelp();
      exit(0);
    }
    if(strcmp(argv[n], "-t") == 0)
    {
      num_threads = atoi(argv[n+1]);
    }
  }

  struct ThreadResults thread_data[num_threads];
  pthread_t threads[num_threads];

  fp = fopen( "data/tycho-trimmed.csv", "r" );

  if( fp == NULL )
  {
    printf("ERROR: Unable to open the file data/tycho-trimmed.csv\n");
    exit(1);
  }

  char line[MAX_LINE];
  while (fgets(line, 1024, fp))
  {
    uint32_t column = 0;

    char* tok;
    for (tok = strtok(line, " ");
            tok && *tok;
            tok = strtok(NULL, " "))
    {
       switch( column )
       {
          case 0:
              star_array[star_count].ID = atoi(tok);
              break;
       
          case 1:
              star_array[star_count].RightAscension = atof(tok);
              break;
       
          case 2:
              star_array[star_count].Declination = atof(tok);
              break;

          default: 
             printf("ERROR: line %d had more than 3 columns\n", star_count );
             exit(1);
             break;
       }
       column++;
    }
    star_count++;
  }
  printf("%d records read\n", star_count );
  struct timeval end; 
	struct timeval begin; 
 // starting time
  gettimeofday( &begin, NULL );
  for(i = 0; i < num_threads; i++)
  {
    thread_data[i].start = i * (NUM_STARS / num_threads);
    thread_data[i].end = (i+1) * (NUM_STARS/num_threads);
    thread_data[i].distance_sum = 0;
    thread_data[i].count = 0;
    // to prevent multiple threads from accessing shared resources
    pthread_mutex_lock(&mutex);
    pthread_create(&threads[i], NULL, (void *(*)(void *)) determineAverageAngularDistance, &thread_data[i]);
    pthread_mutex_unlock(&mutex);
  }
  for (i = 0; i < num_threads; i++)
  {
    pthread_join(threads[i], NULL);
  }

  double distance_sum = 0;
  int count = 0;
  for(i = 0; i < num_threads; i++)
  {
    distance_sum = distance_sum + thread_data[i].distance_sum;
    count = count + thread_data[i].count;

  }
  // Find the average angular distance in the most inefficient way possible
  // double distance =  determineAverageAngularDistance( star_array );

  // calculating average angular distance
  double distance = distance_sum / count;

  // ending time
  gettimeofday( &end, NULL );
  double time_duration = ( ( end.tv_sec - begin.tv_sec ) * 1000000 + 
                            ( end.tv_usec - begin.tv_usec ) )/1000000.0;

  printf("Average distance found is %lf\n", distance );
  printf("Minimum distance found is %lf\n", min );
  printf("Maximum distance found is %lf\n", max );

  printf("Time taken: %.6lf seconds\n",time_duration);
  return 0;
}

