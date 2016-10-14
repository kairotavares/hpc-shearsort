all: test1 test2 test3

build: 
	mpicc -o shear_sort shear_sort.c

test1: build
	mpirun -np 4 ./shear_sort 4 2 1

test2: build
	mpirun -np 9 ./shear_sort 9 3 1

test3: build
	mpirun -np 16 ./shear_sort 16 4 1

