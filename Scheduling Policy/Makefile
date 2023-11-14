.PHONY: clean stat

CFLAGS = -Wfatal-errors #-fno-elide-constructors

all: sched_demo run

sched_demo: sched_demo_312551105.c
	gcc $(CFLAGS) sched_demo_312551105.c -o sched_demo

run:
	./sched_demo -n 4 -t 0.5 -s NORMAL,FIFO,NORMAL,FIFO -p -1,10,-1,30

clean:
	rm -f sched_demo

stat:
	wc -l sched_demo_312551105.c