#ifndef _VNOCS_UTILS_
#define _VNOCS_UTILS_

#include <stdio.h>
#include <stdlib.h>
#include <math.h>


using namespace std;

#define BUFFER_SIZE 256 // general purpose;

const double PI = 3.141592658979323846;

////////////////////////////////////////////////////////////////////////////////
//
// Utility functions;
//
////////////////////////////////////////////////////////////////////////////////

void *my_malloc(size_t size);
void *my_realloc(void *ptr, size_t size);

////////////////////////////////////////////////////////////////////////////////
//
// RANDOM_NUMBER_GENERATOR
//
////////////////////////////////////////////////////////////////////////////////

class RANDOM_NUMBER_GENERATOR
{
    private:
        long _seed;

    private:
        double sflat01();
        double gauss01();
    public:
        RANDOM_NUMBER_GENERATOR(long seed);
        RANDOM_NUMBER_GENERATOR();
        ~RANDOM_NUMBER_GENERATOR() {}

        double flat_d( double low, double high);
        long flat_l( long low, long high);
        unsigned long flat_ul( unsigned long low, unsigned long high);
        unsigned long long flat_ull( unsigned long long low, unsigned long long high);
        double gauss_mean_d( double mean, double variance);
        long gauss_mean_l( long mean, double variance);
        unsigned long gauss_mean_ul( unsigned long mean, double variance);
        unsigned long long gauss_mean_ull( unsigned long long mean, double variance);
        void set_seed( long seed);

        int poisson( double this_mean);
        int pareto( double alpha);
        
};

#endif
