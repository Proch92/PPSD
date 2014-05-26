#include "stdio.h"
#include "stdlib.h"
#include <math.h>
#include "time.h"
#include "mpich/mpi.h"
#include <omp.h>

#define EVENTS 19683 //3^9
#define PLAYERS_PER_ROOM 20
#define MAX_MUTATION_TIMER 500
#define NAME_BUFFER_SIZE 50

//game states
#define GAME_OPEN 0
#define A_WINS 1
#define B_WINS 2
#define DRAW 3

typedef int gameState;

char checkGameState(gameState);
char getPositionsFromState(gameState, char*);

class Player {
public:
	Player();
	~Player();
	char makeMove(gameState);
	void birth(char*, char*);
	void mutate();
	
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
private:
	int localPopulation;
	Player players[PLAYERS_PER_ROOM];

	//statistics
	int generation;
	int draws;

};