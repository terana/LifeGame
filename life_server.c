#include <string.h>
#include "mpi.h"

#include "error.h"
#include "worker_settings.h"

#define WIDTH 5
#define HEIGHT 5

typedef struct {
    int numOfWorkers;
    MPI_Comm workersComm;
    int height;
    int width;
} Settings;

void InitSettings(Settings *settings) {
    settings->numOfWorkers = 2;
    settings->height = HEIGHT;
    settings->width = WIDTH;
}

void Send(int message, Settings* settings) {
    int err = MPI_Send((void *) &message, 1, MPI_INT, 0, 1, settings->workersComm);
    CrashIfError(err, "MPI_Send");
}

int main(int argc, char *argv[]) 
{ 
    int polygon [HEIGHT][WIDTH] = {
        {1, 1, 0, 0, 0}, 
        {1, 0, 0, 0, 0}, 
        {1, 0, 1, 1, 1}, 
        {0, 0, 0, 1, 0}, 
        {0, 0, 0, 0, 0}
    };

    int worldSize = 0;
    char *workerProgram = "worker";
    
    Settings settings;
    InitSettings(&settings);

    MPI_Status mpiStatus;

    int i, err;

    err = MPI_Init(&argc, &argv);
    CrashIfError(err, "MPI_Init");

    MPI_Comm_size(MPI_COMM_WORLD, &worldSize); 
    Assert(worldSize == 1, "Too many managers");

    MPI_Comm_spawn(workerProgram, MPI_ARGV_NULL, settings.numOfWorkers + 1,  
             MPI_INFO_NULL, 0, MPI_COMM_SELF, &(settings.workersComm), MPI_ERRCODES_IGNORE);


    WorkerSettings workerSettings = {settings.height, settings.width};
    err = MPI_Send((void *) &workerSettings, 2, MPI_INT, 0, 1, settings.workersComm);
    CrashIfError(err, "MPI_Send");

    for (i = 0; i < workerSettings.height; ++i) {
        err = MPI_Send((void *) polygon[i], settings.width, MPI_INT, 0, 1, settings.workersComm);
        CrashIfError(err, "MPI_Send");
    }
    

    int message;
    Send(SNAPSHOT, &settings);
    for (i = 0 ; i < settings.height; ++i) {
        err = MPI_Recv((void *) polygon[i], settings.width, MPI_INT, 0, 1, settings.workersComm, &mpiStatus);
        CrashIfError(err, "MPI_Recv");
    }

    message = STOP;
    err = MPI_Send((void *) &message, 1, MPI_INT, 0, 1, settings.workersComm);
    CrashIfError(err, "MPI_Send");




    printf("Server stops working\n");
    MPI_Finalize(); 
    return 0; 
} 