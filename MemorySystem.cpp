/****************************************************************************
*	 DRAMSim2: A Cycle Accurate DRAM simulator 
*	 
*	 Copyright (C) 2010   	Elliott Cooper-Balis
*									Paul Rosenfeld 
*									Bruce Jacob
*									University of Maryland
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


//MemorySystem.cpp
//
//Class file for JEDEC memory system wrapper
//

#include "MemorySystem.h"
#include "IniReader.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h> 
#include <sstream> //stringstream
#include <stdlib.h> // getenv()

using namespace std;

ofstream cmd_verify_out; //used in Rank.cpp and MemoryController.cpp if VERIFICATION_OUTPUT is set

unsigned NUM_DEVICES;
unsigned NUM_RANKS;

namespace DRAMSim {
#ifdef LOG_OUTPUT
ofstream dramsim_log;
#endif

powerCallBack_t MemorySystem::ReportPower = NULL;

MemorySystem::MemorySystem(uint id, string deviceIniFilename, string systemIniFilename, string pwd,
                           string traceFilename, unsigned int megsOfMemory, unsigned int numOfMemoryController) :
		ReturnReadData(NULL),
		WriteDataDone(NULL),
		systemID(0),
		deviceIniFilename(deviceIniFilename),
		systemIniFilename(systemIniFilename),
		traceFilename(traceFilename),
		pwd(pwd)
{
	currentClockCycle = 0;
	//set ID
	systemID = id;

	DEBUG("===== MemorySystem "<<systemID<<" =====");

	if (pwd.length() > 0)
	{
		//ignore the pwd argument if the argument is an absolute path
		if (deviceIniFilename[0] != '/')
		{
			deviceIniFilename = pwd + "/" + deviceIniFilename;
		}

		if (systemIniFilename[0] != '/')
		{
			systemIniFilename = pwd + "/" + systemIniFilename;
		}
	}




	DEBUG("== Loading device model file '"<<deviceIniFilename<<"' == ");
	IniReader::ReadIniFile(deviceIniFilename, false);
	DEBUG("== Loading system model file '"<<systemIniFilename<<"' == ");
	IniReader::ReadIniFile(systemIniFilename, true);

	//calculate the total storage based on the devices the user selected and the number of

	// number of bytes per rank
        uint64_t b1 = (long long int) (NUM_ROWS * (NUM_COLS * DEVICE_WIDTH) * NUM_BANKS);
        //cout <<"b1: "<<b1<<endl;
        uint64_t b2 = (long) (JEDEC_DATA_BUS_WIDTH / DEVICE_WIDTH);
        //cout <<"b2: "<<b2<<endl;
        uint64_t b3 = (b1 * b2)>>20;
        //cout <<"b3: "<<b3<<endl;
        b3 = b3/8;
        //cout <<"b3: "<<b3<<endl;
        unsigned long int megsOfStoragePerRank = b3;
        cout << "per rank: "<<megsOfStoragePerRank <<endl;

	// If this is set, effectively override the number of ranks
	if (megsOfMemory != 0)
	{
#ifndef MMC
		NUM_RANKS = megsOfMemory / megsOfStoragePerRank;
#else
		NUM_RANKS = megsOfMemory / megsOfStoragePerRank / NUM_CONTROLLERS;
#endif
		if (NUM_RANKS == 0)
		{
			PRINT("WARNING: Cannot create memory system with "<<megsOfMemory<<"MB, defaulting to minimum size of "<<megsOfStoragePerRank<<"MB");
			NUM_RANKS=1;
		}
	}

	NUM_DEVICES = 64/DEVICE_WIDTH;
#ifndef MMC
	TOTAL_STORAGE = (NUM_RANKS * megsOfStoragePerRank); 
#else
	TOTAL_STORAGE = (NUM_RANKS * megsOfStoragePerRank * NUM_CONTROLLERS); 
#endif
	DEBUG("TOTAL_STORAGE : "<< TOTAL_STORAGE << "MB | "<<NUM_RANKS<<" Ranks | "<< NUM_DEVICES <<" Devices per rank");

	IniReader::InitEnumsFromStrings();
	if (!IniReader::CheckIfAllSet())
	{
		exit(-1);
	}


	// TODO: change to other vector constructor?

#ifdef MMC //if using multiple memory controllers we must also have multiple vectors of rank; corresponding to 1 vector of rank per controller
	for (size_t j=0; j < NUM_CONTROLLERS; j++)
	{
		memoryController[j] = new MemoryController(this, &visDataOut);
		ranks[j] = new vector<Rank>();

		for (size_t i=0; i<NUM_RANKS; i++)
		{
			Rank r = Rank();
			r.setId( j * NUM_CONTROLLERS + i );
			r.attachMemoryController(memoryController[j]);
			ranks[j]->push_back(r);
		}
		memoryController[j]->attachRanks(ranks[j]);
	}
#else
	memoryController = new MemoryController(this, &visDataOut);
	ranks = new vector<Rank>();

	for (size_t i=0; i<NUM_RANKS; i++)
	{
		Rank r = Rank();
		r.setId(i);
		r.attachMemoryController(memoryController);
		ranks->push_back(r);
	}


	memoryController->attachRanks(ranks);
#endif

}


void MemorySystem::overrideSystemParam(string key, string value)
{
	cerr << "Override key " <<key<<"="<<value<<endl;
	IniReader::SetKey(key, value, true);
}

void MemorySystem::overrideSystemParam(string keyValuePair)
{
	size_t equalsign=-1;
	string overrideKey, overrideVal;
	//FIXME: could use some error checks on the string
	if ((equalsign = keyValuePair.find_first_of('=')) != string::npos) {
		overrideKey = keyValuePair.substr(0,equalsign);
		overrideVal = keyValuePair.substr(equalsign+1);
		overrideSystemParam(overrideKey, overrideVal);
	}
}

MemorySystem::~MemorySystem()
{
	/* the MemorySystem should exist for all time, nothing should be destroying it */  
//	ERROR_DRAM("MEMORY SYSTEM DESTRUCTOR with ID "<<systemID);
//	abort();

/*	delete(memoryController);
	ranks->clear();
	delete(ranks);
	visDataOut.flush();
	visDataOut.close();
	if (VERIFICATION_OUTPUT)
	{
		cmd_verify_out.flush();
		cmd_verify_out.close();
	}*/
#ifdef LOG_OUTPUT
	dramsim_log.flush();
	dramsim_log.close();
#endif
}

bool fileExists(string path)
{
	struct stat stat_buf;
	if (stat(path.c_str(), &stat_buf) != 0) 
	{
		if (errno == ENOENT)
		{
			return false; 
		}
	}
	return true;
}

string MemorySystem::SetOutputFileName(string traceFilename)
{
	size_t lastSlash;
	string deviceName, dramsimLogFilename;
	size_t deviceIniFilenameLength = deviceIniFilename.length();
	char *sim_description = NULL;
	string sim_description_str;
	
	sim_description = getenv("SIM_DESC"); 

#ifdef LOG_OUTPUT
	dramsimLogFilename = "dramsim"; 
	if (sim_description != NULL)
	{
		sim_description_str = string(sim_description);
		dramsimLogFilename += "."+sim_description_str; 
	}
	dramsimLogFilename += ".log";

	dramsim_log.open(dramsimLogFilename.c_str(), ios_base::out | ios_base::trunc );
	if (!dramsim_log) 
	{
		ERROR_DRAM("Cannot open "<< dramsimLogFilename);
		exit(-1); 
	}
#endif

	// create a properly named verification output file if need be
	if (VERIFICATION_OUTPUT)
	{
		string basefilename = deviceIniFilename.substr(deviceIniFilename.find_last_of("/")+1);
		string verify_filename =  "sim_out_"+basefilename;
		if (sim_description != NULL)
		{
			verify_filename += "."+sim_description_str;
		}
		verify_filename += ".tmp";
		cmd_verify_out.open(verify_filename.c_str());
		if (!cmd_verify_out)
		{
			ERROR_DRAM("Cannot open "<< verify_filename);
			exit(-1);
		}
	}

	// chop off the .ini if it's there
	if (deviceIniFilename.substr(deviceIniFilenameLength-4) == ".ini")
	{
		deviceName = deviceIniFilename.substr(0,deviceIniFilenameLength-4);
		deviceIniFilenameLength -= 4;
	}

	cout << deviceName << endl;

	// chop off everything past the last / (i.e. leave filename only)
	if ((lastSlash = deviceName.find_last_of("/")) != string::npos)
	{
		deviceName = deviceName.substr(lastSlash+1,deviceIniFilenameLength-lastSlash-1);
	}

	// working backwards, chop off the next piece of the directory
	if ((lastSlash = traceFilename.find_last_of("/")) != string::npos)
	{
		traceFilename = traceFilename.substr(lastSlash+1,traceFilename.length()-lastSlash-1);
	}
	if (sim_description != NULL)
	{
		traceFilename += "."+sim_description_str;
	}

	string rest;
	stringstream out,tmpNum,tmpSystemID;

	string path = "results/";
	string filename;
	if (pwd.length() > 0)
	{
		path = pwd + "/" + path;
	}

	// create the directories if they don't exist 
	mkdirIfNotExist(path);
	path = path + traceFilename + "/";
	mkdirIfNotExist(path);
	path = path + deviceName + "/";
	mkdirIfNotExist(path);

	// finally, figure out the filename
	string sched = "BtR";
	string queue = "pRank";
	if (schedulingPolicy == RankThenBankRoundRobin)
	{
		sched = "RtB";
	}
	if (queuingStructure == PerRankPerBank)
	{
		queue = "pRankpBank";
	}

	/* I really don't see how "the C++ way" is better than snprintf()  */
	out << (TOTAL_STORAGE>>10) << "GB." << NUM_CHANS << "Ch." << NUM_RANKS <<"R." <<ADDRESS_MAPPING_SCHEME<<"."<<ROW_BUFFER_POLICY<<"."<< TRANS_QUEUE_DEPTH<<"TQ."<<CMD_QUEUE_DEPTH<<"CQ."<<sched<<"."<<queue;
	if (sim_description)
	{
		out << "." << sim_description;
	}

	//filename so far, without .vis extension, see if it exists already
	filename = out.str();
	for (int i=0; i<100; i++)
	{
		if (fileExists(path+filename+tmpNum.str()+".vis"))
		{
			tmpNum.seekp(0);
			tmpNum << "." << i;
		}
		else 
		{
			filename = filename+tmpNum.str()+".vis";
			break;
		}
	}

	if (systemID!=0)
	{
		tmpSystemID<<"."<<systemID;
	}
	path.append(filename+tmpSystemID.str());
	
	return path;
}

void MemorySystem::mkdirIfNotExist(string path)
{
	struct stat stat_buf;
	// dwxr-xr-x on the results directories
	if (stat(path.c_str(), &stat_buf) != 0)
	{
		if (errno == ENOENT)
		{
			DEBUG("\t directory doesn't exist, trying to create ...");
			mode_t mode = (S_IXOTH | S_IXGRP | S_IXUSR | S_IROTH | S_IRGRP | S_IRUSR | S_IWUSR) ;
			if (mkdir(path.c_str(), mode) != 0)
			{
				perror("Error Has occurred while trying to make directory: ");
				cerr << path << endl;
				abort();
			}
		}
	}
	else
	{
		if (!S_ISDIR(stat_buf.st_mode))
		{
			ERROR_DRAM(path << "is not a directory");
			abort();
		}
	}
}

bool MemorySystem::WillAcceptTransaction()
{
	//return true;
	return memoryController->WillAcceptTransaction();
}

bool MemorySystem::addTransaction(bool isWrite, uint64_t addr, uint32_t _transId, uint32_t proc)
{
	TransactionType type = isWrite ? DATA_WRITE : DATA_READ;
	Transaction trans(type,addr,NULL,_transId);
	// push_back in memoryController will make a copy of this during
	// addTransaction so it's kosher for the reference to be local 

#ifdef MMC
	//1.) determine rank # of transaction
	//2.) send to appropriate mem controller
	//3.) rank mapping is sequential (i.e. 8 ranks and 4 MCs: rank 0-1 maps to MC #1)

        //map address to rank,bank,row,col
        uint newTransactionRank, newTransactionBank, newTransactionRow, newTransactionColumn;
        // pass these in as references so they get set by the addressMapping function
        memoryController[0]->addressMapping(trans.address, newTransactionRank, newTransactionBank, newTransactionRow, newTransactionColumn);
	if (memoryController[newTransactionRank%NUM_CONTROLLERS]->WillAcceptTransaction())
	{
		return memoryController[newTransactionRank%NUM_CONTROLLERS]->addTransaction(trans);
	}
	else
	{
		pendingTransactions[newTransactionRank%NUM_CONTROLLERS].push_back(trans);
		return true;
	}
#else
	if (memoryController->WillAcceptTransaction()) 
	{
		return memoryController->addTransaction(trans); // will be true
	}
	else
	{
		pendingTransactions.push_back(trans);
		return true;
	}
#endif
	//cout<<"addTr; address: "<<addr<<" Rank: "<<newTransactionRank;

}

bool MemorySystem::addTransaction(Transaction &trans)
{
#ifdef MMC
        uint newTransactionRank, newTransactionBank, newTransactionRow, newTransactionColumn;
        memoryController[0]->addressMapping(trans.address, newTransactionRank, newTransactionBank, newTransactionRow, newTransactionColumn);
	return memoryController[newTransactionRank%NUM_CONTROLLERS]->addTransaction(trans);
#else
	return memoryController->addTransaction(trans);
#endif
}

//prints statistics
void MemorySystem::printStats()
{
#ifdef MMC
	for (size_t i=0; i < NUM_CONTROLLERS; i++)
	{
		memoryController[i]->printStats(true);	
	}
#else
	memoryController->printStats(true);
#endif
}

void MemorySystem::printStats(bool)
{
	printStats();
}


//update the memory systems state
void MemorySystem::update()
{
	if (currentClockCycle == 0)
	{
		string visOutputFilename = SetOutputFileName(traceFilename);
		cerr << "writing vis file to " <<visOutputFilename<<endl;
		visDataOut.open(visOutputFilename.c_str());
		if (!visDataOut)
		{
			ERROR_DRAM("Cannot open '"<<visOutputFilename<<"'");
			exit(-1);
		}

		//write out the ini config values for the visualizer tool
		IniReader::WriteValuesOut(visDataOut);
	}
	//PRINT(" ----------------- Memory System Update ------------------");

	//updates the state of each of the objects
	// NOTE - do not change order
#ifdef MMC
	//PRINT(" ----------------- MMC Memory System Update ------------------");
	for (size_t j=0; j < NUM_CONTROLLERS; j++)
	{
		for (size_t k=0; k < NUM_RANKS; k++){
			(*ranks[j])[k].update();
		}	
	}

	for (size_t j=0; j < NUM_CONTROLLERS; j++)
	{
		if (pendingTransactions[j].size() > 0 && memoryController[j]->WillAcceptTransaction())
		{
			memoryController[j]->addTransaction(pendingTransactions[j].front());
			pendingTransactions[j].pop_front();	
		}
	}

	for (size_t j=0; j < NUM_CONTROLLERS; j++)
	{
		memoryController[j]->update();
	}

	//PRINT(" ----------------- MMC Memory System Update ------------------");
	for (size_t j=0; j < NUM_CONTROLLERS; j++)
	{
		for (size_t k=0;k<NUM_RANKS;k++){
			(*ranks[j])[k].step();
		}	
	}
	//PRINT(" ----------------- MMC Memory System Update ------------------");
	for (size_t j=0;j < NUM_CONTROLLERS; j++)
	{
		memoryController[j]->step();
	}
	//PRINT(" ----------------- MMC Memory System Update ------------------");
#else
	for (size_t i=0;i<NUM_RANKS;i++)
	{
		(*ranks)[i].update();
	}

	//pendingTransactions will only have stuff in it if MARSS is adding stuff
	if (pendingTransactions.size() > 0 && memoryController->WillAcceptTransaction())
	{
		memoryController->addTransaction(pendingTransactions.front());
		pendingTransactions.pop_front();
	}
	memoryController->update();

	//simply increments the currentClockCycle field for each object
	for (size_t i=0;i<NUM_RANKS;i++)
	{
		(*ranks)[i].step();
	}
	memoryController->step();
#endif
	this->step();

	//PRINT("\n"); // two new lines
}

void MemorySystem::RegisterCallbacks( Callback_t* readCB, Callback_t* writeCB)//,
                                      //void (*reportPower)(double bgpower, double burstpower,
                                                          //double refreshpower, double actprepower))
{
	ReturnReadData = readCB;
	WriteDataDone = writeCB;
	//ReportPower = reportPower;
}

// static allocator for the library interface 
MemorySystem *getMemorySystemInstance(uint id, string dev, string sys, string pwd, string trc, unsigned megsOfMemory, unsigned int numOfMemoryController)
{
	return new MemorySystem(id, dev, sys, pwd, trc, megsOfMemory, numOfMemoryController);
}

} /*namespace DRAMSim */

unsigned int MemorySystem::getRank(uint64_t addr)
{
   uint newTransactionRank, newTransactionBank, newTransactionRow, newTransactionColumn;
#ifdef MMC	
   memoryController[0]->addressMapping(addr, newTransactionRank, newTransactionBank, newTransactionRow, newTransactionColumn);
#else
   memoryController->addressMapping(addr, newTransactionRank, newTransactionBank, newTransactionRow, newTransactionColumn);
#endif
//   cout<<"address: "<<addr<<" on rank: "<<newTransactionRank<<endl;
//	cout<<"rank: "<<newTransactionRank<<endl;
   return newTransactionRank;
}

// This function can be used by autoconf AC_CHECK_LIB since
// apparently it can't detect C++ functions.
// Basically just an entry in the symbol table
extern "C"
{
	void libdramsim_is_present(void)
	{
		;
	}
}

void MemorySystem::printBitWidths(){

#ifdef MMC
	memoryController[0]->printBitWidths();
#else
	memoryController->printBitWidths();
#endif
}
