#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group-31");
MODULE_DESCRIPTION("Elevator kernel module");
MODULE_VERSION("0.1");

#define ENTRY_NAME "elevator"
#define PERMS 0644
#define PARENT NULL

#define MAX_FLOORS 5
#define MAX_PASSENGERS 5
#define MAX_WEIGHT 70

//define passenger types for loading
#define P 0
#define L 1
#define B 2
#define V 3

struct passenger {
	int type;
	int weight;
	int start;
	int destination;
	struct list_head list;
};

enum state {
	OFFLINE,
	IDLE,
	LOADING,
	UP,
	DOWN
};

struct elevator {
	unsigned int num_passengers_onboard;
	unsigned int num_serviced;
	int at_top;
	enum state state;
	int current_floor;
	int current_load;
	struct list_head passenger_list;
	struct task_struct *thread;
	struct mutex mutex;
};

struct floor {
	
	int floor_number;
	int load;
	int offload;

	unsigned int needs_unloading;
	
	
	int passengers_waiting;
	struct list_head passenger_list;
	struct mutex floor_mutex;
	
};

static struct proc_dir_entry* elevator_entry;
static struct elevator my_elevator;
static struct floor floors[MAX_FLOORS];

int start_elevator(void);                                                           
int issue_request(int start_floor, int destination_floor, int type);                
int stop_elevator(void);                                                           
extern int (*STUB_start_elevator)(void);
extern int (*STUB_issue_request)(int,int,int);
extern int (*STUB_stop_elevator)(void);


int start_elevator(void) {
	printk(KERN_INFO "Hello from start_elevator");
	// initialize elevator
	if(my_elevator.state == OFFLINE){
		my_elevator.state = IDLE;
    		my_elevator.current_floor = 1; 
    		my_elevator.current_load = 0;
  		//struct list_head list;  	
       		INIT_LIST_HEAD(&my_elevator.passenger_list);	// initialize linked list of passengers
       		
       		// maybe move back to here
       		
       		for(int i = 0; i < MAX_FLOORS; i++){		// initialize each waiting floor
       			INIT_LIST_HEAD(&floors[i].passenger_list);
       			floors[i].needs_unloading = 0;		
       			floors[i].offload = 0;
    		}
    		
    		
    		return 0;
	}
	else			// if elevator already running	
		return 1;
}


int issue_request(int start_floor, int destination_floor, int type) {
	printk("Hello from issue_request");
	
	if(mutex_lock_interruptible(&my_elevator.mutex) == 0){
		// check for elevator logic
		if (start_floor < 1 || start_floor > MAX_FLOORS ||
		destination_floor < 1 || destination_floor > MAX_FLOORS ||
		type < 0 || type > 3)
			return 1;  
			
		//INIT_LIST_HEAD(&floors[start_floor-1].passenger_list);		// initializes a waiting passenger
		/*
		// line 92, moved here... ****
		for(int i = 0; i < MAX_FLOORS; i++){				// initialize each waiting floor
	       			INIT_LIST_HEAD(&floors[i].passenger_list);
	       			floors[i].needs_unloading = 0;
	    	}
	    	*/
	    			
		
		
			
		// queue elevator for pick up
		floors[start_floor - 1].load++;
		//floors[destination_floor - 1].offload++; // edit
		
		floors[start_floor - 1].passengers_waiting++; // update floors passengers_waiting	
			
		
		
		
		// initialize passenger ptr	
	       	struct passenger *psgr = kmalloc(sizeof(struct passenger), GFP_KERNEL);
	       	
		//if (!psgr)
		//	return -ENOMEM;
		psgr->type = type;
	    	psgr->destination = destination_floor;
	    	psgr->start = start_floor;
	    	switch (type) {
			case P:
				psgr->weight  = 10;
				break;
		
			case L:
				psgr->weight = 15;
				break;
				
			case B:
				psgr->weight = 20;
				break;
				
			case V:
				psgr->weight = 5;
				break; 	
		}
		
	    	list_add_tail(&psgr->list, &floors[start_floor - 1].passenger_list); 	// update  waiting passenger_list
	    	printk(KERN_INFO "type: %d, start: %d, dest:%d\n", psgr->type, psgr->start, psgr->destination);
	    	
		mutex_unlock(&my_elevator.mutex);
		return 0;
	}
	else
		return 1;
	
    	
}

// NEEDS FIXING
int stop_elevator(void) {
	printk(KERN_INFO "Hello from stop_elevator");
	if (mutex_lock_interruptible(&my_elevator.mutex) == 0){
		// free passenger ptr memory
		struct passenger *passenger_ptr, *tmp;
		list_for_each_entry_safe(passenger_ptr, tmp, &my_elevator.passenger_list, list) {
			list_del(&passenger_ptr->list);
			kfree(passenger_ptr);
		}

		kfree(&my_elevator.passenger_list);
		
		my_elevator.state = OFFLINE;
		my_elevator.current_floor = 0;
		mutex_unlock(&my_elevator.mutex);
	    	return 0;
    	}
    	return 1;			// supposed to return 1 if already in process of stopping
}

static int elevator_run(void *data) {
	printk(KERN_INFO "Hello from elevator_run");
	// sys call
	// start_elevator();
	ssleep(5);
	my_elevator.state = OFFLINE;
	
	// kthread
	while (!kthread_should_stop()) {
		if(mutex_lock_interruptible(&my_elevator.mutex) == 0){
	 		// check state
			switch (my_elevator.state) {
			
				case OFFLINE:
					break;		
				case IDLE:	
					ssleep(1);
					
					for(int i = 0; i < MAX_FLOORS - 1; i++){		
						if(floors[i].passengers_waiting > 0)//{	// check if there are waiting passengers
							my_elevator.state = UP;
						//else
							//my_elevator.state = IDLE;
					}	
					
					break;
						
				case UP:
					ssleep(2);
					if (my_elevator.current_floor != MAX_FLOORS){		// turn around elevator
						my_elevator.at_top = 0;
						my_elevator.current_floor++;
						
						int floor = my_elevator.current_floor - 1;
						if(floors[floor].needs_unloading > 0){
							my_elevator.state = LOADING;
							break;
						}	
						else if(floors[floor].passengers_waiting > 0) { // edited to eif 	// check if there are passengers on current floor to load
							my_elevator.state = LOADING;
							break;	
						}
						else{
							my_elevator.state = UP;
							break;
						}		
					}
					else {
						my_elevator.at_top = 1;
						int floor = my_elevator.current_floor - 1;
						if(floors[floor].needs_unloading > 0){
							my_elevator.state = LOADING;
							break;
						}	
						else if (floors[floor].passengers_waiting > 0) { 	// check if there are passengers on current floor to load
							my_elevator.state = LOADING;
							break;
						}
						else { 
							my_elevator.state = DOWN;
							break;
						}
					}
					break;
				case DOWN:
					ssleep(2);
					if (my_elevator.current_floor != 1){			// turn around elevator
						my_elevator.current_floor--;
						
						int floor = my_elevator.current_floor - 1;
						if (floors[floor].needs_unloading > 0){	// passengers getting unloaded
							my_elevator.state = LOADING;
							break;
						}	
						else if (floors[floor].passengers_waiting > 0) { 	// check if there are passengers on current floor to load
							my_elevator.state = LOADING;
							break;
						}
						else {
							my_elevator.state = DOWN;
							break;	
						}	
					}
					else {	
						int floor = my_elevator.current_floor - 1;
						if (floors[floor].needs_unloading > 0){	// passengers getting unloaded
							my_elevator.state = LOADING;
							break;
						}	
						else if (floors[floor].passengers_waiting > 0) { 	// check if there are passengers on current floor to load
							my_elevator.state = LOADING;
							break;
						}
						else {
							my_elevator.state = UP;
							break;	
						}
					}
					break;
				case LOADING:	
					// not yet implemented
					
					ssleep(1);
					int floor = my_elevator.current_floor - 1;
					//if(mutex_lock_interruptible(&floors[floor].floor_mutex) == 0){
					
						// **UNLOAD**
						
					if (floors[floor].needs_unloading == 1 && floors[floor].offload != 0) { 	// check current floor for
												// unloading first
						if (mutex_lock_interruptible(&floors[floor].floor_mutex) == 0){					
												
						// loop through for the amount of people with the destination of current floor						
												
												
							struct passenger *passenger_ptr, *next_passenger_ptr;
							for(int i = 0; i < floors[floor].offload; i++){
								list_for_each_entry_safe(passenger_ptr, next_passenger_ptr, 
									&my_elevator.passenger_list, list) {
									if(passenger_ptr->destination == my_elevator.current_floor){
										
										switch (passenger_ptr->type) {
											case P:
												my_elevator.current_load -= 10;
												break;
											
											case L:
												my_elevator.current_load -= 15;
												break;
													
											case B:
												my_elevator.current_load -=  20;
												break;
													
											case V:
												my_elevator.current_load -=  5;
												break; 	
										}
										
										// free memory
										list_del(&passenger_ptr->list);
										kfree(passenger_ptr);
										
										// update elevator lists
										my_elevator.num_passengers_onboard--;
										my_elevator.num_serviced++; 
										
										// maybe move up?
									// remove from floor lists
										floors[floor].offload--;
										floors[floor].needs_unloading = 0;	
							    			
										
									}
									/*
									// maybe move up?
									// remove from floor lists
										floors[floor].offload--;
										floors[floor].needs_unloading = 0;	
										*/
									/*
									// free memory
									list_del(&passenger_ptr->list);
									kfree(passenger_ptr);
									*/
									
									/*		
									mutex_unlock(&floors[floor].floor_mutex);
									*/													
									
								}
							}
							
							mutex_unlock(&floors[floor].floor_mutex);
						} 
						//mutex_unlock(&floors[floor].floor_mutex);
					}// end **UNLOAD
						// check for loading on-board
					if (my_elevator.current_load < MAX_WEIGHT) {						
						if(mutex_lock_interruptible(&floors[floor].floor_mutex) == 0){
							struct passenger *passenger_ptr, *next_passenger_ptr;
							list_for_each_entry_safe(passenger_ptr, next_passenger_ptr, 
								&floors[floor].passenger_list, list) {
									
								switch (passenger_ptr->type) {
									case P:
										my_elevator.current_load += 10;
										break;
									
									case L:
										my_elevator.current_load += 15;
										break;
										
									case B:
										my_elevator.current_load +=  20;
										break;
											
									case V:
										my_elevator.current_load +=  5;
										break; 	
								}
							
									
								// free memory
								list_del(&passenger_ptr->list);
									
								// remove from floor list
							
								floors[floor].passengers_waiting--;									
									
								// mark destination
								floors[passenger_ptr->destination - 1].offload++;	// check the -1
						    			
								// take the passenger from floors (waiting) and append to elevator list
								list_add_tail(&passenger_ptr->list, &my_elevator.passenger_list);
								//floors[floor].needs_unloading = 1;
								floors[floor].needs_unloading = 1;
								my_elevator.num_passengers_onboard++;
								
							}
							mutex_unlock(&floors[floor].floor_mutex);
						}	
								
					}//end of on-board
						
						
					// if statements for which direction to exit
					if(my_elevator.at_top == 1){
						my_elevator.state = DOWN;
						break;
					}
					else{
						my_elevator.state = UP;
						break;
					}
						
					break;
					
				}//end of switch
				
				mutex_unlock(&my_elevator.mutex);
		}
	
	}//end of while
	return 0;
}
	
static ssize_t elevator_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) {
    	//char buf[256];
    	char buf[900];
    	int len = 0;
    	
    	switch (my_elevator.state) {
        case OFFLINE:
            len += snprintf(buf + len, sizeof(buf) - len, "Elevator state: OFFLINE\n");
            break;
        case IDLE:
            len += snprintf(buf + len, sizeof(buf) - len, "Elevator state: IDLE\n");
            break;
        case LOADING:
            len += snprintf(buf + len, sizeof(buf) - len, "Elevator state: LOADING\n");
            break;
        case UP:
            len += snprintf(buf + len, sizeof(buf) - len, "Elevator state: UP\n");
            break;
        case DOWN:
            len += snprintf(buf + len, sizeof(buf) - len, "Elevator state: DOWN\n");
            break;
        default:
            len += snprintf(buf + len, sizeof(buf) - len, "Elevator state: UNKNOWN\n");
            break;
    	}

    	len += snprintf(buf + len, sizeof(buf) - len, "Current floor: %d\n", my_elevator.current_floor);  	

    	len += snprintf(buf + len, sizeof(buf) - len, "Current load: %d lbs\n", my_elevator.current_load);
  	
  	if(my_elevator.current_load > 0){
  		len += snprintf(buf + len, sizeof(buf) - len, "Elevator status: ");
	  	struct passenger *passenger_ptr;
		list_for_each_entry(passenger_ptr, &my_elevator.passenger_list, list){
		  	switch(passenger_ptr->type){
		  		case 0:
					len += snprintf(buf + len, sizeof(buf) - len, "P%d ", passenger_ptr->destination);
					break;
				case 1:
					len += snprintf(buf + len, sizeof(buf) - len, "L%d ", passenger_ptr->destination);
					break;
				case 2:
					len += snprintf(buf + len, sizeof(buf) - len, "B%d ", passenger_ptr->destination);
					break;
				case 3:
					len += snprintf(buf + len, sizeof(buf) - len, "V%d ", passenger_ptr->destination);
					break;							
			}		
		}
	}
	
	len += snprintf(buf + len, sizeof(buf) - len, "\n");
	
	
	
	
	
	if (my_elevator.state != OFFLINE) {
    		for (int i = 4; i >= 0; i--) {
			int num_wait = 0;
			
			// Print information about the floor
			if (my_elevator.current_floor == i+1) {
			    len += snprintf(buf + len, sizeof(buf) - len, "[*] Floor %d: %d ", i + 1,
			    	floors[i].passengers_waiting);
			} 
			else {
    				len += snprintf(buf + len, sizeof(buf) - len, "[ ] Floor %d: %d ", i + 1,
    					floors[i].passengers_waiting);
			}

					
			struct passenger *psgr;
			// Count the number of passengers waiting on the current floor
			list_for_each_entry(psgr, &floors[i].passenger_list, list) {
		    		num_wait++;
		    		
			    		switch (psgr->type){
					case 0: 
						len += snprintf(buf + len, sizeof(buf) - len, "P%d ", psgr->destination);
						break;
					case 1: 
						len += snprintf(buf + len, sizeof(buf) - len, "L%d ", psgr->destination);
						break;
					case 2: 
						len += snprintf(buf + len, sizeof(buf) - len, "B%d ", psgr->destination);
						break;	
					case 3: 
						len += snprintf(buf + len, sizeof(buf) - len, "V%d ", psgr->destination);
						break;	
					default:
						break;	
					}
				
			}
			len += snprintf(buf + len, sizeof(buf) - len, "\n");
			
				
	    	}
	    	
	    	int num_waiting = 0;
			for(int i = 0; i < MAX_FLOORS; i++){
				num_waiting += floors[i].passengers_waiting;
			}
			
			// number of people in elevator	
			len += snprintf(buf + len, sizeof(buf) - len, "\nNumber of passengers: %d\n",
				my_elevator.num_passengers_onboard);
			// number of people waiting on floors	
			len += snprintf(buf + len, sizeof(buf) - len, "Number of passengers waiting: %d\n", num_waiting);
			
		// something wrong here	
			// number of people that have been serviced
			len += snprintf(buf + len, sizeof(buf) - len, "Number of passengers serviced: %d\n",
				 my_elevator.num_serviced);
	}
	


    	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct proc_ops elevator_fops = {
    	.proc_read = elevator_read
};

static int __init elevator_init(void) {

	printk(KERN_INFO "hello from elevator_init\n");
    	STUB_start_elevator = start_elevator;
	STUB_issue_request = issue_request;
	STUB_stop_elevator = stop_elevator;
	
	// create elevator thread
    	my_elevator.thread = kthread_run(elevator_run, NULL, "elevator_thread");
    	
    	// initialize mutexes
    	mutex_init(&my_elevator.mutex);
    	for(int i = 0; i < MAX_FLOORS; i++)
    		mutex_init(&floors[i].floor_mutex);   		
    		
    	// initialze floor passenger lists
    	for(int i = 0 ; i < MAX_FLOORS; i++)
	    	INIT_LIST_HEAD(&floors[i].passenger_list);
	    		
    	
    	if (IS_ERR(my_elevator.thread)) {
        	printk(KERN_ERR "Failed to create elevator thread\n");
        	return PTR_ERR(my_elevator.thread);
    	}
    	printk(KERN_INFO "created elevator thread\n");
	
	// create proc entry
    	elevator_entry = proc_create(ENTRY_NAME, PERMS, PARENT, &elevator_fops);
    	if (!elevator_entry) {
        	printk(KERN_ERR "Failed to create /proc/elevator entry\n");
        	return -ENOMEM;
    	}
    	printk(KERN_INFO "created proc entry\n");
    	return 0;
}

static void __exit elevator_exit(void) {
    	STUB_start_elevator = NULL;
	STUB_issue_request = NULL;
	STUB_stop_elevator = NULL;
	
	for (int i = 0; i < MAX_FLOORS; ++i) {
        struct passenger *passenger_ptr, *tmp;
        list_for_each_entry_safe(passenger_ptr, tmp, &floors[i].passenger_list, list) {
            list_del(&passenger_ptr->list);
            kfree(passenger_ptr);
            
        }
        
        // remove mutexes
        mutex_destroy(&my_elevator.mutex);
        for(int i = 0; i < MAX_FLOORS; i++)
        	mutex_destroy(&floors[i].floor_mutex);
        	
        	
        

        // Free the passenger list head for each floor
        kfree(&floors[i].passenger_list);
    }
}

module_init(elevator_init);  // Specify the initialization function
module_exit(elevator_exit);  // Specify the exit/cleanup function
