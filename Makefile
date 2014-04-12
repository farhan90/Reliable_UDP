CC=gcc
UNP_DIR=/home/courses/cse533/Stevens/unpv13e_solaris2.10
LIBS= -lresolv -lsocket -lnsl -lm -lpthread ${UNP_DIR}/libunp.a
FLAGS=-g -O2 -Wall
CFLAGS=${FLAGS} -I${UNP_DIR}/lib

all: client server prifinfo_plus
	
# Client stuff

client: client.o common.o readline.o get_ifi_info_plus.o
	${CC} ${FLAGS} -o client client.o common.o datagram.o cbuff.o readline.o get_ifi_info_plus.o ${LIBS}

client.o: client.c
	${CC} ${CFLAGS} -c client.c

# Server stuff

server: server.o server_child.o common.o readline.o get_ifi_info_plus.o
	${CC} ${FLAGS} -o server server.o server_child.o common.o datagram.o cbuff.o readline.o get_ifi_info_plus.o ${LIBS}

server.o: server.c server.h server_conn_list.h rto.h
	${CC} ${CFLAGS} -c server.c

server_child.o: server_child.c server.h rto.h
	${CC} ${CFLAGS} -c server_child.c	common.o

# Common stuff

common.o: common.c common.h datagram.c datagram.h cbuff.c cbuff.h
	${CC} ${CFLAGS} -c common.c datagram.c cbuff.c

# ifi_info stuff

prifinfo_plus: get_ifi_info_plus.o prifinfo_plus.o
	${CC} ${FLAGS} -o prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o ${LIBS}

readline.o: ${UNP_DIR}/threads/readline.c
	${CC} ${CFLAGS} -c ${UNP_DIR}/threads/readline.c

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

prifinfo_plus.o: prifinfo_plus.c
	${CC} ${CFLAGS} -c prifinfo_plus.c

# Clean

clean:
	rm client server prifinfo_plus *.o