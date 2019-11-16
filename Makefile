all: SokobanMP_Andar.exe SokobanMP_Digito.exe Sokoban_HashTableList.exe Sokoban_Trie.exe Sokoban_Heuristic.exe

SokobanMP_Andar.exe: brute-force/multi-thread/SokobanMP_Andar.c
	gcc -O2 -o SokobanMP_Andar.exe -fopenmp brute-force/multi-thread/SokobanMP_Andar.c
SokobanMP_Digito.exe: brute-force/multi-thread/SokobanMP_Digito.c
	gcc -O2 -o SokobanMP_Digito.exe -fopenmp brute-force/multi-thread/SokobanMP_Digito.c
Sokoban_HashTableList.exe: brute-force/single-thread/Sokoban_HashTableList.c
	gcc -O2 -o Sokoban_HashTableList.exe brute-force/single-thread/Sokoban_HashTableList.c
Sokoban_Trie.exe: brute-force/single-thread/Sokoban_Trie.c
	gcc -O2 -o Sokoban_Trie.exe brute-force/single-thread/Sokoban_Trie.c
Sokoban_Heuristic.exe: heuristic/sokoban_heuristic.c
	gcc -O2 -o Sokoban_Heuristic.exe heuristic/sokoban_heuristic.c

clean:
	rm *.exe