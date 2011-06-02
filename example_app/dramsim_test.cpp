/****************************************************************************
*	 DRAMSim2: A Cycle Accurate DRAM simulator 
*	 
*	 Copyright (C) 2010   	Elliott Cooper-Balis
*				Paul Rosenfeld 
*				Bruce Jacob
*				University of Maryland
*
*	 This program is free software: you can redistribute it and/or modify
*	 it under the terms of the GNU General Public License as published by
*	 the Free Software Foundation, either version 3 of the License, or
*	 (at your option) any later version.
*
*	 This program is distributed in the hope that it will be useful,
*	 but WITHOUT ANY WARRANTY; without even the implied warranty of
*	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	 GNU General Public License for more details.
*
*	 You should have received a copy of the GNU General Public License
*	 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*****************************************************************************/



#include "dramsim_test.h"

using namespace DRAMSim;

/* callback functors */
some_object::some_object (){

}

void some_object::read_complete(uint id, uint64_t address, uint64_t clock_cycle, uint32_t transId)
{
	printf("[Callback] read complete: 0x%lx  ", address );
	cout <<" cycle: "<<clock_cycle<<" transaction id: " << transId <<endl;
}


void some_object::write_complete(uint id, uint64_t address, uint64_t clock_cycle,uint32_t transId)
{
	printf("[Callback] write complete: 0x%lx ", address);
	cout <<" cycle: "<<clock_cycle<<" transaction id: " << transId <<endl;
}

/* FIXME: this may be broken, currently */
void power_callback(double a, double b, double c, double d)
{
	printf("power callback: %0.3f, %0.3f, %0.3f, %0.3f\n",a,b,c,d);
}

int some_object::add_one_and_run()
{
	/* pick a DRAM part to simulate */
	int transid=0;
	MemorySystem mem(0, "ini/DDR3_micron_32M_8B_x4_sg125.ini", "system.ini", "../", "dean",1024); 

	/* create and register our callback functions */
	Callback_t *read_cb= new Callback<some_object, void, uint, uint64_t, uint64_t,uint32_t>(this, &some_object::read_complete);
	Callback_t *write_cb= new Callback<some_object, void, uint, uint64_t, uint64_t,uint32_t>(this, &some_object::write_complete);
	mem.RegisterCallbacks(read_cb, write_cb);

	/* create a transaction and add it */
	Transaction tr = Transaction(DATA_READ, 0x50000, NULL,transid++);
	mem.addTransaction(tr);

	/* do a bunch of updates (i.e. clocks) -- at some point the callback will fire */
	for (int i=0; i<10; i++)
	{
	        Transaction tr = Transaction(DATA_READ, 0x50000+(0x00001)*i, NULL,transid++);
		mem.addTransaction(tr);
	}
	for (int i=0; i<10; i++)
	{
	        Transaction tr2 = Transaction(DATA_WRITE, 0x40000+(0x00001)*i*500, NULL,transid++);
	        Transaction tr3 = Transaction(DATA_WRITE, 0x90000, NULL,transid++);
		mem.addTransaction(tr2);
		mem.addTransaction(tr3);
	}
	for (int i=0; i<400; i++)
	{
		mem.update();
	}	

	/* add another some time in the future */
	Transaction tw = Transaction(DATA_WRITE, 0x90012, NULL,transid++);
	mem.addTransaction(tw);

	for (int i=0; i<45; i++)
	{
		mem.update();
	}

	/* get a nice summary of this epoch */
	mem.printStats();

	return 0;
}

int main()
{
	printf("dramsim_test main()\n");
	some_object obj;
	obj.add_one_and_run();
}
