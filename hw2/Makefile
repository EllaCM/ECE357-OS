test:
	gcc -o test recursive_file_lister.c
	./test -m 123 -v .
	rm test

dev:
	gcc -o test recursive_file_lister.c
	./test -m 123 -v /dev
	rm test
	
media:
	gcc -o test recursive_file_lister.c
	./test -m 123 -v /media
	rm test

time:
	gcc -o test recursive_file_lister.c
	touch time
	./test -m -10 -v .
	rm test time
	
clean:
	rm -f test *.o *.stackdump *~