#**************************************************
# Makefile for OIT CST415 Lab4 simple RSP protocol
# CST 415 Lab 4
#
# Author: Philip Howard
# Email:  phil.howard@oit.edu
# Date:   Sept 22, 2016
# Modifications by Erik Andersen, <erik@eoni.com>, last changed 2016-11-14
#
COPTS = -g3 -O0 -Wall -pthread -Wwrite-strings
CPPOPTS = $(COPTS) -std=c++11

LOPTS = -pthread

OBJS = getport.o \
       rsp_if.o \
       rsp.o \
       queue.o \
       RspData.o \

all: rsp_client clearport clearconn sendclient recvclient t_rsp_1 t_rsp_2 t_rsp_4 send2

clean:
	rm -f rsp_server
	rm -f rsp_client
	rm -f clearport
	rm -f clearconn
	rm -f sendclient
	rm -f recvclient
	rm -f t_rsp_1
	rm -f t_rsp_2
	rm -f t_rsp_4
	rm -f send2
	rm -f *.o

.c.o:
	gcc $(COPTS) -c $? -o $@

.cpp.o:
	g++ $(CPPOPTS) -c $? -o $@

rsp_server: rsp_server.o $(OBJS)
	g++ rsp_server.o -o rsp_server $(OBJS) $(LOPTS)

t_rsp_1: t_rsp_1.o $(OBJS)
	g++ $(COPTS) t_rsp_1.o -o t_rsp_1 $(OBJS) $(LOPTS)

t_rsp_2: t_rsp_2.o $(OBJS)
	g++ $(COPTS) t_rsp_2.o -o t_rsp_2 $(OBJS) $(LOPTS)

t_rsp_4: t_rsp_4.o 
	g++ $(COPTS) t_rsp_4.o -o t_rsp_4 $(OBJS) $(LOPTS)

send2: send2.o
	g++ $(COPTS) send2.o -o send2 $(OBJS) $(LOPTS)

rsp_client: rsp_client.o $(OBJS)
	g++ $(COPTS) rsp_client.o -o rsp_client $(OBJS) $(LOPTS)

sendclient: sendclient.o $(OBJS)
	g++ $(COPTS) sendclient.o -o sendclient $(OBJS) $(LOPTS)

recvclient: recvclient.o $(OBJS)
	g++ $(COPTS) recvclient.o -o recvclient $(OBJS) $(LOPTS)

clearconn: clearconn.c $(OBJS)
	gcc $(COPTS) clearconn.c -o clearconn getport.o rsp_if.o

clearport: clearport.c $(OBJS)
	gcc $(COPTS) clearport.c -o clearport getport.o rsp_if.o

