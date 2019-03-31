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

void sequenceWork(int N, double *ans)
{
	double answer=0;
	int x, y;

	for (x = 0; x < N; x++)
		for (y = 0; y < N; y++)
			answer += heavy(x, y);
	*ans = answer;
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

void dynamicTaskAllocation(int numprocs, int N, double *ans)
{
	MPI_Status status;
	int job[2];
	int num_jobs = N * N, x=-1, y=-1;
	double answer=0, temp_ans;
	int i, num_assign, working_processes_count = 0;

	num_assign = num_jobs;

	for (i = 1; i < num_assign; i++)
	{
		if (i > (numprocs - 1))
			break;
		getNextJob(&x, &y, &num_jobs, N);
		job[0] = x;
		job[1] = y;
		MPI_Send(job, 2, MPI_INT, i, START_JOB_TAG, MPI_COMM_WORLD);
		working_processes_count++;
	}

	while (num_jobs > 0 || working_processes_count > 0)
	{
		//Waits for one of the processes to return with an answer, and assigns it a new job
		MPI_Recv(&temp_ans, 1, MPI_DOUBLE, MPI_ANY_SOURCE, FINISH_JOB_TAG, MPI_COMM_WORLD, &status);
		answer += temp_ans;
		if (num_jobs > 0)
		{
			//process returned and more tasks left - assign new task
			int processId = status.MPI_SOURCE;
			getNextJob(&x, &y, &num_jobs, N);
			job[0] = x;
			job[1] = y;
			MPI_Send(job, 2, MPI_INT, processId, START_JOB_TAG, MPI_COMM_WORLD);
		}
		else
		{
			//process returned and no more tasks left - continue
			working_processes_count--;
		}
	}


	*ans = answer;
}

void terminateTasks(int numprocs)
{
	for (int i = 1; i < numprocs; i++)
	{
		MPI_Send(&i, 1, MPI_INT, i, TERMINATE_PROCESS_TAG, MPI_COMM_WORLD);
	}
}
void receiveExecuteTasks()
{
	MPI_Status status;
	int tag;
	int job[2];
	double temp_ans;
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


int main(int argc, char *argv[]) {
	int N = 20;
	double answer = 0, t1, t2;
	int myid, numprocs;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);

	if (numprocs > 1)
	{
		if (myid == 0)
		{
			t1 = MPI_Wtime();
			//assign all processes a task, and sends another one when a process finishes
			dynamicTaskAllocation(numprocs, N, &answer);
			//send termination tags to all processes after finishing calculation
			terminateTasks(numprocs);
			t2 = MPI_Wtime();
			printf("answer = %e\ntime = %1.9f", answer, t2 - t1);
		}
		else
		{
			//slave process - takes tasks until no more are left
			receiveExecuteTasks();
		}
	}
	else
	{
		//Only one process - sequential work

		t1 = MPI_Wtime();
		sequenceWork(N, &answer);
		t2 = MPI_Wtime();
		printf("answer = %e\ntime = %1.8f", answer, t2 - t1);
	}
	MPI_Finalize();
}