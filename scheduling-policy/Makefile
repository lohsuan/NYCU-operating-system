.PHONY: clean stat

all: compile run test

compile: sched_demo_312551105.c
	gcc sched_demo_312551105.c -lpthread  -o sched_demo_312551105

run:
	sudo ./sched_demo_312551105 -n 3 -t 1.0 -s NORMAL,FIFO,FIFO -p -1,10,30   			# Problem 2 
	sudo ./sched_demo_312551105 -n 4 -t 0.5 -s NORMAL,FIFO,NORMAL,FIFO -p -1,10,-1,30  	# Problem 3

test: 
	sudo ./sched_test.sh ./sched_demo ./sched_demo_312551105

clean:
	rm -f sched_demo_312551105 
