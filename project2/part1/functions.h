#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>

#define RATE_MONOTONIC 0
#define DEADLINE_MONOTONIC 1
#define MIN(a,b) \
({__typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  _a > _b ? _b : _a ;})

//function for finding hcf of two numbers
int gcd(int a , int b)
{
	if(b==0)
		return a;
	return gcd(b,a%b);
}

//function for finding lcm of array of numbers
int lcm(float *arr, int n)
{
	int ans,i;
	ans = (int)arr[0];

	for(i=0;i<n;i++)
	{
		ans = ((int)(arr[i])*ans)/gcd((int)(arr[i]),ans);
	}

	return ans;
}

//function to calculate the busy period of a taskset
float busy_fun(float *period, float *wcet, int hyper_period,int num_tasks)
{
	int busy_period,i,temp;

	busy_period = 0;

	//assgining initial value as sum of all execution times
	for (i = 0; i < num_tasks; i++)
	{
		busy_period += wcet[i];
	}

	//loop for finding the busy period using iterative method
	while(1)
	{
		temp = 0;

		for(i=0; i<num_tasks; i++)
		{
			temp += (float)ceil((float)(busy_period)/period[i])*wcet[i];
		}

		//checking if busy period exceeds hyper period
		if(temp <= hyper_period)
		{
			if (temp == busy_period)
				break;
			else
				busy_period = temp;
		}

		else
		{
			busy_period = hyper_period;
			break;
		}
	}

	return busy_period;
}

//sorting the tasks from higher priority to lower priority based on rm or dm condition
void sort(float *deadline, float *period, float *wcet, int num_tasks, bool rm_dm)
{
	float *a, *b, *c;
	int i,j;
	float temp;

	a = (rm_dm == RATE_MONOTONIC) ? period : deadline;
	b = (rm_dm == RATE_MONOTONIC) ? deadline : period;
	c = wcet;

	for (i =0 ; i<num_tasks; i++)
	{
		for(j=0 ; j< (num_tasks-i-1); j++)
		{
			if (a[j] > a[j+1])
			{
				temp = a[j];
				a[j] = a[j+1];
				a[j+1] = temp;
				temp = b[j];
				b[j] = b[j+1];
				b[j+1] = temp;
				temp = c[j];
				c[j] = c[j+1];
				c[j+1] = temp;
			}
		}
	}
}

//function for doing the response time analysis of taskset and return 0 or 1 for non-schedulable and schedulable respectively
bool response_time_analysis(float *deadline,float *period,float *wcet,int num_tasks)
{
	int i,j;
	bool flag = 1;
	float response_time,tmp;

	//assign initial value as sum of all executon times
	for(i=1; i<num_tasks;i++)
	{
		response_time = wcet[i];
		
		for (j =0; j< i; j++)
		{
			response_time += wcet[j];
		}

		//finding response time by iterative method
		while(1)
		{
			tmp = wcet[i];

			for(j=0; j < i; j++ )
			{
				tmp += ceil(response_time/period[j])*wcet[j];
			}

			//break the loop when the response time remains smae on two consecutive iterations
			if ((tmp == response_time) || (tmp > deadline[i]))
			{
				response_time = tmp;
				break;
			}
			else
				response_time = tmp;
		}

		//check if response time is greater than deadline
		if (response_time > deadline[i])
		{
			flag = 0;
			return flag;
		}
	}

	return flag;
}

//function to check the basic necessary utilization condition; if greater than 1 not schedulable
bool nec_util_test(float *period, float *wcet, int num_tasks)
{
	int i;
	float util;
	
	util = 0;

	for (i =0 ; i< num_tasks; i++)
	{
		util += wcet[i]/period[i];
	}

	if (util <= 1)
		return 1;
	else
		return 0;
}


//function to check the generalized sufficient utilization condition of taskset  for rm systems. If less than sufficient bound schedulable
bool rm_gen_suff_test(float *deadline, float *period,float *wcet, int curr_task)
{

	int i, n;
	float suff_util;

	suff_util = wcet[curr_task]/MIN(deadline[curr_task],period[curr_task]);
	n = 1;

	for (i = 0; i < curr_task; i++)
	{
		if (period[i] < deadline[curr_task])
		{
			suff_util += wcet[i]/period[i];
			n++;
		}

		else
			suff_util += wcet[i]/MIN(deadline[curr_task], period[curr_task]);
	}

	//returns 1 if suff utilization is less than or equal to utilization bound else return 0
	if (suff_util <= (n * (powf(2,(1/(float)n))-1)))
		return 1;
	else
		return 0;
}

//algorithm to generate random utilization values for tasksets such that the total utilization of all tasks in taskset is equal to u value
void uunifast(float *utilizations, int n, float u)
{
	float sum,nextsum,temp;
	int i;

	sum = u;

	for (i =1 ; i<n ; i++)
	{	
		temp = (float)rand()/RAND_MAX;
		nextsum = sum * pow(temp,(1/(float)(n-i)));
		utilizations[i-1] = sum-nextsum;
		sum = nextsum;
	}
	utilizations[n-1] = sum;

}

//funciton for assigning random periods to the tasks by considering periods are in the form of a uniform distribution
void assign_periods(float *period, int num_tasks)
{
	int i;
	//divide taskset into three parts and assign periods to tasks 
	//randomly selecting periods for tasks of first part of taskset from the range 1000-10000
	for (i = 0; i < (num_tasks/3); i++)
	{
		period[i] = ((float)rand()/RAND_MAX) * (10000-1000) + 1000;
	}

	//randomly selecting periods for tasks of first part of taskset from the range 10000-100000
	for (; i < 2*(num_tasks/3); i++)
	{
		period[i] = ((float)rand()/RAND_MAX) * (100000-10000) + 10000;
	}

	//randomly selecting periods for tasks of first part of taskset from the range 100000-1000000
	for (; i <= 3*(num_tasks/3); i++)
	{
		period[i] = ((float)rand()/RAND_MAX) * (1000000-100000) + 100000;
	}
}