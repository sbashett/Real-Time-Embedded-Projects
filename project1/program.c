#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/time.h>
#include <syscall.h>
#include <sched.h>
#include <sys/wait.h>

#define lock(x) pthread_mutex_lock(&task_locks[x-1])
#define unlock(x) pthread_mutex_unlock(&task_locks[x-1])
#define compute(x,i,j) for(i=0; i<x; i++) { j++; }
#define APERIODIC_EVENTS 2
#define MAX_TASK_LOCKS 10
#define PI_ENABLE 0


struct node {
	struct node *next;
	char buff[10];
};

/*******************************global variable description***************************************************

task_locks : array to store the mutex locks used in computation block of thread
periodic_activate : conditional variable used as a barrier to activate all the periodic threads at once
periodic_activate_lock : lock for the periodic_activate conditional variable
aperiodic_activate : conditional variables to control the activation of aperiodic threads on mouse trigger
aperiodic_Activate_lock: lock for aperiodic_activate conditional variable
periodic_cond_Var : conditional variables for each periodic task to sleep until end of period
periodic_cond_lock : locks for periodic_cond_var conditional variables

terminate : global flag to indicate end of execution time
start_timer : flag to indicate start of execution and activate the threads
periodic_length : variable to store no:of periodic tasks
num_tasks : variable to store total no:of tasks

**************************************************************************************************************/

pthread_mutex_t task_locks[MAX_TASK_LOCKS];

pthread_mutex_t global_lock;

pthread_mutex_t periodic_activate_lock;
pthread_cond_t periodic_activate;

pthread_mutex_t aperiodic_activate_lock[2];
pthread_cond_t aperiodic_activate[2];

pthread_cond_t *periodic_cond_var;
pthread_mutex_t *periodic_cond_lock;

pthread_cond_t main_process_cond;
pthread_mutex_t main_process_lock;

static bool terminate,start_timer;
static unsigned int periodic_length;
static unsigned int num_tasks;


//periodic thread function t

void *periodic(void *t)
{
	static int thread_count;
	struct node *base_ptr = (struct node *)t;
	struct node *ptr;
	int loop_count;
	unsigned long i,j;
	j = 0;
	int thread_num,temp;
	int period = atoi(base_ptr->buff);
	
	base_ptr = base_ptr->next;
	
	//assgning the thread_num, condition locks and variables to individual threads on first come basis
	pthread_mutex_lock(&global_lock);
	thread_num = thread_count;
	thread_count++;
	printf("periodic thread with tid %ld created\n", syscall(SYS_gettid));
	pthread_mutex_unlock(&global_lock);

	struct timespec abstime_period;
	struct timeval now;

	//sleeping on conditional variable until activation of all periodic threads
	pthread_mutex_lock(&periodic_activate_lock);
	pthread_cond_wait(&periodic_activate, &periodic_activate_lock);
	pthread_mutex_unlock(&periodic_activate_lock);

	while(terminate == 0)
	{	
		//setting the absolute time for end of period by calculating the present time and adding period
		gettimeofday(&now,NULL);

		printf("periodic thread %ld with period %d computing;timestamp %ld %ld\n", syscall(SYS_gettid), period, now.tv_sec, now.tv_usec);
	    
	    temp = (now.tv_usec / 1000) + period;

    	abstime_period.tv_sec = now.tv_sec + (temp / 1000);
    	abstime_period.tv_nsec = (temp % 1000) * 1000000UL;

    	//loop for implementing the computation block with critical section and mutexes
		ptr = base_ptr;
		while(ptr != NULL)
		{
			if( (loop_count = atoi(ptr->buff)) != 0)
			{	
				compute(loop_count,i,j);
			}

			else
			{
				if( (ptr->buff[0] == 'L') || (ptr->buff[0] =='l') )
					lock( (int)(ptr->buff[1]) - 48 );
				else
					unlock( (int)(ptr->buff[1]) - 48 );
			}

			ptr = ptr->next;
		}

		if(terminate == 1)
			break;	
		
		//timed wait of task untl the end of period 
		pthread_mutex_lock(&periodic_cond_lock[thread_num]);	
		pthread_cond_timedwait(&periodic_cond_var[thread_num], &periodic_cond_lock[thread_num], &abstime_period);
		pthread_mutex_unlock(&periodic_cond_lock[thread_num]);
	}

	pthread_mutex_lock(&global_lock);
	num_tasks--;
	pthread_mutex_unlock(&global_lock);
	pthread_exit(NULL);

}

//aperiodic thread function to perform computation on mouse trigger
void *aperiodic(void *t)
{
	struct node *base_ptr = (struct node *)t;
	struct node *ptr;
	int event_num,i,j,loop_count;
	
	j=0;
	event_num = atoi(base_ptr->buff);
	base_ptr = base_ptr->next;

	printf("aperiodic thread with tid %ld created with event num %d\n",syscall(SYS_gettid), event_num);

	
	while(terminate == 0)
	{	
		//thread sleeps here on condition variable until it is triggered by mosue handler		
		pthread_mutex_lock(&aperiodic_activate_lock[event_num]);
		pthread_cond_wait(&aperiodic_activate[event_num] , &aperiodic_activate_lock[event_num]);
		pthread_mutex_unlock(&aperiodic_activate_lock[event_num]);
		
		//checking the termiante flag and exiting if execution time is competed 
		if (terminate == 1)
			break;

		printf("aperiodic thread with tid %ld computing\n",syscall(SYS_gettid));

		ptr = base_ptr;
		while(ptr != NULL)
		{	
			loop_count = atoi(ptr->buff);
			if( loop_count != 0)
			{	
				compute(loop_count,i,j);
			}

			else
			{
				if( (ptr->buff[0] == 'L') || (ptr->buff[0] =='l') )
					lock( (int)(ptr->buff[1]) - 48 );
				else
					unlock( (int)(ptr->buff[1]) - 48 );
			}

			ptr = ptr->next;
		}
	}

	pthread_mutex_lock(&global_lock);

	num_tasks--;
	pthread_mutex_unlock(&global_lock);
	pthread_exit(NULL);
}

//thread function to read the mouse events and trigger the aperiodic task 
void *mouse_handler(void *t)
{
	int fd, bytes,left,right;
    unsigned char data[3];

    //reading the mouse events using device file interface
    const char *pDevice = "/dev/input/mice";

    // Open Mouse
    fd = open(pDevice, O_RDWR);
    if(fd == -1)
    {
        printf("ERROR Opening %s\n", pDevice);
        pthread_exit(NULL);
    }
    
    while(terminate == 0)
    {
        // Read Mouse     
        bytes = read(fd, data, sizeof(data));
        
        if(bytes > 0)
        {
            left = data[0] & 0x1;
            right = data[0] & 0x2;

            //while(read(fd, data, sizeof(data) >= 0));
                        
            //triggering event 0 on left click
            if(left == 1)
            {
            	printf("left click\n");
            	pthread_mutex_lock(&aperiodic_activate_lock[0]);
				pthread_cond_signal(&aperiodic_activate[0]);
				pthread_mutex_unlock(&aperiodic_activate_lock[0]);
            }
            
            //triggering event 1 on right click
            if(right == 2)
            {
            	printf("right click\n");
            	pthread_mutex_lock(&aperiodic_activate_lock[1]);
				pthread_cond_signal(&aperiodic_activate[1]);
				pthread_mutex_unlock(&aperiodic_activate_lock[1]);
            }           
        }   
    }
   pthread_exit(NULL);
}

//timer handler which is called on completion of execution time
void terminate_handler(int sig_no)
{
	int i;

	if(terminate == 0)
	{
		printf("reached signal handler after timer expiration with signum %d\n", sig_no);
		
		//sets the global terminate flag on completion of execution time
		terminate = 1;

		//wakes up all the periodic and aperiodic tasks which are sleeping or waiting 

		//waking aperiodic threads
		for(i=0; i < (APERIODIC_EVENTS) ; i++)
		{
			pthread_mutex_lock(&aperiodic_activate_lock[i]);
			pthread_cond_signal(&aperiodic_activate[i]);
			pthread_mutex_unlock(&aperiodic_activate_lock[i]);

		}

		//waking periodic threads
		for(i=0; i< periodic_length; i++)
		{
			pthread_mutex_lock(&periodic_cond_lock[i]);
			pthread_cond_signal(&periodic_cond_var[i]);
			pthread_mutex_unlock(&periodic_cond_lock[i]);
		}

		//waking main process 
		pthread_mutex_lock(&main_process_lock);
		pthread_cond_signal(&main_process_cond);
		pthread_mutex_unlock(&main_process_lock);
	}

}

void *timer_thread(void *t)
{
	//declaration of timer related variables
	timer_t timer_id;
    struct sigevent sev;
    struct sigaction sa;
    struct itimerspec timer_value;
    int ret, *ex_time;
    sigset_t set;
    union sigval sigev_value;

    sigemptyset(&set);
    sigaddset(&set, SIGALRM); 
    sigev_value.sival_int = 14;
    sigev_value.sival_ptr = NULL;

	ex_time = (int *)t;
    
    //initialising the termination timer
	sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGALRM;
    sev.sigev_value = sigev_value;
    
    sa.sa_handler = terminate_handler;
    sa.sa_mask = set;
    sa.sa_flags = SA_NODEFER;
    sigaction(SIGALRM, &sa, NULL); //sending a SIGALRM signal on timeout

    ret = timer_create(CLOCK_REALTIME, &sev, &timer_id);
    if(ret != 0)
    	printf("error creating timer\n");

    //setting the timer value
    timer_value.it_value.tv_sec = *ex_time / 1000;
    timer_value.it_value.tv_nsec = (*ex_time % 1000) * 1000000;
    timer_value.it_interval.tv_nsec = 0;
    timer_value.it_interval.tv_sec = 0;

    while(1)
    {
    	if(start_timer == 1)
    	{	//setting the timer after start_timer flag is set
    		timer_settime(timer_id, 0, &timer_value, NULL);
    		break;
    	}
    }

    pthread_exit(NULL);
}


int main()
{
	//declaration of file related variables
	FILE *fp = fopen("input.txt" , "r");
	char buff[20];
	int i,k,ex_time,num_tasks_back,ret;
	struct node *list, *ptr;
	fpos_t pos;

    //declaration of thread variables and attribute variables
    pthread_t *threads;
    pthread_attr_t tattr;
    struct sched_param param;

    //extracting data from input file and storing it in an array of linked list
	fscanf(fp, "%s" , buff);
	num_tasks = atoi(buff);
	num_tasks_back = num_tasks;
	list = (struct node *)malloc(num_tasks*sizeof(struct node));
	
	fscanf(fp, "%s", buff);
	ex_time = atoi(buff); //extracting execution time of program

	//loop for storing the data in a linked list
	for (i=0 ; i<num_tasks; i++)
	{	
		ptr = &list[i];

		for(k = 0; ; k++)
		{	
			fscanf(fp, "%s", buff);
			strcpy(ptr->buff, buff);

			fgetpos(fp,&pos);
			fread(buff, 1, 1, fp);
			if (buff[0] == '\n')
			{
				break;
			}
			fsetpos(fp,&pos);

			ptr->next = (struct node *)malloc(sizeof(struct node));
			ptr = ptr->next;
		}		
			
	}

	//calcualating the no:of periodic tasks
	for(i=0 ; i<num_tasks; i++)
	{
		ptr = &list[i];
		if((ptr->buff[0] == 'P') || (ptr->buff[0] == 'p'))
			periodic_length++;
	}

	//dynamic allocation of condition variables and mutex locks for periodic threads
	periodic_cond_var = (pthread_cond_t *)malloc(periodic_length * sizeof(pthread_cond_t));
	periodic_cond_lock = (pthread_mutex_t *)malloc(periodic_length * sizeof(pthread_mutex_t));

	//initializing the condition variables and mutex locks
	for(i=0; i<periodic_length; i++)
	{
		pthread_cond_init(&periodic_cond_var[i],NULL);
		pthread_mutex_init(&periodic_cond_lock[i], NULL);
	}

	for(i=0; i< (num_tasks - periodic_length); i++)
	{
		pthread_cond_init(&aperiodic_activate[i],NULL);
		pthread_mutex_init(&aperiodic_activate_lock[i],NULL);
	}

	pthread_cond_init(&periodic_activate, NULL);
	pthread_mutex_init(&periodic_activate_lock, NULL);

	/*******************enabling priority inheritance on condition**********/
	
	pthread_mutexattr_t *mutex_attr;
	
	if(PI_ENABLE == 1)
	{
		mutex_attr = (pthread_mutexattr_t *)malloc(sizeof(pthread_mutexattr_t));
		pthread_mutexattr_init(mutex_attr);
		pthread_mutexattr_setprotocol(mutex_attr, PTHREAD_PRIO_INHERIT);
	}

	else
	{
		mutex_attr = NULL;
	}

	/****************************************************************************/

	for(i=0; i < MAX_TASK_LOCKS; i++)
	{
		pthread_mutex_init(&task_locks[i], mutex_attr);
	}

	pthread_mutex_init(&global_lock, NULL);

	pthread_cond_init(&main_process_cond, NULL);
	pthread_mutex_init(&main_process_lock, NULL);

    //initializing and creating threads

    //dynamic allocation of thread variables
    threads = (pthread_t *)malloc((num_tasks + 2) * sizeof(pthread_t));

    //setting CPU affinity to threads
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);

    //initialising thread attribute variable to default value
    pthread_attr_init(&tattr);

    //setting detach state of thread
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_JOINABLE);

    //setting the scope of threads to system to compete with all the threads in all the processes
    pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);

    //setting the scheduling policy to FIFO
    pthread_attr_setschedpolicy(&tattr, SCHED_FIFO);

    //setting the inherit property to explicit
    pthread_attr_setinheritsched(&tattr, PTHREAD_EXPLICIT_SCHED);

    //creating threads and assigning thread functions

    for(i=0 ; i<num_tasks; i++)
    {
    	ptr = &list[i];
    	param.sched_priority = atoi(ptr->next->buff);
    	//setting priority to tattr
    	pthread_attr_setschedparam(&tattr, &param);

    	if((ptr->buff[0] == 'P') || (ptr->buff[0] == 'p'))
    	{
    		ret = pthread_create(&threads[i], &tattr, periodic, ptr->next->next);
    		if (ret != 0)
    		{
    			printf("error creating thread error code %d\n", ret);

    		}

    		pthread_setaffinity_np(threads[i], sizeof(cpu_set_t), &cpuset); 	// setting thread affinity to CPU0
    	}

    	else
    	{
    		pthread_create(&threads[i], &tattr, aperiodic, ptr->next->next);
    		pthread_setaffinity_np(threads[i], sizeof(cpu_set_t), &cpuset); // setting thread affinity to CPU0
    	}
    }

    //creating thread for reading reading mouse events with default attributes
    param.sched_priority = 98;
    pthread_attr_setschedparam(&tattr, &param);
    pthread_create(&threads[num_tasks], &tattr, mouse_handler, NULL);

    //creating thread for initialising timer for checking execution time
    param.sched_priority = 99;
    pthread_attr_setschedparam(&tattr, &param);
    pthread_create(&threads[num_tasks+1], &tattr, timer_thread, &ex_time);

    //starting the timer
    start_timer = 1;
    
    //activating all the periodic threads at same time
    pthread_mutex_lock(&periodic_activate_lock);
    pthread_cond_broadcast(&periodic_activate);
    pthread_mutex_unlock(&periodic_activate_lock);

    //sending main thread to sleep until completion of execution time
   	pthread_mutex_lock(&main_process_lock);
   	pthread_cond_wait(&main_process_cond, &main_process_lock);
   	pthread_mutex_unlock(&main_process_lock);

   	
    while(num_tasks != 0); //waiting until all threads are completed
        
    ret = pthread_kill(threads[num_tasks_back], SIGALRM); //unblocking the mouse handler thread
    if(ret != 0)
    	printf("error %d with pthread_kill\n",ret);
	  	
    num_tasks = num_tasks_back;    

    for(i=0; i < num_tasks; i++)
    {
    	pthread_join(threads[i], NULL);
    }

    printf("all threads joined\n");

    //destroying all locks, conditional variables, timer and thread attribute
    pthread_mutex_destroy(&periodic_activate_lock);
    pthread_cond_destroy(&periodic_activate);

    for(i=0; i < (num_tasks - periodic_length); i++)
    {
    	pthread_mutex_destroy(&aperiodic_activate_lock[i]);
		pthread_cond_destroy(&aperiodic_activate[i]);
    }

    for(i=0; i< periodic_length; i++)
	{
		pthread_mutex_destroy(&periodic_cond_lock[i]);
		pthread_cond_destroy(&periodic_cond_var[i]);
	}

    for(i=0; i<10; i++)
    {
    	pthread_mutex_destroy(&task_locks[i]);
    } 

    if(PI_ENABLE == 1)
    	pthread_mutexattr_destroy(mutex_attr);

    //destroying attribute and freeing memory
    pthread_attr_destroy(&tattr);
    free(threads);
    free(list);
    free(periodic_cond_lock);
    free(periodic_cond_var);
    pthread_exit(NULL);

    return 0;

}
