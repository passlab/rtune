RTUNE_INSTALL=../../install

jacobi-rtune_num_threads: jacobi.c
	gcc -fopenmp -g -I${RTUNE_INSTALL}/include -o $@ $< -L${RTUNE_INSTALL}/lib -lrtune -lm

clean:
	rm -rf jacobi-rtune_num_threads
