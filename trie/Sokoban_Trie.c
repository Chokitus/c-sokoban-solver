#include "../common/common.h"
#include "../common/sort.h"
#include "../common/structures.h"
#include "../common/util.h"
#include "trie.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Ponteiro para a raiz da trie de ids.
IdTrie *mainId;

/**
 * Navigates through trie starting with a reference position.
 * @param trie Trie that will be navigated
 * @param position Reference position for navigation
 * @return 1 If any of the navigated positions already existed. 0 otherwise.
 * @author marcos.romero
 */
unsigned char updateTrie(IdTrie **trie, unsigned short position) {
	unsigned short tempValue = 0;
	unsigned char found = 0;

	if (position > 100) {
		found |= moveTrieToLeaf(trie, position / 100);
	}
	tempValue = (position / 10);

	found |= moveTrieToLeaf(trie, tempValue % 10);
	found |= moveTrieToLeaf(trie, position - tempValue * 10);
	return found;
}

/**
 * Searches for an ID representing that state on a trie.
 * @param trie Trie on which the state will be searched.
 * @param s State that will be searched.
 * @return 0 if state not found. 1 otherwise.
 */
unsigned char findId(IdTrie *trie, State *s) {
	IdTrie *tempTrie = trie;

	unsigned char found = 0;

	// Para cada caixa:
	for (short boxId = 0; boxId < s->boxes; boxId++) {
		found |= updateTrie(&tempTrie, s->posBoxes[boxId]);
	}

	found |= updateTrie(&tempTrie, s->posPlayer);

	return found;
}

//---------------------------------------------------------------------------------------------

// Função que retorna se o estado é único ou não
unsigned char getStateId(State *s) {
	// Fazemos um sort pois a ordem das caixas não pode importar
	quickSort(s->posBoxes, 0, s->boxes - 1);
	return findId(mainId, s) == 0;
}

int main(int argc, char *argv[]) {
	struct timespec before, after;
	time_t nSeconds;

	// Começamos a contagem de tempo.
	clock_gettime(CLOCK_REALTIME, &before);

	// Criamos espaço para uma variável compartilhada que verifica se foi
	// encontrado uma solução por algum dos threads
	unsigned char *solution = malloc(1);
	*solution = 0;

	// Criamos espaço para a raiz da lista principal
	State *root = malloc(sizeof(State));
	root->nextState = NULL;

	// Criamos um ponteiro temporário que irá ser movido
	State *s = malloc(sizeof(State));

	// Ponteiro para o último estado da lista principal.
	State **lastMainState = malloc(sizeof(State *));
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
	}

	// Finalizamos a contagem de tempo.
	clock_gettime(CLOCK_REALTIME, &after);

	// Calcula o tempo passado em microssegundos.
	nSeconds =
	    after.tv_nsec - before.tv_nsec + (after.tv_sec - before.tv_sec) * NANOS;

	printf("Tempo total: %lu ns - %lf s\n", nSeconds, (double)nSeconds / NANOS);

	return 0;
}