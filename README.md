# LifeGame
Distributed Convey's Life Game implementation using MPI.

## How does it work?
The only instance of *server* is waiting for the connections from *clients*. 

Once the connection is established, the *server* receives from the *client* the number of routines, *workers*, that should process the calculation of Life Game, and spawns the required number of *workers*.

One of the *workers* is called *manager*, and it continues the communication with the *client*. *Client* can give orders to the *manager*, e.g. to **stop** the work or to send it a **snapshot** of the game field.

## Build
You should have **open-mpi** installed.

The build is conducted using make utility.

The targets are server, client, worker. To build them, run
``` shell
make <target>
```

To remove all the executables,
``` shell
make clean
```

## Run
To run *server* call **run_server.sh** script. The *server* process doesn't require any other actions.

To run *client* call **run_client.sh** script. Enter the height and width of the game polygon, the number of workers and the polygon itself using '0' and '1' to indicate dead and alive cells.

The dialogue should look like this:

<img src="https://github.com/terana/Img/blob/master/LifeGame/ClientDialogue.png" width="226" height="154">
