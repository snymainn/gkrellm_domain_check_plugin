GTK_INCLUDE = `pkg-config gtk+-2.0 --cflags`
GTK_LIB = `pkg-config gtk+-2.0 --libs`

FLAGS = -O2 -Wall -fPIC $(GTK_INCLUDE)
LIBS = $(GTK_LIB)
LFLAGS = -shared
CFLAGS = -Wno-unused-but-set-variable

CC = gcc $(CFLAGS) $(FLAGS) $(DEBUG)

OBJS = domain_check.o

domain_check.so: $(OBJS)
	$(CC) $(OBJS) -o domain_check.so $(LFLAGS) $(LIBS) 

clean:
	rm -f *.o core *.so* *.bak *~
	
domain_check.o: domain_check.c

debug:
	$(MAKE) $(MAKEFILE) DEBUG="-DDEBUG_FLAG"


install:
	if [ -d $(HOME)/.gkrellm2/plugins/ ] ; then \
		install -s -m 644 domain_check.so $(HOME)/.gkrellm2/plugins/ ; \
    fi


uninstall:
	rm -f $(HOME)/.gkrellm2/plugins/domain_check.so
	

