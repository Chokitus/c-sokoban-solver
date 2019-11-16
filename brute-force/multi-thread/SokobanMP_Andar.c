#include "../../common/sort.h"
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define copyState(s, s2) memcpy(s2, s, sizeof(State))

// Quantos estados cada thread deve explorar
#define SIZE_THREAD_LIST 10000

#define NANOS ((unsigned long)1000000000)

// Quantos estados por thread a main deve criar antes de seguir
#define NUM_MAIN_STATES 1000

enum Direcoes { direita = 0, esquerda = 1, cima = 2, baixo = 3 };

// Estado que estaremos analisando
typedef struct State {
	// Todas as posições se baseiam no fato que não há mapa com mais de 19 de
	// largura/altura. Portanto, a posição é 20*y+x; Posição do Sokoban
	unsigned short posPlayer;

	// Posição das caixas
	unsigned short posBoxes[30];

	// Mapa, usado para facilitar no movimento.
	unsigned char grid[400];

	// Quantidade de caixas no objetivo
	unsigned char boxesOnGoals;

	// Caminho para a solução
	struct actionList *lastAction;

	// Próximo Estado, considerando que isto também é uma lista ligada
	struct State *nextState;
} State;

// Número de caixas no nível
unsigned char boxes;

// Trie para os ids.
typedef struct idTrie {
	struct idTrie *idLeafs[10];
} idTrie;

// Ligada ligada para as ações
typedef struct actionList {
	unsigned char action;
	struct actionList *prevAction;
} actionList;

// Locks a serem usados
omp_lock_t **lockLevels;

// Ponteiro para a raiz da trie de ids.
idTrie *mainId;

// Largura e altura do mapa
unsigned int width = 0;
unsigned int height = 0;

// Ponteiro para o último estado da lista principal.
State **lastMainState;

// Função que imprime o caminho para a solução
void printPath(State *s) {
	// Apontamos para a última ação tomada
	actionList *tempAction = s->lastAction;

	// Criamos mais que o espaço necessário para qualquer solução
	unsigned char actions[10000];
	memset(actions, 0, 10000);

	int i;
	// Enquanto não chegamos ao final (que é nulo)
	for (i = 0; tempAction; i++) {
		actions[i] = tempAction->action;
		tempAction = tempAction->prevAction;
	}
	i--;

	// Printamos em ordem reversa
	for (; i >= 0; i--) {
		printf("%c", actions[i]);
	}
	printf("\n");
}
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
	for (short i = 0; i < boxes; i++) {
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

// Função printar o grid: DEBUG
void printGrid(State *s) {
	for (int h = 0; h < (height + 1) * 20; h += 20) {
		for (int w = 0; w < width; w++) {
			if (s->grid[w + h] == 32) {
				printf("  ");
			} else {
				printf("%c ", s->grid[w + h]);
			}
		}
		printf("\n");
	}
	printf("\n");
}

// Função que retorna se o estado é único ou não
unsigned char getStateId(State *s) {
	// Fazemos um sort pois a ordem das caixas não pode importar
	quickSort(s->posBoxes, 0, boxes - 1);

	/*
	    Procuramos o ID na trie. Se estiver, retornamos verdadeiro, se não
	   estiver o colocamos nela.
	*/

	unsigned char newId;
	newId = findId(s);

	return newId;
}

// Função para adicionar a ação ao caminho
void addPath(char *c, State *s) {
	// Criamos uma nova ação
	actionList *nextAction = malloc(sizeof(actionList));

	// Esta ação recebe a ação causada pelo movePlayer
	nextAction->action = *c;

	// E aponta para a ação anterior
	nextAction->prevAction = s->lastAction;

	// O estado agora aponta para a sua ação geradora.
	s->lastAction = nextAction;
};

// Função para construir o grid
void placeThis(char c, int x, int y, struct State *s) {
	if (c == 32 || (c != '@' && c != '$' && c != '.' && c != '*' && c != '+' &&
	                c != '#')) {
		// Espaço vazio
		return;
	}

	// Como o mapa é considerado como um quadrado de 19 por 19, cada posição
	// pode ser linearizada como x+20*y. Assim, a posição (17,4) = 17 + 80 = 97
	int pos = x + 20 * y;

	// Colocamos no grid o objeto
	s->grid[pos] = c;

	if (c == '@') {
		//É o player.
		s->posPlayer = pos;
		return;
	}
	if (c == '$') {
		//É uma caixa.
		s->posBoxes[boxes++] = pos;
		return;
	}

	if (c == '.') {
		//É um alvo.
		return;
	}

	if (c == '*') {
		//É um alvo e uma caixa.
		s->posBoxes[boxes++] = pos;
		s->boxesOnGoals++;
		return;
	}

	if (c == '+') {
		//É o player e um alvo.
		s->posPlayer = pos;
	}
}

// Função para a construção do mapa
void buildMap(struct State *s, char *level) {
	// Número de caixas, global
	boxes = 0;

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
	s->lastAction = malloc(sizeof(actionList));
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

	// Criamos 3*(número_de_caixas+player) locks
	lockLevels = malloc((boxes + 1) * 3 * sizeof(omp_lock_t *));
	for (int levels = 0; levels < (boxes + 1) * 3; levels++) {
		lockLevels[levels] = malloc(sizeof(omp_lock_t *));
		omp_init_lock(lockLevels[levels]);
	}

	width = maxWidth;

	fclose(file);

	getStateId(s);
};

// Função simples que retorna se a posição pos é uma parede
int checkWallsAt(State *s, unsigned int pos) {
	if (s->grid[pos] == '#') {
		return 1;
	}
	return 0;
};

// Função que verifica se prendemos a caixa
/*
     #
    #$
    Procuramos "cantos" como este. Se a caixa está num canto, e este canto não é
   um objetivo, travamos a caixa Esta é uma posição exemplo que não seria
   explorada:
    #######
    #   @$# <- Note que a caixa não poderá ser movida
    #     #
    #. #  #
    #. $  #
    #.$$  #
    #.#  @#
    #######
*/
unsigned char boxTrapped(State *s, int pos) {
	int wallsUp = s->grid[pos - 20] == '#',
	    wallsDown = s->grid[pos + 20] == '#',
	    wallsLeft = s->grid[pos - 1] == '#',
	    wallsRight = s->grid[pos + 1] == '#';
	if (s->grid[pos] == '.' || s->grid[pos] == '*') {
		return 0;
	}
	if (wallsUp && wallsLeft) {
		return 1;
	}
	if (wallsUp && wallsRight) {
		return 1;
	}
	if (wallsDown && wallsLeft) {
		return 1;
	}
	if (wallsDown && wallsRight) {
		return 1;
	}

	return 0;
}

// Função que usaremos intensivamente para mover o sokoban
char movePlayer(struct State *s, int dir) {
	// dir == 0 -> direita
	// dir == 1 -> esquerda
	// dir == 2 -> cima
	// dir == 3 -> baixo
	char c;
	unsigned char box = 0;
	int tempPos;
	int movingParam;
	switch (dir) {
	case direita:
		movingParam = 1;
		c = 'r';
		break;
	case esquerda:
		movingParam = -1;
		c = 'l';
		break;
	case cima:
		movingParam = -20;
		c = 'u';
		break;
	case baixo:
		movingParam = 20;
		c = 'd';
		break;
	}

	// Usamos o trecho de código para obter o parametro de movimento, assim como
	// o caracter. Como parâmetro de movimento, queremos transformar a visao 1D
	// do array no movimento 2D do personagem, ou seja, dado que cada linha
	// possui 20 colunas, "andar para cima" recua 20 colunas, por isso o
	// movingParam é -20. A mesma lógica se aplica para os outros

	tempPos = s->posPlayer + movingParam;

	if (checkWallsAt(s, tempPos)) {
		// Tem uma parede.
		return 0;
	}

	if (s->grid[tempPos] == '$' || s->grid[tempPos] == '*') {
		// Tem uma caixa na direção
		if (checkWallsAt(s, tempPos + movingParam) == 1 ||
		    s->grid[tempPos + movingParam] == '$' ||
		    s->grid[tempPos + movingParam] == '*') {
			// Tem uma parede ou caixa após a caixa.
			return 0;
		};
		// Deixa a letra maiuscula
		c -= 32;
		box = 1;
	}

	// Efetiva o movimento
	if (box != 0) {
		if (boxTrapped(s, tempPos + movingParam)) {
			return 0;
		}
		// Empurrou uma caixa
		if (s->grid[tempPos] == '*') {
			// A caixa estava sobre um alvo
			s->grid[tempPos] = '.';
			s->boxesOnGoals--;
		} else {
			// Não estava sobre nada
			s->grid[tempPos] = 32;
		}
		if (s->grid[tempPos + movingParam] == '.') {
			// A posição de destino da caixa é alvo
			s->grid[tempPos + movingParam] = '*';
			s->boxesOnGoals++;
		} else {
			s->grid[tempPos + movingParam] = '$';
		}
		// Move a caixa nas posições
		for (int i = 0; i < 30 && s->posBoxes[i] != 0; i++) {
			if (s->posBoxes[i] == tempPos) {
				s->posBoxes[i] += movingParam;
			}
		}
	}

	if (s->grid[s->posPlayer] == '+') {
		// Sokoban tava em cima de um alvo
		s->grid[s->posPlayer] = '.';
	} else {
		// Sokoban não estava sobre nada
		s->grid[s->posPlayer] = 32;
	}
	if (s->grid[tempPos] == '.') {
		// Sokoban está indo na direção de um alvo
		s->grid[tempPos] = '+';
	} else {
		// Não havia nada sobre a posição de destino
		s->grid[tempPos] = '@';
	}

	s->posPlayer = tempPos;

	// Verifica se o Id é único.
	if (!getStateId(s)) {
		return 0;
	}

	// Adiciona o caminho
	addPath(&c, s);

	// Retorna o efeito
	return c;
};

// Função que verifica se o estado é final
// Dado que este algoritmo foi implementado possuindo os nívels -1, 00 e 01 em
// mente, este não está preparado para níveis que possuam mais caixas que
// objetivos
unsigned char isFinal(State *s) {
	if (boxes == s->boxesOnGoals) {
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
	State *s;

	// Constroi o primeiro estado, sequencialmente
	buildMap(root, argv[1]);

	// Primeiro thread constroi a lista inicial de estados, de forma sequencial.
	s = malloc(sizeof(State));

	// Quantidade de threads solicitados
	int threads = strtol(argv[2], NULL, 10);

	// Pediremos que main faça NUM_MAIN_STATES estados para cada thread
	unsigned int numStates = NUM_MAIN_STATES * threads;

	while (mainStates < numStates) {
		for (int i = 0; i < 4; i++) {
			// Pra cada direção, nós copiamos o estado inicial
			copyState(root, s);
			if (movePlayer(s, i) != 0) {
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
				if (movePlayer(s, i) != 0) {
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