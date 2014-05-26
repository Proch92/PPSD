#include "trisEvo.h"

int numNodes, id;

int main(int argc, char** argv) {
	//Init
	int length;
	char name[NAME_BUFFER_SIZE];

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &id);			// Ottiene l'identificativo del processo
	MPI_Comm_size(MPI_COMM_WORLD, &numNodes);	// Ottiene quanti processi sono attivi
	MPI_Get_processor_name(name, &length);		// Il nome del nodo dove il processo è in esecuzione

	int generationLimit = atoi(argv[1]);

	unsigned int *seeds = (unsigned int*) malloc(numNodes * sizeof(unsigned int));
	if(id == 0) {
		//generate seeds
		srand(time(NULL));
		for(int i=0; i!=numNodes; i++)
			seeds[i] = rand();
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
	
	free(seeds);

	srand(seed);

	printf("%d: starting room\n", id);
	Room room;
	room.startHungerGames(generationLimit);

	MPI_Barrier(MPI_COMM_WORLD);

	return 0;
}

//Room
Room::Room() {
	generation = 0;
}

Room::~Room() {
}

void Room::startHungerGames(int generationLimit) {
	printf("hunger games started\n");
	int winners[PLAYERS_PER_ROOM/2];
	int losers[PLAYERS_PER_ROOM/2];

	int notdraw;
	int i;
	short mutationTimer = MAX_MUTATION_TIMER;
	char w1, w2;
	do {
		if(generation % 100 == 0)
			printf("%d:generation %d\n", id, generation);

		notdraw = startGames(winners, losers);
		
		generation++;

		for(i=0; i!=notdraw; i++) {//kill and replace
			w1 = winners[rand()%notdraw];
			w2 = winners[rand()%notdraw];
			players[losers[i]].birth(players[w1].decision, players[w2].decision); //this could be dirty
		}

		if(rand()%mutationTimer <= 1) {
			if(notdraw == 0)
				players[rand()%PLAYERS_PER_ROOM].mutate();
			else
				players[losers[rand()%notdraw]].mutate();
			mutationTimer = MAX_MUTATION_TIMER;
		}
		else mutationTimer--;
	} while(generation < generationLimit);
}

void Room::makePairings() {
	//shuffle
	int a, b;
	char *appoggio;
	int appoggio_int;
	for(int i=0; i!=PLAYERS_PER_ROOM/2; i++) {
		a = rand() % PLAYERS_PER_ROOM;
		b = rand() % PLAYERS_PER_ROOM;

		appoggio = players[a].decision;
		players[a].decision = players[b].decision;
		players[b].decision = appoggio;

		appoggio_int = players[a].decisionsMade;
		players[a].decisionsMade = players[b].decisionsMade;
		players[b].decisionsMade = appoggio_int;

		appoggio_int = players[a].wins;
		players[a].wins = players[b].wins;
		players[b].wins = appoggio_int;
	}
}

int Room::startGames(int *winners, int *losers) {
	makePairings();

	int d = 0;
	omp_set_num_threads(PLAYERS_PER_ROOM / 2);
	//omp_set_num_threads(1);
	int notdraw = 0;

	#pragma omp parallel default(none) reduction(+:d) shared(notdraw, winners, losers)
	{	
		int tid = omp_get_thread_num();
		
		Player* duelist[2];
		int aid = tid;
		int bid = tid + (PLAYERS_PER_ROOM/2);
		duelist[0] = &players[aid];
		duelist[1] = &players[bid];
		gameState actual = 0;

		char result;
		char move;
		char turn = 0;
		do {
			move = duelist[turn]->makeMove(actual); //move is a number from 0 to 8
			actual += pow(3, move) * (turn+1); //everything is in base 3
			//turn = (turn==0)?1:0;
			turn ^= 1;
		} while((result = checkGameState(actual)) == GAME_OPEN);

		//manage results
		#pragma omp critical //protecting notdraw;
		{
			if(result==DRAW)
				d++;
			else {
				duelist[result-1]->wins++;

				if(result==A_WINS) {
					winners[notdraw] = aid;
					losers[notdraw] = bid;
				}
				else {
					winners[notdraw] = bid;
					losers[notdraw] = aid;
				}
				notdraw++;
			}
		}
	}

	draws = d;

	return (PLAYERS_PER_ROOM / 2) - d;
}

//Player

Player::Player() {
	wins = 0;
	decisionsMade = 0;

	decision = (char*) malloc(sizeof(char) * EVENTS);
	for(int i=0; i!=EVENTS; i++)
		decision[i] = 10; //null
}

Player::~Player() {
}

char Player::makeMove(gameState state) {
	if(decision[state] == 10) {
		decision[state] = randomDecision(state);
		decisionsMade++;
	}

	return decision[state];
}

char Player::randomDecision(gameState state) { //return a number from 0 to 8
	char freePos = 0;
	char zeros[9];

	gameState s = state;
	short p, r, q;
	for(char i=8; i>=0; i--) {
		p = pow(3, i);
		r = s % p;
		q = s / p;

		if(q == 0) {
			zeros[freePos] = i;
			freePos++;
		}

		s = r;
	}

	if(freePos == 0) {
		printf("freepos == 0!!!!!!!!!!!!!!\n");
		printf("state: %d\n", state);
	}
	r = rand() % freePos;

	return zeros[r];
}

void Player::birth(char* genes1, char* genes2) {
	decisionsMade = 0;

	for(int i=0; i!=EVENTS; i++) {
		decision[i] = 10;
		if(genes1[i] != 10 && genes2[i] == 10) {
			decision[i] = genes1[i];
			decisionsMade++;
		}
		else if(genes1[i] == 10 && genes2[i] != 10) {
			decision[i] = genes2[i];
			decisionsMade++;
		}
		else if(genes1[i] != 10 && genes2[i] != 10) {
			decision[i] = (rand()%2)?genes1[i]:genes2[i];
			decisionsMade++;
		}
	}
}

void Player::mutate() {
	if(decisionsMade == 0)
		return;

	int r = rand() % decisionsMade;

	int i = 0;
	int found = 0;
	while(found < r) {
		if(decision[i] != 10)
			found++;
		i++;
	}
	
	decision[i-1] = randomDecision(i-1);
}

//------------------------------------------------------------

char getPositionsFromState(gameState state, char *positions) {
	gameState s = state;
	char freePos = 0;
	short p, r, q;
	for(char i=8; i>=0; i--) {
		p = pow(3, i);
		r = s % p;
		q = s / p;

		positions[i] = q;

		if(q == 0)
			freePos++;

		s = r;
	}
	
	return freePos;
}

char checkGameState(gameState state) {
	char freePos;
	char positions[9];

	freePos = getPositionsFromState(state, positions);

	char acount;
	char bcount;
	char cell;
	int i, j;
	//check rows
	for(i=0; i!=3; i++) {
		acount = 0;
		bcount = 0;
		for(j=0; j!=3; j++) {
			cell = positions[i*3 + j];
			if(cell == 1)
				acount++;
			else if(cell == 2)
				bcount++;
		}
		if(acount == 3)
			return 1;
		else if(bcount == 3)
			return 2;
	}
	//check columns
	for(i=0; i!=3; i++) {
		acount = 0;
		bcount = 0;
		for(j=0; j!=3; j++) {
			cell = positions[j*3 + i];
			if(cell == 1)
				acount++;
			else if(cell == 2)
				bcount++;
		}
		if(acount == 3)
			return 1;
		else if(bcount == 3)
			return 2;
	}
	//check diagonals
	for(i=0; i!=3; i++) {
		cell = positions[i*4];
		if(cell == 1)
			acount++;
		else if(cell == 2)
			bcount++;
	}
	if(acount == 3)
		return 1;
	else if(bcount == 3)
		return 2;

	for(i=0; i!=3; i++) {
		cell = positions[i*2 + 2];
		if(cell == 1)
			acount++;
		else if(cell == 2)
			bcount++;
	}
	if(acount == 3)
		return 1;
	else if(bcount == 3)
		return 2;

	if(freePos == 0)
		return DRAW;

	return GAME_OPEN;
}