#ifndef _SORT_H_
#define _SORT_H_

extern void swap(unsigned short *a, unsigned short *b);
extern int partition(unsigned short arr[], int low, int high);
extern void quickSort(unsigned short arr[], int low, int high);

#endif