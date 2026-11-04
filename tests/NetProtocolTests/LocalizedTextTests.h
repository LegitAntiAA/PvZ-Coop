#ifndef __LOCALIZEDTEXTTESTS_H__
#define __LOCALIZEDTEXTTESTS_H__

struct LocalizedTextTestResult
{
	int mChecks;
	int mFailures;
	int mSkipped;
};

LocalizedTextTestResult RunLocalizedTextTests();

#endif // __LOCALIZEDTEXTTESTS_H__
