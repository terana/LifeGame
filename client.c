#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "mpi.h" 
#include "error.h"
#include "worker_settings.h"

typedef struct {
    char serverPort[MPI_MAX_PORT_NAME];
    char managerPort[MPI_MAX_PORT_NAME];
    MPI_Comm serverComm;
    MPI_Comm managerComm;
} MPIConstants;

typedef struct {
    int numOfWorkers;
    int height;
    int width;
} GameSettings;

void Send(int message, MPIConstants* mpi) {
    int err = MPI_Send((void *) &message, 1, MPI_INT, 0, 1, mpi->managerComm);
    CrashIfError(err, "MPI_Send");
}

void InitSettings(GameSettings *game) {
    game->numOfWorkers = 5;
    game->height = 4;
    game->width = 4;
}

int main(int argc, char *argv[]) 
{ 
    int polygon[4][4] = {
        {1, 1, 1, 0},
        {1, 1, 0, 0},
        {0, 1, 0, 1},
        {1, 0, 1, 0}
    };

    int i, err;

    MPIConstants mpi;
    GameSettings game;
    InitSettings(&game);

    MPI_Status mpiStatus;

    err = MPI_Init(&argc, &argv);
    CrashIfError(err, "MPI_Init");

    printf("Enter port name: \n");  
    gets(mpi.serverPort);

    err = MPI_Comm_connect(mpi.serverPort, MPI_INFO_NULL, 0, MPI_COMM_SELF, &mpi.serverComm); 
    CrashIfError(err, "MPI_Comm_connect");
    printf("Connected to server\n");

    err = MPI_Send(&game.numOfWorkers, 1, MPI_INT, 0, 1, mpi.serverComm);
    CrashIfError(err, "MPI_Send");

    printf("numOfWorkers\n"); 

    err = MPI_Recv(mpi.managerPort, MPI_MAX_PORT_NAME, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, mpi.serverComm, &mpiStatus);
    CrashIfError(err, "MPI_Recv");

    err = MPI_Comm_connect(mpi.managerPort, MPI_INFO_NULL, 0, MPI_COMM_SELF, &mpi.managerComm); 
    CrashIfError(err, "MPI_Comm_connect");
    printf("Connected to manager\n");

    WorkerSettings workerSettings = {game.height, game.width};
    err = MPI_Send((void *) &workerSettings, 2, MPI_INT, 0, 1, mpi.managerComm);
    CrashIfError(err, "MPI_Send");

    printf("settings\n"); 

    for (i = 0; i < game.height; ++i) {
        err = MPI_Send((void *) polygon[i], game.width, MPI_INT, 0, 1, mpi.managerComm);
        CrashIfError(err, "MPI_Send");
    }

    printf("polygon\n");

    int message;
    Send(SNAPSHOT, &mpi);
    for (i = 0 ; i < game.height; ++i) {
        err = MPI_Recv((void *) polygon[i], game.width, MPI_INT, 0, 1, mpi.managerComm, &mpiStatus);
        CrashIfError(err, "MPI_Recv");
    }

    printf("snapshot\n");

    message = STOP;
    err = MPI_Send((void *) &message, 1, MPI_INT, 0, 1, mpi.managerComm);
    CrashIfError(err, "MPI_Send");

    printf("Recieved comm\n");
    MPI_Finalize();
    return 0;
}