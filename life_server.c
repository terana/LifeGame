#include <string.h>
#include "mpi.h"

#include "error.h"
#include "worker_settings.h"

#define WIDTH 5
#define HEIGHT 5

typedef struct {
    int numOfWorkers;
    int height;
    int width;
} Settings;

void InitSettings(Settings *settings) {
    settings->numOfWorkers = 2;
}

int main(int argc, char *argv[]) 
{ 
    int i, j;

    int fullPolygon [HEIGHT + 2][WIDTH] = {
        {0, 0, 0, 0, 0},
        {1, 1, 0, 0, 0}, 
        {1, 0, 0, 0, 0}, 
        {1, 0, 0, 0, 0}, 
        {0, 0, 0, 0, 0}, 
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0}
    };

    int worldSize = 0;
    MPI_Comm workersComm;
    char *workerProgram = "worker";
    
    Settings settings;
    InitSettings(&settings);

    int err = MPI_Init(&argc, &argv);
    CrashIfError(err, "MPI_Init");

    MPI_Comm_size(MPI_COMM_WORLD, &worldSize); 
    Assert(worldSize == 1, "Too many managers");

    MPI_Comm_spawn(workerProgram, MPI_ARGV_NULL, settings.numOfWorkers,  
             MPI_INFO_NULL, 0, MPI_COMM_SELF, &workersComm, MPI_ERRCODES_IGNORE);

    WorkerSettings workerSettings = {0, WIDTH};
    int from;
    for (i = 0; i < settings.numOfWorkers; ++i) {
        if (i < HEIGHT % settings.numOfWorkers) {
            workerSettings.height = HEIGHT / settings.numOfWorkers + 1;
            from = workerSettings.height * i;
        } else {
            workerSettings.height = HEIGHT / settings.numOfWorkers;
            from = workerSettings.height * i + HEIGHT % settings.numOfWorkers;
        }
        workerSettings.height += 2;

        err = MPI_Send((void *) &workerSettings, 2, MPI_INT, i, 1, workersComm);
        CrashIfError(err, "MPI_Send");

        for (j = 0; j < workerSettings.height; ++j) {
            err = MPI_Send((void *) fullPolygon[from + j], WIDTH, MPI_INT, i, 1, workersComm);
            CrashIfError(err, "MPI_Send");
        }
    }

    printf("Server stops working\n");
    MPI_Finalize(); 
    return 0; 
} 