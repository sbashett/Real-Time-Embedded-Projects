#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "functions.h"

/************This program takes a input file from user and tests the schedulability of all tasksets for edf,rm and dm scheduling************

Global variable declaration:

struct node_each_task : structure fro each task of a taskset in a linked list
struct node_each taskset : structure which acts as a head node for each taskset

This program incudes "functions.h" header file which 
*********************************************************************************************************************************************/

struct node_each_task{
	struct node_each_task *next;
	int task;
	float deadline;
	float wcet;
	float period;
};

struct node_each_taskset {
	struct node_each_taskset *down;
	struct node_each_task *next;
	int num_tasks;
	int task_set;
};

//function for checking the edf schedulablity of all the tasksets
void edf_test(struct node_each_taskset *list)
{
	struct node_each_task *task;

	int num_tasks,i,busy_period,hyper_period,j,k,count;

	float *deadline, *period, *wcet;
	float suff_util,tmp, processor_demand, loading_factor;
	bool flag = 1;

	//array pointers to store the deadline,period and wcet of all tasks in a particular taskset
	deadline = (float *)malloc(sizeof(float));
	period = (float *)malloc(sizeof(float));
	wcet = (float *)malloc(sizeof(float));

	printf("performing edf schedulability test....\n");

	//outer loop for traversing through all the tasksets in the list
	while (1)
	{	
		if (list->next != NULL)
		task = list->next;

		else 
			goto HERE;

		//variables to store suff_utilizaion, processor_demand and loading_factor
		suff_util = 0;
		processor_demand = 0;
		loading_factor = 0;

		num_tasks = list->num_tasks;

		//reallocating the variables depending on the number of tasks in the taskset
		deadline = (float *)realloc(deadline,num_tasks*sizeof(float));
		period = (float *)realloc(period, num_tasks*sizeof(float));
		wcet = (float *)realloc(wcet, num_tasks*sizeof(float));

		//loop for storing the deadline,period,wcet from list into arrays and also calculating the sufficient util test
		for (i = 0; i < num_tasks; i++)
		{
			deadline[i] = task->deadline;
			period[i] = task->period;
			wcet[i] = task->wcet;

			suff_util += wcet[i]/MIN(deadline[i],period[i]);
			
			task = (task->next == NULL) ? NULL : task->next;
		}

		//checking the necessary utilization test fro taskset
		if (!nec_util_test(period,wcet,num_tasks))
		{
			printf("task set %d is NOT edf schedulable\n", list->task_set);
			goto HERE;
		}

		//comparing the sufficient utilization test and deciding schedulability
		if (suff_util <= 1)
			printf("task set %d is edf schedulable\n", list->task_set);

		//if sufficient condition not satisfied we go to utlization factor analysis
		else 
		{	
			//calculating the busy period and hyper period using functions written in "functions.h" header file
			hyper_period = lcm(period, num_tasks);
			busy_period = busy_fun(period, wcet, hyper_period,num_tasks);

			//outer loop for iterating over all testing instances in loading factor analysis
			for (i=0; ; i++)
			{
				count = 0;

				//inner loop for iterating over number of tasks
				for (j =0 ; j < num_tasks; j++)
				{
					tmp = deadline[j]+(i*period[j]);

					if (tmp <= busy_period)
					{	
						processor_demand = 0;
						
						//calculating processor demand at each time instant for each task
						for (k=0; k<num_tasks; k++)
						{
							if (deadline[k] <= tmp)
							{
								processor_demand += (1 + (float)floor((tmp - deadline[k])/period[k]))*wcet[k];
							}
						}

						loading_factor = processor_demand/tmp;

						//if loading factor greater than 1 at any instant then its not schedulable
						if (loading_factor > 1)
						{
							flag = 0;
							break;
						}
					}

					else
						count++;
				}

				//checking if scheduling test for all tasks has been done
				if ((count == num_tasks) || (flag == 0))
					break;
			}

			//checking the flags to decide schedulable or not not schedulable
			if (flag == 1)
				printf("task set %d is edf schedulable\n", (list->task_set + 1));
			else
				printf("task set %d is not edf schedulable\n", list->task_set);
		}

		//moving to next taskset
		HERE :
		if(list->down == NULL)
			 break;
		else
			list = list->down;
	}
}

//function for checking rm scheduling 
void rm_test(struct node_each_taskset *list)
{
	struct node_each_task *task;

	int num_tasks,i;

	float *deadline, *period, *wcet;
	bool flag = 1;

	//array pointers to store the deadline,period and wcet of all tasks in a particular taskset
	deadline = (float *)malloc(sizeof(float));
	period = (float *)malloc(sizeof(float));
	wcet = (float *)malloc(sizeof(float));

	printf("performing rm schedulability test....\n");

	//outer loop for traversing through all the tasksets in the list
	while(1)
	{
		if (list->next != NULL)
		task = list->next;

		else
			goto HERE;

		num_tasks = list->num_tasks;

		//reallocating the variables depending on the number of tasks in the taskset
		deadline = (float *)realloc(deadline,num_tasks*sizeof(float));
		period = (float *)realloc(period, num_tasks*sizeof(float));
		wcet = (float *)realloc(wcet, num_tasks*sizeof(float));

		//loop for storing the deadline,period,wcet from list into arrays and also calculating the sufficient util test
		for (i = 0; i < num_tasks; i++)
		{
			deadline[i] = task->deadline;
			period[i] = task->period;
			wcet[i] = task->wcet;
			
			task = (task->next == NULL) ? NULL : task->next;
		}

		//sorting the array elements in decreasing order of priority based on rm static priority assignment 
		//function sort can be referenced in functions.h header file
		sort(deadline,period,wcet,num_tasks, RATE_MONOTONIC);		
		
		//checking the necessary condition for utilization
		if (!nec_util_test(period,wcet,num_tasks))
		{
			flag = 0;
			goto JUMP;
		}

		//checking the general sufficient utilization test using function written in functions.h header file
		//if utilization les than sufficient bound it is schedulable otherwise move to response time analysis
		if(rm_gen_suff_test(deadline,period,wcet,num_tasks-1))
			flag = 1;

		//response_time_analysis fucntion written in functions.h header file
		else
			flag = response_time_analysis(deadline,period,wcet,num_tasks);

		//checking the flag returned by the above funcitons to decide schedulability
		JUMP:
		if (flag == 0)
			printf("task_set %d not rm schedulable\n",list->task_set);			

		else
			printf("task_set %d is rm schedulable\n",list->task_set);
		
		//moving to next taskset	
		HERE :
		if(list->down == NULL)
			 break;
		else
			list = list->down;
	}
}

//function for testing the dm schedulability of tasksets
void dm_test(struct node_each_taskset *list)
{
	struct node_each_task *task;

	int num_tasks,i;

	float *deadline, *period, *wcet;
	bool flag = 1,suff_flag;

	//array pointers to store the deadline,period and wcet of all tasks in a particular taskset
	deadline = (float *)malloc(sizeof(float));
	period = (float *)malloc(sizeof(float));
	wcet = (float *)malloc(sizeof(float));

	printf("performing dm schedulability test....\n");
	//outer loop for traversing through all the tasksets in the list
	while(1)
	{
		if (list->next != NULL)
		task = list->next;

		else
			goto HERE;

		num_tasks = list->num_tasks;

		//reallocating the variables depending on the number of tasks in the taskset
		deadline = (float *)realloc(deadline,num_tasks*sizeof(float));
		period = (float *)realloc(period, num_tasks*sizeof(float));
		wcet = (float *)realloc(wcet, num_tasks*sizeof(float));

		//loop for storing the deadline,period,wcet from list into arrays and also calculating the sufficient util test
		for (i = 0; i < num_tasks; i++)
		{
			deadline[i] = task->deadline;
			period[i] = task->period;
			wcet[i] = task->wcet;
			
			task = (task->next == NULL) ? NULL : task->next;
		}
		
		//sorting the array elements in decreasing order of priority based on dm static priority assignment 
		//function sort can be referenced in functions.h header file		
		sort(deadline,period,wcet,num_tasks, DEADLINE_MONOTONIC);

		for (i = 0 ; i < num_tasks-1; i++)
		{
			if(period[i] < period[i+1])
				continue;
			else
				break;
		}		

		if (!nec_util_test(period,wcet,num_tasks))
		{
			flag = 0;
			goto JUMP;
		}
		
		//checking the general sufficient utilization test using function written in functions.h header file
		//if utilization les than sufficient bound it is schedulable otherwise move to response time analysis
		for(i = i+1; i < num_tasks; i++)
		{
			suff_flag = rm_gen_suff_test(deadline,period,wcet,i);
			if(suff_flag == 0)
				goto TIME_BASED_ANALYSIS;
		}

		if (suff_flag == 1)
			flag = 1;
		
		//response_time_analysis fucntion written in functions.h header file
		else
		{
			TIME_BASED_ANALYSIS:
			flag = response_time_analysis(deadline,period,wcet,num_tasks);
		}

		//checking the flag returned by the above funcitons to decide schedulability
		JUMP:
		if(flag == 0)
			printf("task_set %d not dm schedulable\n",list->task_set);
		else
			printf("task_set %d is dm schedulable\n",list->task_set);

		HERE :
		if(list->down == NULL)
			 break;
		else
			list = list->down;
	}
}


//main function for storing the data in input.txt file into list and calling the schedualbilty test functions
int main()
{
	FILE *fp = fopen("input.txt" , "r");
	char buff[20];
	int num_task_sets,num_tasks,i,k;
	struct node_each_taskset *list, *list_ptr;
	struct node_each_task *ptr;
	
	fscanf(fp, "%s" , buff);
	num_task_sets = atoi(buff); 
	printf("num_task_sets: %d \n",num_task_sets );
	list = (struct node_each_taskset *)malloc(num_task_sets*sizeof(struct node_each_taskset));

	//loop for running throgh the input text file and storing the data in a list pointed by list pointer
	for (i=0 ; i < num_task_sets; i++)
	{	
		list_ptr = &list[i];
		if(i < num_task_sets-1 )
		{
			list_ptr->down = &list[i+1];
		}

		fscanf(fp,"%s",buff);
		num_tasks = atoi(buff);
		list_ptr->num_tasks = num_tasks;

		for(k = 0; k < num_tasks ; k++)
		{	
			if (k == 0)
			{
				list_ptr->next = (struct node_each_task *)malloc(sizeof(struct node_each_task));
				ptr = list_ptr->next;
			}

			else
			{
				ptr->next = (struct node_each_task *)malloc(sizeof(struct node_each_task));
				ptr = ptr->next;
			}			

			fscanf(fp, "%s", buff);
			ptr->task = k+1;
			ptr->wcet = atof(buff);

			fscanf(fp, "%s", buff);
			ptr->deadline = atof(buff);

			fscanf(fp, "%s", buff);
			ptr->period = atof(buff);
		}				
	}

	//calling the edf, rm and dm schedulability testing fucntions by passing the list pointer
	edf_test(list);
	rm_test(list);
	dm_test(list);

return 0;
}