/************************************************/
// Progetto: Generazione height map con perlin	//
// noise 										//
// Autore: Michele Probverbio					//
// Mail: michele.proverbio@studio.unibo.it 		//
//												//
// 1 master - n slaves							//
// Il master dvide la height map in quadranti	//
// e li distribuisce ai nodi slave.				//
// Gli slave si occupano dell'interpolazione.	//
// Quando uno slave finisce il suo lavoro ne 	//
// chiede un altro e manda i suoi dati al 		//
// thread data listener.						//
/************************************************/


#include "stdio.h"
#include "stdlib.h"
#include <iostream>
#include "mpich/mpi.h"
#include <omp.h>
#include "time.h"
#include "math.h"
#include <thread>

#define NAME_BUFFER_SIZE 50
typedef struct _quad {int i; int j; double nw; double ne; double sw; double se;} quad;
#define MAX_THREADS 100

#define WIDTH 1000
#define HEIGHT 1000
#define FREQUENCY 100 //which is also quads edge
#define MIN_Z 0
#define MAX_Z 200

#define TASK_REQUEST 0
#define DATA_DELIVER 1

using namespace std;

MPI_Datatype MPI_QUAD;
MPI_Datatype MPI_MAP_DATA;
MPI_Datatype MPI_FILE_N_CONT;

double randomDouble();
//void buildQuadDatatype();
void buildMapDatatype();
void buildFileNonContDatatype();
double interpolate(double, double, double);
//void dataListener();

int width, height, freq;
int wquads, hquads;

double *finalMap;

int main(int argc, char **argv) {
	if(argc != 4) {
		printf("3 arguments expected\nUsage: mpirun -n num_nodes ./perlin width height frequency\n");
		return 0;
	}

	width = atoi(argv[1]);
	height = atoi(argv[2]);
	freq = atoi(argv[3]);
	wquads = WIDTH / FREQUENCY;
	hquads = HEIGHT / FREQUENCY;

	//Init
	int id, numNodes, length;
	char name[NAME_BUFFER_SIZE];

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &id);			// Ottiene l'identificativo del processo
	MPI_Comm_size(MPI_COMM_WORLD, &numNodes);	// Ottiene quanti processi sono attivi
	MPI_Get_processor_name(name, &length);		// Il nome del nodo dove il processo è in esecuzione

	//buildQuadDatatype();

	double *map;
	bool done = false;
	quad quadrante;
	MPI_Status status;
	if(id == 0) { //master
		//spawn data listener
		thread listener(dataListener);
		//generate map
		map = (double*) malloc((wquads+1) * (hquads+1) * sizeof(MPI_DOUBLE));

		for(int i=0; i!=wquads+1; i++)
			for(int j=0; j!=hquads+1; j++)
				map[i + (j * wquads)] = MIN_Z + (randomDouble() * (MAX_Z - MIN_Z + 1));

		//listen for requests
		int requestingId;
		for(int i=0; i!=wquads; i++)
			for(int j=0; j!=hquads; j++) {
				MPI_Recv(&requestingId, 1, MPI_INT, MPI_ANY_SOURCE, TASK_REQUEST, MPI_COMM_WORLD, &status);
				quadrante.i = i;
				quadrante.j = j;
				quadrante.nw = map[i + (j * wquads)];
				quadrante.ne = map[i + 1 + (j * wquads)];
				quadrante.sw = map[i + ((j+1) * wquads)];
				quadrante.se = map[i + 1 + ((j+1) * wquads)];
				MPI_Send(&quadrante, 1, MPI_QUAD, requestingId, TASK_REQUEST, MPI_COMM_WORLD);
			}

		quadrante.ne = quadrante.nw = quadrante.se = quadrante.sw = 0;
		MPI_Bcast(&quadrante, 1, MPI_QUAD, 0, MPI_COMM_WORLD);

		listener.join();
	}
	else { //slaves
		//allocate height map quadrant
		double *hmquad = (double*) malloc(((FREQUENCY * FREQUENCY) + 2) * sizeof(double)); //last 2 doubles will get the quad coordinates
		do {
			//send my id to receive a task
			MPI_Send(&id, 1, MPI_INT, 0, TASK_REQUEST, MPI_COMM_WORLD);
			//receive either the task or the quit command
			MPI_Recv(&quadrante, 1, MPI_QUAD,+ 0, TASK_REQUEST, MPI_COMM_WORLD, &status);
			if(quadrante.nw == quadrante.ne == quadrante.sw == quadrante.se) //lavoro finito
				done = true;
			else { //work
				//bilinear interpolation // !!!!!!!!! INVERTIRE COLONNE CON RIGHE!!!!!!!
				for(int i=0; i!=FREQUENCY; i++) {
					hmquad[i] = interpolate(quadrante.nw, quadrante.ne, (double)i + 0.5);
					hmquad[i + ((FREQUENCY-1) * FREQUENCY)] = interpolate(quadrante.sw, quadrante.se, (double)i + 0.5);
				}
				for(int i=0; i!=FREQUENCY; i++)
					for(int j=1; j!=FREQUENCY-1; j++)
						hmquad[i + (j * FREQUENCY)] = interpolate(hmquad[i], hmquad[i + ((FREQUENCY-1) * FREQUENCY)], (double)j + 0.5);

			//send it back. puts quad coordinate on the last 2 doubles of the array
			hmquad[FREQUENCY * FREQUENCY] = quadrante.i;
			hmquad[(FREQUENCY * FREQUENCY) + 1] = quadrante.j;

			MPI_Send(hmquad, 1, MPI_MAP_DATA, 0, DATA_DELIVER, MPI_COMM_WORLD);
			}
		} while(!done);

		free(hmquad);
	}

	//finalize
	if(id == 0) {
		free(map);
		free(finalMap);
	}
	MPI_Finalize();
}

double randomDouble() {
	return (double)rand() / RAND_MAX;
}

void buildQuadDatatype() {
	MPI_Datatype oldtypes[2];
	int blockcounts[2];
	MPI_Aint offsets[2], extent;

	offsets[0] = 0; 
	oldtypes[0] = MPI_INT; 
	blockcounts[0] = 2; 

	MPI_Type_extent(MPI_INT, &extent); 
	offsets[1] = 2 * extent; 
	oldtypes[1] = MPI_FLOAT; 
	blockcounts[1] = 4; 

	MPI_Type_struct(2, blockcounts, offsets, oldtypes, &MPI_QUAD); 
	MPI_Type_commit(&MPI_QUAD); 
}

/*void buildMapDatatype() {
	MPI_Datatype oldtype = MPI_DOUBLE;
	MPI_Type_contiguous((FREQUENCY * FREQUENCY) + 2, oldtype, &MPI_MAP_DATA);
	MPI_Type_commit(&MPI_MAP_DATA); 
}*/

void buildFileNonContDatatype() {
	MPI_Type_vector(WIDTH / FREQUENCY, FREQUENCY, WIDTH, MPI_FLOAT, &MPI_FILE_N_CONT);
	MPI_Type_commit(&MPI_FILE_N_CONT);
}

double interpolate(double y0, double y1, double x) {
	return y0 + ((y1 - y0) * (x / FREQUENCY));
}

/*void dataListener() {
	finalMap = (double*) malloc(width * height * sizeof(double));

	int nquads = wquads * hquads;
	for(int i=0; i!=nquads; i++) {

	}
}*/

/*
vector per datatype non contigui
per gestire i quadranti sul file
(mpi_ex7_FILE_IO_all.cpp)
praticamente obbligatorio

write atmp
*/