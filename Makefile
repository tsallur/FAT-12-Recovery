#Do not modify anything in this file
make:
	gcc -Wall -g notjustcats.c -o notjustcats
run:
	./notjustcats ./simple2.img ./files

clean:
	rm notjustcats

