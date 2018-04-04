#include "mpi.h" 
#include "error.h"
#include "worker_settings.h"

typedef struct {
    MPI_Comm managerComm;
    int managerSize;
    int managerRank;
    int myRank;
    int prevRank;
    int nextRank;
    int numOfWorkers;
} MPIConstants;


void GetMPIConstants(MPIConstants* mpi) {
    int err;
    err = MPI_Comm_get_parent(&mpi->managerComm); 
    CrashIfError(err, "MPI_Comm_get_parent");
    Assert(mpi->managerComm != MPI_COMM_NULL, "No manager");

    err = MPI_Comm_remote_size(mpi->managerComm, &mpi->managerSize); 
    CrashIfError(err, "MPI_Comm_remote_size");
    Assert(mpi->managerSize == 1, "Too many managers");

    mpi->managerRank = 0;

    err = MPI_Comm_size(MPI_COMM_WORLD, &mpi->numOfWorkers);
    CrashIfError(err,  "MPI_Comm_size");

    err =  MPI_Comm_rank(MPI_COMM_WORLD, &mpi->myRank);
    CrashIfError(err,  "MPI_Comm_rank");

    mpi->prevRank = mpi->myRank == 0 ?  MPI_PROC_NULL : mpi->myRank - 1;
    mpi->nextRank = mpi->myRank == mpi->numOfWorkers - 1 ? MPI_PROC_NULL : mpi->myRank + 1;
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

int NewState(int currState, int potential) {
    if(potential == 3) 
        return 1; 
    if (potential == 2) 
        return currState;  
   return 0;
}

int CalculatePotential(int i, int j, const int** polygon, int width) { 
    int p = 0; 
    p += polygon[i - 1][j];
    p += polygon[i + 1][j];
    
    if (j - 1 > 0) {
        p += polygon[i][j - 1];
    }
    if (j + 1 < width) {
        p += polygon[i][j + 1];
    }

    return p; 
} 

void CalculateNewPolygon(const WorkerSettings *settings, const int** polygon, int** newPolygon) {
    int i, j;
    for (j = 0; j < settings->width; ++j) {
        newPolygon[0][j] = polygon[0][j];
        newPolygon[settings->height - 1][j] = polygon[settings->height - 1][j];
    }
    for (i = 1; i < settings->height - 1; ++i) {
        for (j = 0; j < settings->width; ++j) {
            newPolygon[i][j] = NewState(polygon[i][j], CalculatePotential(i, j, polygon, settings->width));
        }
    }
}

void Swap(int*** polygon1, int*** polygon2) {
    int **tmp = *polygon1;
    *polygon1 = *polygon2;
    *polygon2 = tmp;
}

void ExchangeBorders(const MPIConstants *mpi, const WorkerSettings *settings, int **polygon) {
    int status;
    MPI_Status mpiStatus;
    if (mpi->myRank % 2) {
        status =  MPI_Sendrecv((void *) polygon[1], settings->width, MPI_INT, mpi->prevRank, 2, 
                            (void *) polygon[0], settings->width, MPI_INT, mpi->prevRank, 2, MPI_COMM_WORLD, &mpiStatus);
        CrashIfError(status, "MPI_Sendrecv");

        status =  MPI_Sendrecv((void *) (polygon[settings->height - 2]), settings->width, MPI_INT, mpi->nextRank, 1, 
                            (void *) (polygon[settings->height - 1]), settings->width, MPI_INT, mpi->nextRank, 1,
                            MPI_COMM_WORLD, &mpiStatus);
        CrashIfError(status, "MPI_Sendrecv");
    } else {
        status =  MPI_Sendrecv((void *) (polygon[settings->height - 2]), settings->width, MPI_INT, mpi->nextRank, 2, 
                            (void *) (polygon[settings->height - 1]), settings->width, MPI_INT, mpi->nextRank, 2,
                            MPI_COMM_WORLD, &mpiStatus);
        CrashIfError(status, "MPI_Sendrecv");

        status =  MPI_Sendrecv((void *) polygon[1], settings->width, MPI_INT, mpi->prevRank, 1, 
                            (void *) polygon[0], settings->width, MPI_INT, mpi->prevRank, 1,
                            MPI_COMM_WORLD, &mpiStatus);
        CrashIfError(status, "MPI_Sendrecv");
    }
}

int main(int argc, char *argv[]) { 
    int i, err;
    WorkerSettings settings;

    MPI_Status mpiStatus;
    MPIConstants mpi;


    err = MPI_Init(&argc, &argv); 
    CrashIfError(err, "MPI_Init");
    
    GetMPIConstants(&mpi);

    
    err = MPI_Recv((void *) &settings, 2, MPI_INT, mpi.managerRank, 1, mpi.managerComm, &mpiStatus);
    CrashIfError(err, "MPI_Recv");
 
    int **polygon = (int **) malloc((settings.height) * sizeof(int *));
    int **newPolygon = (int **) malloc((settings.height) * sizeof(int *));

    for (i = 0; i < settings.height; ++i) {
        polygon[i] = malloc(settings.width * sizeof(int));
        newPolygon[i] = malloc(settings.width * sizeof(int));

        err = MPI_Recv((void *) polygon[i], settings.width, MPI_INT, 0, 1, mpi.managerComm, &mpiStatus);
        CrashIfError(err, "MPI_Recv");
    }

    PrintPolygon((const int **) polygon, settings.height, settings.width);

    CalculateNewPolygon(&settings, (const int **) polygon, newPolygon);
    Swap(&polygon, &newPolygon);
    ExchangeBorders(&mpi, &settings, polygon);

    PrintPolygon((const int **) polygon, settings.height, settings.width);


    MPI_Finalize(); 

    for (i = 0; i < settings.height; ++i) {
        free(polygon[i]);
        free(newPolygon[i]);
    }
    free(polygon);
    free(newPolygon);
    return 0; 
} 