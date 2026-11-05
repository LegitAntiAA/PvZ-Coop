#ifndef __NETMOWERSNAPSHOTTESTS_H__
#define __NETMOWERSNAPSHOTTESTS_H__

struct NetMowerSnapshotTestResult
{
	int mChecks;
	int mFailures;
};

NetMowerSnapshotTestResult RunNetMowerSnapshotTests();

#endif // __NETMOWERSNAPSHOTTESTS_H__
