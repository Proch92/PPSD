/************************************************/
// Progetto: Integrazione con metodo Montecarlo //
// Autore: Michele Probverbio					//
// Mail: michele.proverbio@studio.unibo.it 		//
//												//
// Implementazione mista. Il master genera dei	//
// seed random e li distribuisce ai nodi del 	//
// cluster. Ogni nodo genera dei thread figli	//
// che si occupano di fare i calcoli.			//
//												//
// Usage: mpirun -n ? ./imc num_threads			//
/************************************************/


#include "stdio.h"
#include "stdlib.h"
#include <iostream>
#include "mpich/mpi.h"
#include <omp.h>
#include "time.h"
#include "math.h"

#define NAME_BUFFER_SIZE 50
typedef struct _vec2 {double x; double y;} vec2;
#define MAX_THREADS 100

using namespace std;

double randomDouble();

int main(int argc, char **argv) {
	if(argc != 2)
		printf("1 argument expected\nUsage: mpirun -n num_nodes ./imc num_threads\n");

	//Init
	int id, numNodes, length;
	char name[NAME_BUFFER_SIZE];

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &id);			// Ottiene l'identificativo del processo
	MPI_Comm_size(MPI_COMM_WORLD, &numNodes);	// Ottiene quanti processi sono attivi
	MPI_Get_processor_name(name, &length);		// Il nome del nodo dove il processo è in esecuzione

	int numTasks = atoi(argv[1]);

	unsigned int *seeds;
	if(id == 0) {
		//generate seeds
		seeds = (unsigned int*) malloc(numNodes * sizeof(unsigned int));

		srand(time(NULL));
		for(int i=0; i!=numNodes; i++)
			seeds[i] = rand();
		
		printf("generated seeds:");
		for(int i=0; i!=4; i++)
			printf(" %d", seeds[i]);
		printf("\n");
	}

	//send seeds to all nodes
	unsigned int seed;
	MPI_Scatter(seeds,		//send buffer
		1,					//elements sent to each node
		MPI_UNSIGNED,		//type of data
		&seed,				//receive buffer
		1,					//number of receiving data
		MPI_UNSIGNED,		//type of receiving data
		0,					//sender rank
		MPI_COMM_WORLD);	//comm

	printf("%d: my seed is %d\n", id, seed);

	srand(seed);

	//each node generates his random numbers
	vec2* randoms = (vec2*) malloc(numTasks * sizeof(vec2));
	for(int i=0; i!=numTasks; i++) {
		randoms[i].x = randomDouble();
		randoms[i].y = randomDouble();
	}

	//openMP stuff
	omp_set_num_threads(MAX_THREADS);

	vec2 center;
	center.x = 0.5;
	center.y = 0.5;
	double radius = 0.5;

	double d;
	long unsigned int hits;
	
	hits = 0;
	#pragma omp parallel for reduction(+:hits)
	for(int i=0; i<numTasks; i++) {
		//is the point inside the area?
		//pitagora
		d = sqrtl(powl(randoms[i].x - center.x, 2) + powl(randoms[i].y - center.y, 2));
		if(d < radius) //hit
			hits++;
	}

	printf("%d: hits = %ld\n", id, hits);

	//barrier than reduction
	MPI_Barrier(MPI_COMM_WORLD);

	long unsigned int allHits;
	MPI_Reduce(
		&hits,
		&allHits,
		1,
		MPI_UNSIGNED_LONG,
		MPI_SUM,
		0,
		MPI_COMM_WORLD);

	if(id == 0) {
		printf("allHits: %ld\n", allHits);
		double area = (double)allHits / (numTasks * numNodes);
		printf("area: %f\n", area);
	}

	//finalize
	if(id == 0)
		free(seeds);
	MPI_Finalize();
}

double randomDouble() {
	return (double)rand() / RAND_MAX;
}