#include "trie.h"
#include <stdlib.h>

// Cria um novo nó na trie
IdTrie *new_trie() {
	IdTrie *returnTrie = malloc(sizeof(IdTrie));
	memset(returnTrie->idLeafs, 0, 10 * sizeof(IdTrie *));
	return returnTrie;
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
	// Ambos são diferentes, então é o thread solicitando da lista principal
	// Limpamos o que há no thread
	free(*to);

	// Thread recebe o primeiro valor da lista principal
	*to = *from;

	// Lista principal anda em um passo
	*from = (*from)->nextState;

	// Limpamos o próximo estado no thread, de forma que este não esteja
	// conectado com a lista principal.
	(*to)->nextState = NULL;
}

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
void mergeLinkedLists(State **threadRoot, State **lastThreadState,
                      State **mainRoot, State **mainLast) {
	// O último estado da lista principal recebe o primeiro estado do thread
	if ((*mainRoot) == NULL) {
		*mainRoot = *threadRoot;
	}
	(*mainLast)->nextState = (*threadRoot);
	*mainLast = *lastThreadState;
	*threadRoot = malloc(sizeof(State));
	*lastThreadState = NULL;
}

/**
 * Moves trie to leaf on a position. If leaf does not exist, it is created.
 * @param trie Trie that will be moved
 * @param position Leaf position on leaves array.
 * @return 0 if position is already defined, 1 if it was created by this function.
 * @author marcos.romero
 */
unsigned char moveTrieToLeaf(IdTrie **trie, unsigned short position) {
	unsigned char created;
	if ((*trie)->idLeafs[position]) {
		created = 0;
	} else {
		created = 1;
		(*trie)->idLeafs[position] = new_trie();
	}
	*trie = (*trie)->idLeafs[position];
	return created;
}
