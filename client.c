#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "mpi.h" 
#include "error.h"
#include "life_game.h"

typedef struct {
    int numOfWorkers;
    Size size;
    int **polygon;
} GameSettings;

void Send(int message, CommunicationConstants* mpi) {
    int err = MPI_Send((void *) &message, 1, MPI_INT, mpi->rank, mpi->tag, mpi->comm);
    CrashIfError(err, "MPI_Send");
}

void InitSettings(GameSettings *game) {
    memset(game, 0, sizeof(GameSettings));

    printf("Enter polygon height and width:\n");
    scanf("%d %d", &(game->size.height), &(game->size.width));

    printf("Enter numOfWorkers:\n");
    scanf("%d", &(game->numOfWorkers));

    printf("Enter polygon:\n");
    int i, j;
    game->polygon = (int**) malloc(game->size.height * sizeof(int*));
    for (i = 0; i < game->size.height; ++i) {
        game->polygon[i] = (int *) malloc (game->size.width * sizeof(int));
        for (j = 0; j < game->size.width; ++j) {
            scanf("%d", &(game->polygon[i][j]));
        }
    }
}

int main(int argc, char *argv[]) 
{ 
    int i, err;

    CommunicationConstants manager, server;
    //ranks and tags are set to zeroes
    memset(&manager, 0, sizeof(CommunicationConstants));
    memset(&server, 0, sizeof(CommunicationConstants));
    char managerPort [MPI_MAX_PORT_NAME];
    char serverPort [MPI_MAX_PORT_NAME];

    GameSettings game;
    InitSettings(&game);

    MPI_Status mpiStatus;

    err = MPI_Init(&argc, &argv);
    CrashIfError(err, "MPI_Init");

    printf("Enter port name: \n");
    scanf("%s", serverPort);

    err = MPI_Comm_connect(serverPort, MPI_INFO_NULL, 0, MPI_COMM_SELF, &server.comm); 
    CrashIfError(err, "MPI_Comm_connect");
    printf("Connected to server\n");

    err = MPI_Send(&game.numOfWorkers, 1, MPI_INT, server.rank, server.tag, server.comm);
    CrashIfError(err, "MPI_Send");

    err = MPI_Recv(managerPort, MPI_MAX_PORT_NAME, MPI_CHAR, server.rank, server.tag, server.comm, &mpiStatus);
    CrashIfError(err, "MPI_Recv");

    err = MPI_Comm_connect(managerPort, MPI_INFO_NULL, 0, MPI_COMM_SELF, &manager.comm); 
    CrashIfError(err, "MPI_Comm_connect");
    printf("Connected to manager\n");

    err = MPI_Send((void *) &(game.size), 2, MPI_INT, manager.rank, manager.tag, manager.comm);
    CrashIfError(err, "MPI_Send");

    for (i = 0; i < game.size.height; ++i) {
        err = MPI_Send((void *) game.polygon[i], game.size.width, MPI_INT, manager.rank, manager.tag, manager.comm);
        CrashIfError(err, "MPI_Send");
    }

    int message;
    Send(SNAPSHOT, &manager);
    for (i = 0 ; i < game.size.height; ++i) {
        err = MPI_Recv((void *) game.polygon[i], game.size.width, MPI_INT, manager.rank, manager.tag, manager.comm, &mpiStatus);
        CrashIfError(err, "MPI_Recv");
    }

    printf("Snapshot:\n");
    PrintPolygon((const int **) game.polygon, game.size.height, game.size.width);

    message = STOP;
    err = MPI_Send((void *) &message, 1, MPI_INT, manager.rank, manager.tag, manager.comm);
    CrashIfError(err, "MPI_Send");

    MPI_Finalize();

    for (i = 0; i < game.size.height; ++i) {
        free(game.polygon[i]);
    }
    free(game.polygon);

    return 0;
}