#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/string.h>

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
#define V 0
#define P 1
#define L 2
#define B 3

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
	LOADINGUP,
	LOADINGDOWN,
	UP,
	DOWN
};

struct elevator {
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
    	
    		return 0;
	}
	else			// if elevator already running	
		return 1;
}


int issue_request(int start_floor, int destination_floor, int type) {
	printk("Hello from issue_request");
	// check for elevator logic
	if (start_floor < 1 || start_floor > MAX_FLOORS ||
        destination_floor < 1 || destination_floor > MAX_FLOORS ||
        type < 0 || type > 3)
        	return 1;  
        	
        // cue elevator for pick up
        floors[start_floor - 1].load++;
        floors[destination_floor - 1].offload++;      
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
        
    	list_add_tail(&psgr->list, &my_elevator.passenger_list); // update passenger_list

    	return 0;
}

int stop_elevator(void) {
	printk(KERN_INFO "Hello from stop_elevator");
	// free passenger ptr memory
	struct passenger *passenger_ptr, *tmp;
	list_for_each_entry_safe(passenger_ptr, tmp, &my_elevator.passenger_list, list) {
		list_del(&passenger_ptr->list);
		kfree(passenger_ptr);
	}

	kfree(&my_elevator.passenger_list);

	my_elevator.state = OFFLINE;
    	return 0;
}

static int elevator_run(void *data) {
	printk(KERN_INFO "Hello from elevator_run");
	// sys call
	// start_elevator();
	ssleep(5);
	my_elevator.state = OFFLINE;
	
	// kthread
	while (!kthread_should_stop()) {
 		// check state
		switch (my_elevator.state) {
		
		case OFFLINE:
			break;
						
        	case IDLE:
        		// start
        		ssleep(1);
			my_elevator.state = LOADINGUP;	
			break;
		
		case LOADINGUP:
			ssleep(1);
			int floor = my_elevator.current_floor - 1;
			if (floors[floor].load > 0) { 	// check floor to load
				if (my_elevator.current_load < MAX_WEIGHT) { 	// check for weight
					struct passenger *passenger_ptr;
					list_for_each_entry(passenger_ptr, &my_elevator.passenger_list, list) {
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
					}
					
					// remove from floor list
					floors[floor].passengers_waiting--;
                    			// free memory
					list_del(&passenger_ptr->list);
				}
			}
			if (floors[floor].offload > 0) { // check floor to offload
				struct passenger *passenger_ptr;
				list_for_each_entry(passenger_ptr, &my_elevator.passenger_list, list) {
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
				}
					
				// add to floor list
				floors[floor].passengers_waiting++;
				// free memory
				list_del(&passenger_ptr->list);
				kfree(passenger_ptr);

				
				// remove from elevator list
				// remove from elevator load
				// add to floor list		
			}
			
			my_elevator.state = UP;
			
			break;
			
		case UP:
			my_elevator.current_floor++;
			ssleep(2);
			if (my_elevator.current_floor != MAX_FLOORS)
				my_elevator.state = LOADINGUP;
			else
				my_elevator.state = LOADINGDOWN;
			break;

		case LOADINGDOWN:
			ssleep(1);
			if (floors[floor].load > 0) { 	// check floor to load
				if (my_elevator.current_load < MAX_WEIGHT) { 	// check for weight
					struct passenger *passenger_ptr;
					list_for_each_entry(passenger_ptr, &my_elevator.passenger_list, list) {
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
					}
					
					// remove from floor list
					floors[floor].passengers_waiting--;
                    			// free memory
					list_del(&passenger_ptr->list);
				}
			}
			if (floors[floor].offload > 0) { // check floor to offload
				struct passenger *passenger_ptr;
				list_for_each_entry(passenger_ptr, &my_elevator.passenger_list, list) {
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
						
					// add to floor list
					floors[floor].passengers_waiting++;
					// free memory
					list_del(&passenger_ptr->list);
					kfree(passenger_ptr);	
				}
			}
			my_elevator.state = DOWN;
			
			break;
		
		case DOWN:
			ssleep(2);
			my_elevator.current_floor--;
			if (my_elevator.current_floor != 1)
				my_elevator.state = LOADINGDOWN;
			else
				my_elevator.state = LOADINGUP;
			break;
		}
		return 0;
	}
	return 0;
}
	
static ssize_t elevator_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) {
    	char buf[256];
    	int len = 0;
    	
    	switch (my_elevator.state) {
        case OFFLINE:
            len += snprintf(buf + len, sizeof(buf) - len, "Elevator state: OFFLINE\n");
            break;
        case IDLE:
            len += snprintf(buf + len, sizeof(buf) - len, "Elevator state: IDLE\n");
            break;
        case LOADINGUP:
            len += snprintf(buf + len, sizeof(buf) - len, "Elevator state: LOADING\n");
            break;
        case LOADINGDOWN:
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
  
	len += snprintf(buf + len, sizeof(buf) - len, "Elevator status: ");
	if (my_elevator.current_load > 0) {
	    struct passenger *passenger_ptr;
	    list_for_each_entry(passenger_ptr, &my_elevator.passenger_list, list) {
	    	switch (passenger_ptr->type) {
		case P:
			len += snprintf(buf + len, sizeof(buf) - len, "P%d ", passenger_ptr->destination);
			break;
		case L:
			len += snprintf(buf + len, sizeof(buf) - len, "L%d ", passenger_ptr->destination);
			break;
		case B:
			len += snprintf(buf + len, sizeof(buf) - len, "B%d ", passenger_ptr->destination);
			break;
		case V:
			len += snprintf(buf + len, sizeof(buf) - len, "V%d ", passenger_ptr->destination);
			break;
		default:
			len += snprintf(buf + len, sizeof(buf) - len, "UNKNOWN");
			break;
		}
		
		len += snprintf(buf + len, sizeof(buf) - len, "\n");
	    }
	}
 /*
	printk("Accessing all floors");
    	for (int i = 0; i < MAX_FLOORS; ++i) {
        	mutex_lock(&floors[i].floor_mutex);
        	len += snprintf(buf + len, sizeof(buf) - len, "[%s] Floor %d: %d", (my_elevator.current_floor == floors[i].floor_number) ? "*" : " ", floors[i].floor_number, floors[i].passengers_waiting);
        	struct passenger *floor_passenger_ptr;
        	list_for_each_entry(floor_passenger_ptr, &floors[i].passenger_list, list) {
            		len += snprintf(buf + len, sizeof(buf) - len, " %c%d", floor_passenger_ptr->type, floor_passenger_ptr->destination);
        	}
        	len += snprintf(buf + len, sizeof(buf) - len, "\n");
        	mutex_unlock(&floors[i].floor_mutex);
    	}  
    	*/
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

        // Free the passenger list head for each floor
        kfree(&floors[i].passenger_list);
    }
}

module_init(elevator_init);  // Specify the initialization function
module_exit(elevator_exit);  // Specify the exit/cleanup function
