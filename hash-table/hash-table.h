
#include "../common/structures.h"

#ifndef _HASH_TABLE_H_
#define _HASH_TABLE_H_

// Valor do módulo usado no hashing do id (que por si só é um hash)
#define ID_HASH 100000000

// Lista ligada (com hash de resto da divisão) para os ids visitados.
typedef struct VisitedId {
	unsigned char *stateId;
	struct VisitedId *nextId;
} VisitedId;

// Lista ligada (ordenada) com cada estado.
typedef struct Node {
	State *state;
	struct Node *nextState;
} Node;

// Função que procura o id na lista
extern unsigned char findId(VisitedId *idList[ID_HASH], char *idHash,
                            unsigned long long int id);

extern void insertId(VisitedId *idList[ID_HASH], unsigned char *idHash,
                     unsigned long long int id);

// Função que retorna o índice relacionado ao ID.
// Esta é a função de hash djb2:
extern unsigned long long int getIdIndex(State *s);

extern unsigned char insertState(Node **root, Node ***lastNode, State *s);

// Função que devolve, em rootState, o primeiro estado na lista cuja raiz é root
extern void popState(Node **root, State **rootState);

#endif