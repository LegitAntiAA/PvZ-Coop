#ifndef FUSION_SIMULATION_TESTS_H
#define FUSION_SIMULATION_TESTS_H

struct FusionSimulationTestResult
{
    int checks;
    int failures;
};

FusionSimulationTestResult RunFusionSimulationTests();

#endif
