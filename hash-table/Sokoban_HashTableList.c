#include "../common/sort.h"
#include "../common/structures.h"
#include "../common/common.h"
#include "../common/util.h"
#include "hash-table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Array para o espalhamento dos IDs.
VisitedId *idList[ID_HASH];

// Ponteiro para o último nó
Node **last;

//-------------------------------------------------------------------

// Função de Hash para pegar o ID do Estado
unsigned char getStateId(State *s) {

	// Fazemos um sort pois a ordem das caixas não pode importar
	quickSort(s->posBoxes, 0, s->boxes - 1);

	// Pensando que cada caixa tem até 3 dígitos, precisamos de 3+1 dígitos para
	// cada baixa, e após 3 dígitos para o player
	unsigned char idHash[s->boxes * 4 + 4];

	// Criamos um buffer
	unsigned char buffer[8];

	memset(idHash, 0, s->boxes * 4 + 4);

	unsigned long long h = getIdIndex(s);

	// Para cada caixa, colocamos sua posição no buffer, e concatenamos no
	// idHash
	for (int i = 0; i < s->boxes; i++) {
		sprintf(buffer, "%d", s->posBoxes[i]);
		strcat(idHash, buffer);
		strcat(idHash, " ");
	}

	// Fazemos o mesmo para a posição do player
	sprintf(buffer, "%d", s->posPlayer);
	strcat(idHash, buffer);

	// Procuramos o ID na lista. Se estiver, retornamos verdadeiro
	if (findId(idList, idHash, h) == 1) {
		return 1;
	}

	// Sendo id único, inserimos o mesmo
	insertId(idList, idHash, h);

	return 0;
};

int main(int argc, char *argv[]) {
	struct timespec before, after;
	time_t nSeconds;

	// Começamos a contagem de tempo.
	clock_gettime(CLOCK_REALTIME, &before);

	// Criamos a raiz
	Node *root = (Node *)malloc(sizeof(Node));

	State *s, *rootState;
	unsigned char final;

	// Alocamos espaço para a raiz
	root->state = (State *)malloc(sizeof(State));
	root->nextState = NULL;

	// Apontamos cada um dos Ids para nulo.
	memset(idList, 0, ID_HASH);

	// Inicializamos o ponteiro para o último estado
	last = (Node **)malloc(sizeof(void *));
	(*last) = NULL;

	// Criamos o mapa
	buildMap(root->state, argv[1]);
	getStateId(root->state);

	// Alocamos espaço para o estado que estará sujeito à ser movido
	s = (State *)malloc(sizeof(State));

	while (final != 1) {
		// Pegamos o primeiro estado em root e devolvemos em rootState
		popState(&root, &rootState);

		// Cada cada direção
		for (int i = 0; i < 4; i++) {

			// Copiamos o estado para s
			copyState(rootState, s);

			// Se foi possível mover, insere o estado
			if (movePlayer(s, i, getStateId) != 0) {

				// Verifica se este estado é final, junto com inseri-lo na lista
				final = insertState(&root, &last, s);
			}

			// Se for final, printamos o caminho
			if (final == 1) {
				printPath(s);
				break;
			}
		}
		// Limpamos o conteudo em rootState
		free(rootState);
	}
	// Finalizamos a contagem de tempo.
	clock_gettime(CLOCK_REALTIME, &after);

	// Calcula o tempo passado em microssegundos.
	nSeconds =
	    after.tv_nsec - before.tv_nsec + (after.tv_sec - before.tv_sec) * NANOS;

	printf("Tempo total: %lu ns - %lf s\n", nSeconds, (double)nSeconds / NANOS);

	return 0;
}