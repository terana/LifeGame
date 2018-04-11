typedef struct {
    int height;
    int width;
} WorkerSettings;

enum Order {
	STOP,
	CONTINUE,
	SNAPSHOT
};