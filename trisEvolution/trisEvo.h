#include "stdio.h"
#include "stdlib.h"
#include <math.h>
#include "time.h"
#include "mpich/mpi.h"
#include <omp.h>
#include <iostream>

#define EVENTS 19683 //3^9
#define PLAYERS_PER_ROOM 40
#define MAX_MUTATION_TIMER 75
#define MIGRATION_TIMER 50
#define NAME_BUFFER_SIZE 50
#define MIGRATION_TAG_1 1
#define MIGRATION_TAG_2 2

//game states
#define GAME_OPEN 0
#define A_WINS 1
#define B_WINS 2
#define DRAW 3

using namespace std;

MPI_Datatype genesType;
typedef int gameState;

char checkGameState(gameState);
char getPositionsFromState(gameState, char*);
char humanMove(gameState);
bool freeSpace(gameState, char);
void printField(gameState);

class Player {
public:
	Player();
	~Player();
	char makeMove(gameState);
	void birth(char*, char*);
	void mutate();
	void copyGenes(char*);
	
	int wins;
	char *decision; //patrimonio genetico
	int decisionsMade;
private:
	char randomDecision(gameState);
};

class Room {
public:
	Room();
	~Room();
	void startHungerGames(int);
	void makePairings();
	int startGames(int*, int*);
	Player* getChampion();
private:
	int localPopulation;
	Player players[PLAYERS_PER_ROOM];

	//statistics
	int generation;
	int draws;

};