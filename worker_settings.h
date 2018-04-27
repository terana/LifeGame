#ifndef LIFE_WORKER_SETTINGS
#define LIFE_WORKER_SETTINGS

typedef struct {
    int height;
    int width;
} WorkerSettings;

enum Order {
	STOP,
	CONTINUE,
	SNAPSHOT
};

#endif