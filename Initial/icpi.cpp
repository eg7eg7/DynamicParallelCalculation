#include <stdio.h>
#include <math.h>
#include <mpi.h>
#define HEAVY 100000

#define FINISH_JOB_TAG 1
#define START_JOB_TAG 0
#define TERMINATE_PROCESS_TAG 2

// This function performs heavy computations,
// its run time depends on x and y values
double heavy(int x, int y) {
	int i, loop = 1;
	double sum = 0;

	// Super heavy tasks
	if (x < 5 || y < 5)
		loop = 10;
	// Heavy calculations
	for (i = 0; i < loop*HEAVY; i++)
		sum += sin(exp(sin((double)i / HEAVY)));

	return sum;
}

void getNextJob(int *x_pointer, int *y_pointer, int *num_jobs_left, int N)
{
	int x = *x_pointer;
	int y = *y_pointer;

	y = (y + 1) % N;
	if (y == 0)
		x = x + 1;
	*x_pointer = x;
	*y_pointer = y;

	(*num_jobs_left) = ((*num_jobs_left) - 1);
}

int main(int argc, char *argv[]) {
	int x = -1, y = -1;
	int N = 20;
	double answer = 0, t1, t2, temp_ans;
	int job[2];
	int myid, numprocs;
	MPI_Status status;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);

	int num_jobs = N * N;
	if (numprocs > 1)
	{
		if (myid == 0)
		{
			t1 = MPI_Wtime();
			int i, num_assign, working_processes = 0;

			num_assign = num_jobs;
			//Assign all processes a job
			for (i = 1; i < num_assign; i++)
			{
				if (i > (numprocs - 1))
					break;
				getNextJob(&x, &y, &num_jobs, N);
				job[0] = x;
				job[1] = y;
				MPI_Send(job, 2, MPI_INT, i, START_JOB_TAG, MPI_COMM_WORLD);
				working_processes++;
			}

			while (num_jobs > 0 || working_processes > 0)
			{
				//Waits for one of the processes to return with an answer, and assigns it a new job
				MPI_Recv(&temp_ans, 1, MPI_DOUBLE, MPI_ANY_SOURCE, FINISH_JOB_TAG, MPI_COMM_WORLD, &status);
				answer += temp_ans;
				if (num_jobs > 0)
				{
					int processId = status.MPI_SOURCE;
					getNextJob(&x, &y, &num_jobs, N);
					job[0] = x;
					job[1] = y;
					MPI_Send(job, 2, MPI_INT, processId, START_JOB_TAG, MPI_COMM_WORLD);
				}
				else
					working_processes--;
			}

			for (i = 1; i < numprocs; i++)
			{
				MPI_Send(job, 2, MPI_INT, i, TERMINATE_PROCESS_TAG, MPI_COMM_WORLD);
			}

			t2 = MPI_Wtime();
			printf("answer = %e\ntime - %1.9f", answer, t2 - t1);
		}
		else
		{
			int tag;
			do {
				MPI_Recv(job, 2, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				tag = status.MPI_TAG;
				if (tag == START_JOB_TAG)
				{
					temp_ans = heavy(job[0], job[1]);
					MPI_Send(&temp_ans, 1, MPI_DOUBLE, 0, FINISH_JOB_TAG, MPI_COMM_WORLD);
				}
			} while (tag != TERMINATE_PROCESS_TAG);
		}
	}
	else
	{
		//Only one process

		t1 = MPI_Wtime();
		for (x = 0; x < N; x++)
			for (y = 0; y < N; y++)
				answer += heavy(x, y);
		t2 = MPI_Wtime();
		printf("answer = %e\ntime = %1.8f", answer, t2 - t1);
	}
	MPI_Finalize();
}