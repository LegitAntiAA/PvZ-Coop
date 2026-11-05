#ifndef __NETCHALLENGERUNTIMETESTS_H__
#define __NETCHALLENGERUNTIMETESTS_H__

struct NetChallengeRuntimeTestResult
{
	int mChecks;
	int mFailures;
};

NetChallengeRuntimeTestResult RunNetChallengeRuntimeTests();

#endif // __NETCHALLENGERUNTIMETESTS_H__
