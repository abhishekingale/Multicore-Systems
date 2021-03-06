/*
// File: Assignment_1.cpp
//              
// Framework to implement Task 1 of the Advances in Computer Architecture lab 
// session. This uses the ACA 2009 library to interface with tracefiles which
// will drive the read/write requests
// 
//
// Extending the code from Task 1, you are to simulate a multiprocessing system with a shared memory
// architecture. Your simulation should be able to support multiple processors, all working in parallel.
// Each of the processors is associated with a local cache, and all cache modules are connected to a
// single bus. A main memory is also connected to the bus, where all the correct data is supposed to
// be stored, although, again, the memory and its contents does not have to be simulated. Assume a
// pipelined memory which can handle a request from each processor in parallel with the same latency.
// In order to make cache data coherent within the system, implement a bus snooping protocol. This
// means that each cache controller connected to the bus receives every memory request on the bus made
// by other caches, and makes the proper modification on its local cache line state. In this task, you
// should implement the simplest VALID-INVALID protocol. The cache line state transition diagram for
// this protocol is shown in Figure 1.
//
//
*/

#include "aca2009.h"
#include <systemc.h>
#include <iostream>

using namespace std;


#define COMMENTS 0
static const unsigned int TAG_MASK = 0xFFFFF000;
static const unsigned int SET_MASK = 0xFE0;
static const unsigned int OFFSET_MASK = 0x1F;

static const int MEM_SIZE = 32768;
static const int BLOCK_SIZE = 32;
static const int TOTAL_WAYS	= 8;
static const int TOTAL_SETS	= MEM_SIZE / (BLOCK_SIZE * TOTAL_WAYS);

typedef struct 
    { 
        unsigned int tag_field;
		unsigned int valid_flag;
		unsigned int word_data[8];
    } set_lines;
 
 
typedef struct 
    {
        unsigned int lru_counter; 
        set_lines line[TOTAL_WAYS]; 

		void cache_sets()
		{
			for(int i=0; i<TOTAL_WAYS;i++)
				line[i].valid_flag=0;
			
		}
    } cache_sets;
/*	
typedef struct 
    {
		cache_sets m_data2[TOTAL_SETS];
		
		unsigned int p_process_read_request(int local_id, unsigned int read_addr);
    	void p_process_write_request(int local_id, unsigned int write_addr);	
		int p_select_lru_line(int set_no);
		void p_update_lru_counter(unsigned int set_no, unsigned int line_no);		
		inline void p_invalidate_way(unsigned int invalidate_addr);	
		
    } cache_def;	
*/



	
SC_MODULE(Cache) 
{

    public:
    	enum Function 
    	{
            FUNC_READ,
            FUNC_WRITE,
        };

    	enum RetCode 
    	{
            RET_READ_DONE,
            RET_WRITE_DONE,
    	};

/*	
    	enum Bus_Function 
        {
	    	BUS_Wr,			
	    	BUS_Rd,
	    	BUS_Rdx,
	    	BUS_Rst,
        };
*/	

    	sc_in<bool>     Port_CLK;
    	sc_in<Function> Port_Func;
    	sc_in<int>      Port_Addr;
    	sc_out<RetCode> Port_Done;
    	sc_inout_rv<32> Port_Data;
/*
		sc_port<Bus_if> Port_Bus;
    	sc_in<int>      Port_BusOwner;	
    	sc_inout_rv<32>	Port_BusAddr;
    	sc_in<Bus_Function> Port_BusValid;
*/
    	unsigned int local_id=0;

		//auxiliaryy functions
    	unsigned int process_read_request(unsigned int local_id, unsigned int read_addr);
    	void process_write_request(unsigned int local_id, unsigned int write_addr);	
		int select_lru_line(int set_no);
		void update_lru_counter(unsigned int set_no, unsigned int line_no);		
		//inline void invalidate_way(unsigned int invalidate_addr);		

    	SC_CTOR(Cache) 
    	{
			m_data = new cache_sets[TOTAL_SETS];
            //SC_THREAD(bus_controller);
            SC_THREAD(execute);
		
            sensitive << Port_CLK.pos();
            dont_initialize();		         
	    
        }		

        ~Cache() 
        {
            delete[] m_data;
        }

    private:

    	cache_sets* m_data;
		//cache_def cache_i;
		unsigned int bus_rx_addr, addr, temp;
		unsigned int sender_cache_id;
		unsigned long long int bus_access_write;
		unsigned long long int bus_access_read;

/*		
    	void bus_controller() 
    	{

            while(true)
            {				
				wait(Port_BusValid.value_changed_event());
	    		sender_cache_id = Port_BusOwner.read();
		
				if (sender_cache_id != local_id)
				{ 
	    			bus_rx_addr = Port_BusAddr.read().to_int();
	
            		switch(Port_BusValid.read())
            		{
						case BUS_Rdx:
						case BUS_Wr:
									if (sender_cache_id != local_id)
									{ invalidate_way(bus_rx_addr);
									
									if(COMMENTS == 2)
										cout<<"cahce "<<local_id<<" invalidates address "<<bus_rx_addr<<"\n";
										
									}
									break;
									
		 				case BUS_Rd: //Do nothing?
									// check for data sharing?
									break;
										
						default:	//reset state. nothing to do.
									break;
 
						
					} //switch-statements

				wait();

				}//if()
					
			}//while()
				
		} //bus_controller()
*/	
		
		void execute() 
		{
			while (true)
			{
				wait(Port_Func.value_changed_event());
				
				Function f = Port_Func.read();
				addr   = Port_Addr.read();				
								
				if (f == FUNC_READ) 
				{		
					if(COMMENTS == 3)
						cout<<"cahce "<<local_id<<" received read at address "<<addr<<"\n";			
					temp = process_read_request(local_id, addr);
					Port_Data.write( temp );
					Port_Done.write( RET_READ_DONE );
					wait();
					Port_Data.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
				}
				else
				{	
					if(COMMENTS == 3)
						cout<<"cahce "<<local_id<<" received write at address "<<addr<<"\n";				
					process_write_request(local_id, addr);
					Port_Done.write( RET_WRITE_DONE );
				}
			}//end of while
			
		}//end of execute

};//end of cache


	unsigned int Cache::process_read_request(unsigned int local_id, unsigned int read_addr)
	{
		int idx, tag_in_memory =0, line_to_replace; 
	    unsigned int read_data, tag_val, set_no, byte_in_line;


	    tag_val = (read_addr & TAG_MASK) >> 12;
	    set_no = (read_addr & SET_MASK) >> 5;
	    byte_in_line = (read_addr & OFFSET_MASK) >> 2;		
		

		for (idx=0; idx < TOTAL_WAYS; idx++)
		{
			if ((m_data[set_no].line[idx].tag_field == tag_val)  && (tag_in_memory == 0))
			{
				if ( m_data[set_no].line[idx].valid_flag == 1)
				{
					tag_in_memory = 1;
					read_data = m_data[set_no].line[idx].word_data[byte_in_line];
					update_lru_counter(set_no, idx);
					stats_readhit(local_id);

				if(COMMENTS == 3)
				{cout<<"cache "<<local_id<<"got read hit at address"<<read_addr;}

					
					wait();
					break;	
				}
				else
				{
					tag_in_memory = 1;
					//Port_Bus->read(local_id, read_addr);					
					wait(100);
					line_to_replace = select_lru_line(set_no) ;	
					m_data[set_no].line[line_to_replace].valid_flag = 1;
					m_data[set_no].line[line_to_replace].tag_field = tag_val;
					read_data = rand();
					m_data[set_no].line[idx].word_data[byte_in_line] = read_data;								
					update_lru_counter(set_no, idx);
					stats_readmiss(local_id);

					if(COMMENTS == 3)
					{cout<<"cache "<<local_id<<"got read miss (dirty data) at address"<<read_addr;}
					
					break;
				}
			}
		}
		
		if (tag_in_memory == 0)
		{
			tag_in_memory = 1;
		    //Port_Bus->read(local_id, read_addr);

		    wait(100);
		    line_to_replace = select_lru_line(set_no) ;	
		    m_data[set_no].line[line_to_replace].valid_flag = 1;
		    m_data[set_no].line[line_to_replace].tag_field = tag_val;
		    read_data = rand();		    
		    m_data[set_no].line[line_to_replace].word_data[byte_in_line] = read_data;
		    update_lru_counter(set_no, line_to_replace);
			stats_readmiss(local_id);

			if(COMMENTS == 3)
			{cout<<"cache "<<local_id<<"got read miss (invalid tag) at address"<<read_addr;}			
		}
		
		return read_data;
		
	}


	void Cache::process_write_request(unsigned int local_id, unsigned int write_addr)
	{		
		int idx = 0, tag_in_memory = 0, line_to_replace, data= 0;
		unsigned int tag_val, set_no, byte_in_line;

	    
		tag_val = (write_addr & TAG_MASK) >> 12;
	    set_no = (write_addr & SET_MASK) >> 5;
	    byte_in_line = (write_addr & OFFSET_MASK) >> 2;	


		for (idx =0; idx < TOTAL_WAYS; idx++)
		{
		    if ((m_data[set_no].line[idx].tag_field == tag_val)  && (tag_in_memory == 0) && (m_data[set_no].line[idx].valid_flag == 1))
		    {
				tag_in_memory = 1;
				data= rand();
				//Port_Bus->write(local_id, write_addr, data);
				//Port_Bus->lock();								
				m_data[set_no].line[idx].valid_flag = 1;
				m_data[set_no].line[idx].word_data[byte_in_line] = data;							
				update_lru_counter(set_no, idx);
				stats_writehit(local_id);

			if(COMMENTS == 3)
			{cout<<"cache "<<local_id<<"got write hit at address"<<write_addr;}
				
				wait();
				break ;
		    }
		}
		if (tag_in_memory == 0)
		{
			data = rand();
		    //Port_Bus->read_exclusive(local_id, write_addr, data); 
		    
		    //Port_Bus->lock();
		    wait(100);

		    line_to_replace = select_lru_line(set_no);
		    m_data[set_no].line[line_to_replace].valid_flag = 1;
		    m_data[set_no].line[line_to_replace].tag_field = tag_val;
			m_data[set_no].line[line_to_replace].word_data[byte_in_line] = data;		    
		    update_lru_counter(set_no, line_to_replace);
			stats_writemiss(local_id);
			
			if(COMMENTS == 3)
			{cout<<"cache "<<local_id<<"got write miss at address"<<write_addr;}
			
		}
		
		wait(100);	
		//Port_Bus->unlock();		
		
	}



	int Cache::select_lru_line(int set_no)
	{
	
    	int curr_lru_tree = m_data[set_no].lru_counter;
    	
    	if( (curr_lru_tree & 0b0001011 ) == 0b0000000)
    	{
            return 0;
    	} 
		else if( (curr_lru_tree & 0b0001011 ) == 0b0001000 ) 
    	{
            return 1;
    	} 
		else if ( (curr_lru_tree & 0b0010011 ) == 0b0000010 )
    	{
            return 2;
		} 
		else if ( (curr_lru_tree & 0b0010011 ) == 0b0010010 )
    	{
            return 3;
    	} 
		else if ( (curr_lru_tree & 0b0100101 ) == 0b0000001 )
    	{
            return 4;
    	} 
		else if ( (curr_lru_tree & 0b0100101 ) == 0b0100001 ) 
    	{
            return 5;
    	}
		else if ( (curr_lru_tree & 0b1000101 ) == 0b0000101 )
    	{
            return 6;
		} 
    	else 
		{
            return 7;  
    	}

			
	}
	
	
	void Cache::update_lru_counter(unsigned int set_no, unsigned int line_no)
	{
		
        if ( line_no == 0 ) 
		{
            m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0b0001011; 
        }
        else if ( line_no == 1 )
		{ 
            m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0b0000011; 
            m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0b1110111; 
        }
        else if ( line_no == 2 )
		{ 
            m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0b0010001; 
            m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0b1111101; 
        }
		else if ( line_no == 3 )
        { 
            m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0b0000001; 
            m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0b1101101;
        }
        else if( line_no ==4) 
		{
            m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0b0100100; 
            m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0b1111110; 
        }
        else if ( line_no == 5 ) 
        {      
	    	m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0b0000100;  
            m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0b1011110; 
        }
        else if ( line_no == 6 )
		{ 
            m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0b1000000; 
            m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0b1111010;  
        }
        else 
		{    
            m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0b111010;    
        } 
		
		
	}
	

/////////////////////////////////////////////////////////////////////////////////////////////
/*
	unsigned int cache_def::p_process_read_request(int local_id, unsigned int read_addr)
	{
		int idx; 
	    unsigned int read_data;
	    int tag_in_memory = 0;
		int line_to_replace;
	    unsigned int tag_val = (read_addr & TAG_MASK) >> 12;
	    unsigned int set_no = (read_addr & SET_MASK) >> 5;
	    unsigned int byte_in_line = (read_addr & OFFSET_MASK) >> 2;		
		

		for (idx=0; idx < TOTAL_WAYS; idx++)
		{
			if ((m_data2[set_no].line[idx].tag_field == tag_val)  && (tag_in_memory == 0))
			{
				if ( m_data2[set_no].line[idx].valid_flag == 1)
				{

					stats_readhit(local_id);
					tag_in_memory = 1;
					read_data = m_data2[set_no].line[idx].word_data[byte_in_line];
					p_update_lru_counter(set_no, idx);
					wait();
					break;	
				}
				else
				{

					Port_Bus->read(local_id, read_addr);
					stats_readmiss(local_id);
					wait(100);
					line_to_replace = p_select_lru_line(set_no) ;	
					m_data[set_no].line[line_to_replace].word_data[byte_in_line] = rand();
					m_data[set_no].line[line_to_replace].valid_flag = 1;
					m_data[set_no].line[line_to_replace].tag_field = tag_val;
					read_data = m_data[set_no].line[idx].word_data[byte_in_line];
					tag_in_memory = 1;
			
					p_update_lru_counter(set_no, idx);
					break;
				}
			}
		}
		
		if (tag_in_memory == 0)
		{

		    Port_Bus->read(local_id, read_addr);
		    stats_readmiss(local_id);

		    wait(100);
		    line_to_replace = p_select_lru_line(set_no) ;	
		    m_data2[set_no].line[line_to_replace].word_data[byte_in_line] = rand();
		    m_data2[set_no].line[line_to_replace].valid_flag = 1;
		    m_data2[set_no].line[line_to_replace].tag_field = tag_val;

		    p_update_lru_counter(set_no, line_to_replace);
		    read_data = m_data[set_no].line[line_to_replace].word_data[byte_in_line];
		    tag_in_memory = 1;		
		}
		
		return read_data;
		
	}


	void cache_def::p_process_write_request(int local_id, unsigned int write_addr)
	{
		
		int idx = 0; 
	    int tag_in_memory = 0;
		int line_to_replace;
		int data= rand();
	    unsigned int tag_val = (write_addr & TAG_MASK) >> 12;
	    unsigned int set_no = (write_addr & SET_MASK) >> 5;
	    unsigned int byte_in_line = (write_addr & OFFSET_MASK) >> 2;	
		
		for (idx =0; idx < TOTAL_WAYS; idx++)
		{
		    if ((m_data2[set_no].line[idx].tag_field == tag_val)  && (tag_in_memory == 0) && (m_data2[set_no].line[idx].valid_flag == 1))
		    {

				Port_Bus->write(local_id, write_addr, data);
				Port_Bus->busLock();
				m_data2[set_no].line[idx].word_data[byte_in_line] = data;
				stats_writehit(local_id);
				m_data2[set_no].line[idx].valid_flag = 1;
				p_update_lru_counter(set_no, idx);
				tag_in_memory = 1;
				
				wait();
				break ;
		    }
		}
		if (tag_in_memory == 0)
		{

		    Port_Bus->read_exclusive(local_id, write_addr, data); 
		    stats_writemiss(local_id);
		    Port_Bus->busLock();
		    wait(100);

		    line_to_replace = p_select_lru_line(set_no);
		    m_data2[set_no].line[line_to_replace].word_data[byte_in_line] = rand();
		    m_data2[set_no].line[line_to_replace].valid_flag = 1;
		    m_data2[set_no].line[line_to_replace].tag_field = tag_val;
		    
		   p_ update_lru_counter(set_no, line_to_replace);
	
		}
		wait(100);	
		Port_Bus->busUnlock();		
		
	}

	inline	void cache_def::p_invalidate_way(unsigned int invalidate_addr)
	{
		unsigned int tag = (invalidate_addr & TAG_CHECK) >> 12;
		unsigned int set = (invalidate_addr & SET_CHECK) >> 5;

		for (int i=0; i < TOTAL_WAYS; i++)
		{
			if ( m_data2[set].line[i].valid_flag == 1 )
			{
				if ( m_data2[set].line[i].tag_field == tag)
				{
					m_data2[set].line[i].valid_flag = 0;
				}
			}
		}
	}


*/
/////////////////////////////////////////////////////////////////////////////////////////////

	
SC_MODULE(CPU) 
{

public:
    sc_in<bool>                	Port_CLK;
    sc_in<Cache::RetCode>   	Port_MemDone;
    sc_out<Cache::Function> 	Port_MemFunc;
    sc_out<int>                	Port_MemAddr;
    sc_inout_rv<32>            	Port_MemData;
	
	int cpu_id=0;
	

    SC_CTOR(CPU) 
    {
        SC_THREAD(execute);
        sensitive << Port_CLK.pos();
        dont_initialize();
    }

private:
    void execute() 
    {
        TraceFile::Entry    tr_data;
        Cache::Function  f;
		
        int addr, data;
		
        while(!tracefile_ptr->eof())
        {            
            if(!tracefile_ptr->next(cpu_id, tr_data))
            {
                cerr << "Error reading trace for CPU" << endl;
                break;
            }

	    	addr = tr_data.addr;

            switch(tr_data.type)
            {
                case TraceFile::ENTRY_TYPE_READ:
                    f = Cache::FUNC_READ;
                    break;

                case TraceFile::ENTRY_TYPE_WRITE:
                    f = Cache::FUNC_WRITE;
                    break;

                case TraceFile::ENTRY_TYPE_NOP:
                    break;

                default:
                    cerr << "Error, got invalid data from Trace" << endl;
                    exit(0);
            }

            if(tr_data.type != TraceFile::ENTRY_TYPE_NOP)
            {
                Port_MemAddr.write(addr);
                Port_MemFunc.write(f);

                if (f == Cache::FUNC_WRITE) 
                {
					if(COMMENTS == 1)
					{ cout << sc_time_stamp() << ": CPU " << cpu_id << " sends write\n"; }

                    data = rand();
                    Port_MemData.write(data);
                    wait();
                    Port_MemData.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
                }
                else
                {
					if(COMMENTS == 1)
					{ cout << sc_time_stamp() << ": CPU_" << cpu_id << " sends read" << endl; }
                }

                wait(Port_MemDone.value_changed_event());

               if (f == Cache::FUNC_READ)
                {
                    if(COMMENTS == 1)
                    { cout << sc_time_stamp() << ": CPU "<< cpu_id<<" reads: " << Port_MemData.read() << "\n"; }
                }
            }
            else
            {
                if(COMMENTS == 1)
				{ cout << sc_time_stamp() << ": CPU " << cpu_id << " executes NOP \n" ; }
            }
            
            wait();
        }
        
        
        sc_stop();
		
    }
};


int sc_main(int argc, char* argv[])
{
    try
    {
        init_tracefile(&argc, &argv);

        // Initialize statistics counters
        stats_init();

        // Instantiate Modules
        Cache cache("main_memory");
        CPU    cpu("cpu");

        // Signals
        sc_buffer<Cache::Function> sigMemFunc;
        sc_buffer<Cache::RetCode>  sigMemDone;
        sc_signal<int>              sigMemAddr;
        sc_signal_rv<32>            sigMemData;

        // The clock that will drive the CPU and Memory
        sc_clock clk("clk", sc_time(1, SC_NS));

        // Connecting module ports with signals
        cache.Port_Func(sigMemFunc);
        cache.Port_Addr(sigMemAddr);
        cache.Port_Data(sigMemData);
        cache.Port_Done(sigMemDone);

        cpu.Port_MemFunc(sigMemFunc);
        cpu.Port_MemAddr(sigMemAddr);
        cpu.Port_MemData(sigMemData);
        cpu.Port_MemDone(sigMemDone);

        sc_trace_file *wf = sc_create_vcd_trace_file("group14_assgn01");
        // Dump the desired signals
        sc_trace(wf, clk, "clk");
        sc_trace(wf, sigMemAddr, "sigMemAddr");
        sc_trace(wf, sigMemDone, "sigMemDone");

        cache.Port_CLK(clk);
        cpu.Port_CLK(clk);

        cout << "Running (press CTRL+C to interrupt)... " << endl;


        // Start Simulation
        sc_start();

		sc_close_vcd_trace_file(wf);
		stats_print();		
    } 

    catch (exception& e)
    {
        cerr << e.what() << endl;
    }
    
    return 0;
}