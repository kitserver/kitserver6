//filedec.cpp

#include <windows.h>
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <string>

#ifdef USE_HASHMAPS
	#include <hash_map>
#else
	#include <map>
#endif

#ifdef USE_HASHMAPS
	/*
	needs this here for compiling:
	http://www.sgi.com/tech/stl/
	*/
	std::hash_map<DWORD,DWORD> testMap;
	std::hash_map<DWORD,DWORD>::iterator testMapIt;
#else
	std::map<DWORD,DWORD> testMap;
	std::map<DWORD,DWORD>::iterator testMapIt;
#endif
	
DWORD numEntries=1000;
DWORD numRuns=50;
#define maxEntries 500000
#define maxRuns 1000

int main(int argc, char* argv[]);

int main(int argc, char* argv[])
{
	DWORD choice;

	DWORD timeBefore=0, timeAfter=0;
	
	printf("This is a small performance test for Bootserver.\n");
	printf("Please run this test with varying parameters and\n");
	printf("report your results together with your CPU on the\n");
	printf("forum at WEvolution.org. Thank you for your feedback!\n");
	printf("Please don't run PES or other applications while testing.\n");
	printf("However, you don't need to close you anti-virus software.\n\n");
	
	printf("Enter 0 to use the default setting.\n");
	printf("1. Number of map entries (default: %d) ", numEntries);
	choice=0;
	if (scanf("%d",&choice)==1 && choice != 0) {
		numEntries=choice;
		if (numEntries>maxEntries) {
			numEntries=maxEntries;
			printf("Such a high value isn't needed. Reset to %d.\n", numEntries);
		};
	};
			
		
	printf("2. Number of runs, in 100000 (default: %d) ", numRuns);
	choice=0;
	if (scanf("%d",&choice)==1 && choice != 0) {
		numRuns=choice;
		if (numRuns>maxRuns) {
			numRuns=maxRuns;
			printf("Such a high value isn't needed. Reset to %d.\n", numRuns*100000);
		};
	};
	numRuns*=100000;
		
	printf("\nStarting tests now, this might take some time, depending\n");
	printf("on the parameters given. If it should take too long, you\n");
	printf("can close this window.\n\n");
	
	//TESTING
	
	//fill map
	for (int i=1; i<=numEntries; i++)
		testMap[i] = i;

	//take the time
	timeBefore=GetTickCount();
	
	//do a lot of unsuccessful find commands (worst case)
	for (DWORD run=0; run<numRuns; run++)
		testMapIt=testMap.find(numEntries+10);
	
	//take time again
	timeAfter=GetTickCount();
	
	//free memory
	testMap.clear();
	
	printf("Tests finished now. Time needed: %d ms\n", timeAfter-timeBefore);
	printf("You can run this program again with other paramters now.\n\n");

	system("PAUSE");
	return 0;
}
