#include "mpi.h" 
#include <string.h>

#include "error.h"
#include "life_game.h"

typedef struct {
    CommunicationConstants server;
    CommunicationConstants client;
    char port[MPI_MAX_PORT_NAME];
    int numOfWorkers;
} ManagerConstants;

typedef struct {
    int myRank;
    int prevRank;
    int nextRank;
    int managerRank;
} RankConstants;

void GetManagerConstants(ManagerConstants* mpi) {
    memset(mpi, 0, sizeof(ManagerConstants));

    int err, serverSize;
    err = MPI_Comm_get_parent(&(mpi->server.comm)); 
    CrashIfError(err, "MPI_Comm_get_parent");
    Assert(mpi->server.comm != MPI_COMM_NULL, "No server");

    err = MPI_Comm_remote_size(mpi->server.comm, &serverSize); 
    CrashIfError(err, "MPI_Comm_remote_size");
    Assert(serverSize == 1, "Too many servers");

    mpi->server.rank = 0;

    err = MPI_Comm_size(MPI_COMM_WORLD, &mpi->numOfWorkers);
    CrashIfError(err,  "MPI_Comm_size");
    --(mpi->numOfWorkers);

    err = MPI_Open_port(MPI_INFO_NULL, mpi->port); 
    CrashIfError(err, "MPI_Open_port");

    err = MPI_Send((void *) mpi->port, MPI_MAX_PORT_NAME, MPI_CHAR, mpi->server.rank, mpi->server.tag, mpi->server.comm);
    CrashIfError(err, "MPI_Send");

    err = MPI_Comm_accept(mpi->port, MPI_INFO_NULL, 0, MPI_COMM_SELF, &(mpi->client.comm)); 
    CrashIfError(err, "MPI_Comm_accept");
    printf("Manager accepted connection\n");
}

void GetRankConstants(RankConstants* mpi) {
    memset(mpi, 0, sizeof(RankConstants));

    int err, numOfWorkers;
    err =  MPI_Comm_rank(MPI_COMM_WORLD, &mpi->myRank);
    CrashIfError(err,  "MPI_Comm_rank");

    err = MPI_Comm_size(MPI_COMM_WORLD, &numOfWorkers);
    CrashIfError(err,  "MPI_Comm_size");

    mpi->prevRank = mpi->myRank == 1 ?  MPI_PROC_NULL : mpi->myRank - 1;
    mpi->nextRank = mpi->myRank == numOfWorkers - 1 ? MPI_PROC_NULL : mpi->myRank + 1;

    mpi->managerRank = 0;
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

void CalculateNewPolygon(const Size *size, const int** polygon, int** newPolygon) {
    int i, j;
    for (j = 0; j < size->width; ++j) {
        newPolygon[0][j] = polygon[0][j];
        newPolygon[size->height - 1][j] = polygon[size->height - 1][j];
    }
    for (i = 1; i < size->height - 1; ++i) {
        for (j = 0; j < size->width; ++j) {
            newPolygon[i][j] = NewState(polygon[i][j], CalculatePotential(i, j, polygon, size->width));
        }
    }
}

void Swap(int*** polygon1, int*** polygon2) {
    int **tmp = *polygon1;
    *polygon1 = *polygon2;
    *polygon2 = tmp;
}

void ExchangeBorders(const RankConstants *mpi, const Size *size, int **polygon) {
    int status;
    MPI_Status mpiStatus;
    if (mpi->myRank % 2) {
        status =  MPI_Sendrecv((void *) polygon[1], size->width, MPI_INT, mpi->prevRank, 2, 
                            (void *) polygon[0], size->width, MPI_INT, mpi->prevRank, 2, MPI_COMM_WORLD, &mpiStatus);
        CrashIfError(status, "MPI_Sendrecv");

        status =  MPI_Sendrecv((void *) (polygon[size->height - 2]), size->width, MPI_INT, mpi->nextRank, 1, 
                            (void *) (polygon[size->height - 1]), size->width, MPI_INT, mpi->nextRank, 1,
                            MPI_COMM_WORLD, &mpiStatus);
        CrashIfError(status, "MPI_Sendrecv");
    } else {
        status =  MPI_Sendrecv((void *) (polygon[size->height - 2]), size->width, MPI_INT, mpi->nextRank, 2, 
                            (void *) (polygon[size->height - 1]), size->width, MPI_INT, mpi->nextRank, 2,
                            MPI_COMM_WORLD, &mpiStatus);
        CrashIfError(status, "MPI_Sendrecv");

        status =  MPI_Sendrecv((void *) polygon[1], size->width, MPI_INT, mpi->prevRank, 1, 
                            (void *) polygon[0], size->width, MPI_INT, mpi->prevRank, 1,
                            MPI_COMM_WORLD, &mpiStatus);
        CrashIfError(status, "MPI_Sendrecv");
    }
}

void WorkerRoutine(RankConstants *rankConst) {
    int i, err;
    Size size;

    int message;
    int continueFlag = 1;

    MPI_Status mpiStatus;
    
    err = MPI_Recv((void *) &size, 2, MPI_INT, rankConst->managerRank, 1, MPI_COMM_WORLD, &mpiStatus);
    CrashIfError(err, "MPI_Recv");
 
    int **polygon = (int **) malloc((size.height) * sizeof(int *));
    int **newPolygon = (int **) malloc((size.height) * sizeof(int *));

    for (i = 0; i < size.height; ++i) {
        polygon[i] = malloc(size.width * sizeof(int));
        newPolygon[i] = malloc(size.width * sizeof(int));

        err = MPI_Recv((void *) polygon[i], size.width, MPI_INT, rankConst->managerRank, 1, MPI_COMM_WORLD, &mpiStatus);
        CrashIfError(err, "MPI_Recv");
    }

    PrintPolygon((const int **) polygon, size.height, size.width);

    while(continueFlag) {
        CalculateNewPolygon(&size, (const int **) polygon, newPolygon);
        Swap(&polygon, &newPolygon);
        ExchangeBorders(rankConst, &size, polygon);

        printf("proc %d\n", rankConst->myRank);
        PrintPolygon((const int **) polygon, size.height, size.width);

        err = MPI_Bcast((void*) &message, 1, MPI_INT, 0, MPI_COMM_WORLD);
        CrashIfError(err, "MPI_Bcast");

        switch(message) {
            case STOP:
                continueFlag = 0;
                break;
            case SNAPSHOT:
                for (i = 1; i < size.height - 1; ++i) {
                    err = MPI_Send((void *) polygon[i], size.width, MPI_INT, rankConst->managerRank, 1, MPI_COMM_WORLD);
                    CrashIfError(err, "MPI_Send");
                }
            case CONTINUE:
                break;
            default:
                CrashIfError(1, "Unrecognised message");
        }
    }

    for (i = 0; i < size.height; ++i) {
        free(polygon[i]);
        free(newPolygon[i]);
    }
    free(polygon);
    free(newPolygon);
}

void ManagerRoutine() {
    int i, err;

    int workerMsg, serverMsg;
    int continueFlag = 1;

    MPI_Status mpiStatus;
    MPI_Request mpiRequest;
    int receiveFlag;
    ManagerConstants mpi;

    GetManagerConstants(&mpi);

    Size size;
    err = MPI_Recv((void *) &size, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, mpi.client.comm, &mpiStatus);
    CrashIfError(err, "MPI_Recv");
    FillCommunicationConstants(&(mpi.client), &mpiStatus);

    int **polygon = (int **) malloc((size.height + 2) * sizeof(int *));
    polygon[0] = calloc(size.width, sizeof(int));
    for (i = 1 ; i < size.height + 1; ++i) {
        polygon[i] = malloc(size.width * sizeof(int));

        err = MPI_Recv((void *) polygon[i], size.width, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, mpi.client.comm, &mpiStatus);
        CrashIfError(err, "MPI_Recv");
    }
    polygon[size.height + 1] = calloc(size.width, sizeof(int));

    Size workerSize = {0, size.width};
    int from, j;
    for (i = 0; i < mpi.numOfWorkers; ++i) {
        if (i < size.height % mpi.numOfWorkers) {
            workerSize.height = size.height / mpi.numOfWorkers + 1;
            from = workerSize.height * i;
        } else {
            workerSize.height = size.height / mpi.numOfWorkers;
            from = workerSize.height * i + size.height % mpi.numOfWorkers;
        }
        workerSize.height += 2;

        err = MPI_Send((void *) &workerSize, 2, MPI_INT, i + 1, 1, MPI_COMM_WORLD);
        CrashIfError(err, "MPI_Send");

        for (j = 0; j < workerSize.height; ++j) {
            err = MPI_Send((void *) polygon[from + j], size.width, MPI_INT, i + 1, 1, MPI_COMM_WORLD);
            CrashIfError(err, "MPI_Send");
        }
    }

    err = MPI_Irecv((void*) &serverMsg, 1, MPI_INT, mpi.client.rank, mpi.client.tag, mpi.client.comm, &mpiRequest);
    CrashIfError(err, "MPI_Irecv");

    while (continueFlag) {
        workerMsg = CONTINUE;
        err = MPI_Bcast((void*) &workerMsg, 1, MPI_INT, 0, MPI_COMM_WORLD);
        CrashIfError(err, "MPI_Bcast CONTINUE");

        err = MPI_Test(&mpiRequest, &receiveFlag, &mpiStatus);
        CrashIfError(err, "MPI_Iprobe");   
        if (receiveFlag) {
            switch(serverMsg) {
                case STOP:
                    workerMsg = STOP;
                    err = MPI_Bcast((void*) &workerMsg, 1, MPI_INT, 0, MPI_COMM_WORLD);
                    CrashIfError(err, "MPI_Bcast STOP");
                    continueFlag = 0;
                    break;
                case SNAPSHOT:
                    workerMsg = SNAPSHOT;
                    err = MPI_Bcast((void*) &workerMsg, 1, MPI_INT, 0, MPI_COMM_WORLD);
                    CrashIfError(err, "MPI_Bcast SNAPSHOT");
                    for (i = 0; i < mpi.numOfWorkers; ++i) {
                        if (i < size.height % mpi.numOfWorkers) {
                            workerSize.height = size.height / mpi.numOfWorkers + 1;
                            from = workerSize.height * i;
                        } else {
                            workerSize.height = size.height / mpi.numOfWorkers;
                            from = workerSize.height * i + size.height % mpi.numOfWorkers;
                        }
                        for (j = 0; j < workerSize.height; ++j) {
                            err = MPI_Recv((void *) polygon[from + j], size.width, MPI_INT, i + 1, 1, MPI_COMM_WORLD,  &mpiStatus);
                            CrashIfError(err, "MPI_Recv");
                        }
                    }
                    PrintPolygon((const int **)polygon, size.height, size.width);

                    for (i = 1; i < size.height + 1; ++i) {
                        err = MPI_Send((void *) polygon[i], size.width, MPI_INT, mpi.client.rank, mpi.client.tag, mpi.client.comm);
                        CrashIfError(err, "MPI_Send");
                    }
                    break;
                default:
                    workerMsg = STOP;
                    err = MPI_Bcast((void*) &workerMsg, 1, MPI_INT, 0, MPI_COMM_WORLD);
                    CrashIfError(1, "Unrecognised command");
            }
            err = MPI_Irecv((void*) &serverMsg, 1, MPI_INT, mpi.client.rank, mpi.client.tag, mpi.client.comm, &mpiRequest);
            CrashIfError(err, "MPI_Irecv");
        }
    }


    for (i = 0; i < size.height + 2; ++i) {
        free(polygon[i]);
    }
    free(polygon);
}


int main(int argc, char *argv[]) { 
    RankConstants rankConst;
    int err;

    err = MPI_Init(&argc, &argv); 
    CrashIfError(err, "MPI_Init");
    
    GetRankConstants(&rankConst);

    if (rankConst.myRank == rankConst.managerRank) {
        ManagerRoutine();
    } else {
        WorkerRoutine(&rankConst);
    }
    
    MPI_Finalize(); 

    return 0; 
} 