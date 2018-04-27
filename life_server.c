#include <string.h>
#include "mpi.h"

#include "error.h"

typedef struct {
    int numOfWorkers;
    MPI_Comm workersComm;
    MPI_Comm clientComm;
    int clientRank;
    int clientTag;
} Routine;

static const int maxConnections = 5;

void fillClientInfo(Routine *routine, const MPI_Status *status) {
    routine->clientRank = status->MPI_SOURCE;
    routine->clientTag  = status->MPI_TAG;
}

int main(int argc, char *argv[]) 
{ 
    int worldSize = 0;
    char *workerProgram = "worker";

    MPI_Status mpiStatus;

    int err;

    char mpiPort[MPI_MAX_PORT_NAME];
    char managerPort[MPI_MAX_PORT_NAME];

    err = MPI_Init(&argc, &argv);
    CrashIfError(err, "MPI_Init");

    err = MPI_Comm_size(MPI_COMM_WORLD, &worldSize); 
    CrashIfError(err, "MPI_Comm_size");
    Assert(worldSize == 1, "Too many servers");

    err = MPI_Open_port(MPI_INFO_NULL, mpiPort); 
    CrashIfError(err, "MPI_Open_port");
    printf("port name is: %s\n", mpiPort); 

    Routine routines [maxConnections];
    int connections = 0;

    while (1) {
        if (connections < maxConnections) {
            err = MPI_Comm_accept(mpiPort, MPI_INFO_NULL, 0, MPI_COMM_SELF, &(routines[connections].clientComm)); 
            CrashIfError(err, "MPI_Comm_accept");
            printf("Accepted connection\n");

            err = MPI_Recv(&(routines[connections].numOfWorkers), 1, MPI_INT,  
                MPI_ANY_SOURCE, MPI_ANY_TAG, routines[connections].clientComm, &mpiStatus);
            CrashIfError(err, "MPI_Recv");
            fillClientInfo(&(routines[connections]), &mpiStatus);

            err = MPI_Comm_spawn(workerProgram, MPI_ARGV_NULL, routines[connections].numOfWorkers + 1,  
                MPI_INFO_NULL, 0, MPI_COMM_SELF, &(routines[connections].workersComm), MPI_ERRCODES_IGNORE);
            CrashIfError(err, "MPI_Comm_spawn");

            err = MPI_Recv(managerPort, MPI_MAX_PORT_NAME, MPI_CHAR,  
                0, MPI_ANY_TAG, routines[connections].workersComm, &mpiStatus);
            CrashIfError(err, "MPI_Recv");

            err = MPI_Send((void *) managerPort, MPI_MAX_PORT_NAME, MPI_CHAR, 
                routines[connections].clientRank, routines[connections].clientTag, routines[connections].clientComm);
            CrashIfError(err, "MPI_Send");

            ++connections;
        }
    }

    printf("Life server stops working\n");
    MPI_Finalize(); 
    return 0; 
} 