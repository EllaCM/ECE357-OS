test:
	gcc -o test recursive_file_lister.c
	./test -v .
	rm test

root:
	gcc -o test recursive_file_lister.c
	./test -v -u dodo /
	rm test

user:
	gcc -o test recursive_file_lister.c
	./test -v -u dodo .
	rm test

userd:
	gcc -o test recursive_file_lister.c
	./test -v -u 1000 .
	rm test

dev:
	gcc -o test recursive_file_lister.c
	./test /dev
	rm test
	
devv:
	gcc -o test recursive_file_lister.c
	./test -m 123 -v /dev
	rm test
	
media:
	gcc -o test recursive_file_lister.c
	./test -v /media
	rm test
	
download:
	gcc -o test recursive_file_lister.c
	./test -v ~/Downloads
	rm test

time:
	gcc -o test recursive_file_lister.c
	touch time
	./test -m -10 -v .
	rm test time

day:
	gcc -o test recursive_file_lister.c
	./test -m -86400 /home/dodo/Projects
	rm test 
	
timed:
	gcc -o test recursive_file_lister.c
	./test -m -86400 /home/dodo/Projects
	rm test
	
ntime:
	gcc -o test recursive_file_lister.c
	touch time
	./test -m 10 -v .
	rm test time
	
clean:
	rm -f test *.o *.stackdump *~