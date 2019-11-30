#include "sort.h"

// Funções para o quicksort. Foram pegas diretamente de
// https://www.geeksforgeeks.org/quick-sort/
void swap(unsigned short *a, unsigned short *b) {
	short t = *a;
	*a = *b;
	*b = t;
}

int partition(unsigned short arr[], int low, int high) {
	int pivot = arr[high];
	int i = (low - 1);

	for (int j = low; j <= high - 1; j++) {
		if (arr[j] <= pivot) {
			i++;
			swap(&arr[i], &arr[j]);
		}
	}
	swap(&arr[i + 1], &arr[high]);
	return (i + 1);
}

void quickSort(unsigned short arr[], int low, int high) {
	if (low < high) {
		int pi = partition(arr, low, high);

		quickSort(arr, low, pi - 1);
		quickSort(arr, pi + 1, high);
	}
}