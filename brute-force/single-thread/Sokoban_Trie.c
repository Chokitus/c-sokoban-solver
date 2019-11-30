#include "../../common/sort.h"
#include "../../common/structures.h"
#include "../../common/common.h"
#include "../../common/util.h"
#include <omp.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Trie para os ids.
typedef struct idTrie {
	struct idTrie *idLeafs[10];
} idTrie;

// Ponteiro para a raiz da trie de ids.
idTrie *mainId;

// Largura e altura do mapa
unsigned int width = 0;
unsigned int height = 0;

// Ponteiro para o último estado da lista principal.
State **lastMainState;

void insertId(idTrie *tempTrie, State *s, unsigned short lastI);

// Cria um novo nó na trie
idTrie *new_trie() {
	idTrie *returnTrie = malloc(sizeof(idTrie));
	memset(returnTrie->idLeafs, 0, 10 * sizeof(idTrie *));
	return returnTrie;
}
int memoryInsert = 0;

// Função que procura o id na lista
unsigned char findId(State *s) {
	// Apontamos para a mainTrie
	idTrie *tempTrie = mainId;

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

// Função para a construção do mapa
void buildMap(State *s, char *level) {
	// Número de caixas, global
	s->boxes = 0;

	// Inicializamos o número de caixas em objetivos
	s->boxesOnGoals = 0;

	// Inicializamos a posição das caixas
	memset(s->posBoxes, 0, 30);

	// Inicializamos o grid com 32, ou ' '
	memset(s->grid, 32, 400);

	// Ponteiro para o último estado principal é inicializado.
	lastMainState = malloc(sizeof(State *));
	*lastMainState = NULL;

	// Ponteiro para a raiz da trie de Ids
	mainId = malloc(sizeof(idTrie));
	memset(mainId->idLeafs, 0, 10 * sizeof(idTrie *));

	// Inicializamos a ação com nulo, pois é como saberemos que voltamos até o
	// começo do processo
	s->lastAction = malloc(sizeof(ActionList));
	s->lastAction->prevAction = NULL;
	s->lastAction->action = 0;

	// Abrimos o nível
	char str[16] = "levels/";

	strcat(str, level);

	FILE *file = fopen(str, "r");

	char line[20];

	int x, maxWidth;
	maxWidth = 0;
	height = -1;

	// Pegamos 20 caracteres de cada linha, até que a linha retorne nulo.
	while (fgets(line, 20, file) != NULL) {
		// Se a linha for 0 ou 10 (nova linha), estamos fora do mapa
		if (line[0] != 0 && line[0] != 10) {
			// Aumentamos a altura
			height++;

			// Pra cada caracter, colocamos o objeto no mapa.
			for (x = 0; (int)line[x] != 0 && (int)line[x] != 10; x++) {
				placeThis(line[x], x, height, s);
			}

			// Procuramos o maior x para chamar de largura
			if (x > maxWidth) {
				maxWidth = x;
			}

			x = 0;
		}
	}

	width = maxWidth;

	fclose(file);

	getStateId(s);
};

// Função que verifica se o estado é final
// Dado que este algoritmo foi implementado possuindo os nívels -1, 00 e 01 em
// mente, este não está preparado para níveis que possuam mais caixas que
// objetivos
unsigned char isFinal(State *s) {
	if (s->boxes == s->boxesOnGoals) {
		return 1;
	}
	return 0;
}

// Função que usamos para inserir o estado
unsigned char insertState(State *root, State *s, State **lastThreadState) {
	if (isFinal(s)) {
		//É final
		return 1;
	}

	// Lista está vazia ou só possui o root.
	if (root->nextState == NULL) {
		// Criamos um novo espaço após root
		root->nextState = malloc(sizeof(State));

		// Copiamos o estado
		copyState(s, root->nextState);

		// Last aponta para o último estado. Este last pode ser o da lista
		// principal, ou do thread
		(*lastThreadState) = root->nextState;

		return 0;
	}

	// A lista possui mais de um, e podemos usar seguramente o last
	(*lastThreadState)->nextState = malloc(sizeof(State));
	copyState(s, (*lastThreadState)->nextState);

	// Mudamos a posição do último estado.
	*lastThreadState = (*lastThreadState)->nextState;
	(*lastThreadState)->nextState = NULL;
	return 0;
}

// Função que move uma das listas, enquanto cria a raiz para a outra
void popState(State **from, State **to) {
	// Se ambos são o mesmo, devemos fazer uma operação de retirar um nó,
	// somente
	if (*to == *from) {
		State *freeableState = *to;
		*from = (*from)->nextState;
		free(freeableState);
		return;
	}
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

	// Constroi o primeiro estado, sequencialmente
	buildMap(root, argv[1]);

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