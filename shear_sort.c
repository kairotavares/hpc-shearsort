#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include <limits.h>
#include <time.h>

#define FALSE 0
#define TRUE  1

#define END -1
#define SEED 11235

/*
########### MATRIX #########
    13    10    12     3
    11    13     7    14
     1     0    13     4
     9     5     0     3
############################
*/

/*
########### MATRIX #########
    0    0    1    3
    7    5    4    3
    9    10   11   12
    14   13   13   13
############################
*/

void populateMatrix(int n, int array[n][n]) {
	srand(SEED);
	int x, y;
	for(x = 0; x < n; x++) {
	    for(y = 0; y < n; y++) {
			array[x][y] = rand() % (n*n);
		}
	}
}

/**
*
*  01 02 | 03 04
*  05 06 | 07 08       0 | 1
*  -------------    =  -----  
*  09 10 | 11 12       2 | 3
*  13 14 | 15 16
*
**/

int compute_section_row(int globalMatrixSize, int localMatrixSize, int section) {
    int x, y, aux;
	int row = 0;
	int maxProcessColumns = globalMatrixSize/localMatrixSize;
	for(x = 0; x < section; x++) {
		aux = globalMatrixSize - (localMatrixSize * ((x % maxProcessColumns) + 1));
		if (aux <= 0) {
		  row++;
		}
	}
	return row;
}

int compute_section_column(int globalMatrixSize, int localMatrixSize, int section) {
	int maxProcessColumns = globalMatrixSize/localMatrixSize;
	return section % maxProcessColumns;
}

void copy_matrix_sector(int n, int input[n][n], int m, int output[m][m], int section) {
    int x, y;
	int column = 0, row = 0;

    row = compute_section_row(n, m, section);
    column = compute_section_column(n, m, section);

	for(x = 0; x < m; x++) {
	    for(y = 0; y < m; y++) {
			int inputRow = (row * m) + x;
			int inputColumn = (column * m) + y;
			output[x][y] = input[inputRow][inputColumn];
		}
	}
}

void update_matrix_sector(int n, int dst[n][n], int m, int src[m][m], int section) {
    int x, y, aux;
	int column = 0, row = 0;
    row = compute_section_row(n, m, section);
    column = compute_section_column(n, m, section);

	for(x = 0; x < m; x++) {
	    for(y = 0; y < m; y++) {
			int inputRow = (row * m) + x;
			int inputColumn = (column * m) + y;
			dst[inputRow][inputColumn] = src[x][y];
		}
	}
}

int main(int argc, char ** argv) {

    int poolSize, rank, i, j;
    int endValue = END;
    double start, end;

    MPI_Status status;
    start = MPI_Wtime();

    MPI_Init(&argc, &argv);    
    MPI_Comm_size(MPI_COMM_WORLD, &poolSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int matrixSize = atoi(argv[1]);
	int matrixAuxSize = atoi(argv[2]);
	int chunk = atoi(argv[3]);

    int matrix[matrixSize][matrixSize];
	int matrixAux[matrixAuxSize][matrixAuxSize];

	int hasChange = FALSE;
	int masterChanged = FALSE;
	int slaveChanged = FALSE;

	populateMatrix(matrixSize, matrix);

    if (rank == 0) {
		printf("\n-------- STARTING --------\n"); fflush(stdout);		
		print_matrix(matrixSize, matrix, rank);
		copy_matrix_sector(matrixSize, matrix, matrixAuxSize, matrixAux, rank);
	
		do {
			hasChange = FALSE;
			masterChanged = FALSE;
			slaveChanged = FALSE;

			// Order lines
			printf("\n--------  ORDER LINES WITH SHEAR SORT --------\n"); fflush(stdout);
			shear_sort_lines(matrixAuxSize, matrixAux);
			receive_matrix(poolSize, matrixSize, matrix, matrixAuxSize, matrixAux);
			print_matrix(matrixSize, matrix, rank);
			MPI_Barrier(MPI_COMM_WORLD);

			// Exchange columns chunks
			printf("\n-------- STEP 1 - EVEN --------\n"); fflush(stdout);		        
		    masterChanged = exchange_columns(rank, chunk, matrixSize, matrixAuxSize, matrixAux, TRUE);
			slaveChanged = receive_changed_status(poolSize);
			if (masterChanged || slaveChanged) {
				hasChange = TRUE;			
			}
			MPI_Barrier(MPI_COMM_WORLD);
			receive_matrix(poolSize, matrixSize, matrix, matrixAuxSize, matrixAux);
			print_matrix(matrixSize, matrix, rank);
			MPI_Barrier(MPI_COMM_WORLD);

			printf("\n-------- STEP 2 - ODD --------\n"); fflush(stdout);		        
		    masterChanged = exchange_columns(rank, chunk, matrixSize, matrixAuxSize, matrixAux, FALSE);
			slaveChanged = receive_changed_status(poolSize);
			if (masterChanged || slaveChanged) {
				hasChange = TRUE;			
			}
			receive_matrix(poolSize, matrixSize, matrix, matrixAuxSize, matrixAux);
			print_matrix(matrixSize, matrix, rank);
			MPI_Barrier(MPI_COMM_WORLD);
		
			// Order columns
			printf("\n-------- ORDER COLUMNS WITH SHEAR SORT --------\n"); fflush(stdout);
			shear_sort_columns(matrixAuxSize, matrixAux);
			receive_matrix(poolSize, matrixSize, matrix, matrixAuxSize, matrixAux);
			print_matrix(matrixSize, matrix, rank);
			MPI_Barrier(MPI_COMM_WORLD);

			// Exchange line chunks
			printf("\n-------- STEP 3 - EVEN --------\n"); fflush(stdout);		        
		    masterChanged = exchange_lines(rank, chunk, matrixSize, matrixAuxSize, matrixAux, TRUE);
			slaveChanged = receive_changed_status(poolSize);
			if (masterChanged || slaveChanged) {
				hasChange = TRUE;			
			}
			receive_matrix(poolSize, matrixSize, matrix, matrixAuxSize, matrixAux);
			print_matrix(matrixSize, matrix, rank);
			MPI_Barrier(MPI_COMM_WORLD);

			printf("\n-------- STEP 4 - ODD --------\n"); fflush(stdout);		        
		    masterChanged = exchange_lines(rank, chunk, matrixSize, matrixAuxSize, matrixAux, FALSE);
			slaveChanged = receive_changed_status(poolSize);
			if (masterChanged || slaveChanged) {
				hasChange = TRUE;			
			}
			receive_matrix(poolSize, matrixSize, matrix, matrixAuxSize, matrixAux);
			print_matrix(matrixSize, matrix, rank);
			MPI_Barrier(MPI_COMM_WORLD);

			send_continue(poolSize, hasChange);

		} while (hasChange);
    } else {
		copy_matrix_sector(matrixSize, matrix, matrixAuxSize, matrixAux, rank);    	
		while (TRUE) {
			// Lines
			shear_sort_lines(matrixAuxSize, matrixAux);
			send_matrix_to_master(matrixAuxSize, matrixAux);
			MPI_Barrier(MPI_COMM_WORLD);
            // Step 1     
            hasChange = exchange_columns(rank, chunk, matrixSize, matrixAuxSize, matrixAux, TRUE);
			send_changed_result_to_master(hasChange);
			MPI_Barrier(MPI_COMM_WORLD);
			send_matrix_to_master(matrixAuxSize, matrixAux);
			MPI_Barrier(MPI_COMM_WORLD);
            // Step 2     
            hasChange = exchange_columns(rank, chunk, matrixSize, matrixAuxSize, matrixAux, FALSE);
			send_changed_result_to_master(hasChange);
			send_matrix_to_master(matrixAuxSize, matrixAux);
			MPI_Barrier(MPI_COMM_WORLD);
			// Columns
			shear_sort_columns(matrixAuxSize, matrixAux);
			send_matrix_to_master(matrixAuxSize, matrixAux);
			MPI_Barrier(MPI_COMM_WORLD);
			// Exchange line chunks
			hasChange = exchange_lines(rank, chunk, matrixSize, matrixAuxSize, matrixAux, TRUE);
			send_changed_result_to_master(hasChange);
			send_matrix_to_master(matrixAuxSize, matrixAux);
			MPI_Barrier(MPI_COMM_WORLD);
			// Exchange line chunks
			hasChange = exchange_lines(rank, chunk, matrixSize, matrixAuxSize, matrixAux, FALSE);
			send_changed_result_to_master(hasChange);
			send_matrix_to_master(matrixAuxSize, matrixAux);
			MPI_Barrier(MPI_COMM_WORLD);

			if (!can_continue()) {
				break;			
			}			
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if(rank == 0) {
        end = MPI_Wtime();
        printf("Total Time = %.8f\n", end-start); fflush(stdout);
    }
    
    MPI_Finalize();

    return 0;
}

void send_continue(int size, int canContinue) {
	int i;
	for(i = 1; i < size; i++) {
	MPI_Send(&canContinue, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
	}
}

int can_continue() {
	int canContinue = FALSE;
	MPI_Status status;
	MPI_Recv(&canContinue, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	return canContinue;
}

int bubble_sort(int size, int array[size], int inverted) {
  int x, y, swap;
  int changed = FALSE;
 
  for (x = 0 ; x < ( size - 1 ); x++)
  {
    for (y = 0 ; y < size - x - 1; y++)
    {
	  int needSwap = FALSE;
	  if (inverted) {
          needSwap = array[y] < array[y+1];
      } else { 
	  	  needSwap = array[y] > array[y+1];
      }

      if (needSwap) /* For decreasing order use < */
      {
        swap = array[y];
        array[y] = array[y+1];
        array[y+1] = swap;
        changed = TRUE;
      }
    }
  }
  return changed;
}

int shear_sort_columns(int n, int matrix[n][n]) {
	int i;
    int changed = FALSE;
	
	transform_matrix(n, n, matrix);
	for(i = 0; i < n; i++) {
		int ret = bubble_sort(n, ((int*)matrix) + (i*n), FALSE);
        if (ret) {
			changed = TRUE;
		}
	}
	transform_matrix(n, n, matrix);
	return changed;
}

int shear_sort_lines(int n, int matrix[n][n]) {
	return shear_sort(n, n, matrix);
}

int shear_sort(int x, int y,  int matrix[x][y]) {
	int i;
    int changed = FALSE;
	for(i = 0; i < x; i++) {
		int ret = bubble_sort(y, ((int*)matrix) + (i*y), i % 2);
        if (ret) {
			changed = TRUE;
		}
	}
	return changed;
}

// TODO Use pointers
int merge_sort_columns(int n, int matrix[n][n], int chunk, int input[n][chunk], int rank, int isSender) {
	int arrayAux[n][n+chunk];
	int row, column;
	if (isSender) {
		// Merge the two arrays 
		for (row=0; row<n; row++)
		{
			for(column=0; column<n; column++)
				arrayAux[row][column] = matrix[row][column];
		}
		for (row=0; row<n; row++)
		{
			for(column=0; column<chunk; column++)
				arrayAux[row][n+column] = input[row][column];
		}
	} else {
		// Merge the two arrays 
		for (row=0; row<n; row++)
		{
			for(column=0; column<chunk; column++)
				arrayAux[row][column] = input[row][column];
		}
		for (row=0; row<n; row++)
		{
			for(column=0; column<n; column++)
				arrayAux[row][chunk+column] = matrix[row][column];
		}
	}

	// Shear sort the matrix
	int changed = shear_sort(n, n+chunk, arrayAux);

	// Update Matrix
	if (isSender) {
		for (row=0; row<n; row++) {
			for(column=0; column<n; column++)
				matrix[row][column] = arrayAux[row][column];
		}
	} else {
		for (row=0; row<n; row++) {
			for(column=0; column<n; column++)
				matrix[row][column] = arrayAux[row][chunk + column];
		}	
	}

	return changed;
}


// TODO Use pointers
int merge_sort_lines(int n, int matrix[n][n], int chunk, int input[chunk][n], int rank, int isSender) {
    int changed = FALSE;
	int row, column;
	int arrayAux[n+chunk][n+chunk];
	zero_fill(n+chunk, n+chunk, arrayAux);

	// Merge the two arrays
	if (isSender) {
		for (row=0; row<n; row++) {
			for(column=0; column<n; column++)
				arrayAux[row][column] = matrix[row][column];
		}
		for (row=0; row<chunk; row++) {
			for(column=0; column<n; column++)
				arrayAux[n+row][column] = input[row][column];
		}
	} else {
		for (row=0; row<chunk; row++) {
			for(column=0; column<n; column++)
				arrayAux[row][column] = input[row][column];
		}		
		for (row=0; row<n; row++) {
			for(column=0; column<n; column++)
				arrayAux[chunk+row][column] = matrix[row][column];
		}
	}

	changed = shear_sort_columns(chunk+n, arrayAux);

	// Update Matrix
	if (isSender) {
		for (row=0; row<n; row++) {
			for(column=0; column<n; column++)
				matrix[row][column] = arrayAux[row][column];
		}
	} else {
		for (row=0; row<n; row++) {
			for(column=0; column<n; column++)
				matrix[row][column] = arrayAux[chunk + row][column];
		}	
	}

	return changed;
}

int exchange_columns(int id, int chunk, int globalMatrixSize, int n, int matrix[n][n], int evenSender) {
	int sendArray[n*chunk], receiveArray[n*chunk];
	int maxProcessColumns = globalMatrixSize/n;
	int processColumn = id % maxProcessColumns;
    MPI_Status status;
	int changed = FALSE;
	
	int isSender = FALSE;
	// Check for even or odd sender
	if (evenSender) {
		isSender = isEven(processColumn);
	} else { 
		isSender = !(isEven(processColumn));		
	}

	if (isSender) {
		if (processColumn + 1 < maxProcessColumns) {
			// Exchange columns
			// TODO Possible bug coping column with chunk > 1, maybe use the transport matrix (transform_matrix)
			copyColumn(n-chunk, n, n, matrix, sendArray);
			MPI_Send(sendArray, n * chunk, MPI_INT, id + 1, 0, MPI_COMM_WORLD);
	        MPI_Recv(receiveArray, n * chunk, MPI_INT, id + 1, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		    // Merging Received columns
			changed = merge_sort_columns(n, matrix, chunk, (int**)receiveArray, id, TRUE);
		}
	} else {
		if (processColumn > 0) {
			// Exchange columns
		    MPI_Recv(receiveArray, n * chunk, MPI_INT, id - 1, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			// TODO Possible bug coping column with chunk > 1, maybe use the transport matrix (transform_matrix)
			copyColumn(0, chunk, n, matrix, sendArray);
			MPI_Send(sendArray, n * chunk, MPI_INT, id - 1, 0, MPI_COMM_WORLD);
			// Merging Received columns
			changed = merge_sort_columns(n, matrix, chunk, (int**)receiveArray, id, FALSE);
		}
	}

	return changed;
}

int exchange_lines(int id, int chunk, int globalMatrixSize, int n, int matrix[n][n], int evenSender) {
	int receiveArray[n*chunk];
	int maxProcessColumns = globalMatrixSize/n;
	int processLine = compute_section_row(globalMatrixSize, n, id);
    MPI_Status status;
	int changed = FALSE;

	int isSender = FALSE;
	// Check for even or odd sender
	if (evenSender) {
		isSender = isEven(processLine);
	} else { 
		isSender = !(isEven(processLine));		
	}

	if (isSender) {
		if (processLine + 1 < maxProcessColumns) {
			// Exchange lines
			MPI_Send(matrix[n-chunk], n * chunk, MPI_INT, id + maxProcessColumns, 0, MPI_COMM_WORLD);
	        MPI_Recv(receiveArray, n * chunk, MPI_INT, id + maxProcessColumns, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		    // Merging Received columns
			changed = merge_sort_lines(n, matrix, chunk, (int**)receiveArray, id, TRUE);
		}
	} else {
		if (processLine > 0) {
			// Exchange lines
		    MPI_Recv(receiveArray, n * chunk, MPI_INT, id - maxProcessColumns, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			MPI_Send(matrix[0], n * chunk, MPI_INT, id - maxProcessColumns, 0, MPI_COMM_WORLD);
			// Merging Received columns
			changed = merge_sort_lines(n, matrix, chunk, (int**)receiveArray, id, FALSE);
		}
	}
	return changed;
}

int isEven(int number) {
	return !(number % 2);
}

void copyColumn(int initialColumn, 
					int finalColumn, 
					int n, 
					int matrix[n][n], 
					int output[n * (finalColumn-initialColumn)]) {
	int i, j;
	for(i = 0; i < n; i++) {
	  for(j = initialColumn; j < finalColumn; j++) {
		 output[i] = matrix[i][j];
      } 	
	}
}

void send_matrix_to_master(int n, int matrix[n][n]) {
	MPI_Send((int*)matrix, n*n, MPI_INT, 0, 0, MPI_COMM_WORLD);
}

void send_changed_result_to_master(int changed) {
	MPI_Send(&changed, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
}

int receive_changed_status(int size) {
	int i;
	int receiveStatus;
	int changed = FALSE;
    MPI_Status status;
	
    for(i = 1; i < size; i++) {
        MPI_Recv(&receiveStatus, 1, MPI_INT, i, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		//printf("received status %d = %d\n", i, receiveStatus);fflush(stdout);
		if(receiveStatus) {
			changed = TRUE;
		}
    }

	return changed;
}

void receive_matrix(int size, 
					int globalMatrixSize, 
					int globalMatrix[globalMatrixSize][globalMatrixSize], 
					int localMatrixSize,
					int localMatrix[localMatrixSize][localMatrixSize]) {
    
	int i;
	int receiveArray[localMatrixSize*localMatrixSize];
    MPI_Status status;
	update_matrix_sector(globalMatrixSize, globalMatrix, localMatrixSize, localMatrix, 0);
    for(i = 1; i < size; i++) {
        MPI_Recv(receiveArray, localMatrixSize*localMatrixSize, MPI_INT, i, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		update_matrix_sector(globalMatrixSize, globalMatrix, localMatrixSize, (int**)receiveArray, i);
    }
}
			
void print_matrix(int len, int matrix[len][len], int id) {
    print_matrix_values(len, len, matrix, id);
}

void print_matrix_values(int rows, int columns, int matrix[rows][columns], int id) { 
	int row, column;
    printf("########### Node %d ###########\n", id); fflush(stdout);
	for (row=0; row<rows; row++)
	{
	    for(column=0; column<columns; column++)
			printf("%6i", matrix[row][column]); fflush(stdout);
	    printf("\n"); fflush(stdout);
	}
    printf("############################\n"); fflush(stdout);
}

void zero_fill(int rows, int columns, int matrix[rows][columns]) { 
	int row, column;
	for (row=0; row<rows; row++)
	{
	    for(column=0; column<columns; column++)
			matrix[row][column] = 0;
	}
}

// Make the matrix transposition, it change rows to columns
void transform_matrix(int x, int y, int matrix[x][y]) {
  int i, j, aux;
  
  for (i = 0; i < x; i++) {
    for (j = i+1; j < y; j++) {
      if (j != i) {
		   aux = matrix[i][j];
		   matrix[i][j] = matrix[j][i];
		   matrix[j][i] = aux;
      }
    }
  }
}

