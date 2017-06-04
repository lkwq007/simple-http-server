httpserver: main.c libinetsocket.c thpool.c
	gcc main.c libinetsocket.c thpool.c -pthread -o httpserver