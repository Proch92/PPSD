//commento!

#include "stdio.h"
#include "stdlib.h"
#include <math>
#include "time.h"
#include "mpich/mpi.h"
#include <omp.h>
#include <SDL.h>
#include <gl/gl.h>
#include "glm/glm.hpp"

#define NAME_BUFFER_SIZE 50

#define TRACK_WIDTH 10
#define WIDTH 400
#define HEIGHT 400

using namespace std;
//using namespace glm;

typedef struct _vec2 {
	float x;
	float y;
} vec2;

int trackLength;
int numNodes;
vec2 *track;
vec2 *path;
vec2 *finalpath;
float *trackAngles;
float *pathAngles;
float *steps;
float *diff;
int *anglesOrder;

void loadTrack(const char*);
void showResult();
void computeAngles(float*, vec2*);
void computeSteps(float*, vec2*);
void randomInit(int);
float randomFloat(); //random 0..1 float
int sortWithAngles(int, int);
int partition(int, int);
bool willDie(int);

int main(int argc, char **argv) {
	//Init
	int id, length;
	char name[NAME_BUFFER_SIZE];

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &id);			// Ottiene l'identificativo del processo
	MPI_Comm_size(MPI_COMM_WORLD, &numNodes);	// Ottiene quanti processi sono attivi
	MPI_Get_processor_name(name, &length);		// Il nome del nodo dove il processo è in esecuzione

	if(id == 0) {
		loadTrack("");
		//send track to all nodes
		MPI_Bcast(track, trackLength * 2, MPI_FLOAT, 0, MPI_COMM_WORLD);
	}
	else {
		MPI_Status status;
		MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		MPI_Get_count(&status, MPI_FLOAT, &trackLength);
		track = (vec2*) malloc(sizeof(vec2) * trackLength);
		MPI_Recv(track, trackLength, MPI_FLOAT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		trackLength /= 2;
	}

	//start working
	trackAngles = (float*) malloc(sizeof(float) * trackLength);
	steps = (float*) malloc(sizeof(float) * trackLength);
	anglesOrder = (int*) malloc(sizeof(int) * trackLength);
	diff = (float*) malloc(sizeof(float) * trackLength);

	computeAngles(trackAngles, track);

	//genetic algorithm
	MPI_Barrier();
	randomInit(id);
	pathAngles = (float*) malloc(sizeof(float) * trackLength);

	bool done = false;
	do {
		computeAngles(pathAngles, path);
		//classificazione
		for(int i=0; i!=trackLength; i++) anglesOrder[i] = i;
		sortWithAngles(0, trackLength - 1);
		//eliminazione angoli più piccoli e ripopolamento
		int media, a, b;
		for(int i=0; i!=trackLength; i++) {
			if(!willDie(i))
				a = 
		}
		//mutazioni
	} while(!done);

	free(trackAngles);
	free(steps);
	free(pathAngles);
	MPI_Finalize();
}

bool willDie(int index) {
	bool death = false;
	for(int i=0; i!=trackLength/2; i++)
		if(index == anglesOrder[i])
			death = true;

	return death;
}

void computeAngles(float* angles, vec2* points) {
	computeSteps(steps, track);

	vec2 a, b;
	float ab;
	for(int i=0; i!=trackLength; i++) {
		a = points[(i-1)%trackLength];
		b = points[(i+1)%trackLength];
		ab = sqrtf(powf(b.x - a.x, 2) + pow(b.y - a.y, 2));

		angles[i] = acos((powf(steps[(i-1)%trackLength], 2) + powf(steps[i], 2) - powf(ab, 2)) / (2 * steps[(i-1)%trackLength] * steps[i]));
	}
}

void computeSteps(float* steps, vec2* points) {
	vec2 a,b;
	for(int i=0; i!=trackLength; i++) {
		a = points[i%trackLength];
		b = points[(i+1)%trackLength];
		steps[i] = sqrtf(powf(b.x - a.x, 2) + pow(b.y - a.y, 2));
	}
}

int sortWithAngles(int l, int r) {
	//quick sort
	int j;

   if(l < r) {
		j = partition(l, r);
		sortWithAngles(l, j-1);
		sortWithAngles(j+1, r);
	}
}

int partition(int l, int r) {
	int pivot, i, j, t;
	pivot = pathAngles[anglesOrder[l]];
	i = l; j = r+1;

	while(i < j) {
		do ++i; while(pathAngles[anglesOrder[i]] <= pivot && i <= r);
		do --j; while(pathAngles[anglesOrder[j]] > pivot);
		swap(i, j);
	}
	swap(l, j);
	return j;
}

void swap(int a, int b) {
	int c;
	c = anglesOrder[a];
	anglesOrder[a] = anglesOrder[b];
	anglesOrder[b] = c;
}

void loadTrack(const char *filename) {
	FILE* f;
	f = fopen(filename, "r");

	fread(&trackLength, sizeof(int), 1, f);

	track = (vec2) malloc(sizeof(vec2) * trackLength);
	fread(&track, sizeof(vec2) * trackLength, 1, f);
}

void randomInit(int id) {
	unsigned int *seeds;
	if(id==0) {
		//generate seeds
		seeds = (unsigned int*) malloc(numNodes * sizeof(MPI_UNSIGNED));

		srand(time(NULL));
		char *ptr = (char*) seeds;
		for(int i=0; i!=numNodes * sizeof(MPI_UNSIGNED); i++) { // rand() restituisce un solo byte
			*ptr = rand();
			ptr++;
		}
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

	srand(seed);

	//populate the world
	float r;
	for(int i=0; i!=trackLength; i++) {
		diff[i] = r = (randomFloat() * TRACK_WIDTH) - (TRACK_WIDTH / 2); //if T_W is 10, this returns a rabdom float from -5 to 5
		path[i].x = track[i].x + (r * cosf(angles[i]));
		path[i].y = track[i].y + (r * sinf(angles[i]));
	}
}

void showResult() {
	//init
	if(SDL_Init(SDL_INIT_EVERYTHING) < 0)
        return;

    if(SDL_SetVideoMode(WIDTH, HEIGHT, 32, SDL_SWSURFACE | SDL_OPENGL) == NULL)
		return;

	glClearColor(255, 255, 255, 0);

	glViewport(0, 0, WIDTH, HEIGHT);
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, WIDTH, HEIGHT, 0, 1, -1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//render
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	//draw track
	glBegin(GL_LINE_STRIP);
		for(int i=0; i!=trackLength; i++) {
			glColor3f(); //color might be the angle
			glVertex3f();
		}
	glEnd();

	SDL_GL_SwapBuffers();

	//wait for esc

	SDL_Quit();
}

float randomFloat() {
	return (float)rand() / RAND_MAX;
}