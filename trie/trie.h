
#include "../common/structures.h"
#include "../common/sort.h"
#include "../common/common.h"

#ifndef _TRIE_H_
#define _TRIE_H_

// Quantos estados cada thread deve explorar
#define SIZE_THREAD_LIST 10000

// Quantos estados por thread a main deve criar antes de seguir
#define NUM_MAIN_STATES 1000

// Trie para os ids.
typedef struct IdTrie {
	struct IdTrie *idLeafs[10];
} IdTrie;

// Cria um novo nó na trie
extern IdTrie *new_trie();

// Função que usamos para inserir o estado
extern unsigned char insertState(State *root, State *s,
                                 State **lastThreadState);

// Função que move uma das listas, enquanto cria a raiz para a outra
extern void popState(State **from, State **to);

// Fazemos merge entre as duas listas, conectando o final da main com o começo
// da thread
/*
    mainLast					threadroot
    ------------				------------
    |			|   nextState  |			|
    |			|------------->|			|
    |			|              |			|
    ------------				------------
*/
extern void mergeLinkedLists(State **threadRoot, State **lastThreadState,
                      State **mainRoot, State **mainLast);

#endif