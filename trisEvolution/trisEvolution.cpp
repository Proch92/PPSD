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

	MPI_Type_vector(EVENTS, 1, 0, MPI_BYTE, &genesType);
	MPI_Type_commit(&genesType);

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

	Room room;
	room.startHungerGames(generationLimit);

	MPI_Barrier(MPI_COMM_WORLD);

	Player *champion = room.getChampion();
	printf("%d: the Champion has %d wins under his belt\n", id, champion->wins);

	MPI_Barrier(MPI_COMM_WORLD);

	if(id==0) {
		bool again = true;
		do {
			printf("wanna play against the champion?(y/n)\n");
			char response;
			cin >> response;
			if(response == 'y' || response == 's') {
				char turn = rand()%2;
				char result;
				char move;
				gameState actual = 0;
				do {
					if(turn == 1) { //champion
						move = champion->makeMove(actual); //move is a number from 0 to 8
						actual += pow(3, move) * (turn+1); //everything is in base 3
					}
					else { //you
						printf("your turn!\n");
						printField(actual);
						move = humanMove(actual);
						actual += pow(3, move) * (turn+1); //everything is in base 3
					}
					turn ^= 1;
				} while((result = checkGameState(actual)) == GAME_OPEN);

				printf("result: %d\n", result);
			}
			else again = false;
		} while(again);
	}

	MPI_Barrier(MPI_COMM_WORLD);

	MPI_Finalize();

	return 0;
}

//Room
Room::Room() {
}

Room::~Room() {
}

void Room::startHungerGames(int generationLimit) {
	printf("hunger games started\n");
	int winners[PLAYERS_PER_ROOM/2];
	int losers[PLAYERS_PER_ROOM/2];

	generation = 0;
	int notdraw;
	short mutationTimer = MAX_MUTATION_TIMER;
	char w1, w2;
	char genesToSend[EVENTS];
	char receiveBuffer1[EVENTS];
	char receiveBuffer2[EVENTS];
	MPI_Request reqs[4];
	MPI_Status stats[2];
	int flag;
	do {
		if(id==0 && generation % 1000 == 0)
			printf("generation: %d\n", generation);

		notdraw = startGames(winners, losers);
		generation++;

		if(generation % MIGRATION_TIMER == 0) {
			char *championGenes = getChampion()->decision;
			for(int i=0; i!=EVENTS; i++)
				genesToSend[i] = championGenes[i]; //make a copy to send

			int prev = (id + numNodes - 1) % numNodes;
			int next = (id + 1) % numNodes;
			MPI_Irecv(receiveBuffer1, EVENTS, MPI_BYTE, prev, MIGRATION_TAG_1, MPI_COMM_WORLD, &reqs[0]);
			MPI_Irecv(receiveBuffer2, EVENTS, MPI_BYTE, next, MIGRATION_TAG_2, MPI_COMM_WORLD, &reqs[1]);

			MPI_Isend(genesToSend, EVENTS, MPI_BYTE, prev, MIGRATION_TAG_2, MPI_COMM_WORLD, &reqs[2]);
			MPI_Isend(genesToSend, EVENTS, MPI_BYTE, next, MIGRATION_TAG_1, MPI_COMM_WORLD, &reqs[3]);

			MPI_Waitall(4, reqs, stats);
		}


		int j = 0;
		if(generation % MIGRATION_TIMER == 0) {
			if(notdraw == 0) {
				players[rand()%PLAYERS_PER_ROOM].copyGenes(receiveBuffer1);
				players[rand()%PLAYERS_PER_ROOM].copyGenes(receiveBuffer2);
			}
			else if(notdraw == 1) {
				players[losers[0]].copyGenes(receiveBuffer1);
				players[rand()%PLAYERS_PER_ROOM].copyGenes(receiveBuffer2);
				j=1;
			}
			else {
				players[losers[0]].copyGenes(receiveBuffer1);
				players[losers[1]].copyGenes(receiveBuffer2);
				j=2;
			}
		}

		if(notdraw == 1)
			j=1;

		for(; j!=notdraw; j++) {//kill and replace
			w1 = winners[rand()%notdraw];
			w2 = winners[rand()%notdraw];
			players[losers[j]].birth(players[w1].decision, players[w2].decision); //this could be dirty
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
		char turn = rand()%2;
		do {
			move = duelist[turn]->makeMove(actual); //move is a number from 0 to 8
			actual += pow(3, move) * (turn+1); //everything is in base 3
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

Player* Room::getChampion() {
	int max = 0;
	int maxPos = 0;
	for(int i=0; i!=PLAYERS_PER_ROOM; i++) {
		if(players[i].wins > max) {
			max = players[i].wins;
			maxPos = i;
		}
	}

	return &(players[maxPos]);
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

void Player::copyGenes(char* genes) {
	decisionsMade = 0;
	for(int i=0; i!=EVENTS; i++) {
		decision[i] = genes[i];

		if(decision[i] != 10)
			decisionsMade++;
	}
	wins = 0;
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

void printField(gameState state) {
	char field[9];
	getPositionsFromState(state, field);

	for(int i=0; i!=3; i++) {
		for(int j=0; j!=3; j++)
			printf("%d ", field[i*3 + j]);

		printf("\n");
	}
}

bool freeSpace(gameState state, char choice) {
	char field[9];

	getPositionsFromState(state, field);

	return field[choice] == 0;
}

char humanMove(gameState state) {
	bool done = false;

	int choice;
	do {
		printf("make a choice: (1..9)\n");
		cin >> choice;
		if(choice > 0 && choice < 10 && freeSpace(state, choice-1))
			done = true;
	} while(!done);

	return choice-1;
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
	acount = 0;
	bcount = 0;
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

	acount = 0;
	bcount = 0;
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