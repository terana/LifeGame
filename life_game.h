#ifndef LIFE_GAME
#define LIFE_GAME

#include "mpi.h"

typedef struct {
    int height;
    int width;
} Size;

enum Order {
	STOP,
	CONTINUE,
	SNAPSHOT
};

typedef struct {
    MPI_Comm comm;
    int tag;
    int rank;
} CommunicationConstants;

void FillCommunicationConstants(CommunicationConstants *mpi, const MPI_Status *status) {
    mpi->rank = status->MPI_SOURCE;
    mpi->tag  = status->MPI_TAG;
}

void PrintPolygon(const int** polygon, int high, int width) {
    int i, j;
    char * str = malloc(high * (width + 1) + 1);
    char *cursor = str;
    for (i = 0; i < high; ++i) {
        for (j = 0; j < width; ++j) {
            polygon[i][j] ? sprintf(cursor++, "*") : sprintf(cursor++, "o");
        }
        sprintf(cursor++, "\n");
    }
    printf("%s\n", str);
    free(str);
}

#endif