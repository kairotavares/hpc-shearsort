# test1 = 4 Process -  2n x 2n
# test2 = 9 Process -  3n x 3n
# test3 = 16 Process - 4n x 4n
# n = 10 and n = (max 15 min duration)
# chunksize: 1, n/2 e n

all: test1 test2 test3

build: 
	mpicc -o shear_sort shear_sort.c

test1: build
	mpirun -np 4 ./shear_sort 40 20 1
	mpirun -np 4 ./shear_sort 40 20 10
	mpirun -np 4 ./shear_sort 40 20 20

test2: build
	mpirun -np 9 ./shear_sort 90 30 1
	mpirun -np 9 ./shear_sort 90 30 15
	mpirun -np 9 ./shear_sort 90 30 30

test3: build
	mpirun -np 16 ./shear_sort 160 40 1
	mpirun -np 16 ./shear_sort 160 40 20
	mpirun -np 16 ./shear_sort 160 40 40

clean:
	rm shear_sort
