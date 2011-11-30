#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdlib.h>

typedef void (*sighandler_t)(int);

static sighandler_t handle_signal(int sig_nr, sighandler_t signalhandler){
	return SIG_ERR;
}


int main(){
	return (0);
}
