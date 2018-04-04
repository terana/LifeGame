#include "mpi.h" 
#include "error.h"
#include "worker.h"


void PrintPolygon(int** polygon, int high, int width) {
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

int CalculatePotential(int i, int j, int** polygon) { 
    int p = 0; 
    p += polygon[i - 1][j];
    p += polygon[i + 1][j];
    
    if (j - 1 > 0) {
        p += polygon[i][j - 1];
    }
    if (j + 1 < WIDTH) {
        p += polygon[i][j + 1];
    }

    return p; 
} 

int main(int argc, char *argv[]) { 
    int size, i, err; 
    MPI_Comm managerComm; 
    MPI_Status mpiStatus;

    err = MPI_Init(&argc, &argv); 
    CrashIfError(err, "MPI_Init");

    MPI_Comm_get_parent(&managerComm); 
    Assert(managerComm != MPI_COMM_NULL, "No manager");
    
    MPI_Comm_remote_size(managerComm, &size); 
    Assert(size == 1, "Too many managers");

    WorkerSettings settings;

    err = MPI_Recv((void *) &settings, 2, MPI_INT, 0, 1, managerComm, &mpiStatus);
    CrashIfError(err, "MPI_Recv");
 
    int **polygon = (int **) malloc((settings.height) * sizeof(int *));
    int **newPolygon = (int **) malloc((settings.height) * sizeof(int *));

    for (i = 0; i < settings.height; ++i) {
        polygon[i] = malloc(settings.width * sizeof(int));
        newPolygon[i] = malloc(settings.width * sizeof(int));

        err = MPI_Recv((void *) polygon[i], settings.width, MPI_INT, 0, 1, managerComm, &mpiStatus);
        CrashIfError(err, "MPI_Recv");
    }

    PrintPolygon(polygon, settings.height, settings.width);

    printf("got the point");
 
    MPI_Finalize(); 
    return 0; 
} 