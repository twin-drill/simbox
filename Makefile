main: sim.o langton.o vis.o ant.c
	gcc sim.o langton.o vis.o -o ant ant.c

vis.o: vis.c
	gcc -c vis.c

sim.o: sim.c
	gcc -c sim.c

langton.o : langton.c
	gcc -c langton.c

clean:
	rm -f *.o ant