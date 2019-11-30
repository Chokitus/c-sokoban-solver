#include "../../common/sort.h"
#include "../../common/structures.h"
#include "../../common/common.h"
#include "../../common/util.h"
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Quantos estados cada thread deve explorar
#define SIZE_THREAD_LIST 10000

// Quantos estados por thread a main deve criar antes de seguir
#define NUM_MAIN_STATES 1000

// Trie para os ids.
typedef struct idTrie {
	struct idTrie *idLeafs[10];
} idTrie;

// Locks a serem usados
omp_lock_t **lockLevels;

// Ponteiro para a raiz da trie de ids.
idTrie *mainId;

// Ponteiro para o último estado da lista principal.
State **lastMainState;

// Cria um novo nó na trie
idTrie *new_trie() {
	idTrie *returnTrie = malloc(sizeof(idTrie));
	memset(returnTrie->idLeafs, 0, 10 * sizeof(idTrie *));
	return returnTrie;
}

// Função que procura o id na lista
unsigned char findId(State *s) {
	// Apontamos para a mainTrie
	idTrie *tempTrie = mainId;

	unsigned short tempValue = 0;
	unsigned char found = 0;
	unsigned short visitedLevel = 0;

	/*
	    É importante somente travar cada "andar" da trie com um lock.
	    Esta implementação possui, pelo menos, 3*(caixas+player) locks, o que
	   significa que conseguimos travar cada andar uma vez.

	*/

	// Para cada caixa:
	for (short i = 0; i < s->boxes; i++) {
		if (s->posBoxes[i] > 100) {
			omp_set_lock(lockLevels[visitedLevel]);

			if (!tempTrie->idLeafs[s->posBoxes[i] / 100]) {
				tempTrie->idLeafs[s->posBoxes[i] / 100] = new_trie();
				found = 1;
			}
			tempTrie = tempTrie->idLeafs[s->posBoxes[i] / 100];

			omp_unset_lock(lockLevels[visitedLevel]);

			visitedLevel++;
		}
		tempValue = (s->posBoxes[i] / 10);

		omp_set_lock(lockLevels[visitedLevel]);

		if (!tempTrie->idLeafs[tempValue % 10]) {
			tempTrie->idLeafs[tempValue % 10] = new_trie();
			found = 1;
		}
		tempTrie = tempTrie->idLeafs[tempValue % 10];

		omp_unset_lock(lockLevels[visitedLevel]);

		visitedLevel++;

		omp_set_lock(lockLevels[visitedLevel]);

		if (!tempTrie->idLeafs[s->posBoxes[i] - tempValue * 10]) {
			tempTrie->idLeafs[s->posBoxes[i] - tempValue * 10] = new_trie();
			found = 1;
		}
		tempTrie = tempTrie->idLeafs[s->posBoxes[i] - tempValue * 10];

		omp_unset_lock(lockLevels[visitedLevel]);

		visitedLevel++;
	}

	omp_set_lock(lockLevels[visitedLevel]);

	if (s->posPlayer > 100) {
		if (!tempTrie->idLeafs[s->posPlayer / 100]) {
			tempTrie->idLeafs[s->posPlayer / 100] = new_trie();
			found = 1;
		}
		tempTrie = tempTrie->idLeafs[s->posPlayer / 100];
	}
	tempValue = (s->posPlayer / 10);

	omp_unset_lock(lockLevels[visitedLevel]);

	visitedLevel++;

	omp_set_lock(lockLevels[visitedLevel]);

	if (!tempTrie->idLeafs[tempValue % 10]) {
		tempTrie->idLeafs[tempValue % 10] = new_trie();
		found = 1;
	}
	tempTrie = tempTrie->idLeafs[tempValue % 10];

	omp_unset_lock(lockLevels[visitedLevel]);

	visitedLevel++;

	omp_set_lock(lockLevels[visitedLevel]);

	if (!tempTrie->idLeafs[s->posPlayer - tempValue * 10]) {
		tempTrie->idLeafs[s->posPlayer - tempValue * 10] = new_trie();
		found = 1;
	}

	omp_unset_lock(lockLevels[visitedLevel]);
	return found;
}

//---------------------------------------------------------------------------------------------

// Função que retorna se o estado é único ou não
unsigned char getStateId(State *s) {
	// Fazemos um sort pois a ordem das caixas não pode importar
	quickSort(s->posBoxes, 0, s->boxes - 1);

	/*
	    Procuramos o ID na trie. Se estiver, retornamos verdadeiro, se não
	   estiver o colocamos nela.
	*/

	unsigned char newId;
	newId = findId(s);

	return newId;
}

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
    mainLast						 threadroot
    ----------					----------
    |			|   nextState  |			|
    |			|------------->|			|
    |			|              |			|
    ----------              ----------
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
	mainId = malloc(sizeof(idTrie));
	memset(mainId->idLeafs, 0, 10 * sizeof(idTrie *));

	// Constroi o primeiro estado, sequencialmente
	buildMap(root, argv[1]);
	getStateId(root);

	// Criamos 3*(número_de_caixas+player) locks
	lockLevels = malloc((root->boxes + 1) * 3 * sizeof(omp_lock_t *));
	for (int levels = 0; levels < (root->boxes + 1) * 3; levels++) {
		lockLevels[levels] = malloc(sizeof(omp_lock_t *));
		omp_init_lock(lockLevels[levels]);
	}

	// Quantidade de threads solicitados
	int threads = strtol(argv[2], NULL, 10);

	// Pediremos que main faça NUM_MAIN_STATES estados para cada thread
	unsigned int numStates = NUM_MAIN_STATES * threads;

	while (mainStates < numStates) {
		for (int i = 0; i < 4; i++) {
			// Pra cada direção, nós copiamos o estado inicial
			copyState(root, s);
			if (movePlayer(s, i, getStateId) != 0) {
				/*movePlayer retorna 0 se não foi possível mover, seja por uma
				caixa sendo empurrada numa parede,
				seja por estarmos andando de cara na parede*/
				mainStates++;
				if (insertState(root, s, lastMainState)) {
					// Se ele entrou aqui, quer dizer que, durante a inserção,
					// foi notado que ele é um estado final.
					printPath(s);
					*solution = 1;
					// Finalizamos a contagem de tempo.
					clock_gettime(CLOCK_REALTIME, &after);

					// Calcula o tempo passado em microssegundos.
					nSeconds = after.tv_nsec - before.tv_nsec +
					           (after.tv_sec - before.tv_sec) * NANOS;

					printf("Achei sem threads: %lu ns - %lf s\n", nSeconds,
					       (double)nSeconds / NANOS);
					return 0;
				}
			}
		}
		// Movemos root, colocando root como próximo estado
		popState(&root, &root);
	}
	// Chegando aqui, temos uma lista ligada à root com n<=4 estados.
	/*
	   A estratégia aqui é: criar n threads, e sequencialmente cada um pega um
	   estado da lista para si. Abriremos estes estados, agora paralelamente, em
	   cada thread, criando uma lista ligada parcial. Cada thread procedirá para
	   criar SIZE_THREAD_LIST estados, e então conectá-lo á lista principal.
	*/

	// root, lastMainState e solution serão compartilhados, todo resto é
	// declarado internamente e portanto, são privados.
#pragma omp parallel num_threads(threads) shared(root, lastMainState, solution)
	{
		// threadRoot será a raiz da lista ligada temporária de cada thread
		State *threadRoot = NULL;

		// Estado para ser movido
		State *s;

		// Variável de condição que nos diz se devemos pegar um estado da lista
		// principal ou não
		unsigned char popMainList = 1;

		// Quantidade de estados ativos no thread
		unsigned int activeThreadStates = 0;

		// Criamos espaço para o estado temporário móvel
		s = malloc(sizeof(State));

		// Criamos espaço para o ponteiro para o último estado presente neste
		// thread
		State **lastThreadState;
		lastThreadState = malloc(sizeof(State *));
		(*lastThreadState) = NULL;

		// Enquanto não foi encontrado uma solução por nenhum thread
		while (!(*solution)) {

			// Se a variável de condição foi 1, devemos pegar um estado da lista
			// principal. Isto só acontecerá caso chegamos no limite estipulado
			// para cada thread, ou caso esta seja a primeira iteração de cada
			// thread
			if (popMainList) {
				// Esta região deve ser crítica, pois estamos mexendo com a
				// lista principal (e portante shared)
#pragma omp critical(popMerge)
				popState(&root, &threadRoot);

				// Limpamos o popMainList
				popMainList = 0;
			}

			// Pra cada direção, iremos mover o estado, e depois adicionar na
			// nossa lista temporária.
			for (int i = 0; i < 4 && !(*solution); i++) {
				copyState(threadRoot, s);
				if (movePlayer(s, i, getStateId) != 0) {
					// Entrou aqui, quer dizer que ele conseguiu se mover, ou
					// seja, era um movimento válido.
					activeThreadStates++;
					if (insertState(threadRoot, s, lastThreadState)) {
						// Entrou aqui quer dizer que o estado era final, de
						// acordo com a definição de estado final.
						printPath(s);
						*solution = 1;
					}
				}
			}
			// Chegado aqui, exploramos as quatro direções.

			// Tentaremos criar uma lista de pelo menos SIZE_THREAD_LIST
			// elementos antes de adicionar à lista principal. Caso não
			// conseguimos estados suficientes, activeThreadStates = -1, todos os
			// nós que pegamos eram inúteis. Isso significa que precisamos pegar
			// novos nós da lista principal
			if (activeThreadStates < SIZE_THREAD_LIST &&
			    activeThreadStates > 0) {
				// Desempilhamos mais um, agora da nossa lista temporária, pois
				// não passamos da quantidade necessária
				popState(&threadRoot, &threadRoot);
				activeThreadStates--;
			} else {
				if (activeThreadStates > 0 && !(*solution)) {
					// há lista para empilhar
					// Como no pop acima, esta região é critica (e de mesmo nome
					// do pop) pois mexe com a lista principal
#pragma omp critical(popMerge)
					mergeLinkedLists(&threadRoot, lastThreadState, &root,
					                 lastMainState);

					// Não há mais estados ativos no thread
					activeThreadStates = 0;
				} /*if*/
				// Ordenamos que se retire da lista principal mais um nó para
				// ser expandido
				popMainList = 1;
			} /*else*/

		} /*while*/

	} /*pragma*/

	// Finalizamos a contagem de tempo.
	clock_gettime(CLOCK_REALTIME, &after);

	// Calcula o tempo passado em microssegundos.
	nSeconds =
	    after.tv_nsec - before.tv_nsec + (after.tv_sec - before.tv_sec) * NANOS;

	printf("Tempo total: %lu ns - %lf s\n", nSeconds, (double)nSeconds / NANOS);

	return 0;
}