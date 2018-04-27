#include "mpi.h" 
#include "error.h"
#include "worker_settings.h"

typedef struct {
    MPI_Comm serverComm;
    MPI_Comm clientComm;
    char port[MPI_MAX_PORT_NAME];
    int serverSize;
    int serverRank;
    int numOfWorkers;
    int clientRank;
    int clientTag;
} ManagerConstants;

typedef struct {
    int myRank;
    int prevRank;
    int nextRank;
    int managerRank;
} RankConstants;

void GetManagerConstants(ManagerConstants* mpi) {
    int err;
    err = MPI_Comm_get_parent(&mpi->serverComm); 
    CrashIfError(err, "MPI_Comm_get_parent");
    Assert(mpi->serverComm != MPI_COMM_NULL, "No server");

    err = MPI_Comm_remote_size(mpi->serverComm, &mpi->serverSize); 
    CrashIfError(err, "MPI_Comm_remote_size");
    Assert(mpi->serverSize == 1, "Too many servers");

    mpi->serverRank = 0;

    err = MPI_Comm_size(MPI_COMM_WORLD, &mpi->numOfWorkers);
    CrashIfError(err,  "MPI_Comm_size");
    --(mpi->numOfWorkers);

    err = MPI_Open_port(MPI_INFO_NULL, mpi->port); 
    CrashIfError(err, "MPI_Open_port");

    err = MPI_Send((void *) mpi->port, MPI_MAX_PORT_NAME, MPI_CHAR, 0, 1, mpi->serverComm);
    CrashIfError(err, "MPI_Send");

    err = MPI_Comm_accept(mpi->port, MPI_INFO_NULL, 0, MPI_COMM_SELF, &(mpi->clientComm)); 
    CrashIfError(err, "MPI_Comm_accept");
    printf("Manager accepted connection\n");
}

void fillClientInfo(ManagerConstants *mpi, const MPI_Status *status) {
    mpi->clientRank = status->MPI_SOURCE;
    mpi->clientTag  = status->MPI_TAG;
}

void GetRankConstants(RankConstants* mpi) {
    int err, numOfWorkers;
    err =  MPI_Comm_rank(MPI_COMM_WORLD, &mpi->myRank);
    CrashIfError(err,  "MPI_Comm_rank");

    err = MPI_Comm_size(MPI_COMM_WORLD, &numOfWorkers);
    CrashIfError(err,  "MPI_Comm_size");

    mpi->prevRank = mpi->myRank == 1 ?  MPI_PROC_NULL : mpi->myRank - 1;
    mpi->nextRank = mpi->myRank == numOfWorkers - 1 ? MPI_PROC_NULL : mpi->myRank + 1;

    mpi->managerRank = 0;
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

void ExchangeBorders(const RankConstants *mpi, const WorkerSettings *settings, int **polygon) {
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

void WorkerRoutine(RankConstants *rankConst) {
    int i, err;
    WorkerSettings settings;

    int message;
    int continueFlag = 1;

    MPI_Status mpiStatus;
    
    err = MPI_Recv((void *) &settings, 2, MPI_INT, rankConst->managerRank, 1, MPI_COMM_WORLD, &mpiStatus);
    CrashIfError(err, "MPI_Recv");
 
    int **polygon = (int **) malloc((settings.height) * sizeof(int *));
    int **newPolygon = (int **) malloc((settings.height) * sizeof(int *));

    for (i = 0; i < settings.height; ++i) {
        polygon[i] = malloc(settings.width * sizeof(int));
        newPolygon[i] = malloc(settings.width * sizeof(int));

        err = MPI_Recv((void *) polygon[i], settings.width, MPI_INT, rankConst->managerRank, 1, MPI_COMM_WORLD, &mpiStatus);
        CrashIfError(err, "MPI_Recv");
    }

    PrintPolygon((const int **) polygon, settings.height, settings.width);

    while(continueFlag) {
        CalculateNewPolygon(&settings, (const int **) polygon, newPolygon);
        Swap(&polygon, &newPolygon);
        ExchangeBorders(rankConst, &settings, polygon);

        printf("proc %d\n", rankConst->myRank);
        PrintPolygon((const int **) polygon, settings.height, settings.width);

        err = MPI_Bcast((void*) &message, 1, MPI_INT, 0, MPI_COMM_WORLD);
        CrashIfError(err, "MPI_Bcast");

        switch(message) {
            case STOP:
                continueFlag = 0;
                break;
            case SNAPSHOT:
                for (i = 1; i < settings.height - 1; ++i) {
                    err = MPI_Send((void *) polygon[i], settings.width, MPI_INT, rankConst->managerRank, 1, MPI_COMM_WORLD);
                    CrashIfError(err, "MPI_Send");
                }
            case CONTINUE:
                break;
            default:
                CrashIfError(1, "Unrecognised message");
        }
    }

    for (i = 0; i < settings.height; ++i) {
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

    WorkerSettings settings;
    err = MPI_Recv((void *) &settings, 2, MPI_INT, mpi.serverRank, 1, mpi.clientComm, &mpiStatus);
    CrashIfError(err, "MPI_Recv");
    fillClientInfo(&mpi, &mpiStatus);

    int **polygon = (int **) malloc((settings.height + 2) * sizeof(int *));
    polygon[0] = calloc(settings.width, sizeof(int));
    for (i = 1 ; i < settings.height + 1; ++i) {
        polygon[i] = malloc(settings.width * sizeof(int));

        err = MPI_Recv((void *) polygon[i], settings.width, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, mpi.clientComm, &mpiStatus);
        CrashIfError(err, "MPI_Recv");
    }
    polygon[settings.height + 1] = calloc(settings.width, sizeof(int));

    WorkerSettings workerSettings = {0, settings.width};
    int from, j;
    for (i = 0; i < mpi.numOfWorkers; ++i) {
        if (i < settings.height % mpi.numOfWorkers) {
            workerSettings.height = settings.height / mpi.numOfWorkers + 1;
            from = workerSettings.height * i;
        } else {
            workerSettings.height = settings.height / mpi.numOfWorkers;
            from = workerSettings.height * i + settings.height % mpi.numOfWorkers;
        }
        workerSettings.height += 2;

        err = MPI_Send((void *) &workerSettings, 2, MPI_INT, i + 1, 1, MPI_COMM_WORLD);
        CrashIfError(err, "MPI_Send");

        for (j = 0; j < workerSettings.height; ++j) {
            err = MPI_Send((void *) polygon[from + j], settings.width, MPI_INT, i + 1, 1, MPI_COMM_WORLD);
            CrashIfError(err, "MPI_Send");
        }
    }

    err = MPI_Irecv((void*) &serverMsg, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, mpi.clientComm, &mpiRequest);
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
                        if (i < settings.height % mpi.numOfWorkers) {
                            workerSettings.height = settings.height / mpi.numOfWorkers + 1;
                            from = workerSettings.height * i;
                        } else {
                            workerSettings.height = settings.height / mpi.numOfWorkers;
                            from = workerSettings.height * i + settings.height % mpi.numOfWorkers;
                        }
                        for (j = 0; j < workerSettings.height; ++j) {
                            err = MPI_Recv((void *) polygon[from + j], settings.width, MPI_INT, i + 1, 1, MPI_COMM_WORLD,  &mpiStatus);
                            CrashIfError(err, "MPI_Recv");
                        }
                    }
                    PrintPolygon((const int **)polygon, settings.height, settings.width);

                    for (i = 1; i < settings.height + 1; ++i) {
                        err = MPI_Send((void *) polygon[i], settings.width, MPI_INT, mpi.clientRank, mpi.clientTag, mpi.clientComm);
                        CrashIfError(err, "MPI_Send");
                    }
                    break;
                default:
                    workerMsg = STOP;
                    err = MPI_Bcast((void*) &workerMsg, 1, MPI_INT, 0, MPI_COMM_WORLD);
                    CrashIfError(1, "Unrecognised command");
            }
            err = MPI_Irecv((void*) &serverMsg, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, mpi.clientComm, &mpiRequest);
            CrashIfError(err, "MPI_Irecv");
        }
    }


    for (i = 0; i < settings.height + 2; ++i) {
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