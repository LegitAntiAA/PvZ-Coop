#ifndef __NETENDLESSWIRETESTS_H__
#define __NETENDLESSWIRETESTS_H__

struct NetEndlessWireTestResult
{
	int mChecks;
	int mFailures;
};

NetEndlessWireTestResult RunNetEndlessWireTests();

#endif // __NETENDLESSWIRETESTS_H__
