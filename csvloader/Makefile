all : test_way2.out test_way1.out

test_way1.out : csv.c test_way1.c
	gcc -g -O2 -Wall -fPIC $^ -o $@

test_way2.out : csv.c test_way2.c
	gcc -g -O2 -Wall -fPIC $^ -o $@

clean :
	rm -rf ./test_way2.out ./test_way1.out