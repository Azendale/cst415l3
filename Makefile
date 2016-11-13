#**************************************************
# Makefile for OIT CST415 Lab3 simple RSP protocol
# CST 415 Lab 3
#
# Author: Philip Howard
# Email:  phil.howard@oit.edu
# Date:   Sept 22, 2016
# Modifications by Erik Andersen, <erik@eoni.com>, last changed 2016-10-31
#
COPTS = -g3 -O0 -Wall -pthread -Wwrite-strings
CPPOPTS = $(COPTS) -std=c++11

LOPTS = -pthread

OBJS = getport.o \
       rsp_if.o \
       rsp.o \
       queue.o \
       RspData.o \

all: rsp_client clearport clearconn sendclient recvclient t_rsp_1 t_rsp_2 t_rsp_3_postconnect_rst_client t_rsp_3_rst_inject

clean:
	rm -f rsp_server
	rm -f rsp_client
	rm -f clearport
	rm -f clearconn
	rm -f sendclient
	rm -f recvclient
	rm -f t_rsp_1
	rm -f t_rsp_2
	rm -f t_rsp_3_postconnect_rst_client
	rm -f t_rsp_3_rst_inject
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

t_rsp_3_rst_inject: t_rsp_3_rst_inject.o $(OBJS)
	g++ $(COPTS) t_rsp_3_rst_inject.o -o t_rsp_3_rst_inject $(OBJS) $(LOPTS)

t_rsp_3_postconnect_rst_client: t_rsp_3_postconnect_rst_client.o 
	g++ $(COPTS) t_rsp_3_postconnect_rst_client.o -o t_rsp_3_postconnect_rst_client $(OBJS) $(LOPTS)

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

