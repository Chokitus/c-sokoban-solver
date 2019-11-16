#include "../../common/sort.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <omp.h>
#include <semaphore.h>
#include <time.h>

#define copyState(s, s2) memcpy(s2, s, sizeof(State))

#define NANOS ((unsigned long)1000000000)

enum Direcoes{
	direita = 0,
	esquerda = 1,
	cima = 2,
	baixo = 3
};

//Estado que estaremos analisando
typedef struct State{
	//Todas as posições se baseiam no fato que não há mapa com mais de 19 de largura/altura. Portanto, a posição é 20*y+x;
	//Posição do Sokoban
	unsigned short posPlayer;

	//Posição das caixas
	unsigned short posBoxes[30];

	//Mapa, usado para facilitar no movimento.
	unsigned char grid[400];

	//Quantidade de caixas no objetivo
	unsigned char boxesOnGoals;

	//Caminho para a solução
	struct actionList *lastAction;

	//Próximo Estado, considerando que isto também é uma lista ligada
	struct State *nextState;
} State;

//Número de caixas no nível
unsigned char boxes;

//Trie para os ids.
typedef struct idTrie{
	struct idTrie *idLeafs[10];
} idTrie;

//Ligada ligada para as ações
typedef struct actionList{
	unsigned char action;
	struct actionList *prevAction;
} actionList;

//Ponteiro para a raiz da trie de ids.
idTrie *mainId;

//Largura e altura do mapa
unsigned int width = 0;
unsigned int height = 0;

//Ponteiro para o último estado da lista principal.
State **lastMainState;

void insertId(idTrie *tempTrie, State* s, unsigned short lastI);

//Função que imprime o caminho para a solução
void printPath(State *s){
	//Apontamos para a última ação tomada
	actionList *tempAction = s->lastAction;

	//Criamos mais que o espaço necessário para qualquer solução
	unsigned char actions[10000];
	memset(actions, 0, 10000);

	int i;
	//Enquanto não chegamos ao final (que é nulo)
	for(i = 0; tempAction; i++){
		actions[i] = tempAction->action;
		tempAction = tempAction->prevAction;
	}
	i--;

	//Printamos em ordem reversa
	for(;i>=0;i--){
		printf("%c", actions[i]);
	}
	printf("\n");
}
//Cria um novo nó na trie
idTrie *new_trie(){
	idTrie *returnTrie = malloc(sizeof(idTrie));
	memset(returnTrie->idLeafs, 0, 10*sizeof(idTrie*));
	return returnTrie;
}
int memoryInsert = 0;

//Função que procura o id na lista
unsigned char findId(State* s){
	//Apontamos para a mainTrie
	idTrie *tempTrie = mainId;

	unsigned short tempValue = 0;
	unsigned char found = 0;

	/*
		É importante somente travar cada "andar" da trie com um lock.
		Esta implementação possui, pelo menos, 3*(caixas+player) locks, o que significa que
		conseguimos travar cada andar uma vez.

	*/

	//Para cada caixa:
	for(short i = 0; i < boxes; i++){
		if(s->posBoxes[i] > 100){

			if(!tempTrie->idLeafs[s->posBoxes[i] / 100]){
				memoryInsert++;
				tempTrie->idLeafs[s->posBoxes[i] / 100] = new_trie();
				found = 1;
			}
			tempTrie = tempTrie->idLeafs[s->posBoxes[i] / 100];
		}
		tempValue = (s->posBoxes[i] / 10);

		if(!tempTrie->idLeafs[tempValue % 10]){
			memoryInsert++;
			tempTrie->idLeafs[tempValue % 10] = new_trie();
			found = 1;
		}
		tempTrie = tempTrie->idLeafs[tempValue % 10];

		if(!tempTrie->idLeafs[s->posBoxes[i] - tempValue*10]){
			memoryInsert++;
			tempTrie->idLeafs[s->posBoxes[i] - tempValue*10] = new_trie();
			found = 1;
		}
		tempTrie = tempTrie->idLeafs[s->posBoxes[i] - tempValue*10];
	}

	if(s->posPlayer > 100){
		if(!tempTrie->idLeafs[s->posPlayer / 100]){
			memoryInsert++;
			tempTrie->idLeafs[s->posPlayer / 100] = new_trie();
			found = 1;
		}
		tempTrie = tempTrie->idLeafs[s->posPlayer / 100];
	}
	tempValue = (s->posPlayer / 10);

	if(!tempTrie->idLeafs[tempValue % 10]){
		memoryInsert++;
		tempTrie->idLeafs[tempValue % 10] = new_trie();
		found = 1;
	}
	tempTrie = tempTrie->idLeafs[tempValue % 10];

	if(!tempTrie->idLeafs[s->posPlayer - tempValue*10]){
		memoryInsert++;
		tempTrie->idLeafs[s->posPlayer - tempValue*10] = new_trie();
		found = 1;
	}

	return found;
}

//---------------------------------------------------------------------------------------------

//Função printar o grid: DEBUG
void printGrid(State *s){
	for(int h = 0; h < (height+1)*20; h+=20){
		for(int w = 0; w < width; w++){
			if(s->grid[w+h] == 32){
				printf("  ");
			}
			else{
				printf("%c ", s->grid[w+h]);
			}
		}
		printf("\n");
	}
	printf("\n");
}

//Função que retorna se o estado é único ou não
unsigned char getStateId(State *s){
	//Fazemos um sort pois a ordem das caixas não pode importar
	quickSort(s->posBoxes, 0, boxes - 1);

	/*
		Procuramos o ID na trie. Se estiver, retornamos verdadeiro, se não estiver
		o colocamos nela.
	*/

	unsigned char newId;
	newId = findId(s);

	return newId;
}

//Função para adicionar a ação ao caminho
void addPath(char *c, State *s){
	//Criamos uma nova ação
	actionList *nextAction = malloc(sizeof(actionList));

	//Esta ação recebe a ação causada pelo movePlayer
	nextAction->action = *c;

	//E aponta para a ação anterior
	nextAction->prevAction = s->lastAction;

	//O estado agora aponta para a sua ação geradora.
	s->lastAction = nextAction;
};

//Função para construir o grid
void placeThis(char c, int x, int y, struct State *s){
	if(c == 32 || (c != '@' && c != '$' && c != '.' && c != '*' && c != '+' && c != '#')){
		// Espaço vazio
		return;
	}


	//Como o mapa é considerado como um quadrado de 19 por 19, cada posição pode ser linearizada
	//como x+20*y. Assim, a posição (17,4) = 17 + 80 = 97
	int pos = x + 20*y;

	//Colocamos no grid o objeto
	s->grid[pos] = c;

	if(c == '@'){
		//É o player.
		s->posPlayer = pos;
		return;
	}
	if(c == '$'){
		//É uma caixa.
		s->posBoxes[boxes++] = pos;
		return;
	}

	if(c == '.'){
		//É um alvo.
		return;
	}

	if(c == '*'){
		//É um alvo e uma caixa.
		s->posBoxes[boxes++] = pos;
		s->boxesOnGoals++;
		return;
	}

	if(c == '+'){
		//É o player e um alvo.
		s->posPlayer = pos;
	}
}

//Função para a construção do mapa
void buildMap(struct State *s, char *level){
	//Número de caixas, global
	boxes = 0;

	//Inicializamos o número de caixas em objetivos
	s->boxesOnGoals = 0;

	//Inicializamos a posição das caixas
	memset(s->posBoxes, 0, 30);

	//Inicializamos o grid com 32, ou ' '
	memset(s->grid, 32, 400);

	//Ponteiro para o último estado principal é inicializado.
	lastMainState = malloc(sizeof(State*));
	*lastMainState = NULL;

	//Ponteiro para a raiz da trie de Ids
	mainId = malloc(sizeof(idTrie));
	memset(mainId->idLeafs, 0, 10*sizeof(idTrie*));

	//Inicializamos a ação com nulo, pois é como saberemos que voltamos até o começo do processo
	s->lastAction = malloc(sizeof(actionList));
	s->lastAction->prevAction = NULL;
	s->lastAction->action = 0;

	//Abrimos o nível
	char str[16] = "levels/";

	strcat(str, level);

	FILE *file = fopen(str,"r");

	char line[20];

	int x, maxWidth;
	maxWidth = 0;
	height = -1;

	//Pegamos 20 caracteres de cada linha, até que a linha retorne nulo.
	while(fgets(line, 20, file) != NULL){
		//Se a linha for 0 ou 10 (nova linha), estamos fora do mapa
		if(line[0] != 0 && line[0] != 10){
			//Aumentamos a altura
			height++;

			//Pra cada caracter, colocamos o objeto no mapa.
			for(x = 0; (int) line[x] != 0 && (int) line[x] != 10; x++){
				placeThis(line[x], x, height, s);
			}

			//Procuramos o maior x para chamar de largura
			if(x > maxWidth){
				maxWidth = x;
			}

			x = 0;
		}
	}

	width = maxWidth;

	fclose(file);

	getStateId(s);
};

//Função simples que retorna se a posição pos é uma parede
int checkWallsAt(State *s, unsigned int pos){
	if(s->grid[pos] == '#'){
		return 1;
	}
	return 0;
};

int sign(int x){
	return (x > 0) - (x < 0);
}

//Função que verifica se prendemos a caixa
/*
	 #
	#$
	Procuramos "cantos" como este. Se a caixa está num canto, e este canto não é um objetivo, travamos a caixa
	Esta é uma posição exemplo que não seria explorada:
	#######
	#   @$# <- Note que a caixa não poderá ser movida
	#     #
	#. #  #
	#. $  #
	#.$$  #
	#.#  @#
	#######
*/
unsigned char boxTrapped(State *s, int pos){
    int wallsUp = s->grid[pos-20] == '#', wallsDown = s->grid[pos+20] == '#', wallsLeft = s->grid[pos-1] == '#', wallsRight = s->grid[pos+1] == '#';
    if(s->grid[pos] == '.' || s->grid[pos] == '*'){
        return 0;
    }
    if(wallsUp && wallsLeft){
      	return 1;
    }
    if(wallsUp && wallsRight){
        return 1;
    }
    if(wallsDown && wallsLeft){
        return 1;
    }
    if(wallsDown && wallsRight){
        return 1;
    }

    return 0;
}

//Função que usaremos intensivamente para mover o sokoban
char movePlayer(struct State *s, int dir){
	//dir == 0 -> direita
	//dir == 1 -> esquerda
	//dir == 2 -> cima
	//dir == 3 -> baixo
	char c;
	unsigned char box = 0;
	int tempPos = 0;
	int movingParam = 0;
	switch(dir){
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

	//Usamos o trecho de código para obter o parametro de movimento, assim como o caracter.
	//Como parâmetro de movimento, queremos transformar a visao 1D do array no movimento 2D do personagem, ou seja,
	//dado que cada linha possui 20 colunas, "andar para cima" recua 20 colunas, por isso o movingParam é -20.
	//A mesma lógica se aplica para os outros

	tempPos = s->posPlayer + movingParam;

	if(checkWallsAt(s, tempPos)){
		//Tem uma parede.
		return 0;
	}

	if(s->grid[tempPos] == '$' || s->grid[tempPos] == '*'){
		//Tem uma caixa na direção
		if(checkWallsAt(s, tempPos + movingParam) == 1 ||
				s->grid[tempPos + movingParam] == '$' ||
				s->grid[tempPos + movingParam] == '*'){
			//Tem uma parede ou caixa após a caixa.
			return 0;
		};
		//Deixa a letra maiuscula
		c -= 32;
		box = 1;
	}

	//Efetiva o movimento
	if(box != 0){
		if(boxTrapped(s, tempPos + movingParam)){
            return 0;
      }
		//Empurrou uma caixa
		if(s->grid[tempPos] == '*'){
			//A caixa estava sobre um alvo
			s->grid[tempPos] = '.';
			s->boxesOnGoals--;
		}
		else{
			//Não estava sobre nada
			s->grid[tempPos] = 32;
		}
		if(s->grid[tempPos + movingParam] == '.'){
			//A posição de destino da caixa é alvo
			s->grid[tempPos + movingParam] = '*';
			s->boxesOnGoals++;
		}
		else{
			s->grid[tempPos + movingParam] = '$';
		}
		//Move a caixa nas posições
		for(int i = 0; i < 30 && s->posBoxes[i] != 0; i++){
			if(s->posBoxes[i] == tempPos){
				s->posBoxes[i] += movingParam;
			}
		}
	}

	if(s->grid[s->posPlayer] == '+'){
		//Sokoban tava em cima de um alvo
		s->grid[s->posPlayer] = '.';
	}
	else{
		//Sokoban não estava sobre nada
		s->grid[s->posPlayer] = 32;
	}
	if(s->grid[tempPos] == '.'){
		//Sokoban está indo na direção de um alvo
		s->grid[tempPos] = '+';
	}
	else{
		//Não havia nada sobre a posição de destino
		s->grid[tempPos] = '@';
	}

	s->posPlayer = tempPos;

	//Verifica se o Id é único.
	if(!getStateId(s)){
		return 0;
	}

	//Adiciona o caminho
	addPath(&c, s);

	//Retorna o efeito
	return c;
};

//Função que verifica se o estado é final
//Dado que este algoritmo foi implementado possuindo os nívels -1, 00 e 01 em mente,
//este não está preparado para níveis que possuam mais caixas que objetivos
unsigned char isFinal(State *s){
	if(boxes == s->boxesOnGoals){
		return 1;
	}
	return 0;
}

//Função que usamos para inserir o estado
unsigned char insertState(State *root, State *s, State **lastThreadState){
	if(isFinal(s)){
		//É final
		return 1;
	}

	//Lista está vazia ou só possui o root.
	if(root->nextState == NULL){
		//Criamos um novo espaço após root
		root->nextState = malloc(sizeof(State));

		//Copiamos o estado
		copyState(s, root->nextState);

		//Last aponta para o último estado. Este last pode ser o da lista principal, ou do thread
		(*lastThreadState) = root->nextState;

		return 0;
	}

	//A lista possui mais de um, e podemos usar seguramente o last
	(*lastThreadState)->nextState = malloc(sizeof(State));
	copyState(s, (*lastThreadState)->nextState);

	//Mudamos a posição do último estado.
	*lastThreadState = (*lastThreadState)->nextState;
	(*lastThreadState)->nextState = NULL;
	return 0;
}

//Função que move uma das listas, enquanto cria a raiz para a outra
void popState(State **from, State **to){
	//Se ambos são o mesmo, devemos fazer uma operação de retirar um nó, somente
	if(*to == *from){
		State *freeableState = *to;
		*from = (*from)->nextState;
		free(freeableState);
		return;
	}
}

int main(int argc, char* argv[]){
	struct timespec before, after;
   time_t nSeconds;

   //Começamos a contagem de tempo.
   clock_gettime(CLOCK_REALTIME, &before);

	//Começamos um contador para a lista principal
	unsigned int mainStates = 1;

	//Criamos espaço para uma variável compartilhada que verifica se foi encontrado uma solução por algum dos threads
	unsigned char *solution = malloc(1);
	*solution = 0;

   //Criamos espaço para a raiz da lista principal
	State *root = malloc(sizeof(State));
	root->nextState = NULL;

	//Criamos um ponteiro temporário que irá ser movido
	State *s;

	//Constroi o primeiro estado, sequencialmente
	buildMap(root, argv[1]);

   //Primeiro thread constroi a lista inicial de estados, de forma sequencial.
   s = malloc(sizeof(State));
   int moved = 0;
	while(!(*solution)){
		for(int i = 0; i < 4; i++){
			//Pra cada direção, nós copiamos o estado inicial
			copyState(root, s);
			if(movePlayer(s, i) != 0){
				moved++;
				/*movePlayer retorna 0 se não foi possível mover, seja por uma caixa sendo empurrada numa parede,
				seja por estarmos andando de cara na parede*/
				mainStates++;
				if(insertState(root, s, lastMainState)){
					//Se ele entrou aqui, quer dizer que, durante a inserção, foi notado que ele é um estado final.
					printPath(s);
					*solution = 1;
				}
			}
		}
		if(moved == 0)
			free(root->lastAction);
		moved=0;
		//Movemos root, colocando root como próximo estado
		popState(&root, &root);
		mainStates--;
	}

	//Finalizamos a contagem de tempo.
	clock_gettime(CLOCK_REALTIME, &after);

	//Calcula o tempo passado em microssegundos.
	nSeconds = after.tv_nsec - before.tv_nsec + (after.tv_sec - before.tv_sec) * NANOS;

	printf("Tempo total: %lu ns - %lf s\n",  nSeconds, (double)nSeconds/NANOS);

	return 0;
}