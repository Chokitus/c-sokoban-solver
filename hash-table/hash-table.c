#include "hash-table.h"
#include "../common/common.h"
#include "../common/sort.h"
#include "../common/util.h"
#include <stdio.h>
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

// Função que devolve o primeiro estado na lista cuja raiz é root
State *popState(Node **root) {
	State *rootState = (*root)->state;

	Node *tempNode = (*root);
	(*root) = (*root)->nextState;
	free(tempNode);

	return rootState;
}

/**
 * Checks if a state has an ID on the hash table.
 * @param idList ?
 * @param s State for which it will search the id
 * @return 1 if ID is found. 0 otherwise
 * @author marcos.romero
 */
unsigned char checkIfStateIdExists(VisitedId *idList[ID_HASH], State *s) {

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
