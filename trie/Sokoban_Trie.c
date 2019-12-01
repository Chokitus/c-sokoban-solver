#include "../common/common.h"
#include "../common/sort.h"
#include "../common/structures.h"
#include "../common/util.h"
#include "trie.h"
#include <omp.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Ponteiro para a raiz da trie de ids.
IdTrie *mainId;

// Ponteiro para o último estado da lista principal.
State **lastMainState;

int memoryInsert = 0;

// Função que procura o id na lista
unsigned char findId(State *s) {
	// Apontamos para a mainTrie
	IdTrie *tempTrie = mainId;

	unsigned short tempValue = 0;
	unsigned char found = 0;

	/*
	    É importante somente travar cada "andar" da trie com um lock.
	    Esta implementação possui, pelo menos, 3*(caixas+player) locks, o que
	    significa que conseguimos travar cada andar uma vez.
	*/

	// Para cada caixa:
	for (short i = 0; i < s->boxes; i++) {
		if (s->posBoxes[i] > 100) {

			if (!tempTrie->idLeafs[s->posBoxes[i] / 100]) {
				memoryInsert++;
				tempTrie->idLeafs[s->posBoxes[i] / 100] = new_trie();
				found = 1;
			}
			tempTrie = tempTrie->idLeafs[s->posBoxes[i] / 100];
		}
		tempValue = (s->posBoxes[i] / 10);

		if (!tempTrie->idLeafs[tempValue % 10]) {
			memoryInsert++;
			tempTrie->idLeafs[tempValue % 10] = new_trie();
			found = 1;
		}
		tempTrie = tempTrie->idLeafs[tempValue % 10];

		if (!tempTrie->idLeafs[s->posBoxes[i] - tempValue * 10]) {
			memoryInsert++;
			tempTrie->idLeafs[s->posBoxes[i] - tempValue * 10] = new_trie();
			found = 1;
		}
		tempTrie = tempTrie->idLeafs[s->posBoxes[i] - tempValue * 10];
	}

	if (s->posPlayer > 100) {
		if (!tempTrie->idLeafs[s->posPlayer / 100]) {
			memoryInsert++;
			tempTrie->idLeafs[s->posPlayer / 100] = new_trie();
			found = 1;
		}
		tempTrie = tempTrie->idLeafs[s->posPlayer / 100];
	}
	tempValue = (s->posPlayer / 10);

	if (!tempTrie->idLeafs[tempValue % 10]) {
		memoryInsert++;
		tempTrie->idLeafs[tempValue % 10] = new_trie();
		found = 1;
	}
	tempTrie = tempTrie->idLeafs[tempValue % 10];

	if (!tempTrie->idLeafs[s->posPlayer - tempValue * 10]) {
		memoryInsert++;
		tempTrie->idLeafs[s->posPlayer - tempValue * 10] = new_trie();
		found = 1;
	}

	return found;
}

//---------------------------------------------------------------------------------------------

// Função que retorna se o estado é único ou não
unsigned char getStateId(State *s) {
	// Fazemos um sort pois a ordem das caixas não pode importar
	quickSort(s->posBoxes, 0, s->boxes - 1);
	return findId(s) == 0;
}

int main(int argc, char *argv[]) {
	struct timespec before, after;
	time_t nSeconds;

	// Começamos a contagem de tempo.
	clock_gettime(CLOCK_REALTIME, &before);

	// Começamos um contador para a lista principal
	unsigned int mainStates = 1;

	// Criamos espaço para uma variável compartilhada que verifica se foi
	// encontrado uma solução por algum dos threads
	unsigned char *solution = malloc(1);
	*solution = 0;

	// Criamos espaço para a raiz da lista principal
	State *root = malloc(sizeof(State));
	root->nextState = NULL;

	// Criamos um ponteiro temporário que irá ser movido
	State *s = malloc(sizeof(State));

	// Ponteiro para o último estado principal é inicializado.
	lastMainState = malloc(sizeof(State *));
	*lastMainState = NULL;

	// Ponteiro para a raiz da trie de Ids
	mainId = malloc(sizeof(IdTrie));
	memset(mainId->idLeafs, 0, 10 * sizeof(IdTrie *));

	// Constroi o primeiro estado, sequencialmente
	buildMap(root, argv[1]);
	getStateId(root);

	int moved = 0;
	while (!(*solution)) {
		for (int i = 0; i < 4; i++) {
			// Pra cada direção, nós copiamos o estado inicial
			copyState(root, s);
			if (movePlayer(s, i, getStateId) != 0) {
				moved++;
				/*movePlayer retorna 0 se não foi possível mover, seja por uma
				caixa sendo empurrada numa parede,
				seja por estarmos andando de cara na parede*/
				mainStates++;
				if (insertState(root, s, lastMainState)) {
					// Se ele entrou aqui, quer dizer que, durante a inserção,
					// foi notado que ele é um estado final.
					printPath(s);
					*solution = 1;
				}
			}
		}
		if (moved == 0)
			free(root->lastAction);
		moved = 0;
		// Movemos root, colocando root como próximo estado
		popState(&root, &root);
		mainStates--;
	}

	// Finalizamos a contagem de tempo.
	clock_gettime(CLOCK_REALTIME, &after);

	// Calcula o tempo passado em microssegundos.
	nSeconds =
	    after.tv_nsec - before.tv_nsec + (after.tv_sec - before.tv_sec) * NANOS;

	printf("Tempo total: %lu ns - %lf s\n", nSeconds, (double)nSeconds / NANOS);

	return 0;
}