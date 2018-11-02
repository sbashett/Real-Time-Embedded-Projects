#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>

#include "functions.h"

#define NUM_UTIL_LEVELS 10
#define NUM_TASK_SETS 5000
#define NUM_TASKS 25
#define DEADLINE_DIST 2

/********************************This program makes a comparative analysis between *********************************************
******************************	schedulabilities of edf rm and dm by generatng synthetic tasksets*****************************

MACRO definitions:

NUM_UTIL_LEVELS : This macro gives the number of utilization levels to be generated
NUM_TASK_SETS : This macro gives the number of tasksets 
NUM_TASKS : This macro specifies the number of tasks in each taskset
DEADLINE_DIST : This macro is used to specify the deadline distribution
	It takes two values 1,2
	case 1 : deadline distribution is [ci,Ti]
	case 2 : deadline dsitribution is [ci+(ti-ci)/2 , ti]

************************************************************************************************************************************/

//funciton for testing the edf schedulability for a given taskset
bool edf_test(float *deadline, float *period, float *wcet)
{
	int i,busy_period,hyper_period,j,k,count;

	float suff_util,tmp, processor_demand, loading_factor;
	
	suff_util = 0;
	processor_demand = 0;
	loading_factor = 0;
	
	//finding necessary utilization test function in funcitons.h header file
	if (!nec_util_test(period,wcet,NUM_TASKS))
	{
		return 0;
	}

	//performing the sufficient utilization test
	for (i = 0; i < NUM_TASKS; i++)
	{
		suff_util += wcet[i]/MIN(deadline[i],period[i]);			
	}

	if (suff_util <= 1)
		return 1;

	//if sufficeint util test didnt satisfy do utilization factor analysis
	else 
	{
		hyper_period = lcm(period, NUM_TASKS);
		busy_period = busy_fun(period, wcet, hyper_period,NUM_TASKS);

		//loop for iterating over all time instances on which to perform the utilization factor analysis
		for (i=0; ; i++)
		{
			count = 0;
			for (j =0 ; j < NUM_TASKS; j++)
			{
				tmp = deadline[j]+(i*period[j]);
				if (tmp <= busy_period)
				{	
					//calcualting processor demand for each task at tets instances
					processor_demand = 0;
					for (k=0; k<NUM_TASKS; k++)
					{
						if (deadline[k] <= tmp)
						{
							processor_demand += (1 + (float)floor((tmp - deadline[k])/period[k]))*wcet[k];
						}
					}

					//checking and comparing loading factor
					loading_factor = processor_demand/tmp;

					if (loading_factor > 1)
					{
						return 0;
					}
				}

				else
					count++;
			}

			if (count == NUM_TASKS)
				break;
		}

		return 1;
	}
}

//function to perform rm schedulability test
bool rm_test(float *deadline, float *period, float *wcet)
{	
	//sorting the given deadline,period and wcet elements based on rm priority assignment
	//function written in funcitons.h header file
	sort(deadline,period,wcet,NUM_TASKS, RATE_MONOTONIC);

	//checking necessary utilization condition
	if (!nec_util_test(period,wcet,NUM_TASKS))
	{
		return 0;
	}

	//performing the generalized sufficient utilization test. Refer funcitons.h header file
	if(rm_gen_suff_test(deadline,period,wcet,NUM_TASKS-1))
		return 1;
	//if sufficient utilizaiton not satisfied move to response_time_analysis. Refer funcitons.h header file
	else
		return response_time_analysis(deadline,period,wcet,NUM_TASKS);			
}


bool dm_test(float *deadline, float *period, float *wcet)
{
	int i;
	bool suff_flag;

	//sorting the given deadline,period and wcet elements based on dm priority assignment
	//function written in funcitons.h header file
	sort(deadline,period,wcet,NUM_TASKS, DEADLINE_MONOTONIC);

	for (i = 0 ; i < NUM_TASKS-1; i++)
	{
		if(period[i] < period[i+1])
			continue;
		else
			break;
	}
	//checking necessary utilization condition
	if (!nec_util_test(period,wcet,NUM_TASKS))
		return 0;
	
	//performing the genralized sufficient utilization test for all tasks. Refer funcitons.h header file
	for(i = i+1; i < NUM_TASKS; i++)
	{
		suff_flag = rm_gen_suff_test(deadline,period,wcet,i);
		if(suff_flag == 0)
			goto TIME_BASED_ANALYSIS;
	}

	if (suff_flag == 1)
		return 1;

	//if sufficient utilizaiton not satisfied move to response_time_analysis. Refer funcitons.h header file
	TIME_BASED_ANALYSIS:
	return response_time_analysis(deadline,period,wcet,NUM_TASKS);
}

//main funciton which generates synthetic task sets
int main()
{
	float ***periods, ***deadline, ***wcet;

	float *utilizations;
	int i,j,k;

	//array of utilizations for which tasks are generated
	float u[] = {0.05, 0.15, 0.25, 0.35, 0.45, 0.55, 0.65, 0.75, 0.85, 0.95 };
	
	int edf,rm,dm;

	//opening file to store the final values in a csv file
	FILE *fp = fopen("values.csv","w");

	if(fp == NULL)
		printf("error opening file\n");

	utilizations = (float *)malloc(NUM_TASKS*sizeof(float));
	
	//seeding the rand function
	srand(time(NULL));

	//dynamic allocation of period, deadline and wcet arrays
	periods = (float ***)malloc(NUM_UTIL_LEVELS * sizeof(float **));
	deadline = (float ***)malloc(NUM_UTIL_LEVELS * sizeof(float **));
	wcet = (float ***)malloc(NUM_UTIL_LEVELS * sizeof(float **));

	//forming a 3d array for storing the array elements of all tasks of 5000 tasksets for all utilizations
	for (j =0 ; j< NUM_UTIL_LEVELS; j++)
	{
		periods[j] = (float **)malloc(NUM_TASK_SETS * sizeof(float *));
		deadline[j] = (float **)malloc(NUM_TASK_SETS * sizeof(float *));
		wcet[j] = (float **)malloc(NUM_TASK_SETS * sizeof(float *));

		for (k =0 ; k< NUM_TASK_SETS; k++)
			{
				periods[j][k] = (float *)malloc(NUM_TASKS * sizeof(float));
				deadline[j][k] = (float *)malloc(NUM_TASKS * sizeof(float));
				wcet[j][k] = (float *)malloc(NUM_TASKS * sizeof(float));
			}
	}	

	//generating the periods, wcet and deadlines
	for ( i = 0; i < NUM_UTIL_LEVELS; i++)
	{
		for (j = 0; j<NUM_TASK_SETS; j++)
		{
			//uunifast function from funcitons.h header file generates set of utilizations for all the tasks in a particular taskset
			uunifast(utilizations,NUM_TASKS,u[i]);

			//assign_periods funciton in funcitons.h header file assigns periods to tasks by sampling a uniform distribution
			assign_periods(&periods[i][j][0],NUM_TASKS);

			//loop for assigning above generated values to tasks
			for (k =0 ; k < NUM_TASKS; k++)
			{	
				//calculating wcet using generated utilization and period for each task
				wcet[i][j][k] = periods[i][j][k] * utilizations[k];

				//assigning deadlines by sampling a single value from uniform distribution
				switch (DEADLINE_DIST)
				{
				
				//deadline distribution of [ci,Ti]
				case 1 :
					deadline[i][j][k] = ((float)rand()/RAND_MAX)*(periods[i][j][k]-wcet[i][j][k])+ wcet[i][j][k];
					break;

				//case with deadline distribution of [ci+(ti-ci)/2, ti]
				case 2 :
					deadline[i][j][k] = ((((float)rand()/RAND_MAX)*((periods[i][j][k]-wcet[i][j][k])))/2)+ ((wcet[i][j][k] + periods[i][j][k])/2);
					break;

				default :
					printf("wrong deadline distrbution option\n");
					exit(0);
				}
			}
		}
	}


	fprintf(fp, "util,edf,rm,dm\n");

	//loop for testing the schedulability of all tasksets for edf rm and dm case 
	for (i =0 ; i < NUM_UTIL_LEVELS; i++)
	{
		edf = rm = dm = 0;
		printf("utilization : %f\n",u[i]);
		for (j =0; j < NUM_TASK_SETS; j++)
		{
			edf += edf_test(&deadline[i][j][0],&periods[i][j][0],&wcet[i][j][0]);
			rm += rm_test(&deadline[i][j][0],&periods[i][j][0],&wcet[i][j][0]);
			dm += dm_test(&deadline[i][j][0],&periods[i][j][0],&wcet[i][j][0]);
		}

		fprintf(fp, "%f,%f,%f,%f\n", u[i], (edf/(float)NUM_TASK_SETS), (rm/(float)NUM_TASK_SETS), (dm/(float)NUM_TASK_SETS));
		printf("fraction of tasksets schedulable ...\nedf : %f, rm : %f, dm : %f\n",(edf/(float)NUM_TASK_SETS), (rm/(float)NUM_TASK_SETS), (dm/(float)NUM_TASK_SETS));
	}
	
	fclose(fp);
	return 0;

}
