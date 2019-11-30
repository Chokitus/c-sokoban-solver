#include "../common/sort.h"
#include "../common/structures.h"
#include "../common/common.h"
#include "../common/util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Valor do módulo usado no hashing do id (que por si só é um hash)
#define ID_HASH 100000000

// Lista ligada (com hash de resto da divisão) para os ids visitados.
typedef struct visitedId {
	unsigned char *stateId;
	struct visitedId *nextId;
} visitedId;

// Lista ligada com cada estado.
typedef struct Node {
	State *state;
	struct Node *nextState;
} Node;

// Quantidade de nós visitados
unsigned int numberOfNodes;

// Array para o espalhamento dos IDs.
visitedId *idList[ID_HASH];

// Ponteiro para o último nó
Node **last;

// Função que procura o id na lista
unsigned char findId(char *idHash, unsigned long long int id) {
	// Primeiro pegamos o index
	unsigned int idIndex = id % ID_HASH;

	// Começamos da cabeça da lista
	visitedId **idIterator = &(idList[idIndex]);

	// E iteramos até o final
	while ((*idIterator) != NULL) {
		if (strcmp((*idIterator)->stateId, idHash) == 0) {
			// Encontrou o id, significa que este estado foi visitado antes
			return 1;
		}
		idIterator = &((*idIterator)->nextId);
	}
	// Não encontrou, é um estado novo
	return 0;
}

void insertId(unsigned char *idHash, unsigned long long int id) {
	// Começamos da cabeça da lista
	visitedId **idIterator = &(idList[id % ID_HASH]);

	// Se o primeiro id é vazio, devemos criar um novo id
	if ((*idIterator) == NULL) {
		*idIterator = (visitedId *)malloc(sizeof(visitedId));
		(*idIterator)->stateId = idHash;
		(*idIterator)->nextId = NULL;
		return;
	}
	// O primeiro não era vazio, então iteramos até encontrar o último
	while ((*idIterator)->nextId != NULL) {
		idIterator = &((*idIterator)->nextId);
	}
	// Criamos um id
	(*idIterator)->nextId = (visitedId *)malloc(sizeof(visitedId));
	(*idIterator)->nextId->stateId = idHash;
	(*idIterator)->nextId->nextId = NULL;
}

//-------------------------------------------------------------------

// Função que retorna o índice relacionado ao ID.
// Esta é a função de hash djb2:
unsigned long long int getIdIndex(State *s) {
	unsigned long long h = 5381;

	for (int i = 0; i < s->boxes; i++) {
		h = ((h << 5) + h) + s->posBoxes[i] / 2 + s->posBoxes[i] % 20;
	}
	h = ((h << 5) + h) + s->posPlayer / 2 + s->posPlayer % 20;
}

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
	if (findId(idHash, h) == 1) {
		return 1;
	}

	// Sendo id único, inserimos o mesmo
	insertId(idHash, h);

	return 0;
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
unsigned char insertState(Node **root, State *s) {
	if (isFinal(s)) {
		//É final
		return 1;
	}

	// Lista está vazia.
	if ((*root) == NULL) {
		// Criamos o nó
		(*root) = (Node *)malloc(sizeof(Node));

		// Last também estará nulo neste caso, portanto criaremos um novo.
		(*last) = (Node *)malloc(sizeof(Node));
		(*last)->state = NULL;

		// A raiz aponta para o último
		(*root)->nextState = (*last);

		// Colocamos o estado no nó raiz
		(*root)->state = (State *)malloc(sizeof(State));

		copyState(s, (*root)->state);
		return 0;
	}

	// Colocamos este estado no último
	(*last)->state = (State *)malloc(sizeof(State));
	copyState(s, (*last)->state);
	(*last)->nextState = (Node *)malloc(sizeof(Node));
	(*last)->nextState->state = NULL;

	// Mudamos a posição do último estado.
	last = &((*last)->nextState);
	return 0;
}

// Função que devolve, em rootState, o primeiro estado na lista cuja raiz é root
void popState(Node **root, State **rootState) {

	// Transfere o estado
	*rootState = (*root)->state;

	// Se o próximo esta vazio, este deve ser anulado
	if ((*root)->nextState == NULL) {
		free((*root));
		(*root) = NULL;
		return;
	}

	// Há mais de um, portanto devemos trocar de lugar o antigo com o novo
	Node *tempNode = (*root);
	(*root) = (*root)->nextState;
	free(tempNode);
}

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
				final = insertState(&root, s);
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