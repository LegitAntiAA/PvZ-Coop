#ifndef __NETENDLESSWEATHERTESTS_H__
#define __NETENDLESSWEATHERTESTS_H__

struct NetEndlessWeatherTestResult
{
	int mChecks;
	int mFailures;
};

NetEndlessWeatherTestResult RunNetEndlessWeatherTests();

#endif
