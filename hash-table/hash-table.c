#include "hash-table.h"
#include "../common/common.h"
#include "../common/util.h"
#include <stdlib.h>

// Função que procura o id na lista
unsigned char findId(VisitedId *idList[ID_HASH], char *idHash,
                     unsigned long long int id) {
	// Primeiro pegamos o index
	unsigned int idIndex = id % ID_HASH;

	// Começamos da cabeça da lista
	VisitedId **idIterator = &(idList[idIndex]);

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

void insertId(VisitedId *idList[ID_HASH], unsigned char *idHash,
              unsigned long long int id) {
	// Começamos da cabeça da lista
	VisitedId **idIterator = &(idList[id % ID_HASH]);

	// Se o primeiro id é vazio, devemos criar um novo id
	if ((*idIterator) == NULL) {
		*idIterator = (VisitedId *)malloc(sizeof(VisitedId));
		(*idIterator)->stateId = idHash;
		(*idIterator)->nextId = NULL;
		return;
	}
	// O primeiro não era vazio, então iteramos até encontrar o último
	while ((*idIterator)->nextId != NULL) {
		idIterator = &((*idIterator)->nextId);
	}
	// Criamos um id
	(*idIterator)->nextId = (VisitedId *)malloc(sizeof(VisitedId));
	(*idIterator)->nextId->stateId = idHash;
	(*idIterator)->nextId->nextId = NULL;
}

// Função que retorna o índice relacionado ao ID.
// Esta é a função de hash djb2:
unsigned long long int getIdIndex(State *s) {
	unsigned long long h = 5381;

	for (int i = 0; i < s->boxes; i++) {
		h = ((h << 5) + h) + s->posBoxes[i] / 2 + s->posBoxes[i] % 20;
	}
	h = ((h << 5) + h) + s->posPlayer / 2 + s->posPlayer % 20;
	return h;
}

unsigned char insertState(Node **root, Node **last, State *s) {
	if (isFinal(s)) {
		//É final
		return 1;
	}

	// Lista está vazia.
	if ((*root) == NULL) {
		// Criamos o nó
		(*root) = (Node *)malloc(sizeof(Node));

		// Last também estará nulo neste caso, portanto criaremos um novo.
		*last = (Node *)malloc(sizeof(Node));
		(*last)->state = NULL;

		// A raiz aponta para o último
		(*root)->nextState = (*last);

		// Colocamos o estado no nó
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
	*last = (*last)->nextState;
	return 0;
}

// Função que devolve o primeiro estado na lista cuja raiz é root
State *popState(Node **root) {
	State *rootState = (*root)->state;

	Node *tempNode = (*root);
	(*root) = (*root)->nextState;
	free(tempNode);

	return rootState;
}
